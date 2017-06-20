// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_PROFILING_MEMLOG_ALLOCATOR_SHIM_H_
#define CHROME_COMMON_PROFILING_MEMLOG_ALLOCATOR_SHIM_H_

#include "chrome/common/profiling/memlog_sender_pipe.h"

// This is a temporary allocator shim for testing out-of-process heap
// profiling.
//
// TODO(brettw) replace this with the base allocator shim, plus a way to get
// the events at the Chrome layer.

namespace profiling {

void InitAllocatorShim(MemlogSenderPipe* sender_pipe);

void AllocatorShimLogAlloc(void* address, size_t sz);
void AllocatorShimLogFree(void* address);

}  // namespace profiling

#endif  // CHROME_COMMON_PROFILING_MEMLOG_ALLOCATOR_SHIM_H_
