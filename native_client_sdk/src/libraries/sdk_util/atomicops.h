/* Copyright 2013 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef LIBRARIES_SDK_UTIL_ATOMICOPS_H_
#define LIBRARIES_SDK_UTIL_ATOMICOPS_H_

#ifndef WIN32

#include <stdint.h>
typedef int32_t Atomic32;

#ifndef __llvm__
static inline void MemoryBarrier() {
  __sync_synchronize();
}
#endif

inline Atomic32 AtomicAddFetch(volatile Atomic32* ptr, Atomic32 value) {
  return __sync_add_and_fetch(ptr, value);
}

#else

#include <windows.h>

/* Undefine many Windows.h macros that we almost certainly do not want. */
#undef min
#undef max
#undef PostMessage
#undef interface

typedef long Atomic32;

/* Windows.h already defines a MemoryBarrier macro. */

inline Atomic32 AtomicAddFetch(volatile Atomic32* ptr, Atomic32 value) {
  return InterlockedExchangeAdd(ptr, value);
}
#endif


#endif  /* LIBRARIES_SDK_UTIL_ATOMICOPS_H_ */
