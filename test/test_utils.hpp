/***************************************************************************
* Copyright (c) Johan Mabille, Sylvain Corlay, Wolf Vollprecht and         *
* Martin Renou                                                             *
* Copyright (c) QuantStack                                                 *
*                                                                          *
* Distributed under the terms of the BSD 3-Clause License.                 *
*                                                                          *
* The full license is in the file LICENSE, distributed with this software. *
****************************************************************************/

#include <array>
#include <climits>
#include <limits>
#include <sstream>
#include <type_traits>
#include <vector>

#include "gtest/gtest.h"

#include "xsimd/xsimd.hpp"

#ifndef XSIMD_TEST_UTILS_HPP
#define XSIMD_TEST_UTILS_HPP

/*******************
 * Pretty printers *
 *******************/

class simd_test_names
{
public:

    template <class T>
    static std::string GetName(int)
    {
        using value_type = typename T::value_type;
        std::string prefix = "fallback_";
#if XSIMD_X86_INSTR_SET >= XSIMD_X86_SSE2_VERSION
        size_t register_size = T::size * sizeof(value_type) * CHAR_BIT;
        if (register_size == size_t(128))
        {
            prefix = "sse_";
        }
        else if (register_size == size_t(256))
        {
            prefix = "avx_";
        }
        else if (register_size == size_t(512))
        {
            prefix = "avx512_";
        }
#elif XSIMD_ARM_INSTR_SET >= XSIMD_ARM7_NEON_VERSION
        size_t register_size = T::size * sizeof(value_type) * CHAR_BIT;
        if (register_size == size_t(128))
        {
            prefix = "arm_";
        }
#endif
        if (std::is_same<value_type, uint8_t>::value) { return prefix + "uint8_t"; }
        if (std::is_same<value_type, int8_t>::value) { return prefix + "int8_t"; }
        if (std::is_same<value_type, uint16_t>::value) { return prefix + "uint16_t"; }
        if (std::is_same<value_type, int16_t>::value) { return prefix + "int16_t"; }
        if (std::is_same<value_type, uint32_t>::value) { return prefix + "uint32_t"; }
        if (std::is_same<value_type, int32_t>::value) { return prefix + "int32_t"; }
        if (std::is_same<value_type, uint64_t>::value) { return prefix + "uint64_t"; }
        if (std::is_same<value_type, int64_t>::value) { return prefix + "int64_t"; }
        if (std::is_same<value_type, float>::value) { return prefix + "float"; }
        if (std::is_same<value_type, double>::value) { return prefix + "double"; }
        if (std::is_same<value_type, std::complex<float>>::value) { return prefix + "complex<float>"; }
        if (std::is_same<value_type, std::complex<double>>::value) { return prefix + "complex<double>"; }
#ifdef XSIMD_ENABLE_XTL_COMPLEX
        if (std::is_same<value_type, xtl::xcomplex<float>>::value) { return prefix + "xcomplex<float>"; }
        if (std::is_same<value_type, xtl::xcomplex<double>>::value) { return prefix + "xcomplex<double>"; }
#endif

        return prefix + "unknow_type";
    }
};

inline std::string print_function_name(const std::string& func)
{
    return std::string("  while testing ") + func;
}

/************************
 * Comparison functions *
 ************************/

namespace xsimd
{
    template <class T, size_t N>
    inline bool operator==(const batch<T, N>& lhs, const std::array<T, N>& rhs)
    {
        std::array<T, N> tmp;
        lhs.store_unaligned(tmp.data());
        return tmp == rhs;
    }

    template <class T, size_t N>
    inline bool operator==(const std::array<T, N>& lhs, const batch<T, N>& rhs)
    {
        return rhs == lhs;
    }
}

namespace detail
{
    namespace utils
    {
        // define some overloads here as integer versions do not exist for msvc
        template <class T>
        inline typename std::enable_if<!std::is_integral<T>::value, bool>::type isinf(const T& c)
        {
            return std::isinf(c);
        }

        template <class T>
        inline typename std::enable_if<std::is_integral<T>::value, bool>::type isinf(const T&)
        {
            return false;
        }

