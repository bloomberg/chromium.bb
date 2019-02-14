// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/heap_profiler_controller.h"

#include "base/command_line.h"
#include "base/metrics/metrics_hashes.h"
#include "base/sampling_heap_profiler/poisson_allocation_sampler.h"
#include "base/test/test_mock_time_task_runner.h"
#include "components/metrics/call_stack_profile_builder.h"
#include "content/public/common/content_switches.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/sampled_profile.pb.h"

constexpr size_t kAllocationSize = 42 * 1024;

void CheckProfile(int* profiles_count,
                  base::TimeTicks time,
                  metrics::SampledProfile profile) {
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

  bool found = false;
  for (const metrics::CallStackProfile::StackSample& sample :
       profile.call_stack_profile().stack_sample()) {
    EXPECT_LT(0, sample.metadata_size());
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

  ++*profiles_count;
}

TEST(HeapProfilerControllerTest, ProfileCollectionsScheduler) {
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  base::TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner.get());

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  command_line->AppendSwitchASCII(switches::kSamplingHeapProfiler, "1");

  int profiles_collected = 0;
  metrics::CallStackProfileBuilder::SetBrowserProcessReceiverCallback(
      base::BindRepeating(&CheckProfile, &profiles_collected));
  base::PoissonAllocationSampler::Init();
  HeapProfilerController controller;
  controller.SetTaskRunnerForTest(task_runner.get());
  controller.StartIfEnabled();
  auto* sampler = base::PoissonAllocationSampler::Get();
  sampler->SuppressRandomnessForTest(true);
  sampler->RecordAlloc(reinterpret_cast<void*>(0x1337), kAllocationSize,
                       base::PoissonAllocationSampler::kMalloc, nullptr);
  sampler->RecordAlloc(reinterpret_cast<void*>(0x7331), kAllocationSize,
                       base::PoissonAllocationSampler::kMalloc, nullptr);
  do {
    task_runner->FastForwardBy(base::TimeDelta::FromHours(1));
  } while (profiles_collected < 2);
}
