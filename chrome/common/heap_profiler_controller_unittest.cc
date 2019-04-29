// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/heap_profiler_controller.h"

#include "base/command_line.h"
#include "base/metrics/metrics_hashes.h"
#include "base/sampling_heap_profiler/sampling_heap_profiler.h"
#include "base/test/test_mock_time_task_runner.h"
#include "build/build_config.h"
#include "components/metrics/call_stack_profile_builder.h"
#include "content/public/common/content_switches.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/sampled_profile.pb.h"

constexpr size_t kAllocationSize = 42 * 1024;
constexpr int kSnapshotsToCollect = 3;

class HeapProfilerControllerTest : public testing::Test {
 public:
  HeapProfilerControllerTest() = default;

  void StartController(scoped_refptr<base::TaskRunner> task_runner) {
    metrics::CallStackProfileBuilder::SetBrowserProcessReceiverCallback(
        base::BindRepeating(&HeapProfilerControllerTest::CheckProfile,
                            base::Unretained(this)));
    controller_ = std::make_unique<HeapProfilerController>();
    controller_->SetTaskRunnerForTest(std::move(task_runner));
    controller_->Start();
  }

  void CheckProfile(base::TimeTicks time, metrics::SampledProfile profile) {
    const uint64_t kMetadataCountHash =
        base::HashMetricName("HeapProfiler.AllocationInBytes");
    EXPECT_EQ(metrics::SampledProfile::PERIODIC_HEAP_COLLECTION,
              profile.trigger_event());
    EXPECT_LT(0, profile.call_stack_profile().stack_sample_size());
    const auto& metadata_hashes =
        profile.call_stack_profile().metadata_name_hash();
    EXPECT_LT(0, metadata_hashes.size());

    const auto* metadata_count_iterator = std::find(
        metadata_hashes.begin(), metadata_hashes.end(), kMetadataCountHash);
    EXPECT_NE(metadata_count_iterator, metadata_hashes.end());
    int metadata_count_index =
        static_cast<int>(metadata_count_iterator - metadata_hashes.begin());

    // The first sample is guaranteed to have metadata. It will be removed in
    // subsequent samples if its value is the same as in the first sample.
    EXPECT_EQ(1, profile.call_stack_profile().stack_sample(0).metadata_size());

    bool found = false;
    for (const metrics::CallStackProfile::StackSample& sample :
         profile.call_stack_profile().stack_sample()) {
      for (const metrics::CallStackProfile::MetadataItem& item :
           sample.metadata()) {
        if (item.name_hash_index() == metadata_count_index &&
            item.value() >= static_cast<int64_t>(kAllocationSize)) {
          found = true;
          break;
        }
      }
    }
    EXPECT_TRUE(found);

    if (++snapshots_count_ == kSnapshotsToCollect)
      controller_.reset();
  }

 protected:
  int snapshots_count_ = 0;
  std::unique_ptr<HeapProfilerController> controller_;

  DISALLOW_COPY_AND_ASSIGN(HeapProfilerControllerTest);
};

// Sampling profiler is not capable of unwinding stack on Android under tests.
#if !defined(OS_ANDROID)
TEST_F(HeapProfilerControllerTest, ProfileCollectionsScheduler) {
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  base::TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner.get());

  base::SamplingHeapProfiler::Init();
  auto* profiler = base::SamplingHeapProfiler::Get();
  profiler->SetSamplingInterval(1024);
  profiler->Start();

  base::PoissonAllocationSampler::Init();
  StartController(task_runner);
  auto* sampler = base::PoissonAllocationSampler::Get();
  sampler->SuppressRandomnessForTest(true);
  sampler->RecordAlloc(reinterpret_cast<void*>(0x1337), kAllocationSize,
                       base::PoissonAllocationSampler::kMalloc, nullptr);
  sampler->RecordAlloc(reinterpret_cast<void*>(0x7331), kAllocationSize,
                       base::PoissonAllocationSampler::kMalloc, nullptr);

  task_runner->FastForwardUntilNoTasksRemain();
  EXPECT_GE(snapshots_count_, kSnapshotsToCollect);
}
#endif