        template <class T>
        inline typename std::enable_if<!std::is_integral<T>::value, bool>::type isnan(const T& c)
        {
            return std::isnan(c);
        }

        template <class T>
        inline typename std::enable_if<std::is_integral<T>::value, bool>::type isnan(const T&)
        {
            return false;
        }
    }

    template <class T>
    inline T uabs(T val)
    {
        return std::abs(val);
    }

    inline uint32_t uabs(uint32_t val)
    {
        return val;
    }

    inline uint64_t uabs(uint64_t val)
    {
        return val;
    }

#ifdef XSIMD_32_BIT_ABI
    inline unsigned long uabs(unsigned long val)
    {
        return val;
    }
#endif

    template <class T>
    bool check_is_small(const T& value, const T& tolerance)
    {
        using std::abs;
        return uabs(value) < uabs(tolerance);
    }

    template <class T>
    T safe_division(const T& lhs, const T& rhs)
    {
        if (rhs < static_cast<T>(1) && lhs > rhs * (std::numeric_limits<T>::max)())
        {
            return (std::numeric_limits<T>::max)();
        }
        if (lhs == static_cast<T>(0) ||
            rhs > static_cast<T>(1) &&
                lhs < rhs * (std::numeric_limits<T>::min)())
        {
            return static_cast<T>(0);
        }
        return lhs / rhs;
    }

    template <class T>
    bool check_is_close(const T& lhs, const T& rhs, const T& relative_precision)
    {
        using std::abs;
        T diff = uabs(lhs - rhs);
        T d1 = safe_division(diff, T(uabs(rhs)));
        T d2 = safe_division(diff, T(uabs(lhs)));

        return d1 <= relative_precision && d2 <= relative_precision;
    }

    template <class T>
    struct scalar_comparison_near
    {
        static bool run(const T& lhs, const T& rhs)
        {
            using std::max;
            using std::abs;

            // direct compare integers -- but need tolerance for inexact double conversion
            if (std::is_integral<T>::value && lhs < 10e6 && rhs < 10e6)
            {
                return lhs == rhs;
            }

            if (utils::isnan(lhs))
            {
                return utils::isnan(rhs);
            }

            if (utils::isinf(lhs))
            {
                return utils::isinf(rhs) && (lhs * rhs > 0) /* same sign */;
            }

            T relative_precision = 2048 * std::numeric_limits<T>::epsilon();
            T absolute_zero_prox = 2048 * std::numeric_limits<T>::epsilon();

            if (max(uabs(lhs), uabs(rhs)) < T(1e-3))
            {
                using res_type = decltype(lhs - rhs);
                return detail::check_is_small(lhs - rhs, res_type(absolute_zero_prox));
            }
            else
            {
                return detail::check_is_close(lhs, rhs, relative_precision);
            }
        }
    };

    template <class T>
    struct scalar_comparison
    {
        static bool run(const T& lhs, const T& rhs)
        {
            return lhs == rhs;
        }
    };

    template <>
    struct scalar_comparison<float> : scalar_comparison_near<float>
    {
    };

    template <>
    struct scalar_comparison<double> : scalar_comparison_near<double>
    {
    };

    template <class T>
    struct scalar_comparison<std::complex<T>>
    {
        static bool run(const std::complex<T>& lhs, const std::complex<T>& rhs)
        {
            using real_comparison = scalar_comparison<T>;
            return real_comparison::run(lhs.real(), rhs.real()) &&
                real_comparison::run(lhs.imag(), rhs.imag());
        }
    };

#ifdef XSIMD_ENABLE_XTL_COMPLEX
    template <class T, bool i3ec>
    struct scalar_comparison<xtl::xcomplex<T, T, i3ec>>
    {
        static bool run(const xtl::xcomplex<T, T, i3ec>& lhs, const xtl::xcomplex<T, T, i3ec>& rhs)
        {
            using real_comparison = scalar_comparison<T>;
            return real_comparison::run(lhs.real(), rhs.real()) &&
                real_comparison::run(lhs.imag(), rhs.imag());
        }
    };
#endif

