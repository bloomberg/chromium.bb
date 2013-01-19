/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef NACL_MOUNTS_OSINTTYPES_H_
#define NACL_MOUNTS_OSINTTYPES_H_

// Define printf/scanf format strings for size_t.

#if defined(WIN32)

#if !defined(PRIuS)
#define PRIuS "Iu"
#endif

#if !defined(SCNuS)
#define SCNuS "Iu"
#endif

#elif defined(__native_client__)

#include <inttypes.h>

#if !defined(PRIuS)
#define PRIuS "zu"
#endif

#if !defined(SCNuS)
#define SCNuS "zu"
#endif

#endif  // defined(__native_client__)

#endif  // NACL_MOUNTS_OSINTTYPES_H_
