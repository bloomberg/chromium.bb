// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/chromeos/rotation/tray_rotation_lock.h"

#include "ash/ash_switches.h"
#include "ash/display/display_manager.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/status_area_widget_test_helper.h"
#include "ash/wm/maximize_mode/maximize_mode_controller.h"
#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/views/view.h"

namespace ash {

class TrayRotationLockTest : public test::AshTestBase {
 public:
  TrayRotationLockTest() {}
  virtual ~TrayRotationLockTest() {}

  TrayRotationLock* tray() {
    return tray_.get();
  }

  views::View* tray_view() {
    return tray_view_.get();
  }

  views::View* default_view() {
    return default_view_.get();
  }

  // Sets up a TrayRotationLock, its tray view, and its default view, for the
  // given SystemTray and its display. On a primary display all will be
  // created. On a secondary display both the tray view and default view will
  // be null.
  void SetUpForStatusAreaWidget(StatusAreaWidget* status_area_widget);

  // Resets |tray_| |tray_view_| and |default_view_| so that all components of
  // TrayRotationLock have been cleared. Tests may then call
  // SetUpForStatusAreaWidget in order to initial the components.
  void TearDownViews();

  // test::AshTestBase:
  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

 private:
  scoped_ptr<TrayRotationLock> tray_;
  scoped_ptr<views::View> tray_view_;
  scoped_ptr<views::View> default_view_;

  DISALLOW_COPY_AND_ASSIGN(TrayRotationLockTest);
};

void TrayRotationLockTest::SetUpForStatusAreaWidget(
    StatusAreaWidget* status_area_widget) {
  tray_.reset(new TrayRotationLock(status_area_widget->system_tray()));
  tray_view_.reset(tray_->CreateTrayView(
      StatusAreaWidgetTestHelper::GetUserLoginStatus()));
  default_view_.reset(tray_->CreateDefaultView(
      StatusAreaWidgetTestHelper::GetUserLoginStatus()));
}

void TrayRotationLockTest::TearDownViews() {
  tray_view_.reset();
  default_view_.reset();
  tray_.reset();
}

void TrayRotationLockTest::SetUp() {
  // The Display used for testing is not an internal display. This flag
  // allows for DisplayManager to treat it as one. TrayRotationLock is only
  // visible on internal primary displays.
  CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kAshUseFirstDisplayAsInternal);
  test::AshTestBase::SetUp();
  SetUpForStatusAreaWidget(StatusAreaWidgetTestHelper::GetStatusAreaWidget());
}

void TrayRotationLockTest::TearDown() {
  TearDownViews();
  test::AshTestBase::TearDown();
}

// Tests that when the tray view is initially created, that it is created
// not visible.
TEST_F(TrayRotationLockTest, CreateTrayView) {
  EXPECT_FALSE(tray_view()->visible());
}

// Tests that when the tray view is created, while MaximizeMode is active, that
// it is not visible.
TEST_F(TrayRotationLockTest, CreateTrayViewDuringMaximizeMode) {
  TearDownViews();
  Shell::GetInstance()->maximize_mode_controller()->
      EnableMaximizeModeWindowManager(true);
  SetUpForStatusAreaWidget(StatusAreaWidgetTestHelper::GetStatusAreaWidget());
  EXPECT_FALSE(tray_view()->visible());
  Shell::GetInstance()->maximize_mode_controller()->
      EnableMaximizeModeWindowManager(false);
}

// Tests that when the tray view is created, while MaximizeMode is active, and
// rotation is locked, that it is visible.
TEST_F(TrayRotationLockTest, CreateTrayViewDuringMaximizeModeAndRotationLock) {
  TearDownViews();
  Shell::GetInstance()->maximize_mode_controller()->
      EnableMaximizeModeWindowManager(true);
  Shell::GetInstance()-> maximize_mode_controller()->SetRotationLocked(true);
  SetUpForStatusAreaWidget(StatusAreaWidgetTestHelper::GetStatusAreaWidget());
  EXPECT_TRUE(tray_view()->visible());
  Shell::GetInstance()->maximize_mode_controller()->
      EnableMaximizeModeWindowManager(false);
  EXPECT_FALSE(tray_view()->visible());
}