    template <class V>
    struct vector_comparison
    {
        static bool run(const V& lhs, const V& rhs)
        {
            using value_type = typename V::value_type;
            for (size_t i = 0; i < lhs.size(); ++i)
            {
                if (!scalar_comparison<value_type>::run(lhs[i], rhs[i]))
                    return false;
            }
            return true;
        }
    };

    template <class T>
    testing::AssertionResult expect_scalar_near(const char* lhs_expression,
                                                const char* rhs_expression,
                                                const T& lhs,
                                                const T& rhs)
    {
        if (scalar_comparison<T>::run(lhs, rhs))
        {
            return testing::AssertionSuccess();
        }

        std::stringstream lhs_ss;
        lhs_ss << std::setprecision(std::numeric_limits<T>::digits10 + 2)
               << lhs;

        std::stringstream rhs_ss;
        rhs_ss << std::setprecision(std::numeric_limits<T>::digits10 + 2)
               << rhs;

        return testing::internal::EqFailure(lhs_expression,
                                            rhs_expression,
                                            lhs_ss.str(),
                                            rhs_ss.str(),
                                            false);
    }

    template <class V>
    testing::AssertionResult expect_container_near(const char* lhs_expression,
                                                   const char* rhs_expression,
                                                   const V& lhs,
                                                   const V& rhs)
    {
        if (vector_comparison<V>::run(lhs, rhs))
        {
            return testing::AssertionSuccess();
        }

        using value_type = typename V::value_type;
        std::stringstream lhs_ss;
        lhs_ss << std::setprecision(std::numeric_limits<value_type>::digits10 + 2);
        testing::internal::PrintTo(lhs, &lhs_ss);

        std::stringstream rhs_ss;
        rhs_ss << std::setprecision(std::numeric_limits<value_type>::digits10 + 2);
        testing::internal::PrintTo(rhs, &rhs_ss);

        return testing::internal::EqFailure(lhs_expression,
                                            rhs_expression,
                                            lhs_ss.str(),
                                            rhs_ss.str(),
                                            false);
    }

    template <class T, size_t N>
    testing::AssertionResult expect_array_near(const char* lhs_expression,
                                               const char* rhs_expression,
                                               const std::array<T, N>& lhs,
                                               const std::array<T, N>& rhs)
    {
        return expect_container_near(lhs_expression, rhs_expression, lhs, rhs);
    }

    template <class T, class A>
    testing::AssertionResult expect_vector_near(const char* lhs_expression,
                                                const char* rhs_expression,
                                                const std::vector<T, A>& lhs,
                                                const std::vector<T, A>& rhs)
    {
        return expect_container_near(lhs_expression, rhs_expression, lhs, rhs);
    }

    template <class T, size_t N>
    testing::AssertionResult expect_batch_near(const char* lhs_expression,
                                               const char* rhs_expression,
                                               const ::xsimd::batch<T, N>& lhs,
                                               const std::array<T, N>& rhs)
    {
        std::array<T, N> tmp;
        lhs.store_unaligned(tmp.data());
        return expect_array_near(lhs_expression, rhs_expression, tmp, rhs);
    }

    template <class T, size_t N>
    testing::AssertionResult expect_batch_near(const char* lhs_expression,
                                               const char* rhs_expression,
                                               const std::array<T, N>& lhs,
                                               const ::xsimd::batch<T, N>& rhs)
    {
        std::array<T, N> tmp;
        rhs.store_unaligned(tmp.data());
        return expect_array_near(lhs_expression, rhs_expression, lhs, tmp);

    }

    template <class T, size_t N>
    testing::AssertionResult expect_batch_near(const char* lhs_expression,
                                               const char* rhs_expression,
                                               const ::xsimd::batch<T, N>& lhs,
                                               const ::xsimd::batch<T, N>& rhs)
    {
        std::array<T, N> tmp;
        lhs.store_unaligned(tmp.data());
        return expect_batch_near(lhs_expression, rhs_expression, tmp, rhs);
    }

