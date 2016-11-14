// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/chromeos/screen_security/screen_tray_item.h"
#include "ash/common/system/tray/system_tray.h"
#include "ash/common/wm/overview/window_selector_controller.h"
#include "ash/common/wm_shell.h"
#include "ash/test/ash_test_base.h"
#include "base/run_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/ui/ash/multi_user/user_switch_util.h"
#include "chrome/browser/ui/simple_message_box.h"
#include "ui/aura/window.h"

namespace ash {

class TrySwitchingUserTest : public ash::test::AshTestBase {
 public:
  // The action type to perform / check for upon user switching.
  enum ActionType {
    NO_DIALOG,       // No dialog should be shown.
    ACCEPT_DIALOG,   // A dialog should be shown and we should accept it.
    DECLINE_DIALOG,  // A dialog should be shown and we do not accept it.
  };
  TrySwitchingUserTest()
      : capture_item_(NULL),
        share_item_(NULL),
        stop_capture_callback_hit_count_(0),
        stop_share_callback_hit_count_(0),
        switch_callback_hit_count_(0) {}
  ~TrySwitchingUserTest() override {}

  void SetUp() override {
    test::AshTestBase::SetUp();
    TrayItemView::DisableAnimationsForTest();
    SystemTray* system_tray = GetPrimarySystemTray();
    share_item_ = system_tray->GetScreenShareItem();
    capture_item_ = system_tray->GetScreenCaptureItem();
    EXPECT_TRUE(share_item_);
    EXPECT_TRUE(capture_item_);
  }

  // Accessing the capture session functionality.
  // Simulates a screen capture session start.
  void StartCaptureSession() {
    capture_item_->Start(base::Bind(&TrySwitchingUserTest::StopCaptureCallback,
                                    base::Unretained(this)));
  }

  // The callback which gets called when the screen capture gets stopped.
  void StopCaptureSession() { capture_item_->Stop(); }

  // Simulates a screen capture session stop.
  void StopCaptureCallback() { stop_capture_callback_hit_count_++; }

  // Accessing the share session functionality.
  // Simulate a Screen share session start.
  void StartShareSession() {
    share_item_->Start(base::Bind(&TrySwitchingUserTest::StopShareCallback,
                                  base::Unretained(this)));
  }

  // Simulates a screen share session stop.
  void StopShareSession() { share_item_->Stop(); }

  // The callback which gets called when the screen share gets stopped.
  void StopShareCallback() { stop_share_callback_hit_count_++; }

  // Issuing a switch user call which might or might not create a dialog.
  // The passed |action| type parameter defines the outcome (which will be
  // checked) and the action the user will choose.
  void SwitchUser(ActionType action) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(&CloseMessageBox, action));
    TrySwitchingActiveUser(base::Bind(&TrySwitchingUserTest::SwitchCallback,
                                      base::Unretained(this)));
    base::RunLoop().RunUntilIdle();
  }

  // Called when the user will get actually switched.
  void SwitchCallback() { switch_callback_hit_count_++; }

  // Methods needed to test with overview mode.
  const WindowSelectorController* window_selector_controller() const {
    return WmShell::Get()->window_selector_controller();
  }
  WindowSelectorController* window_selector_controller() {
    return const_cast<WindowSelectorController*>(
        const_cast<const TrySwitchingUserTest*>(this)
            ->window_selector_controller());
  }
  bool ToggleOverview() {
    return window_selector_controller()->ToggleOverview();
  }
  bool IsSelecting() const {
    return window_selector_controller()->IsSelecting();
  }

  // Various counter accessors.
  int stop_capture_callback_hit_count() const {
    return stop_capture_callback_hit_count_;
  }
  int stop_share_callback_hit_count() const {
    return stop_share_callback_hit_count_;
  }
  int switch_callback_hit_count() const { return switch_callback_hit_count_; }

 private:
  static void CloseMessageBox(ActionType action) {
    switch (action) {
      case NO_DIALOG:
        EXPECT_FALSE(chrome::CloseMessageBoxForTest(true));
        return;
      case ACCEPT_DIALOG:
        EXPECT_TRUE(chrome::CloseMessageBoxForTest(true));
        return;
      case DECLINE_DIALOG:
        EXPECT_TRUE(chrome::CloseMessageBoxForTest(false));
        return;
    }
  }

  // The two items from the SystemTray for the screen capture / share
  // functionality.
  ScreenTrayItem* capture_item_;
  ScreenTrayItem* share_item_;

  // Various counters to query for.
  int stop_capture_callback_hit_count_;
  int stop_share_callback_hit_count_;
  int switch_callback_hit_count_;

  DISALLOW_COPY_AND_ASSIGN(TrySwitchingUserTest);
};

// Test that when there is no screen operation going on the user switch will be
// performed as planned.
TEST_F(TrySwitchingUserTest, NoLock) {
  EXPECT_EQ(0, switch_callback_hit_count());
  SwitchUser(TrySwitchingUserTest::NO_DIALOG);
  EXPECT_EQ(1, switch_callback_hit_count());
}

