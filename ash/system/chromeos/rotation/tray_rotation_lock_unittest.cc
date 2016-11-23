// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/chromeos/rotation/tray_rotation_lock.h"

#include <memory>

#include "ash/common/system/status_area_widget.h"
#include "ash/common/system/tray/system_tray.h"
#include "ash/common/system/tray/system_tray_delegate.h"
#include "ash/common/wm/maximize_mode/maximize_mode_controller.h"
#include "ash/common/wm_shell.h"
#include "ash/display/screen_orientation_controller_chromeos.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/status_area_widget_test_helper.h"
#include "base/command_line.h"
#include "base/time/time.h"
#include "ui/display/display_switches.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/types/display_constants.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/views/view.h"

namespace ash {

class TrayRotationLockTest : public test::AshTestBase {
 public:
  TrayRotationLockTest() {}
  ~TrayRotationLockTest() override {}

  TrayRotationLock* tray() { return tray_.get(); }

  views::View* tray_view() { return tray_view_.get(); }

  views::View* default_view() { return default_view_.get(); }

  // Creates the tray view associated to |tray_rotation_lock|.
  views::View* CreateTrayView(TrayRotationLock* tray_rotation_lock);

  // Destroys only the |tray_view_|. Tests may call this to simulate destruction
  // order during the deletion of the StatusAreaWidget.
  void DestroyTrayView();

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
  void SetUp() override;
  void TearDown() override;

 private:
  std::unique_ptr<TrayRotationLock> tray_;
  std::unique_ptr<views::View> tray_view_;
  std::unique_ptr<views::View> default_view_;

  DISALLOW_COPY_AND_ASSIGN(TrayRotationLockTest);
};

views::View* TrayRotationLockTest::CreateTrayView(
    TrayRotationLock* tray_rotation_lock) {
  return tray_rotation_lock->CreateTrayView(
      StatusAreaWidgetTestHelper::GetUserLoginStatus());
}

void TrayRotationLockTest::DestroyTrayView() {
  tray_view_.reset();
  tray_->DestroyTrayView();
}

void TrayRotationLockTest::SetUpForStatusAreaWidget(
    StatusAreaWidget* status_area_widget) {
  tray_.reset(new TrayRotationLock(status_area_widget->system_tray()));
  tray_view_.reset(
      tray_->CreateTrayView(StatusAreaWidgetTestHelper::GetUserLoginStatus()));
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
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      ::switches::kUseFirstDisplayAsInternal);
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
  WmShell::Get()->maximize_mode_controller()->EnableMaximizeModeWindowManager(
      true);
  SetUpForStatusAreaWidget(StatusAreaWidgetTestHelper::GetStatusAreaWidget());
  EXPECT_FALSE(tray_view()->visible());
  WmShell::Get()->maximize_mode_controller()->EnableMaximizeModeWindowManager(
      false);
}

// Tests that when the tray view is created, while MaximizeMode is active, and
// rotation is locked, that it is visible.
TEST_F(TrayRotationLockTest, CreateTrayViewDuringMaximizeModeAndRotationLock) {
  TearDownViews();
  WmShell::Get()->maximize_mode_controller()->EnableMaximizeModeWindowManager(
      true);
  Shell::GetInstance()->screen_orientation_controller()->SetRotationLocked(
      true);
  SetUpForStatusAreaWidget(StatusAreaWidgetTestHelper::GetStatusAreaWidget());
  EXPECT_TRUE(tray_view()->visible());
  WmShell::Get()->maximize_mode_controller()->EnableMaximizeModeWindowManager(
      false);
  EXPECT_FALSE(tray_view()->visible());
}