    template <class T, size_t N>
    testing::AssertionResult expect_batch_near(const char* lhs_expression,
                                               const char* rhs_expression,
                                               const ::xsimd::batch_bool<T, N>& lhs,
                                               const std::array<bool, N>& rhs)
    {
        std::array<bool, N> tmp;
        lhs.store_unaligned(tmp.data());
        return expect_array_near(lhs_expression, rhs_expression, tmp, rhs);
    }

    template <class T, size_t N>
    testing::AssertionResult expect_batch_near(const char* lhs_expression,
                                               const char* rhs_expression,
                                               const std::array<bool, N>& lhs,
                                               const ::xsimd::batch_bool<T, N>& rhs)
    {
        std::array<bool, N> tmp;
        rhs.store_unaligned(tmp.data());
        return expect_array_near(lhs_expression, rhs_expression, lhs, tmp);
    }

    template <class T, size_t N>
    testing::AssertionResult expect_batch_near(const char* lhs_expression,
                                               const char* rhs_expression,
                                               const ::xsimd::batch_bool<T, N>& lhs,
                                               const ::xsimd::batch_bool<T, N>& rhs)
    {
        std::array<bool, N> tmp;
        lhs.store_unaligned(tmp.data());
        return expect_batch_near(lhs_expression, rhs_expression, tmp, rhs);
    }

    template <class It>
    size_t get_nb_diff(It lhs_begin, It lhs_end, It rhs_begin)
    {
        size_t res = 0;
        using value_type = typename std::iterator_traits<It>::value_type;
        while (lhs_begin != lhs_end)
        {
            if (!scalar_comparison<value_type>::run(*lhs_begin++, *rhs_begin++))
            {
                ++res;
            }
        }
        return res;
    }

    template <class T, class A>
    size_t get_nb_diff(const std::vector<T, A>& lhs, const std::vector<T, A>& rhs)
    {
        return get_nb_diff(lhs.begin(), lhs.end(), rhs.begin());
    }

    template <class T, size_t N>
    size_t get_nb_diff(const std::array<T, N>& lhs, const std::array<T, N>& rhs)
    {
        return get_nb_diff(lhs.begin(), lhs.end(), rhs.begin());
    }

    template <class B, class S>
    void load_batch(B& b, const S& src, size_t i = 0)
    {
        b.load_unaligned(src.data() + i);
    }

    template <class B, class D>
    void store_batch(const B& b, D& dst, size_t i = 0)
    {
        b.store_unaligned(dst.data() + i);
    }
}

#define EXPECT_BATCH_EQ(b1, b2) EXPECT_PRED_FORMAT2(::detail::expect_batch_near, b1, b2)
#define EXPECT_SCALAR_EQ(s1, s2) EXPECT_PRED_FORMAT2(::detail::expect_scalar_near, s1, s2)
#define EXPECT_VECTOR_EQ(v1, v2) EXPECT_PRED_FORMAT2(::detail::expect_vector_near, v1, v2)

namespace xsimd
{
    /************************
     * Enable metafunctions *
     ************************/

    // Backport of C++14 std::enable_if
    template <bool B, class T = void>
    using enable_if_t = typename std::enable_if<B, T>::type;

    template <class T, class R>
    using enable_integral_t = enable_if_t<std::is_integral<T>::value, R>;

    template <class T, class R>
    using enable_floating_point_t = enable_if_t<std::is_floating_point<T>::value, R>;

    namespace mpl
    {
        /**************
         * types_list *
         **************/
        template <class... T>
        struct type_list {};

        /***************
         * concatenate *
         ***************/

        template <class... TL>
        struct concatenate;

        template <template <class...> class TL, class... T, class... U>
        struct concatenate<TL<T...>, TL<U...>>
        {
            using type = TL<T..., U...>;
        };

        template <class... TL>
        using concatenate_t = typename concatenate<TL...>::type;

        /********
         * cast *
         ********/

        template <class S, template <class...> class D>
        struct cast;

        template <template <class...> class S, class... T, template <class...> class D>
        struct cast<S<T...>, D>
        {
            using type = D<T...>;
        };

        template <class S, template <class...> class D>
        using cast_t = typename cast<S, D>::type;
    }
}

/***********************
 * Testing types lists *
 ***********************/

