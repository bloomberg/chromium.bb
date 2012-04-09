/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef LIBRARIES_WIN_STDINT_H_
#define LIBRARIES_WIN_STDINT_H_

/**
* Implementation of pthread.h for building the SDK natively Windows.
*/

#include <sys\types.h>

#ifdef _WIN64
typedef __int64 ssize_t;
#else
typedef long ssize_t;
#endif

typedef char int8_t;
typedef short int16_t;
typedef long int32_t;
typedef long long int64_t;

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned long uint32_t;
typedef unsigned long long uint64_t;

typedef uint32_t mode_t;

#endif  /* LIBRARIES_WIN_STDINT_H_ */