// Tests that the enabling of MaximizeMode affects a previously created tray
// view, changing the visibility.
TEST_F(TrayRotationLockTest, TrayViewVisibilityChangesDuringMaximizeMode) {
  ASSERT_FALSE(tray_view()->visible());
  WmShell::Get()->maximize_mode_controller()->EnableMaximizeModeWindowManager(
      true);
  Shell::GetInstance()->screen_orientation_controller()->SetRotationLocked(
      true);
  EXPECT_TRUE(tray_view()->visible());
  WmShell::Get()->maximize_mode_controller()->EnableMaximizeModeWindowManager(
      false);
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
  WmShell::Get()->maximize_mode_controller()->EnableMaximizeModeWindowManager(
      true);
  EXPECT_FALSE(tray_view()->visible());
  WmShell::Get()->maximize_mode_controller()->EnableMaximizeModeWindowManager(
      false);
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
  WmShell::Get()->maximize_mode_controller()->EnableMaximizeModeWindowManager(
      true);
  SetUpForStatusAreaWidget(StatusAreaWidgetTestHelper::GetStatusAreaWidget());
  EXPECT_TRUE(default_view()->visible());
  WmShell::Get()->maximize_mode_controller()->EnableMaximizeModeWindowManager(
      false);
}

// Tests that the enabling of MaximizeMode affects a previously created default
// view, changing the visibility.
TEST_F(TrayRotationLockTest, DefaultViewVisibilityChangesDuringMaximizeMode) {
  WmShell::Get()->maximize_mode_controller()->EnableMaximizeModeWindowManager(
      true);
  EXPECT_TRUE(default_view()->visible());
  WmShell::Get()->maximize_mode_controller()->EnableMaximizeModeWindowManager(
      false);
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
  MaximizeModeController* maximize_mode_controller =
      WmShell::Get()->maximize_mode_controller();
  ScreenOrientationController* screen_orientation_controller =
      Shell::GetInstance()->screen_orientation_controller();
  ASSERT_FALSE(screen_orientation_controller->rotation_locked());
  maximize_mode_controller->EnableMaximizeModeWindowManager(true);
  ASSERT_FALSE(tray_view()->visible());

  ui::GestureEvent tap(0, 0, 0, base::TimeTicks(),
                       ui::GestureEventDetails(ui::ET_GESTURE_TAP));
  default_view()->OnGestureEvent(&tap);
  EXPECT_TRUE(screen_orientation_controller->rotation_locked());
  EXPECT_TRUE(tray_view()->visible());

  maximize_mode_controller->EnableMaximizeModeWindowManager(false);
}

// Tests that when the tray is created without the internal display being known,
// that it will still display correctly once the internal display is known.
TEST_F(TrayRotationLockTest, InternalDisplayNotAvailableAtCreation) {
  int64_t internal_display_id = display::Display::InternalDisplayId();
  TearDownViews();
  display::Display::SetInternalDisplayId(display::kInvalidDisplayId);

  std::unique_ptr<TrayRotationLock> tray(new TrayRotationLock(
      StatusAreaWidgetTestHelper::GetStatusAreaWidget()->system_tray()));

  display::Display::SetInternalDisplayId(internal_display_id);
  std::unique_ptr<views::View> tray_view(CreateTrayView(tray.get()));
  std::unique_ptr<views::View> default_view(tray->CreateDefaultView(
      StatusAreaWidgetTestHelper::GetUserLoginStatus()));
  EXPECT_TRUE(default_view);
  WmShell::Get()->maximize_mode_controller()->EnableMaximizeModeWindowManager(
      true);
  EXPECT_TRUE(default_view->visible());
}

// Tests that when the tray view is deleted, while TrayRotationLock has not been
// deleted, that updates to the rotation lock state do not crash.
TEST_F(TrayRotationLockTest, LockUpdatedDuringDesctruction) {
  WmShell::Get()->maximize_mode_controller()->EnableMaximizeModeWindowManager(
      true);
  DestroyTrayView();
  Shell::GetInstance()->screen_orientation_controller()->SetRotationLocked(
      true);
  WmShell::Get()->maximize_mode_controller()->EnableMaximizeModeWindowManager(
      false);
}

}  // namespace ash
