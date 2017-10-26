// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/oom_intervention/near_oom_monitor.h"

#include "base/message_loop/message_loop.h"
#include "base/test/test_mock_time_task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"

class MockNearOomMonitor : public NearOomMonitor {
 public:
  explicit MockNearOomMonitor(
      scoped_refptr<base::SequencedTaskRunner> task_runner)
      : NearOomMonitor(task_runner) {
    // Start with 128MB swap total and 64MB swap free.
    memory_info_.swap_total = 128 * 1024;
    memory_info_.swap_free = 64 * 1024;
  }

  void SetSwapTotal(int swap_total) { memory_info_.swap_total = swap_total; }

  void SetSwapFree(int swap_free) { memory_info_.swap_free = swap_free; }

  bool is_get_system_memory_info_called() const {
    return is_get_system_memory_info_called_;
  }

 private:
  bool GetSystemMemoryInfo(base::SystemMemoryInfoKB* memory_info) override {
    *memory_info = memory_info_;
    is_get_system_memory_info_called_ = true;
    return true;
  }

  base::SystemMemoryInfoKB memory_info_;
  bool is_get_system_memory_info_called_ = false;

  DISALLOW_COPY_AND_ASSIGN(MockNearOomMonitor);
};

class TestNearOomObserver {
 public:
  explicit TestNearOomObserver(NearOomMonitor* monitor) {
    DCHECK(monitor);
    subscription_ = monitor->RegisterCallback(base::Bind(
        &TestNearOomObserver::OnNearOomDetected, base::Unretained(this)));
  }

  void Unsubscribe() { subscription_.reset(); }

  bool is_detected() const { return is_detected_; }

 private:
  void OnNearOomDetected() { is_detected_ = true; }

  bool is_detected_ = false;
  std::unique_ptr<NearOomMonitor::Subscription> subscription_;

  DISALLOW_COPY_AND_ASSIGN(TestNearOomObserver);
};

class NearOomMonitorTest : public testing::Test {
 public:
  void SetUp() override {
    task_runner_ = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
    monitor_ = std::make_unique<MockNearOomMonitor>(task_runner_);
  }

 protected:
  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;
  base::MessageLoop message_loop_;
  std::unique_ptr<MockNearOomMonitor> monitor_;
};

TEST_F(NearOomMonitorTest, NoSwap) {
  monitor_->SetSwapTotal(0);
  EXPECT_FALSE(monitor_->Start());
  EXPECT_FALSE(monitor_->IsRunning());
}

TEST_F(NearOomMonitorTest, Observe) {
  ASSERT_TRUE(monitor_->Start());
  ASSERT_TRUE(monitor_->IsRunning());

  TestNearOomObserver observer1(monitor_.get());
  TestNearOomObserver observer2(monitor_.get());

  base::TimeDelta interval =
      monitor_->GetMonitoringInterval() + base::TimeDelta::FromSeconds(1);

  task_runner_->FastForwardBy(interval);
  EXPECT_TRUE(monitor_->is_get_system_memory_info_called());
  EXPECT_FALSE(observer1.is_detected());
  EXPECT_FALSE(observer2.is_detected());

  observer2.Unsubscribe();

  // Simulate near-OOM situation by setting swap free to zero.
  monitor_->SetSwapFree(0);
  task_runner_->FastForwardBy(interval);
  EXPECT_TRUE(observer1.is_detected());
  EXPECT_FALSE(observer2.is_detected());

  observer1.Unsubscribe();
}
