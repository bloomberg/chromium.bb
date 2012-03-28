// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "../client/atomicops.h"

#if !defined(__native_client__)
#include "base/atomicops.h"
#endif

namespace gpu {

void MemoryBarrier() {
#if defined(__native_client__)
  __sync_synchronize();
#else
  base::subtle::MemoryBarrier();
#endif
}

}  // namespace gpu

