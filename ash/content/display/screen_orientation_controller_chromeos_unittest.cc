// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ash_switches.h"
#include "ash/content/display/screen_orientation_controller_chromeos.h"
#include "ash/display/display_info.h"
#include "ash/display/display_manager.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_helper.h"
#include "ash/test/test_shell_delegate.h"
#include "ash/test/test_system_tray_delegate.h"
#include "ash/wm/maximize_mode/maximize_mode_controller.h"
#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "chromeos/accelerometer/accelerometer_reader.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_browser_context.h"
#include "third_party/WebKit/public/platform/WebScreenOrientationLockType.h"
#include "ui/gfx/display.h"
#include "ui/message_center/message_center.h"
#include "ui/views/test/webview_test_helper.h"
#include "ui/views/view.h"
#include "ui/views/views_delegate.h"

namespace ash {

namespace {

const float kDegreesToRadians = 3.1415926f / 180.0f;
const float kMeanGravity = 9.8066f;

void EnableMaximizeMode(bool enable) {
  Shell::GetInstance()
      ->maximize_mode_controller()
      ->EnableMaximizeModeWindowManager(enable);
}

gfx::Display::Rotation GetInternalDisplayRotation() {
  return Shell::GetInstance()
      ->display_manager()
      ->GetDisplayInfo(gfx::Display::InternalDisplayId())
      .rotation();
}

gfx::Display::Rotation Rotation() {
  return Shell::GetInstance()
      ->display_manager()
      ->GetDisplayInfo(gfx::Display::InternalDisplayId())
      .rotation();
}

bool RotationLocked() {
  return Shell::GetInstance()
      ->screen_orientation_controller()
      ->rotation_locked();
}

void SetInternalDisplayRotation(gfx::Display::Rotation rotation) {
  Shell::GetInstance()->display_manager()->SetDisplayRotation(
      gfx::Display::InternalDisplayId(), rotation);
}

void SetRotationLocked(bool rotation_locked) {
  Shell::GetInstance()->screen_orientation_controller()->SetRotationLocked(
      rotation_locked);
}

void TriggerLidUpdate(const gfx::Vector3dF& lid) {
  ui::AccelerometerUpdate update;
  update.Set(ui::ACCELEROMETER_SOURCE_SCREEN, lid.x(), lid.y(), lid.z());
  Shell::GetInstance()->screen_orientation_controller()->OnAccelerometerUpdated(
      update);
}

}  // namespace

class ScreenOrientationControllerTest : public test::AshTestBase {
 public:
  ScreenOrientationControllerTest();
  ~ScreenOrientationControllerTest() override;

  ScreenOrientationController* delegate() {
    return screen_orientation_controller_;
  }

  // Creates and initializes and empty content::WebContents that is backed by a
  // content::BrowserContext and that has an aura::Window.
  content::WebContents* CreateWebContents();

  // Creates a secondary content::WebContents, with a separate
  // content::BrowserContext.
  content::WebContents* CreateSecondaryWebContents();

  // test::AshTestBase:
  void SetUp() override;

 private:
  ScreenOrientationController* screen_orientation_controller_;

  // Optional content::BrowserContext used for two window tests.
  scoped_ptr<content::BrowserContext> secondary_browser_context_;

  // Setups underlying content layer so that content::WebContents can be
  // generated.
  scoped_ptr<views::WebViewTestHelper> webview_test_helper_;

  DISALLOW_COPY_AND_ASSIGN(ScreenOrientationControllerTest);
};

ScreenOrientationControllerTest::ScreenOrientationControllerTest() {
  webview_test_helper_.reset(new views::WebViewTestHelper());
}

ScreenOrientationControllerTest::~ScreenOrientationControllerTest() {
}

content::WebContents* ScreenOrientationControllerTest::CreateWebContents() {
  return views::ViewsDelegate::views_delegate->CreateWebContents(
      ash_test_helper()->test_shell_delegate()->GetActiveBrowserContext(),
      nullptr);
}

content::WebContents*
ScreenOrientationControllerTest::CreateSecondaryWebContents() {
  secondary_browser_context_.reset(new content::TestBrowserContext());
  return views::ViewsDelegate::views_delegate->CreateWebContents(
      secondary_browser_context_.get(), nullptr);
}

void ScreenOrientationControllerTest::SetUp() {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kAshUseFirstDisplayAsInternal);
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kAshEnableTouchViewTesting);
  test::AshTestBase::SetUp();
  screen_orientation_controller_ =
      Shell::GetInstance()->screen_orientation_controller();
}

