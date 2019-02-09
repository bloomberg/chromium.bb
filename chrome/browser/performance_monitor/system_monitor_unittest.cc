// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_monitor/system_monitor.h"

#include "base/test/scoped_task_environment.h"
#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_monitor {

namespace {

using SystemObserver = SystemMonitor::SystemObserver;
using MetricsRefreshFrequencies = SystemObserver::MetricRefreshFrequencies;

constexpr base::TimeDelta kRefreshInterval = base::TimeDelta::FromSeconds(1);

bool operator==(const MetricsRefreshFrequencies& lhs,
                const MetricsRefreshFrequencies& rhs) {
  return std::tie(lhs.free_phys_memory_mb_frequency) ==
         std::tie(rhs.free_phys_memory_mb_frequency);
}

class MockMetricsMonitorObserver : public SystemObserver {
 public:
  ~MockMetricsMonitorObserver() override {}
  MOCK_METHOD1(OnFreePhysicalMemoryMbSample, void(int free_phys_memory_mb));
};

class TestSystemMonitorHelper : public SystemMonitorHelper {
 public:
  TestSystemMonitorHelper() = default;
  ~TestSystemMonitorHelper() override = default;

 protected:
  // SystemMonitorHelper:
  base::TimeDelta GetRefreshInterval(
      const SystemMonitor::SystemObserver::MetricRefreshFrequencies&
          metrics_and_frequencies) override {
    return kRefreshInterval;
  }
  MetricsRefresh RefreshMetrics(
      const SystemMonitor::SystemObserver::MetricRefreshFrequencies
          metrics_and_frequencies,
      const base::TimeTicks& refresh_time) override {
    SystemMonitorHelper::MetricsRefresh metrics;
    metrics.free_phys_memory_mb =
        SystemMonitorHelper::MetricAndRefreshReason<int>(
            42, SystemMonitor::SamplingFrequency::kDefaultFrequency);
    return metrics;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestSystemMonitorHelper);
};

class SystemMonitorTest : public testing::Test {
 protected:
  SystemMonitorTest()
      : scoped_task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::MOCK_TIME) {
    EXPECT_EQ(nullptr, SystemMonitor::Get());
    system_monitor_ = SystemMonitor::Create();
  }

  std::unique_ptr<SystemMonitor> system_monitor_;

 protected:
  base::test::ScopedTaskEnvironment scoped_task_environment_;

  DISALLOW_COPY_AND_ASSIGN(SystemMonitorTest);
};

TEST_F(SystemMonitorTest, GetReturnsSingleInstance) {
  EXPECT_EQ(system_monitor_.get(), SystemMonitor::Get());
  system_monitor_.reset();
  EXPECT_EQ(nullptr, SystemMonitor::Get());
}

TEST_F(SystemMonitorTest, AddAndUpdateObservers) {
  SystemObserver obs1;
  SystemObserver obs2;
  SystemObserver obs3;

  // The first observer doesn't observe anything yet.
  MetricsRefreshFrequencies obs1_metrics_frequencies = {};
  system_monitor_->AddOrUpdateObserver(&obs1, obs1_metrics_frequencies);
  MetricsRefreshFrequencies expected_metrics_frequencies =
      obs1_metrics_frequencies;
  auto observed_metrics_and_frequencies =
      system_monitor_->metric_refresh_frequencies_for_testing();
  EXPECT_TRUE(observed_metrics_and_frequencies == expected_metrics_frequencies);

  // Add a second observer that observes the amount of free memory at the
  // default frequency.
  MetricsRefreshFrequencies obs2_metrics_frequencies = {
      .free_phys_memory_mb_frequency =
          SystemMonitor::SamplingFrequency::kDefaultFrequency};
  system_monitor_->AddOrUpdateObserver(&obs2, obs2_metrics_frequencies);
  expected_metrics_frequencies.free_phys_memory_mb_frequency =
      SystemMonitor::SamplingFrequency::kDefaultFrequency;
  observed_metrics_and_frequencies =
      system_monitor_->metric_refresh_frequencies_for_testing();
  EXPECT_TRUE(observed_metrics_and_frequencies == expected_metrics_frequencies);

  // Stop observing any metric with the second observer.
  obs2_metrics_frequencies.free_phys_memory_mb_frequency =
      SystemMonitor::SamplingFrequency::kNoSampling;
  system_monitor_->AddOrUpdateObserver(&obs2, obs2_metrics_frequencies);
  expected_metrics_frequencies.free_phys_memory_mb_frequency =
      SystemMonitor::SamplingFrequency::kNoSampling;
  observed_metrics_and_frequencies =
      system_monitor_->metric_refresh_frequencies_for_testing();
  EXPECT_TRUE(observed_metrics_and_frequencies == expected_metrics_frequencies);

  // Add a second observer that observes the amount of free memory at the
  // default frequency.
  MetricsRefreshFrequencies obs3_metrics_frequencies = {
      .free_phys_memory_mb_frequency =
          SystemMonitor::SamplingFrequency::kDefaultFrequency};
  system_monitor_->AddOrUpdateObserver(&obs3, obs3_metrics_frequencies);
  expected_metrics_frequencies.free_phys_memory_mb_frequency =
      SystemMonitor::SamplingFrequency::kDefaultFrequency;
  observed_metrics_and_frequencies =
      system_monitor_->metric_refresh_frequencies_for_testing();
  EXPECT_TRUE(observed_metrics_and_frequencies == expected_metrics_frequencies);

  // Remove the third observe, ensure that no metrics are observed anymore.
  system_monitor_->RemoveObserver(&obs3);
  expected_metrics_frequencies.free_phys_memory_mb_frequency =
      SystemMonitor::SamplingFrequency::kNoSampling;
  observed_metrics_and_frequencies =
      system_monitor_->metric_refresh_frequencies_for_testing();
  EXPECT_TRUE(observed_metrics_and_frequencies == expected_metrics_frequencies);
}

TEST_F(SystemMonitorTest, ObserverGetsCalled) {
  system_monitor_->SetHelperForTesting(
      std::make_unique<TestSystemMonitorHelper>());

  ::testing::StrictMock<MockMetricsMonitorObserver> mock_observer_1;
  system_monitor_->AddOrUpdateObserver(
      &mock_observer_1,
      {.free_phys_memory_mb_frequency =
           SystemMonitor::SamplingFrequency::kDefaultFrequency});

  ::testing::StrictMock<MockMetricsMonitorObserver> mock_observer_2;
  system_monitor_->AddOrUpdateObserver(&mock_observer_2, {});

  // Ensure that we get several samples to verify that the timer logic works.
  EXPECT_CALL(mock_observer_1, OnFreePhysicalMemoryMbSample(::testing::Eq(42)))
      .Times(2);

  // The second observer shouldn't be called.
  EXPECT_CALL(mock_observer_2, OnFreePhysicalMemoryMbSample(::testing::_))
      .Times(0);

  // Fast forward by enough time to get multiple samples and wait for the tasks
  // to complete.
  scoped_task_environment_.FastForwardBy(2 * kRefreshInterval);
  scoped_task_environment_.RunUntilIdle();

  ::testing::Mock::VerifyAndClear(&mock_observer_1);
  ::testing::Mock::VerifyAndClear(&mock_observer_2);
}

}  // namespace

}  // namespace performance_monitor
