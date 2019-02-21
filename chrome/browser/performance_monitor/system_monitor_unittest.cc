// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_monitor/system_monitor.h"

#include "base/memory/ptr_util.h"
#include "base/test/scoped_task_environment.h"
#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_monitor {

namespace {

using SamplingFrequency = SystemMonitor::SamplingFrequency;
using SystemObserver = SystemMonitor::SystemObserver;
using MetricsRefreshFrequencies = SystemObserver::MetricRefreshFrequencies;

const int kFakeFreePhysMemoryMb = 42;

class MockMetricsMonitorObserver : public SystemObserver {
 public:
  ~MockMetricsMonitorObserver() override {}
  MOCK_METHOD1(OnFreePhysicalMemoryMbSample, void(int free_phys_memory_mb));
};

class TestSystemMonitor : public SystemMonitor {
 public:
  using SystemMonitor::FreePhysMemoryMetricEvaluator;
  using SystemMonitor::GetMetricEvaluatorMetadataForTesting;
  using SystemMonitor::GetMetricSamplingFrequencyArrayForTesting;
  using SystemMonitor::MetricEvaluator;
  using SystemMonitor::MetricMetadata;

  TestSystemMonitor();
  ~TestSystemMonitor() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestSystemMonitor);
};

class TestFreePhysMemoryMetricEvaluator
    : public TestSystemMonitor::FreePhysMemoryMetricEvaluator {
 public:
  TestFreePhysMemoryMetricEvaluator() = default;
  ~TestFreePhysMemoryMetricEvaluator() override = default;

  // FreePhysMemoryMetricEvaluator::Metric:
  void Evaluate() override {
    set_value_for_testing(kFakeFreePhysMemoryMb);
    has_value_ = true;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestFreePhysMemoryMetricEvaluator);
};

TestSystemMonitor::TestSystemMonitor() {
  auto* free_memory_helper = GetMetricEvaluatorMetadataForTesting(
      MetricEvaluator::Type::kFreeMemoryMb);
  free_memory_helper->create_metric_evaluator_function = []() {
    std::unique_ptr<SystemMonitor::MetricEvaluator> metric =
        base::WrapUnique(new TestFreePhysMemoryMetricEvaluator());
    return metric;
  };
}

}  // namespace

class SystemMonitorTest : public testing::Test {
 protected:
  SystemMonitorTest()
      : scoped_task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::MOCK_TIME) {}

  void SetUp() override {
    EXPECT_EQ(nullptr, SystemMonitor::Get());
    system_monitor_ = std::make_unique<TestSystemMonitor>();
  }

  void TearDown() override { system_monitor_.reset(); }

  void EnsureMetricsAreObservedAtExpectedFrequency(
      SamplingFrequency expected_free_memory_mb_freq) {
    const auto& observed_metrics_and_frequencies =
        system_monitor_->GetMetricSamplingFrequencyArrayForTesting();

    EXPECT_EQ(1U, observed_metrics_and_frequencies.size());
    EXPECT_EQ(expected_free_memory_mb_freq,
              observed_metrics_and_frequencies[static_cast<size_t>(
                  TestSystemMonitor::MetricEvaluator::Type::kFreeMemoryMb)]);
  }

  std::unique_ptr<TestSystemMonitor> system_monitor_;

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
  EnsureMetricsAreObservedAtExpectedFrequency(SamplingFrequency::kNoSampling);

  // Add a second observer that observes the amount of free memory at the
  // default frequency.
  MetricsRefreshFrequencies obs2_metrics_frequencies = {
      .free_phys_memory_mb_frequency = SamplingFrequency::kDefaultFrequency};
  system_monitor_->AddOrUpdateObserver(&obs2, obs2_metrics_frequencies);
  EnsureMetricsAreObservedAtExpectedFrequency(
      SamplingFrequency::kDefaultFrequency);

  // Add a third observer that also observes the amount of free memory at the
  // default frequency.
  MetricsRefreshFrequencies obs3_metrics_frequencies = {
      .free_phys_memory_mb_frequency = SamplingFrequency::kDefaultFrequency};
  system_monitor_->AddOrUpdateObserver(&obs3, obs3_metrics_frequencies);
  EnsureMetricsAreObservedAtExpectedFrequency(
      SamplingFrequency::kDefaultFrequency);

  // Stop observing any metric with the second observer.
  obs2_metrics_frequencies.free_phys_memory_mb_frequency =
      SamplingFrequency::kNoSampling;
  system_monitor_->AddOrUpdateObserver(&obs2, obs2_metrics_frequencies);
  EnsureMetricsAreObservedAtExpectedFrequency(
      SamplingFrequency::kDefaultFrequency);

  // Remove the third observer, ensure that no metrics are observed anymore.
  system_monitor_->RemoveObserver(&obs3);
  EnsureMetricsAreObservedAtExpectedFrequency(SamplingFrequency::kNoSampling);
}

TEST_F(SystemMonitorTest, ObserverGetsCalled) {
  ::testing::StrictMock<MockMetricsMonitorObserver> mock_observer;
  system_monitor_->AddOrUpdateObserver(
      &mock_observer,
      {.free_phys_memory_mb_frequency = SamplingFrequency::kDefaultFrequency});

  // Ensure that we get several samples to verify that the timer logic works.
  EXPECT_CALL(mock_observer, OnFreePhysicalMemoryMbSample(
                                 ::testing::Eq(kFakeFreePhysMemoryMb)))
      .Times(2);

  EXPECT_TRUE(system_monitor_->refresh_timer_for_testing().IsRunning());

  // Fast forward by enough time to get multiple samples and wait for the tasks
  // to complete.
  scoped_task_environment_.FastForwardBy(
      2 * system_monitor_->refresh_timer_for_testing().GetCurrentDelay());
  scoped_task_environment_.RunUntilIdle();

  ::testing::Mock::VerifyAndClear(&mock_observer);
}

}  // namespace performance_monitor