// Tests that a content::WebContents can lock rotation.
TEST_F(ScreenOrientationControllerTest, LockOrientation) {
  scoped_ptr<content::WebContents> content(CreateWebContents());
  ASSERT_NE(nullptr, content->GetNativeView());
  ASSERT_EQ(gfx::Display::ROTATE_0, Rotation());
  ASSERT_FALSE(RotationLocked());

  delegate()->Lock(content.get(), blink::WebScreenOrientationLockLandscape);
  EXPECT_EQ(gfx::Display::ROTATE_0, Rotation());
  EXPECT_TRUE(RotationLocked());
}

// Tests that a content::WebContents can unlock rotation.
TEST_F(ScreenOrientationControllerTest, Unlock) {
  scoped_ptr<content::WebContents> content(CreateWebContents());
  ASSERT_NE(nullptr, content->GetNativeView());
  ASSERT_EQ(gfx::Display::ROTATE_0, Rotation());
  ASSERT_FALSE(RotationLocked());

  delegate()->Lock(content.get(), blink::WebScreenOrientationLockLandscape);
  EXPECT_EQ(gfx::Display::ROTATE_0, Rotation());
  EXPECT_TRUE(RotationLocked());

  delegate()->Unlock(content.get());
  EXPECT_FALSE(RotationLocked());
}

// Tests that a content::WebContents is able to change the orientation of the
// display after having locked rotation.
TEST_F(ScreenOrientationControllerTest, OrientationChanges) {
  scoped_ptr<content::WebContents> content(CreateWebContents());
  ASSERT_NE(nullptr, content->GetNativeView());
  ASSERT_EQ(gfx::Display::ROTATE_0, Rotation());
  ASSERT_FALSE(RotationLocked());

  delegate()->Lock(content.get(), blink::WebScreenOrientationLockPortrait);
  EXPECT_EQ(gfx::Display::ROTATE_90, Rotation());
  EXPECT_TRUE(RotationLocked());

  delegate()->Lock(content.get(), blink::WebScreenOrientationLockLandscape);
  EXPECT_EQ(gfx::Display::ROTATE_0, Rotation());
}

// Tests that a user initiated rotation lock cannot be unlocked by a
// content::WebContents.
TEST_F(ScreenOrientationControllerTest, UserLockRejectsUnlock) {
  delegate()->SetRotationLocked(true);

  scoped_ptr<content::WebContents> content(CreateWebContents());
  delegate()->Unlock(content.get());
  EXPECT_TRUE(RotationLocked());
}

// Tests that orientation can only be set by the first content::WebContents that
// has set a rotation lock.
TEST_F(ScreenOrientationControllerTest, SecondContentCannotChangeOrientation) {
  scoped_ptr<content::WebContents> content1(CreateWebContents());
  scoped_ptr<content::WebContents> content2(CreateSecondaryWebContents());
  ASSERT_NE(content1->GetNativeView(), content2->GetNativeView());

  delegate()->Lock(content1.get(), blink::WebScreenOrientationLockLandscape);
  delegate()->Lock(content2.get(), blink::WebScreenOrientationLockPortrait);
  EXPECT_EQ(gfx::Display::ROTATE_0, Rotation());
}

// Tests that only the content::WebContents that set a rotation lock can perform
// an unlock.
TEST_F(ScreenOrientationControllerTest, SecondContentCannotUnlock) {
  scoped_ptr<content::WebContents> content1(CreateWebContents());
  scoped_ptr<content::WebContents> content2(CreateSecondaryWebContents());
  ASSERT_NE(content1->GetNativeView(), content2->GetNativeView());

  delegate()->Lock(content1.get(), blink::WebScreenOrientationLockLandscape);
  delegate()->Unlock(content2.get());
  EXPECT_TRUE(RotationLocked());
}

// Tests that alternate content::WebContents can set a rotation lock after a
// preexisting lock has been released.
TEST_F(ScreenOrientationControllerTest, AfterUnlockSecondContentCanLock) {
  scoped_ptr<content::WebContents> content1(CreateWebContents());
  scoped_ptr<content::WebContents> content2(CreateSecondaryWebContents());
  ASSERT_NE(content1->GetNativeView(), content2->GetNativeView());

  delegate()->Lock(content1.get(), blink::WebScreenOrientationLockLandscape);
  delegate()->Unlock(content1.get());
  delegate()->Lock(content2.get(), blink::WebScreenOrientationLockPortrait);
  EXPECT_EQ(gfx::Display::ROTATE_90, Rotation());
  EXPECT_TRUE(RotationLocked());
}

