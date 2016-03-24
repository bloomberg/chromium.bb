/*
 * Copyright (c) 2016, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#ifndef VPX_VPX_INTEGER_H_
#define VPX_VPX_INTEGER_H_

/* get ptrdiff_t, size_t, wchar_t, NULL */
#include <stddef.h>

#if defined(_MSC_VER)
#define VPX_FORCE_INLINE __forceinline
#define VPX_INLINE __inline
#else
#define VPX_FORCE_INLINE __inline__ __attribute__(always_inline)
// TODO(jbb): Allow a way to force inline off for older compilers.
#define VPX_INLINE inline
#endif

#if (defined(_MSC_VER) && (_MSC_VER < 1600)) || defined(VPX_EMULATE_INTTYPES)
typedef signed char int8_t;
typedef signed short int16_t;
typedef signed int int32_t;

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;

#if (defined(_MSC_VER) && (_MSC_VER < 1600))
typedef signed __int64 int64_t;
typedef unsigned __int64 uint64_t;
#define INT64_MAX _I64_MAX
#define INT32_MAX _I32_MAX
#define INT32_MIN _I32_MIN
#define INT16_MAX _I16_MAX
#define INT16_MIN _I16_MIN
#endif

#ifndef _UINTPTR_T_DEFINED
typedef size_t uintptr_t;
#endif

#else

/* Most platforms have the C99 standard integer types. */

#if defined(__cplusplus)
#if !defined(__STDC_FORMAT_MACROS)
#define __STDC_FORMAT_MACROS
#endif
#if !defined(__STDC_LIMIT_MACROS)
#define __STDC_LIMIT_MACROS
#endif
#endif  // __cplusplus

#include <stdint.h>

#endif

/* VS2010 defines stdint.h, but not inttypes.h */
#if defined(_MSC_VER) && _MSC_VER < 1800
#define PRId64 "I64d"
#else
#include <inttypes.h>
#endif

#endif  // VPX_VPX_INTEGER_H_
