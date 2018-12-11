// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/perf/metric_collector.h"

#include <stdint.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/sampled_profile.pb.h"

namespace metrics {

namespace {

const long kMsAfterBoot = 10000;
const long kMsAfterLogin = 2000;

// Returns an example PerfDataProto. The contents don't have to make sense. They
// just need to constitute a semantically valid protobuf.
// |proto| is an output parameter that will contain the created protobuf.
PerfDataProto GetExamplePerfDataProto() {
  PerfDataProto proto;
  proto.set_timestamp_sec(1435604013);  // Time since epoch in seconds.

  PerfDataProto_PerfFileAttr* file_attr = proto.add_file_attrs();
  file_attr->add_ids(61);
  file_attr->add_ids(62);
  file_attr->add_ids(63);

  PerfDataProto_PerfEventAttr* attr = file_attr->mutable_attr();
  attr->set_type(1);
  attr->set_size(2);
  attr->set_config(3);
  attr->set_sample_period(4);
  attr->set_sample_freq(5);

  PerfDataProto_PerfEventStats* stats = proto.mutable_stats();
  stats->set_num_events_read(100);
  stats->set_num_sample_events(200);
  stats->set_num_mmap_events(300);
  stats->set_num_fork_events(400);
  stats->set_num_exit_events(500);

  return proto;
}

// Allows access to some private methods for testing.
class TestMetricCollector : public MetricCollector {
 public:
  TestMetricCollector() {}
  explicit TestMetricCollector(const CollectionParams& collection_params)
      : MetricCollector(collection_params) {}

  void CollectProfile(
      std::unique_ptr<SampledProfile> sampled_profile) override {
    PerfDataProto perf_data_proto = GetExamplePerfDataProto();
    sampled_profile->set_ms_after_boot(kMsAfterBoot);
    sampled_profile->set_ms_after_login(kMsAfterLogin);
    sampled_profile->mutable_perf_data()->Swap(&perf_data_proto);

    // Add the collected data to the container of collected SampledProfiles.
    cached_profile_data_.resize(cached_profile_data_.size() + 1);
    cached_profile_data_.back().Swap(sampled_profile.get());
  }

  using MetricCollector::collection_params;
  using MetricCollector::login_time_;
  using MetricCollector::ScheduleIntervalCollection;
  using MetricCollector::timer;

 private:
  std::vector<SampledProfile> stored_profiles_;

  DISALLOW_COPY_AND_ASSIGN(TestMetricCollector);
};

}  // namespace

class MetricCollectorTest : public testing::Test {
 public:
  MetricCollectorTest()
      : task_runner_(base::MakeRefCounted<base::TestSimpleTaskRunner>()),
        task_runner_handle_(task_runner_),
        perf_data_proto_(GetExamplePerfDataProto()) {}

  void SetUp() override {
    CollectionParams test_params;
    // Set the sampling factors for the triggers to 1, so we always trigger
    // collection.
    test_params.resume_from_suspend.sampling_factor = 1;
    test_params.restore_session.sampling_factor = 1;

    metric_collector_ = std::make_unique<TestMetricCollector>(test_params);
    metric_collector_->Init();
  }

  void TearDown() override { metric_collector_.reset(); }

 protected:
  std::unique_ptr<TestMetricCollector> metric_collector_;

  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  base::ThreadTaskRunnerHandle task_runner_handle_;

  // Store a sample perf data protobuf for testing.
  PerfDataProto perf_data_proto_;