// Tests that accelerometer readings in each of the screen angles will trigger a
// rotation of the internal display.
TEST_F(ScreenOrientationControllerTest, DisplayRotation) {
  EnableMaximizeMode(true);
  // Now test rotating in all directions.
  TriggerLidUpdate(gfx::Vector3dF(-kMeanGravity, 0.0f, 0.0f));
  EXPECT_EQ(gfx::Display::ROTATE_90, GetInternalDisplayRotation());
  TriggerLidUpdate(gfx::Vector3dF(0.0f, kMeanGravity, 0.0f));
  EXPECT_EQ(gfx::Display::ROTATE_180, GetInternalDisplayRotation());
  TriggerLidUpdate(gfx::Vector3dF(kMeanGravity, 0.0f, 0.0f));
  EXPECT_EQ(gfx::Display::ROTATE_270, GetInternalDisplayRotation());
  TriggerLidUpdate(gfx::Vector3dF(0.0f, -kMeanGravity, 0.0f));
  EXPECT_EQ(gfx::Display::ROTATE_0, GetInternalDisplayRotation());
}

// Tests that low angles are ignored by the accelerometer (i.e. when the device
// is almost laying flat).
TEST_F(ScreenOrientationControllerTest, RotationIgnoresLowAngles) {
  EnableMaximizeMode(true);
  TriggerLidUpdate(gfx::Vector3dF(0.0f, -kMeanGravity, -kMeanGravity));
  EXPECT_EQ(gfx::Display::ROTATE_0, GetInternalDisplayRotation());
  TriggerLidUpdate(gfx::Vector3dF(-2.0f, 0.0f, -kMeanGravity));
  EXPECT_EQ(gfx::Display::ROTATE_0, GetInternalDisplayRotation());
  TriggerLidUpdate(gfx::Vector3dF(0.0f, 2.0f, -kMeanGravity));
  EXPECT_EQ(gfx::Display::ROTATE_0, GetInternalDisplayRotation());
  TriggerLidUpdate(gfx::Vector3dF(2.0f, 0.0f, -kMeanGravity));
  EXPECT_EQ(gfx::Display::ROTATE_0, GetInternalDisplayRotation());
  TriggerLidUpdate(gfx::Vector3dF(0.0f, -2.0f, -kMeanGravity));
  EXPECT_EQ(gfx::Display::ROTATE_0, GetInternalDisplayRotation());
}

// Tests that the display will stick to the current orientation beyond the
// halfway point, preventing frequent updates back and forth.
TEST_F(ScreenOrientationControllerTest, RotationSticky) {
  EnableMaximizeMode(true);
  gfx::Vector3dF gravity(0.0f, -kMeanGravity, 0.0f);
  TriggerLidUpdate(gravity);
  EXPECT_EQ(gfx::Display::ROTATE_0, GetInternalDisplayRotation());

  // Turn past half-way point to next direction and rotation should remain
  // the same.
  float degrees = 50.0;
  gravity.set_x(-sin(degrees * kDegreesToRadians) * kMeanGravity);
  gravity.set_y(-cos(degrees * kDegreesToRadians) * kMeanGravity);
  TriggerLidUpdate(gravity);
  EXPECT_EQ(gfx::Display::ROTATE_0, GetInternalDisplayRotation());

  // Turn more and the screen should rotate.
  degrees = 70.0;
  gravity.set_x(-sin(degrees * kDegreesToRadians) * kMeanGravity);
  gravity.set_y(-cos(degrees * kDegreesToRadians) * kMeanGravity);
  TriggerLidUpdate(gravity);
  EXPECT_EQ(gfx::Display::ROTATE_90, GetInternalDisplayRotation());

  // Turn back just beyond the half-way point and the new rotation should
  // still be in effect.
  degrees = 40.0;
  gravity.set_x(-sin(degrees * kDegreesToRadians) * kMeanGravity);
  gravity.set_y(-cos(degrees * kDegreesToRadians) * kMeanGravity);
  TriggerLidUpdate(gravity);
  EXPECT_EQ(gfx::Display::ROTATE_90, GetInternalDisplayRotation());
}

// Tests that the display will stick to its current orientation when the
// rotation lock has been set.
TEST_F(ScreenOrientationControllerTest, RotationLockPreventsRotation) {
  EnableMaximizeMode(true);
  SetRotationLocked(true);

  // Turn past the threshold for rotation.
  float degrees = 90.0;
  gfx::Vector3dF gravity(-sin(degrees * kDegreesToRadians) * kMeanGravity,
                         -cos(degrees * kDegreesToRadians) * kMeanGravity,
                         0.0f);
  TriggerLidUpdate(gravity);
  EXPECT_EQ(gfx::Display::ROTATE_0, GetInternalDisplayRotation());

  SetRotationLocked(false);
  TriggerLidUpdate(gravity);
  EXPECT_EQ(gfx::Display::ROTATE_90, GetInternalDisplayRotation());
}

