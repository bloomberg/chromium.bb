// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/ime/tray_ime_chromeos.h"

#include "ash/accessibility_delegate.h"
#include "ash/shell.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/status_area_widget_test_helper.h"
#include "ash/test/virtual_keyboard_test_helper.h"
#include "base/command_line.h"
#include "ui/keyboard/keyboard_util.h"

namespace ash {

class TrayIMETest : public test::AshTestBase {
 public:
  TrayIMETest() {}
  ~TrayIMETest() override {}

  TrayIME* tray() { return tray_.get(); }

  views::View* default_view() { return default_view_.get(); }

  views::View* detailed_view() { return detailed_view_.get(); }

  // Sets up a TrayIME and its default view.
  void SetUpForStatusAreaWidget(StatusAreaWidget* status_area_widget);

  // Mocks enabling the a11y virtual keyboard since the actual a11y manager
  // is not created in ash tests.
  void SetAccessibilityKeyboardEnabled(bool enabled);

  // Resets |tray_| and |default_view_| so that all components of
  // TrayIME have been cleared. Tests may then call
  // SetUpForStatusAreaWidget in order to initialize the components.
  void TearDownViews();

  // Sets the current number of active IMEs.
  void SetIMELength(int length);

  // Returns the view in the detailed views scroll content at the provided
  // index.
  views::View* GetScrollChildView(int index);

  // test::AshTestBase:
  void SetUp() override;
  void TearDown() override;

 private:
  scoped_ptr<TrayIME> tray_;
  scoped_ptr<views::View> default_view_;
  scoped_ptr<views::View> detailed_view_;
};

void TrayIMETest::SetUpForStatusAreaWidget(
    StatusAreaWidget* status_area_widget) {
  tray_.reset(new TrayIME(status_area_widget->system_tray()));
  default_view_.reset(tray_->CreateDefaultView(
      StatusAreaWidgetTestHelper::GetUserLoginStatus()));
  detailed_view_.reset(tray_->CreateDetailedView(
      StatusAreaWidgetTestHelper::GetUserLoginStatus()));
}

void TrayIMETest::SetAccessibilityKeyboardEnabled(bool enabled) {
  Shell::GetInstance()->accessibility_delegate()->SetVirtualKeyboardEnabled(
      enabled);
  keyboard::SetAccessibilityKeyboardEnabled(enabled);
  ui::AccessibilityNotificationVisibility notification =
      enabled ? ui::AccessibilityNotificationVisibility::A11Y_NOTIFICATION_SHOW
              : ui::AccessibilityNotificationVisibility::A11Y_NOTIFICATION_NONE;
  Shell::GetInstance()->system_tray_notifier()->NotifyAccessibilityModeChanged(
      notification);
}

void TrayIMETest::TearDownViews() {
  tray_.reset();
  default_view_.reset();
  detailed_view_.reset();
}

void TrayIMETest::SetIMELength(int length) {
  tray_->ime_list_.clear();
  IMEInfo ime;
  for (int i = 0; i < length; i++) {
    tray_->ime_list_.push_back(ime);
  }
  tray_->Update();
}

views::View* TrayIMETest::GetScrollChildView(int index) {
  TrayDetailsView* details = static_cast<TrayDetailsView*>(detailed_view());
  views::View* content = details->scroll_content();
  EXPECT_TRUE(content);
  EXPECT_GT(content->child_count(), index);
  return content->child_at(index);
}

void TrayIMETest::SetUp() {
  test::AshTestBase::SetUp();
  SetUpForStatusAreaWidget(StatusAreaWidgetTestHelper::GetStatusAreaWidget());
}

void TrayIMETest::TearDown() {
  SetAccessibilityKeyboardEnabled(false);
  TearDownViews();
  test::AshTestBase::TearDown();
}

// Tests that if the keyboard is not suppressed the default view is hidden
// if less than 2 IMEs are present.
TEST_F(TrayIMETest, HiddenWithNoIMEs) {
  SetIMELength(0);
  EXPECT_FALSE(default_view()->visible());
  SetIMELength(1);
  EXPECT_FALSE(default_view()->visible());
  SetIMELength(2);
  EXPECT_TRUE(default_view()->visible());
}

// Tests that if no IMEs are present the default view is hidden when a11y is
// enabled.
TEST_F(TrayIMETest, HidesOnA11yEnabled) {
  SetIMELength(0);
  test::VirtualKeyboardTestHelper::SuppressKeyboard();
  EXPECT_TRUE(default_view()->visible());
  // Enable a11y keyboard.
  SetAccessibilityKeyboardEnabled(true);
  EXPECT_FALSE(default_view()->visible());
  // Disable the a11y keyboard.
  SetAccessibilityKeyboardEnabled(false);
  EXPECT_TRUE(default_view()->visible());
}

// Tests that clicking on the keyboard toggle causes the virtual keyboard
// to toggle between enabled and disabled.
TEST_F(TrayIMETest, PerformActionOnDetailedView) {
  SetIMELength(0);
  test::VirtualKeyboardTestHelper::SuppressKeyboard();
  EXPECT_FALSE(keyboard::IsKeyboardEnabled());
  views::View* toggle = GetScrollChildView(0);
  ui::GestureEvent tap(0, 0, 0, base::TimeDelta(),
                       ui::GestureEventDetails(ui::ET_GESTURE_TAP));
  // Enable the keyboard.
  toggle->OnGestureEvent(&tap);
  EXPECT_TRUE(keyboard::IsKeyboardEnabled());
  EXPECT_TRUE(default_view()->visible());
  // With no IMEs the toggle should be the first child.
  toggle = GetScrollChildView(0);
  // Clicking again should disable the keyboard.
  tap = ui::GestureEvent(0, 0, 0, base::TimeDelta(),
                         ui::GestureEventDetails(ui::ET_GESTURE_TAP));
  toggle->OnGestureEvent(&tap);
  EXPECT_FALSE(keyboard::IsKeyboardEnabled());
  EXPECT_TRUE(default_view()->visible());
}

}  // namespace ash
