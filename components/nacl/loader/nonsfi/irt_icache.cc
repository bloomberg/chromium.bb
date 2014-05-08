// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if defined(__arm__)  // nacl_irt_icache is supported only on ARM.

#include <errno.h>
#include <sys/syscall.h>
#include <stddef.h>

#include "components/nacl/loader/nonsfi/irt_interfaces.h"

namespace nacl {
namespace nonsfi {

namespace {

// TODO(mazda): Revisit the implementation to consider inlining the syscall
// with assembly when Non-SFI mode's IRT implementations get moved to the
// NaCl repository.
int IrtClearCache(void* addr, size_t size) {
  // The third argument of cacheflush is just ignored for now and should
  // always be zero.
  int result = syscall(__ARM_NR_cacheflush,
                       addr, reinterpret_cast<intptr_t>(addr) + size, 0);
  if (result < 0)
    return errno;
  return 0;
}

}  // namespace

const nacl_irt_icache kIrtIcache = {
  IrtClearCache
};

}  // namespace nonsfi
}  // namespace nacl

#endif
