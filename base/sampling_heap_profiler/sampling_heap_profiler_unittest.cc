// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/sampling_heap_profiler/sampling_heap_profiler.h"

#include <stdlib.h>

#include "base/allocator/allocator_shim.h"
#include "base/debug/stack_trace.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace {

class SamplingHeapProfilerTest : public ::testing::Test {
#if defined(OS_MACOSX)
  void SetUp() override { allocator::InitializeAllocatorShim(); }
#endif
};

class SamplesCollector : public SamplingHeapProfiler::SamplesObserver {
 public:
  explicit SamplesCollector(size_t watch_size) : watch_size_(watch_size) {}

  void SampleAdded(uint32_t id, size_t size, size_t count) override {
    if (sample_added || size != watch_size_ || !count)
      return;
    sample_id_ = id;
    sample_added = true;
  }

  void SampleRemoved(uint32_t id) override {
    if (id == sample_id_)
      sample_removed = true;
  }

  bool sample_added = false;
  bool sample_removed = false;

 private:
  size_t watch_size_;
  uint32_t sample_id_ = 0;
};

TEST_F(SamplingHeapProfilerTest, CollectSamples) {
  SamplesCollector collector(10000);
  SamplingHeapProfiler* profiler = SamplingHeapProfiler::GetInstance();
  profiler->SuppressRandomnessForTest();
  profiler->SetSamplingInterval(1024);
  profiler->Start();
  profiler->AddSamplesObserver(&collector);
  void* volatile p = malloc(10000);
  free(p);
  profiler->Stop();
  profiler->RemoveSamplesObserver(&collector);
  CHECK(collector.sample_added);
  CHECK(collector.sample_removed);
}

}  // namespace
}  // namespace base
