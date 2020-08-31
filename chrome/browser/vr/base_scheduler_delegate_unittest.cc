// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/base_scheduler_delegate.h"

#include "base/memory/scoped_refptr.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/vr/scheduler_ui_interface.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace vr {

class MockSchedulerUi : public SchedulerUiInterface {
 public:
  MockSchedulerUi() = default;
  ~MockSchedulerUi() override = default;

  // SchedulerUiInterface
  MOCK_METHOD0(OnWebXrFrameAvailable, void());
  MOCK_METHOD0(OnWebXrTimedOut, void());
  MOCK_METHOD0(OnWebXrTimeoutImminent, void());
};

class ConcreteSchedulerDelegate : public BaseSchedulerDelegate {
 public:
  explicit ConcreteSchedulerDelegate(SchedulerUiInterface* ui)
      : BaseSchedulerDelegate(ui, true, 2, 5) {}
  ~ConcreteSchedulerDelegate() override = default;

  void ScheduleWebXrFrameTimeout() {
    BaseSchedulerDelegate::ScheduleWebXrFrameTimeout();
  }
  void OnNewWebXrFrame() { BaseSchedulerDelegate::OnNewWebXrFrame(); }

 private:
  void OnPause() override {}
  void OnResume() override {}
  void SetShowingVrDialog(bool showing) override {}
  void SetBrowserRenderer(
      SchedulerBrowserRendererInterface* browser_renderer) override {}
  void SubmitDrawnFrame(FrameType frame_type,
                        const gfx::Transform& head_pose) override {}
  void AddInputSourceState(
      device::mojom::XRInputSourceStatePtr state) override {}
  void ConnectPresentingService(
      device::mojom::VRDisplayInfoPtr display_info,
      device::mojom::XRRuntimeSessionOptionsPtr options) override {}
};

class SchedulerDelegateTest : public testing::Test {
 public:
  void SetUp() override {
    test_task_runner_ =
        base::WrapRefCounted(new base::TestMockTimeTaskRunner());
    on_destroy_ =
        base::ThreadTaskRunnerHandle::OverrideForTesting(test_task_runner_);
  }

  void FastForwardBy(base::TimeDelta delta) {
    test_task_runner_->FastForwardBy(delta);
  }

 private:
  scoped_refptr<base::TestMockTimeTaskRunner> test_task_runner_;
  base::ScopedClosureRunner on_destroy_;
};

TEST_F(SchedulerDelegateTest, NoTimeoutWhenWebXrFrameArrivesFast) {
  MockSchedulerUi ui;
  ConcreteSchedulerDelegate scheduler_delegate(&ui);
  scheduler_delegate.ScheduleWebXrFrameTimeout();

  EXPECT_CALL(ui, OnWebXrTimeoutImminent()).Times(0);
  EXPECT_CALL(ui, OnWebXrTimedOut()).Times(0);
  FastForwardBy(base::TimeDelta::FromSeconds(1));
  scheduler_delegate.OnNewWebXrFrame();
  FastForwardBy(base::TimeDelta::FromSeconds(10));
}

TEST_F(SchedulerDelegateTest, OneTimeoutWhenWebXrFrameArrivesSlow) {
  MockSchedulerUi ui;
  ConcreteSchedulerDelegate scheduler_delegate(&ui);
  scheduler_delegate.ScheduleWebXrFrameTimeout();

  EXPECT_CALL(ui, OnWebXrTimeoutImminent()).Times(1);
  EXPECT_CALL(ui, OnWebXrTimedOut()).Times(0);
  FastForwardBy(base::TimeDelta::FromSeconds(3));
  scheduler_delegate.OnNewWebXrFrame();
  FastForwardBy(base::TimeDelta::FromSeconds(10));
}

TEST_F(SchedulerDelegateTest, TwoTimeoutsWhenWebXrFrameDoesNotArrive) {
  MockSchedulerUi ui;
  ConcreteSchedulerDelegate scheduler_delegate(&ui);
  scheduler_delegate.ScheduleWebXrFrameTimeout();

  EXPECT_CALL(ui, OnWebXrTimeoutImminent()).Times(1);
  EXPECT_CALL(ui, OnWebXrTimedOut()).Times(1);
  FastForwardBy(base::TimeDelta::FromSeconds(10));
}

TEST_F(SchedulerDelegateTest, NoTimeoutIfExitPresent) {
  MockSchedulerUi ui;
  ConcreteSchedulerDelegate scheduler_delegate(&ui);
  scheduler_delegate.ScheduleWebXrFrameTimeout();

  EXPECT_CALL(ui, OnWebXrTimeoutImminent()).Times(0);
  EXPECT_CALL(ui, OnWebXrTimedOut()).Times(0);
  scheduler_delegate.OnExitPresent();
  FastForwardBy(base::TimeDelta::FromSeconds(10));
}

TEST_F(SchedulerDelegateTest, NoTimeoutIfUnsetWebXrMode) {
  MockSchedulerUi ui;
  ConcreteSchedulerDelegate scheduler_delegate(&ui);
  scheduler_delegate.ScheduleWebXrFrameTimeout();

  EXPECT_CALL(ui, OnWebXrTimeoutImminent()).Times(0);
  EXPECT_CALL(ui, OnWebXrTimedOut()).Times(0);
  scheduler_delegate.SetWebXrMode(false);
  FastForwardBy(base::TimeDelta::FromSeconds(10));
}

}  // namespace vr
