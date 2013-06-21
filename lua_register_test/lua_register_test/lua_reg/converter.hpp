#ifndef __LUA_REG_CONVERSION_HPP
#define __LUA_REG_CONVERSION_HPP


#include <map>
#include <vector>
#include <tuple>
#include <string>
#include <type_traits>
#include <cassert>
#include <cstdint>
#include <algorithm>

#include "config.hpp"
#include "state.hpp"

#include "details/converter.hpp"

namespace luareg {
	
	template < typename T, typename EnableT = void >
	struct convertion_t;

	template < typename T >
	struct convertion_t<T, 
		typename std::enable_if<std::is_integral<T>::value || std::is_floating_point<T>::value>::type >
	{
		static T from(state_t &state, int index)
		{
			assert(::lua_isnumber(state, index) != 0);

			return static_cast<T>(::lua_tonumber(state, index));
		}

		static std::uint32_t to(state_t &state, T val)
		{
			::lua_pushnumber(state, static_cast<::lua_Number>(val));
			return 1;
		}
	};

	template <>
	struct convertion_t<bool>
	{
		static bool from(state_t &state, int index)
		{
			assert(lua_isboolean(state, index) != 0);
			int n = ::lua_toboolean(state, index);
			return n != 0;
		}

		static std::uint32_t to(state_t &state, bool val)
		{
			::lua_pushboolean(state, val);
			return 1;
		}
	};


	template < >
	struct convertion_t< const char * >
	{
		static std::string from(state_t &state, int index)
		{
			assert(::lua_isstring(state, index) != 0);
			return ::lua_tostring(state, index);
		}

		static std::uint32_t to(state_t &state, const char *val)
		{
			::lua_pushstring(state, val);
			return 1;
		}
	};

	template < std::uint32_t N >
	struct convertion_t< char [N] >
	{
		static std::string from(state_t &state, int index)
		{
			assert(::lua_isstring(state, index) != 0);
			return ::lua_tostring(state, index);
		}

		static std::uint32_t to(state_t &state, const char *val)
		{
			::lua_pushstring(state, val);
			return 1;
		}
	};

	template < typename CharT, typename CharTraitsT, typename AllocatorT >
	struct convertion_t< std::basic_string<CharT, CharTraitsT, AllocatorT> >
	{
		static std::basic_string<CharT, CharTraitsT, AllocatorT> from(state_t &state, int index)
		{
			assert(::lua_isstring(state, index) != 0);
			return ::lua_tostring(state, index);
		}

		static std::uint32_t to(state_t &state, const std::basic_string<CharT, CharTraitsT, AllocatorT> &val)
		{
			::lua_pushstring(state, val.c_str());
			return 1;
		}
	};

	// struct nil

	template < typename FirstT, typename SecondT >
	struct convertion_t< std::pair<FirstT, SecondT> >
	{
		typedef std::pair<FirstT, SecondT> pair_t;

		static pair_t from(state_t &state, int index)
		{
			assert(lua_istable(state, index));

			const int len = ::lua_objlen(state, index);
			assert(len == 2);

			::lua_rawgeti(state, index, 1);
			FirstT first_val = convertion_t<FirstT>::from(state, -1);
			lua_pop(state, 1);

			::lua_rawgeti(state, index, 2);
			SecondT second_val = convertion_t<SecondT>::from(state, -1);
			lua_pop(state, 1);

			return std::make_pair(first_val, second_val);
		}

		static std::uint32_t to(state_t &state, const pair_t &val)
		{
			::lua_createtable(state, 2, 0);

			convertion_t<FirstT>::to(state, val.first);
			::lua_rawseti(state, -2, 1);

			convertion_t<SecondT>::to(state, val.second);
			::lua_rawseti(state, -2, 2);

			return 1;
		}
	};

	template < typename T >
	struct convertion_t< std::pair<const T *, std::uint32_t> >
	{
		static_assert(std::is_pod<T>::value, "T must be a pod type");