template <class T>
using to_testing_types = xsimd::mpl::cast_t<T, testing::Types>;

namespace xsimd
{
    using batch_int_type_list = mpl::type_list<
#if XSIMD_X86_INSTR_SET >= XSIMD_X86_SSE2_VERSION
                                    batch<uint8_t, 16>,
                                    batch<int8_t, 16>,
                                    batch<uint16_t, 8>,
                                    batch<int16_t, 8>,
                                    batch<uint32_t, 4>,
                                    batch<int32_t, 4>,
                                    batch<uint64_t, 2>,
                                    batch<int64_t, 2>
#endif
#if XSIMD_X86_INSTR_SET >= XSIMD_X86_AVX_VERSION
                                    ,
                                    batch<uint8_t, 32>,
                                    batch<int8_t, 32>,
                                    batch<uint16_t, 16>,
                                    batch<int16_t, 16>,
                                    batch<uint32_t, 8>,
                                    batch<int32_t, 8>,
                                    batch<uint64_t, 4>,
                                    batch<int64_t, 4>
#endif
#if XSIMD_X86_INSTR_SET >= XSIMD_X86_AVX512_VERSION
                                    ,
                                    batch<uint8_t, 64>,
                                    batch<int8_t, 64>,
                                    batch<uint16_t, 32>,
                                    batch<int16_t, 32>,
                                    batch<uint32_t, 16>,
                                    batch<int32_t, 16>,
                                    batch<uint64_t, 8>,
                                    batch<int64_t, 8>
#endif
#if XSIMD_ARM_INSTR_SET >= XSIMD_ARM7_NEON_VERSION
                                    batch<uint8_t, 16>,
                                    batch<int8_t, 16>,
                                    batch<uint16_t, 8>,
                                    batch<int16_t, 8>,
                                    batch<uint32_t, 4>,
                                    batch<int32_t, 4>,
                                    batch<uint64_t, 2>,
                                    batch<int64_t, 2>
#endif
#if defined(XSIMD_ENABLE_FALLBACK)
                                    ,
                                    batch<int32_t, 7>,
                                    batch<int64_t, 3>
#endif
                              >;

    using batch_float_type_list = mpl::type_list<
#if XSIMD_X86_INSTR_SET >= XSIMD_X86_SSE2_VERSION
                                     batch<float, 4>,
                                     batch<double, 2>
#endif
#if XSIMD_X86_INSTR_SET >= XSIMD_X86_AVX_VERSION
                                     ,
                                     batch<float, 8>,
                                     batch<double, 4>
#endif
#if XSIMD_X86_INSTR_SET >= XSIMD_X86_AVX512_VERSION
                                     ,
                                     batch<float, 16>,
                                     batch<double, 8>
#endif
#if XSIMD_ARM_INSTR_SET >= XSIMD_ARM7_NEON_VERSION
                                     batch<float, 4>
#endif
#if XSIMD_ARM_INSTR_SET >= XSIMD_ARM8_64_NEON_VERSION
                                     ,
                                     batch<double, 2>
#endif
#if defined(XSIMD_ENABLE_FALLBACK)
                                     ,
                                     batch<float, 7>,
                                     batch<double, 3>
#endif
                                >;

    using batch_int32_type_list = mpl::type_list<
#if XSIMD_X86_INSTR_SET >= XSIMD_X86_SSE2_VERSION
                                    batch<int32_t, 4>
#endif
#if XSIMD_X86_INSTR_SET >= XSIMD_X86_AVX_VERSION
                                    ,
                                    batch<int32_t, 8>
#endif
#if XSIMD_X86_INSTR_SET >= XSIMD_X86_AVX512_VERSION
                                    ,
                                    batch<int32_t, 16>
#endif
#if XSIMD_ARM_INSTR_SET >= XSIMD_ARM7_NEON_VERSION
                                    batch<int32_t, 4>
#endif
#if defined(XSIMD_ENABLE_FALLBACK)
                                    ,
                                    batch<int32_t, 7>
#endif
    >;