// Test that with a screen capture operation going on, the user will need to
// confirm. Declining will neither change the running state or switch users.
TEST_F(TrySwitchingUserTest, CaptureActiveDeclined) {
  EXPECT_EQ(0, switch_callback_hit_count());
  StartCaptureSession();
  SwitchUser(TrySwitchingUserTest::DECLINE_DIALOG);
  EXPECT_EQ(0, switch_callback_hit_count());
  EXPECT_EQ(0, stop_capture_callback_hit_count());
  EXPECT_EQ(0, stop_share_callback_hit_count());
  StopCaptureSession();
  EXPECT_EQ(0, switch_callback_hit_count());
  EXPECT_EQ(1, stop_capture_callback_hit_count());
  EXPECT_EQ(0, stop_share_callback_hit_count());
}

// Test that with a screen share operation going on, the user will need to
// confirm. Declining will neither change the running state or switch users.
TEST_F(TrySwitchingUserTest, ShareActiveDeclined) {
  EXPECT_EQ(0, switch_callback_hit_count());
  StartShareSession();
  SwitchUser(TrySwitchingUserTest::DECLINE_DIALOG);
  EXPECT_EQ(0, switch_callback_hit_count());
  EXPECT_EQ(0, stop_capture_callback_hit_count());
  EXPECT_EQ(0, stop_share_callback_hit_count());
  StopShareSession();
  EXPECT_EQ(0, switch_callback_hit_count());
  EXPECT_EQ(0, stop_capture_callback_hit_count());
  EXPECT_EQ(1, stop_share_callback_hit_count());
}

// Test that with both operations going on, the user will need to confirm.
// Declining will neither change the running state or switch users.
TEST_F(TrySwitchingUserTest, BothActiveDeclined) {
  EXPECT_EQ(0, switch_callback_hit_count());
  StartShareSession();
  StartCaptureSession();
  SwitchUser(TrySwitchingUserTest::DECLINE_DIALOG);
  EXPECT_EQ(0, switch_callback_hit_count());
  EXPECT_EQ(0, stop_capture_callback_hit_count());
  EXPECT_EQ(0, stop_share_callback_hit_count());
  StopShareSession();
  StopCaptureSession();
  EXPECT_EQ(0, switch_callback_hit_count());
  EXPECT_EQ(1, stop_capture_callback_hit_count());
  EXPECT_EQ(1, stop_share_callback_hit_count());
}

// Test that with a screen capture operation going on, the user will need to
// confirm. Accepting will change to stopped state and switch users.
TEST_F(TrySwitchingUserTest, CaptureActiveAccepted) {
  EXPECT_EQ(0, switch_callback_hit_count());
  StartCaptureSession();
  SwitchUser(TrySwitchingUserTest::ACCEPT_DIALOG);
  EXPECT_EQ(1, switch_callback_hit_count());
  EXPECT_EQ(1, stop_capture_callback_hit_count());
  EXPECT_EQ(0, stop_share_callback_hit_count());
  // Another stop should have no effect.
  StopCaptureSession();
  EXPECT_EQ(1, switch_callback_hit_count());
  EXPECT_EQ(1, stop_capture_callback_hit_count());
  EXPECT_EQ(0, stop_share_callback_hit_count());
}

// Test that with a screen share operation going on, the user will need to
// confirm. Accepting will change to stopped state and switch users.
TEST_F(TrySwitchingUserTest, ShareActiveAccepted) {
  EXPECT_EQ(0, switch_callback_hit_count());
  StartShareSession();
  SwitchUser(TrySwitchingUserTest::ACCEPT_DIALOG);
  EXPECT_EQ(1, switch_callback_hit_count());
  EXPECT_EQ(0, stop_capture_callback_hit_count());
  EXPECT_EQ(1, stop_share_callback_hit_count());
  // Another stop should have no effect.
  StopShareSession();
  EXPECT_EQ(1, switch_callback_hit_count());
  EXPECT_EQ(0, stop_capture_callback_hit_count());
  EXPECT_EQ(1, stop_share_callback_hit_count());
}

// Test that with both operations going on, the user will need to confirm.
// Accepting will change to stopped state and switch users.
TEST_F(TrySwitchingUserTest, BothActiveAccepted) {
  EXPECT_EQ(0, switch_callback_hit_count());
  StartShareSession();
  StartCaptureSession();
  SwitchUser(TrySwitchingUserTest::ACCEPT_DIALOG);
  EXPECT_EQ(1, switch_callback_hit_count());
  EXPECT_EQ(1, stop_capture_callback_hit_count());
  EXPECT_EQ(1, stop_share_callback_hit_count());
  // Another stop should have no effect.
  StopShareSession();
  StopCaptureSession();
  EXPECT_EQ(1, switch_callback_hit_count());
  EXPECT_EQ(1, stop_capture_callback_hit_count());
  EXPECT_EQ(1, stop_share_callback_hit_count());
}

// Test that overview mode is dismissed before switching user profile.
TEST_F(TrySwitchingUserTest, OverviewModeDismissed) {
  EXPECT_EQ(0, switch_callback_hit_count());
  gfx::Rect bounds(0, 0, 100, 100);
  std::unique_ptr<aura::Window> w(CreateTestWindowInShellWithBounds(bounds));
  ASSERT_TRUE(ToggleOverview());
  ASSERT_TRUE(IsSelecting());
  SwitchUser(TrySwitchingUserTest::NO_DIALOG);
  ASSERT_FALSE(IsSelecting());
  EXPECT_EQ(1, switch_callback_hit_count());
}

}  // namespace ash