// The TrayDisplay class that is responsible for adding/updating MessageCenter
// notifications is only added to the SystemTray on ChromeOS.
// Tests that the screen rotation notifications are suppressed when
// triggered by the accelerometer.
TEST_F(ScreenOrientationControllerTest, BlockRotationNotifications) {
  EnableMaximizeMode(true);
  test::TestSystemTrayDelegate* tray_delegate =
      static_cast<test::TestSystemTrayDelegate*>(
          Shell::GetInstance()->system_tray_delegate());
  tray_delegate->set_should_show_display_notification(true);

  message_center::MessageCenter* message_center =
      message_center::MessageCenter::Get();

  EXPECT_EQ(0u, message_center->NotificationCount());
  EXPECT_FALSE(message_center->HasPopupNotifications());

  // Make sure notifications are still displayed when
  // adjusting the screen rotation directly when in maximize mode
  ASSERT_NE(gfx::Display::ROTATE_270, GetInternalDisplayRotation());
  SetInternalDisplayRotation(gfx::Display::ROTATE_270);
  SetRotationLocked(false);
  EXPECT_EQ(gfx::Display::ROTATE_270, GetInternalDisplayRotation());
  EXPECT_EQ(1u, message_center->NotificationCount());
  EXPECT_TRUE(message_center->HasPopupNotifications());

  // Clear all notifications
  message_center->RemoveAllNotifications(false);
  EXPECT_EQ(0u, message_center->NotificationCount());
  EXPECT_FALSE(message_center->HasPopupNotifications());

  // Make sure notifications are blocked when adjusting the screen rotation
  // via the accelerometer while in maximize mode
  // Rotate the screen 90 degrees
  ASSERT_NE(gfx::Display::ROTATE_90, GetInternalDisplayRotation());
  TriggerLidUpdate(gfx::Vector3dF(-kMeanGravity, 0.0f, 0.0f));
  ASSERT_EQ(gfx::Display::ROTATE_90, GetInternalDisplayRotation());
  EXPECT_EQ(0u, message_center->NotificationCount());
  EXPECT_FALSE(message_center->HasPopupNotifications());

  // Make sure notifications are still displayed when
  // adjusting the screen rotation directly when not in maximize mode
  EnableMaximizeMode(false);
  // Reset the screen rotation.
  SetInternalDisplayRotation(gfx::Display::ROTATE_0);
  // Clear all notifications
  message_center->RemoveAllNotifications(false);
  ASSERT_NE(gfx::Display::ROTATE_180, GetInternalDisplayRotation());
  ASSERT_EQ(0u, message_center->NotificationCount());
  ASSERT_FALSE(message_center->HasPopupNotifications());
  SetInternalDisplayRotation(gfx::Display::ROTATE_180);
  EXPECT_EQ(gfx::Display::ROTATE_180, GetInternalDisplayRotation());
  EXPECT_EQ(1u, message_center->NotificationCount());
  EXPECT_TRUE(message_center->HasPopupNotifications());
}

// Tests that if a user has set a display rotation that it is restored upon
// exiting maximize mode.
TEST_F(ScreenOrientationControllerTest, ResetUserRotationUponExit) {
  SetInternalDisplayRotation(gfx::Display::ROTATE_90);
  EnableMaximizeMode(true);

  TriggerLidUpdate(gfx::Vector3dF(0.0f, kMeanGravity, 0.0f));
  EXPECT_EQ(gfx::Display::ROTATE_180, GetInternalDisplayRotation());

  EnableMaximizeMode(false);
  EXPECT_EQ(gfx::Display::ROTATE_90, GetInternalDisplayRotation());
}

// Tests that if a user sets a display rotation that accelerometer rotation
// becomes locked.
TEST_F(ScreenOrientationControllerTest,
       NonAccelerometerRotationChangesLockRotation) {
  EnableMaximizeMode(true);
  ASSERT_FALSE(RotationLocked());
  SetInternalDisplayRotation(gfx::Display::ROTATE_270);
  EXPECT_TRUE(RotationLocked());
}

// Tests that if a user changes the display rotation, while rotation is locked,
// that the updates are recorded. Upon exiting maximize mode the latest user
// rotation should be applied.
TEST_F(ScreenOrientationControllerTest, UpdateUserRotationWhileRotationLocked) {
  EnableMaximizeMode(true);
  SetInternalDisplayRotation(gfx::Display::ROTATE_270);
  // User sets rotation to the same rotation that the display was at when
  // maximize mode was activated.
  SetInternalDisplayRotation(gfx::Display::ROTATE_0);
  EnableMaximizeMode(false);
  EXPECT_EQ(gfx::Display::ROTATE_0, GetInternalDisplayRotation());
}

}  // namespace ash
