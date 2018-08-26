// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/heap_profiling/public/cpp/sampling_profiler_wrapper.h"

#include "components/services/heap_profiling/public/cpp/allocator_shim.h"

namespace heap_profiling {
namespace {

AllocatorType ConvertType(base::SamplingHeapProfiler::AllocatorType type) {
  static_assert(
      static_cast<uint32_t>(base::SamplingHeapProfiler::AllocatorType::kMax) ==
          static_cast<uint32_t>(AllocatorType::kCount),
      "AllocatorType lengths do not match.");
  switch (type) {
    case base::SamplingHeapProfiler::AllocatorType::kMalloc:
      return AllocatorType::kMalloc;
    case base::SamplingHeapProfiler::AllocatorType::kPartitionAlloc:
      return AllocatorType::kPartitionAlloc;
    case base::SamplingHeapProfiler::AllocatorType::kBlinkGC:
      return AllocatorType::kOilpan;
    default:
      NOTREACHED();
      return AllocatorType::kMalloc;
  }
}

}  // namespace

SamplingProfilerWrapper::SamplingProfilerWrapper() {
  base::SamplingHeapProfiler::GetInstance()->AddSamplesObserver(this);
}

SamplingProfilerWrapper::~SamplingProfilerWrapper() {
  base::SamplingHeapProfiler::GetInstance()->RemoveSamplesObserver(this);
}

void SamplingProfilerWrapper::StartProfiling(size_t sampling_rate) {
  auto* profiler = base::SamplingHeapProfiler::GetInstance();
  profiler->SetSamplingInterval(sampling_rate);
  profiler->Start();
}

void SamplingProfilerWrapper::SampleAdded(
    void* address,
    size_t size,
    size_t total,
    base::SamplingHeapProfiler::AllocatorType type,
    const char* context) {
  RecordAndSendAlloc(ConvertType(type), address, size, context);
}

void SamplingProfilerWrapper::SampleRemoved(void* address) {
  RecordAndSendFree(address);
}

}  // namespace heap_profiling