  DISALLOW_COPY_AND_ASSIGN(MetricCollectorTest);
};

TEST_F(MetricCollectorTest, CheckSetup) {
  EXPECT_GT(perf_data_proto_.ByteSize(), 0);

  // Timer is not active before user logs in.
  EXPECT_FALSE(metric_collector_->timer().IsRunning());
  EXPECT_TRUE(metric_collector_->login_time_.is_null());

  std::vector<SampledProfile> stored_profiles;
  EXPECT_FALSE(metric_collector_->GetSampledProfiles(&stored_profiles));
  EXPECT_TRUE(stored_profiles.empty());
}

TEST_F(MetricCollectorTest, EnabledOnLogin) {
  metric_collector_->OnUserLoggedIn();
  EXPECT_TRUE(metric_collector_->timer().IsRunning());
  EXPECT_FALSE(metric_collector_->login_time_.is_null());
}

TEST_F(MetricCollectorTest, Deactivate) {
  auto sampled_profile = std::make_unique<SampledProfile>();
  sampled_profile->set_trigger_event(SampledProfile::PERIODIC_COLLECTION);

  metric_collector_->CollectProfile(std::move(sampled_profile));

  metric_collector_->OnUserLoggedIn();
  EXPECT_TRUE(metric_collector_->timer().IsRunning());
  EXPECT_FALSE(metric_collector_->login_time_.is_null());

  // Timer is stopped by Deactivate(), but login time and cached profiles stay.
  metric_collector_->Deactivate();
  EXPECT_FALSE(metric_collector_->timer().IsRunning());
  EXPECT_FALSE(metric_collector_->login_time_.is_null());

  std::vector<SampledProfile> stored_profiles;
  EXPECT_TRUE(metric_collector_->GetSampledProfiles(&stored_profiles));
}

TEST_F(MetricCollectorTest, SuspendDone) {
  const auto kSuspendDuration = base::TimeDelta::FromMinutes(3);

  // Timer is not active before user logs in.
  EXPECT_FALSE(metric_collector_->timer().IsRunning());
  metric_collector_->SuspendDone(kSuspendDuration);

  // Timer is activated by the SuspendDone call.
  EXPECT_TRUE(metric_collector_->timer().IsRunning());

  // Run all pending tasks. This will run all the tasks already queued, but not
  // any new tasks created while executing the existing pending tasks. This is
  // important, because our collectors always queue a new periodic collection
  // task after each collection, regardless of trigger. Thus, RunUntilIdle
  // would never terminate.
  task_runner_->RunPendingTasks();

  std::vector<SampledProfile> stored_profiles;
  EXPECT_TRUE(metric_collector_->GetSampledProfiles(&stored_profiles));
  ASSERT_EQ(1U, stored_profiles.size());

  // Timer is rearmed for periodic collection after each collection.
  EXPECT_TRUE(metric_collector_->timer().IsRunning());

  const SampledProfile& profile = stored_profiles[0];
  EXPECT_EQ(SampledProfile::RESUME_FROM_SUSPEND, profile.trigger_event());
  EXPECT_EQ(kSuspendDuration.InMilliseconds(), profile.suspend_duration_ms());
  EXPECT_TRUE(profile.has_ms_after_resume());
  EXPECT_EQ(kMsAfterLogin, profile.ms_after_login());
  EXPECT_EQ(kMsAfterBoot, profile.ms_after_boot());

  // Run all new pending tasks. This will run a periodic collection that was
  // scheduled by the previous collection event.
  task_runner_->RunPendingTasks();

  stored_profiles.clear();
  EXPECT_TRUE(metric_collector_->GetSampledProfiles(&stored_profiles));
  ASSERT_EQ(1U, stored_profiles.size());
  const SampledProfile& profile2 = stored_profiles[0];
  EXPECT_EQ(SampledProfile::PERIODIC_COLLECTION, profile2.trigger_event());
}

TEST_F(MetricCollectorTest, OnSessionRestoreDone) {
  const int kRestoredTabs = 7;

  // Timer is not active before user logs in.
  EXPECT_FALSE(metric_collector_->timer().IsRunning());
  metric_collector_->OnSessionRestoreDone(kRestoredTabs);

  // Timer is activated by the OnSessionRestoreDone call.
  EXPECT_TRUE(metric_collector_->timer().IsRunning());

  // Run all pending tasks.
  task_runner_->RunPendingTasks();

  std::vector<SampledProfile> stored_profiles;
  EXPECT_TRUE(metric_collector_->GetSampledProfiles(&stored_profiles));
  ASSERT_EQ(1U, stored_profiles.size());

  // Timer is rearmed for periodic collection after each collection.
  EXPECT_TRUE(metric_collector_->timer().IsRunning());

  const SampledProfile& profile = stored_profiles[0];
  EXPECT_EQ(SampledProfile::RESTORE_SESSION, profile.trigger_event());
  EXPECT_EQ(kRestoredTabs, profile.num_tabs_restored());
  EXPECT_FALSE(profile.has_ms_after_resume());
  EXPECT_EQ(kMsAfterLogin, profile.ms_after_login());
  EXPECT_EQ(kMsAfterBoot, profile.ms_after_boot());

  // A second SessionRestoreDone call is throttled.
  metric_collector_->OnSessionRestoreDone(1);
  // Run all new pending tasks. This will run a periodic collection, but not a
  // second session restore.
  task_runner_->RunPendingTasks();

  stored_profiles.clear();
  EXPECT_TRUE(metric_collector_->GetSampledProfiles(&stored_profiles));
  ASSERT_EQ(1U, stored_profiles.size());
  const SampledProfile& profile2 = stored_profiles[0];
  EXPECT_EQ(SampledProfile::PERIODIC_COLLECTION, profile2.trigger_event());

  // Rerun any new pending tasks. This run should include a new periodic
  // collection, but no session restore.
  task_runner_->RunPendingTasks();

  stored_profiles.clear();
  EXPECT_TRUE(metric_collector_->GetSampledProfiles(&stored_profiles));
  ASSERT_EQ(1U, stored_profiles.size());
  const SampledProfile& profile3 = stored_profiles[0];
  EXPECT_EQ(SampledProfile::PERIODIC_COLLECTION, profile3.trigger_event());
}

TEST_F(MetricCollectorTest, ScheduleIntervalCollection) {
  // Timer is not active before user logs in.
  EXPECT_FALSE(metric_collector_->timer().IsRunning());
  metric_collector_->ScheduleIntervalCollection();

  // Timer is activated by the ScheduleIntervalCollection call.
  EXPECT_TRUE(metric_collector_->timer().IsRunning());

  // Run all pending tasks.
  task_runner_->RunPendingTasks();

  std::vector<SampledProfile> stored_profiles;
  EXPECT_TRUE(metric_collector_->GetSampledProfiles(&stored_profiles));
  ASSERT_EQ(1U, stored_profiles.size());

  // Timer is rearmed after each collection.
  EXPECT_TRUE(metric_collector_->timer().IsRunning());

  const SampledProfile& profile = stored_profiles[0];
  EXPECT_EQ(SampledProfile::PERIODIC_COLLECTION, profile.trigger_event());
  EXPECT_FALSE(profile.has_suspend_duration_ms());
  EXPECT_FALSE(profile.has_ms_after_resume());
  EXPECT_EQ(kMsAfterLogin, profile.ms_after_login());
  EXPECT_EQ(kMsAfterBoot, profile.ms_after_boot());

  ASSERT_TRUE(profile.has_perf_data());
  EXPECT_FALSE(profile.has_perf_stat());
  EXPECT_EQ(perf_data_proto_.SerializeAsString(),
            profile.perf_data().SerializeAsString());
}

// Setting the sampling factors to zero should disable the triggers.
// Otherwise, it could cause a div-by-zero crash.
TEST_F(MetricCollectorTest, ZeroSamplingFactorDisablesTrigger) {
  // Define params with zero sampling factors.
  CollectionParams test_params;
  test_params.resume_from_suspend.sampling_factor = 0;
  test_params.restore_session.sampling_factor = 0;

  metric_collector_ = std::make_unique<TestMetricCollector>(test_params);
  metric_collector_->Init();

  // Cancel the background collection.
  metric_collector_->Deactivate();
  EXPECT_FALSE(metric_collector_->timer().IsRunning())
      << "Sanity: timer should not be running.";

  // Calling SuspendDone or OnSessionRestoreDone should not start the timer
  // that triggers collection.
  metric_collector_->SuspendDone(base::TimeDelta::FromMinutes(10));
  EXPECT_FALSE(metric_collector_->timer().IsRunning());

  metric_collector_->OnSessionRestoreDone(100);
  EXPECT_FALSE(metric_collector_->timer().IsRunning());
}

}  // namespace metrics
