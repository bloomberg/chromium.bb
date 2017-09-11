// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_PROFILING_MEMLOG_ALLOCATOR_SHIM_H_
#define CHROME_COMMON_PROFILING_MEMLOG_ALLOCATOR_SHIM_H_

#include "chrome/common/profiling/memlog_sender_pipe.h"
#include "chrome/common/profiling/memlog_stream.h"

// This is a temporary allocator shim for testing out-of-process heap
// profiling.
//
// TODO(brettw) replace this with the base allocator shim, plus a way to get
// the events at the Chrome layer.

namespace profiling {

// Begin profiling all allocations in the process. Send the results to
// |sender_pipe|.
void InitAllocatorShim(MemlogSenderPipe* sender_pipe);

// Stop profiling allocations by dropping shim callbacks. There is no way to
// consistently, synchronously stop the allocator shim without negatively
// impacting fast-path performance. This method eventually "turns off" the
// allocator shim by turning future calls to AllocatorShimLogAlloc and
// AllocatorShimLogFree into no-ops, modulo caching [g_send_buffers is not
// volatile, intentionally]. This method is well-defined, but isn't guaranteed
// to stop all messages to sender_pipe, since another thread might already be in
// the process of forming a message.
void StopAllocatorShimDangerous();

// Logs an allocation. The context is a null-terminated string of
// allocator-specific context information. It can be null if there is no
// context.
void AllocatorShimLogAlloc(AllocatorType type,
                           void* address,
                           size_t sz,
                           const char* context);

void AllocatorShimLogFree(void* address);

}  // namespace profiling

#endif  // CHROME_COMMON_PROFILING_MEMLOG_ALLOCATOR_SHIM_H_