// Tests that the enabling of MaximizeMode affects a previously created tray
// view, changing the visibility.
TEST_F(TrayRotationLockTest, TrayViewVisibilityChangesDuringMaximizeMode) {
  ASSERT_FALSE(tray_view()->visible());
  Shell::GetInstance()->maximize_mode_controller()->
      EnableMaximizeModeWindowManager(true);
  Shell::GetInstance()->maximize_mode_controller()->SetRotationLocked(true);
  EXPECT_TRUE(tray_view()->visible());
  Shell::GetInstance()->maximize_mode_controller()->
      EnableMaximizeModeWindowManager(false);
  EXPECT_FALSE(tray_view()->visible());
}

// Tests that the when the tray view is created for a secondary display, that
// it is not visible, and that MaximizeMode does not affect visibility.
TEST_F(TrayRotationLockTest, CreateSecondaryTrayView) {
  if (!SupportsMultipleDisplays())
    return;
  UpdateDisplay("400x400,200x200");

  SetUpForStatusAreaWidget(
      StatusAreaWidgetTestHelper::GetSecondaryStatusAreaWidget());
  EXPECT_FALSE(tray_view()->visible());
  Shell::GetInstance()->maximize_mode_controller()->
      EnableMaximizeModeWindowManager(true);
  EXPECT_FALSE(tray_view()->visible());
  Shell::GetInstance()->maximize_mode_controller()->
      EnableMaximizeModeWindowManager(false);
  EXPECT_FALSE(tray_view()->visible());
}

// Tests that when the default view is initially created, that it is created
// not visible.
TEST_F(TrayRotationLockTest, CreateDefaultView) {
  EXPECT_FALSE(default_view()->visible());
}

// Tests that when the default view is created, while MaximizeMode is active,
// that it is visible.
TEST_F(TrayRotationLockTest, CreateDefaultViewDuringMaximizeMode) {
  TearDownViews();
  Shell::GetInstance()->maximize_mode_controller()->
      EnableMaximizeModeWindowManager(true);
  SetUpForStatusAreaWidget(StatusAreaWidgetTestHelper::GetStatusAreaWidget());
  EXPECT_TRUE(default_view()->visible());
  Shell::GetInstance()->maximize_mode_controller()->
      EnableMaximizeModeWindowManager(false);
}

// Tests that the enabling of MaximizeMode affects a previously created default
// view, changing the visibility.
TEST_F(TrayRotationLockTest, DefaultViewVisibilityChangesDuringMaximizeMode) {
  Shell::GetInstance()->maximize_mode_controller()->
      EnableMaximizeModeWindowManager(true);
  EXPECT_TRUE(default_view()->visible());
  Shell::GetInstance()->maximize_mode_controller()->
      EnableMaximizeModeWindowManager(false);
  EXPECT_FALSE(default_view()->visible());
}

// Tests that no default view is created when the target is a secondary
// display.
TEST_F(TrayRotationLockTest, CreateSecondaryDefaultView) {
  if (!SupportsMultipleDisplays())
    return;
  UpdateDisplay("400x400,200x200");

  TearDownViews();
  SetUpForStatusAreaWidget(
      StatusAreaWidgetTestHelper::GetSecondaryStatusAreaWidget());
  EXPECT_EQ(NULL, default_view());
}

// Tests that activating the default view causes the display to have its
// rotation locked, and that the tray view becomes visible.
TEST_F(TrayRotationLockTest, PerformActionOnDefaultView) {
  MaximizeModeController* maximize_mode_controller = Shell::GetInstance()->
      maximize_mode_controller();
  ASSERT_FALSE(maximize_mode_controller->rotation_locked());
  Shell::GetInstance()->maximize_mode_controller()->
      EnableMaximizeModeWindowManager(true);
  ASSERT_FALSE(tray_view()->visible());

  ui::GestureEvent tap(
      0, 0, 0, base::TimeDelta(), ui::GestureEventDetails(ui::ET_GESTURE_TAP));
  default_view()->OnGestureEvent(&tap);
  EXPECT_TRUE(maximize_mode_controller->rotation_locked());
  EXPECT_TRUE(tray_view()->visible());

  Shell::GetInstance()->maximize_mode_controller()->
      EnableMaximizeModeWindowManager(false);
}

}  // namespace ash
