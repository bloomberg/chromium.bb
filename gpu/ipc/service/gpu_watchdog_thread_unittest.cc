// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/gpu_watchdog_thread.h"

#include "base/bind.h"
#include "base/synchronization/waitable_event.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu {

class GpuWatchdogThreadTest : public ::testing::Test {
 public:
  GpuWatchdogThreadTest()
      : terminate_event_(base::WaitableEvent::ResetPolicy::MANUAL,
                         base::WaitableEvent::InitialState::NOT_SIGNALED),
        thread_(GpuWatchdogThread::Create()) {
    thread_->SetTimeoutForTesting(base::TimeDelta::FromMilliseconds(50));
    thread_->SetAlternativeTerminateFunctionForTesting(base::BindRepeating(
        &GpuWatchdogThreadTest::OnTerminated, base::Unretained(this)));
  }

  void CheckNoTerminate() {
    ASSERT_FALSE(terminate_event_.IsSignaled());
    thread_->CheckArmed();
    EXPECT_FALSE(
        terminate_event_.TimedWait(base::TimeDelta::FromMilliseconds(500)));
  }

  void CheckTerminate() {
    ASSERT_FALSE(terminate_event_.IsSignaled());
    thread_->CheckArmed();
    EXPECT_TRUE(
        terminate_event_.TimedWait(base::TimeDelta::FromMilliseconds(500)));
  }

  GpuWatchdogThread* thread() { return thread_.get(); }
  void OnPowerObserverSuspend() {
    thread_->task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&base::PowerObserver::OnSuspend,
                                  base::Unretained(thread_.get())));
  }
  void OnPowerObserverResume() {
    thread_->task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&base::PowerObserver::OnResume,
                                  base::Unretained(thread_.get())));
  }

 private:
  void OnTerminated() {
    ASSERT_FALSE(terminate_event_.IsSignaled());
    terminate_event_.Signal();
  }

  base::MessageLoop message_loop_;
  base::WaitableEvent terminate_event_;
  std::unique_ptr<GpuWatchdogThread> thread_;
};

TEST_F(GpuWatchdogThreadTest, BasicTerminate) {
  CheckTerminate();
}

TEST_F(GpuWatchdogThreadTest, NoTerminateIfBackgrounded) {
  thread()->OnBackgrounded();
  CheckNoTerminate();
  thread()->OnForegrounded();
  CheckTerminate();
}

TEST_F(GpuWatchdogThreadTest, NoTerminateIfPowerStateSuspended) {
  OnPowerObserverSuspend();
  CheckNoTerminate();
  OnPowerObserverResume();
  CheckTerminate();
}

TEST_F(GpuWatchdogThreadTest, NoTerminateIfEitherPowerOrBackgrounded) {
  thread()->OnBackgrounded();
  OnPowerObserverSuspend();
  CheckNoTerminate();
  OnPowerObserverResume();
  CheckNoTerminate();
  OnPowerObserverSuspend();
  thread()->OnForegrounded();
  CheckNoTerminate();
  OnPowerObserverResume();
  CheckTerminate();
}

TEST_F(GpuWatchdogThreadTest, ShutdownWithActiveRefs) {
  OnPowerObserverSuspend();
  thread()->OnBackgrounded();
  CheckNoTerminate();

  // Exit without releasing the ref (no OnForegrounded), to ensure that cleanup
  // happens correctly (no DCHECKs).
}

}  // namespace gpu
