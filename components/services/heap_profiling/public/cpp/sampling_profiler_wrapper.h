// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_HEAP_PROFILING_PUBLIC_CPP_SAMPLING_PROFILER_WRAPPER_H_
#define COMPONENTS_SERVICES_HEAP_PROFILING_PUBLIC_CPP_SAMPLING_PROFILER_WRAPPER_H_

#include <unordered_map>
#include <unordered_set>
#include <vector>

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

  mojom::HeapProfilePtr RetrieveHeapProfile();

 private:
  struct Sample {
    Sample();
    Sample(Sample&& sample);
    ~Sample();

    Sample& operator=(Sample&&) = default;

    AllocatorType allocator;
    size_t size;
    const char* context = nullptr;
    std::vector<uint64_t> stack;

    DISALLOW_COPY_AND_ASSIGN(Sample);
  };

  // base::PoissonAllocationSampler::SamplesObserver
  void SampleAdded(void* address,
                   size_t size,
                   size_t total,
                   base::PoissonAllocationSampler::AllocatorType,
                   const char* context) override;
  void SampleRemoved(void* address) override;

  void CaptureMixedStack(const char* context, Sample* sample);
  void CaptureNativeStack(const char* context, Sample* sample);
  const char* RecordString(const char* string);

  bool stream_samples_ = false;

  // Mutex to access |samples_| and |strings_|.
  base::Lock mutex_;

  // Samples of the currently live allocations.
  std::unordered_map<void*, Sample> samples_;

  // When CaptureMode::PSEUDO_STACK or CaptureMode::MIXED_STACK is enabled
  // the call stack contents of samples may contain strings besides
  // PC addresses.
  // In this case each string pointer is also added to the |strings_| set.
  // The set does only contain pointers to static strings that are never
  // deleted.
  std::unordered_set<const char*> strings_;
};

}  // namespace heap_profiling

#endif  // COMPONENTS_SERVICES_HEAP_PROFILING_PUBLIC_CPP_SAMPLING_PROFILER_WRAPPER_H_