    using batch_complex_type_list = mpl::type_list<
#if XSIMD_X86_INSTR_SET >= XSIMD_X86_SSE2_VERSION
                                    batch<std::complex<float>, 4>,
                                    batch<std::complex<double>, 2>
#ifdef XSIMD_ENABLE_XTL_COMPLEX
                                    ,
                                    batch<xtl::xcomplex<float>, 4>,
                                    batch<xtl::xcomplex<double>, 2>
#endif
#endif
#if XSIMD_X86_INSTR_SET >= XSIMD_X86_AVX_VERSION
                                    ,
                                    batch<std::complex<float>, 8>,
                                    batch<std::complex<double>, 4>
#ifdef XSIMD_ENABLE_XTL_COMPLEX
                                    ,
                                    batch<xtl::xcomplex<float>, 8>,
                                    batch<xtl::xcomplex<double>, 4>
#endif
#endif
#if XSIMD_X86_INSTR_SET >= XSIMD_X86_AVX512_VERSION
                                    ,
                                    batch<std::complex<float>, 16>,
                                    batch<std::complex<double>, 8>
#ifdef XSIMD_ENABLE_XTL_COMPLEX
                                    ,
                                    batch<xtl::xcomplex<float>, 16>,
                                    batch<xtl::xcomplex<double>, 8>
#endif
#endif
#if XSIMD_ARM_INSTR_SET >= XSIMD_ARM7_NEON_VERSION
                                     batch<std::complex<float>, 4>
#ifdef XSIMD_ENABLE_XTL_COMPLEX
                                     ,
                                     batch<xtl::xcomplex<float>, 4>
#endif
#endif
#if XSIMD_ARM_INSTR_SET >= XSIMD_ARM8_64_NEON_VERSION
                                     ,
                                     batch<std::complex<double, 2>
#ifdef XSIMD_ENABLE_XTL_COMPLEX
                                     ,
                                     batch<xtl::xcomplex<double>, 2>
#endif
#endif
    >;

    using batch_math_type_list = mpl::concatenate_t<batch_int32_type_list, batch_float_type_list>;
    using batch_type_list = mpl::concatenate_t<batch_int_type_list, batch_float_type_list>;
}

using batch_int_types = to_testing_types<xsimd::batch_int_type_list>;
using batch_float_types = to_testing_types<xsimd::batch_float_type_list>;
using batch_complex_types = to_testing_types<xsimd::batch_complex_type_list>;
using batch_math_types = to_testing_types<xsimd::batch_math_type_list>;
using batch_types = to_testing_types<xsimd::batch_type_list>;


/********************
 * conversion utils *
 ********************/

template <size_t N, size_t A>
struct conversion_param
{
    static constexpr size_t size = N;
    static constexpr size_t alignment = A;
};

class conversion_test_names
{
public:

    template <class T>
    static std::string GetName(int)
    {
#if XSIMD_X86_INSTR_SET >= XSIMD_X86_SSE2_VERSION
        if (T::size == 2) { return "sse"; }
        if (T::size == 4) { return "avx"; }
        if (T::size == 8) { return "avx512";}
        return "fallback";
#elif XSIMD_ARM_INSTR_SET >= XSIMD_ARM7_NEON_VERSION
        if (T::size == 2) { return "neon"; }
        return "fallback";
#else
        return "fallback";
#endif
    }
};

using conversion_type_list = xsimd::mpl::type_list<
#if XSIMD_X86_INSTR_SET >= XSIMD_X86_SSE2_VERSION
                               conversion_param<2, 16>
#endif
#if XSIMD_X86_INSTR_SET >= XSIMD_X86_AVX_VERSION
                               ,
                               conversion_param<4, 32>
#endif
#if XSIMD_X86_INSTR_SET >= XSIMD_X86_AVX512_VERSION
                               ,
                               conversion_param<8, 64>
#endif
#if XSIMD_ARM_INSTR_SET >= XSIMD_ARM8_64_NEON_VERSION
                               conversion_param<2, 16>
#endif
#if defined(XSIMD_ENABLE_FALLBACK)
                               ,
                               conversion_param<3, 32>
#endif
                               >;
using conversion_types = to_testing_types<conversion_type_list>;

#endif // XXSIMD_TEST_UTILS_HPP

