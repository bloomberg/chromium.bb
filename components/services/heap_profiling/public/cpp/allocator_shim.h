// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_HEAP_PROFILING_PUBLIC_CPP_ALLOCATOR_SHIM_H_
#define COMPONENTS_SERVICES_HEAP_PROFILING_PUBLIC_CPP_ALLOCATOR_SHIM_H_

#include "components/services/heap_profiling/public/cpp/sender_pipe.h"
#include "components/services/heap_profiling/public/cpp/stream.h"
#include "components/services/heap_profiling/public/mojom/heap_profiling_client.mojom.h"

namespace heap_profiling {

// Initializes the TLS slot globally. This will be called early in Chrome's
// lifecycle to prevent re-entrancy from occurring while trying to set up the
// TLS slot, which is the entity that's supposed to prevent re-entrancy.
void InitTLSSlot();

// This method closes sender pipe.
void FlushBuffersAndClosePipe();

// Ensures all send buffers are flushed. The given barrier ID is sent to the
// logging process so it knows when this operation is complete.
void AllocatorShimFlushPipe(uint32_t barrier_id);

// Initializes allocation recorder.
void InitAllocationRecorder(SenderPipe* sender_pipe,
                            mojom::ProfilingParamsPtr params);

// Creates allocation info record, populates it with current call stack,
// thread name, allocator type and sends out to the client. Safe to call this
// method after TLS is destroyed.
void RecordAndSendAlloc(AllocatorType type,
                        void* address,
                        size_t sz,
                        const char* context);

// Creates the record for free operation and sends it out to the client. Safe
// to call this method after TLS is destroyed.
void RecordAndSendFree(void* address);

// Exists for testing only.
// A return value of |true| means that the allocator shim was already
// initialized and |callback| will never be called. Otherwise, |callback| will
// be called on |task_runner| after the allocator shim is initialized.
bool SetOnInitAllocatorShimCallbackForTesting(
    base::OnceClosure callback,
    scoped_refptr<base::TaskRunner> task_runner);

// Notifies the test clients that allocation hooks have been initialized.
void AllocatorHooksHaveBeenInitialized();

}  // namespace heap_profiling

#endif  // COMPONENTS_SERVICES_HEAP_PROFILING_PUBLIC_CPP_ALLOCATOR_SHIM_H_