		static std::pair<const T *, std::uint32_t> from(state_t &state, int index)
		{
			std::uint32_t len = 0;
			const char *p = ::lua_tolstring(state, index, &len);

			return std::make_pair(reinterpret_cast<const T *>(p), len / sizeof(T));
		}

		static std::uint32_t to(state_t &state, const std::pair<const T *, std::uint32_t> &val)
		{
			::lua_pushlstring(state, reinterpret_cast<const char *>(val.first), val.second * sizeof(T));
			return 1;
		}
	};

	// for MS BUG Args...
	template < 
		typename T1, 
		typename T2, 
		typename T3, 
		typename T4,
		typename T5,
		typename T6,
		typename T7,
		typename T8
	>
	struct convertion_t< std::tuple<T1, T2, T3, T4, T5, T6, T7, T8> >
	{
		typedef std::tuple<T1, T2, T3, T4, T5, T6, T7, T8> tuple_t;

		static tuple_t from(state_t &state, int index)
		{
			assert(lua_istable(state, index));

			const int len = ::lua_objlen(state, index);
			assert(len == std::tuple_size<tuple_t>::value);
			if( len != std::tuple_size<tuple_t>::value )
				return tuple_t();

			tuple_t val;
			details::tuple_helper_t<tuple_t, std::tuple_size<tuple_t>::value>::from(state, index, val);

			return val;
		}

		static std::uint32_t to(state_t &state, const tuple_t &val)
		{
			::lua_createtable(state, std::tuple_size<tuple_t>::value, 0);

			details::tuple_helper_t<tuple_t, std::tuple_size<tuple_t>::value>::to(state, val);

			return 1;
		}
	};

	template < typename T, typename AllocatorT >
	struct convertion_t< std::vector<T, AllocatorT> >
	{
		typedef std::vector<T, AllocatorT> vector_t;

		static vector_t from(state_t &state, int index)
		{
			assert(lua_istable(state, index));

			vector_t vec;
			const int len = ::lua_objlen(state, index);
			vec.reserve(len);

			for(auto i = 1; i <= len; ++i) 
			{
				::lua_rawgeti(state, index, i);
				vec.push_back(static_cast<T>(convertion_t<T>::from(state, -1)));
				lua_pop(state, 1);
			}

			return vec;
		}

		static std::uint32_t to(state_t &state, const vector_t &val)
		{
			::lua_createtable(state, val.size(), 0);

			std::uint32_t i = 1;
			std::for_each(val.cbegin(), val.cend(), 
				[&state, &i](const T &t)
			{
				convertion_t<T>::to(state, t);
				::lua_rawseti(state, -2, i++);
			});

			return val.size() == 0 ? 0 : 1;
		}
	};

	template < typename KeyT, typename ValueT, typename CompareT, typename AllocatorT >
	struct convertion_t< std::map<KeyT, ValueT, CompareT, AllocatorT> >
	{
		typedef std::map<KeyT, ValueT, CompareT, AllocatorT> map_t;

		static map_t from(state_t &state, int index)
		{
			assert(lua_istable(state, index));

			map_t map_val;
			const int len = ::lua_objlen(state, index);

			for(auto i = 1; i <= len; ++i) 
			{
				::lua_rawgeti(state, index, i);

				typedef typename map_t::value_type value_t;
				value_t val = convertion_t<value_t>::from(state, -1);
				map_val.insert(std::move(val));

				lua_pop(state, 1);
			}
			return map_val;
		}

		static std::uint32_t to(state_t &state, const map_t &map_val)
		{
			::lua_createtable(state, map_val.size(), 0);

			std::for_each(map_val.cbegin(), map_val.cend(), 
				[&state](const typename map_t::value_type &val)
			{
				convertion_t<typename map_t::key_type>::to(state, val.first);
				convertion_t<typename map_t::mapped_type>::to(state, val.second);

				::lua_settable(state, -3);
			});

			return map_val.size() == 0 ? 0 : 1;
		}
	};
}

#endif