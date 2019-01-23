// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_HEAP_PROFILING_PUBLIC_CPP_SAMPLING_PROFILER_WRAPPER_H_
#define COMPONENTS_SERVICES_HEAP_PROFILING_PUBLIC_CPP_SAMPLING_PROFILER_WRAPPER_H_

#include "base/sampling_heap_profiler/poisson_allocation_sampler.h"
#include "components/services/heap_profiling/public/cpp/sender_pipe.h"
#include "components/services/heap_profiling/public/cpp/stream.h"
#include "components/services/heap_profiling/public/mojom/heap_profiling_client.mojom.h"

namespace heap_profiling {

// Initializes the TLS slot globally. This will be called early in Chrome's
// lifecycle to prevent re-entrancy from occurring while trying to set up the
// TLS slot, which is the entity that's supposed to prevent re-entrancy.
void InitTLSSlot();

// Exists for testing only.
// A return value of |true| means that the allocator shim was already
// initialized and |callback| will never be called. Otherwise, |callback| will
// be called on |task_runner| after the allocator shim is initialized.
bool SetOnInitAllocatorShimCallbackForTesting(
    base::OnceClosure callback,
    scoped_refptr<base::TaskRunner> task_runner);

// The class listens for allocation samples records the necessary attributes
// and passes allocations down the pipeline.
class SamplingProfilerWrapper
    : private base::PoissonAllocationSampler::SamplesObserver {
 public:
  SamplingProfilerWrapper();
  ~SamplingProfilerWrapper() override;

  void StartProfiling(SenderPipe* sender_pipe,
                      mojom::ProfilingParamsPtr params);
  void StopProfiling();

  // This method closes sender pipe.
  static void FlushBuffersAndClosePipe();

  // Ensures all send buffers are flushed. The given barrier ID is sent to the
  // logging process so it knows when this operation is complete.
  static void FlushPipe(uint32_t barrier_id);

 private:
  // base::PoissonAllocationSampler::SamplesObserver
  void SampleAdded(void* address,
                   size_t size,
                   size_t total,
                   base::PoissonAllocationSampler::AllocatorType,
                   const char* context) override;
  void SampleRemoved(void* address) override;
};

}  // namespace heap_profiling

#endif  // COMPONENTS_SERVICES_HEAP_PROFILING_PUBLIC_CPP_SAMPLING_PROFILER_WRAPPER_H_
