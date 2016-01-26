// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "ash/ash_switches.h"
#include "ash/content/shell_content_state.h"
#include "ash/display/display_info.h"
#include "ash/display/display_manager.h"
#include "ash/display/screen_orientation_controller_chromeos.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_helper.h"
#include "ash/test/content/test_shell_content_state.h"
#include "ash/test/display_manager_test_api.h"
#include "ash/test/test_shell_delegate.h"
#include "ash/test/test_system_tray_delegate.h"
#include "ash/wm/maximize_mode/maximize_mode_controller.h"
#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "chromeos/accelerometer/accelerometer_reader.h"
#include "chromeos/accelerometer/accelerometer_types.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_browser_context.h"
#include "third_party/WebKit/public/platform/modules/screen_orientation/WebScreenOrientationLockType.h"
#include "ui/aura/window.h"
#include "ui/gfx/display.h"
#include "ui/message_center/message_center.h"
#include "ui/views/test/webview_test_helper.h"
#include "ui/views/view.h"
#include "ui/views/views_delegate.h"
#include "ui/wm/public/activation_client.h"

namespace ash {

namespace {

const float kDegreesToRadians = 3.1415926f / 180.0f;
const float kMeanGravity = -9.8066f;

DisplayInfo CreateDisplayInfo(int64_t id, const gfx::Rect& bounds) {
  DisplayInfo info(id, "dummy", false);
  info.SetBounds(bounds);
  return info;
}

void EnableMaximizeMode(bool enable) {
  Shell::GetInstance()
      ->maximize_mode_controller()
      ->EnableMaximizeModeWindowManager(enable);
}

bool RotationLocked() {
  return Shell::GetInstance()
      ->screen_orientation_controller()
      ->rotation_locked();
}

void SetDisplayRotationById(int64_t display_id,
                            gfx::Display::Rotation rotation) {
  Shell::GetInstance()->display_manager()->SetDisplayRotation(
      display_id, rotation, gfx::Display::ROTATION_SOURCE_USER);
}

void SetInternalDisplayRotation(gfx::Display::Rotation rotation) {
  SetDisplayRotationById(gfx::Display::InternalDisplayId(), rotation);
}

void SetRotationLocked(bool rotation_locked) {
  Shell::GetInstance()->screen_orientation_controller()->SetRotationLocked(
      rotation_locked);
}

void TriggerLidUpdate(const gfx::Vector3dF& lid) {
  scoped_refptr<chromeos::AccelerometerUpdate> update(
      new chromeos::AccelerometerUpdate());
  update->Set(chromeos::ACCELEROMETER_SOURCE_SCREEN, lid.x(), lid.y(), lid.z());
  Shell::GetInstance()->screen_orientation_controller()->OnAccelerometerUpdated(
      update);
}

// Attaches the NativeView of |web_contents| to |parent| without changing the
// currently active window.
void AttachWebContents(content::WebContents* web_contents,
                       aura::Window* parent) {
  aura::Window* window = web_contents->GetNativeView();
  window->Show();
  parent->AddChild(window);
}

// Attaches the NativeView of |web_contents| to |parent|, ensures that it is
// visible, and activates the parent window.
void AttachAndActivateWebContents(content::WebContents* web_contents,
                                  aura::Window* parent) {
  AttachWebContents(web_contents, parent);
  Shell::GetInstance()->activation_client()->ActivateWindow(parent);
}

}  // namespace

class ScreenOrientationControllerTest : public test::AshTestBase {
 public:
  ScreenOrientationControllerTest();
  ~ScreenOrientationControllerTest() override;

  content::ScreenOrientationDelegate* delegate() {
    return ash_test_helper()
        ->test_shell_content_state()
        ->screen_orientation_delegate();
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
  return views::ViewsDelegate::GetInstance()->CreateWebContents(
      ShellContentState::GetInstance()->GetActiveBrowserContext(), nullptr);
}

content::WebContents*
ScreenOrientationControllerTest::CreateSecondaryWebContents() {
  secondary_browser_context_.reset(new content::TestBrowserContext());
  return views::ViewsDelegate::GetInstance()->CreateWebContents(
      secondary_browser_context_.get(), nullptr);
}

void ScreenOrientationControllerTest::SetUp() {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kAshUseFirstDisplayAsInternal);
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kAshEnableTouchViewTesting);
  test::AshTestBase::SetUp();
}

// Tests that a content::WebContents can lock rotation.
TEST_F(ScreenOrientationControllerTest, LockOrientation) {
  scoped_ptr<content::WebContents> content(CreateWebContents());
  scoped_ptr<aura::Window> focus_window(CreateTestWindowInShellWithId(0));
  ASSERT_NE(nullptr, content->GetNativeView());
  ASSERT_EQ(gfx::Display::ROTATE_0, GetCurrentInternalDisplayRotation());
  ASSERT_FALSE(RotationLocked());

  AttachAndActivateWebContents(content.get(), focus_window.get());
  delegate()->Lock(content.get(), blink::WebScreenOrientationLockLandscape);
  EXPECT_EQ(gfx::Display::ROTATE_0, GetCurrentInternalDisplayRotation());
  EXPECT_TRUE(RotationLocked());
}

// Tests that a content::WebContents can unlock rotation.
TEST_F(ScreenOrientationControllerTest, Unlock) {
  scoped_ptr<content::WebContents> content(CreateWebContents());
  scoped_ptr<aura::Window> focus_window(CreateTestWindowInShellWithId(0));
  ASSERT_NE(nullptr, content->GetNativeView());
  ASSERT_EQ(gfx::Display::ROTATE_0, GetCurrentInternalDisplayRotation());
  ASSERT_FALSE(RotationLocked());

  AttachAndActivateWebContents(content.get(), focus_window.get());
  delegate()->Lock(content.get(), blink::WebScreenOrientationLockLandscape);
  EXPECT_EQ(gfx::Display::ROTATE_0, GetCurrentInternalDisplayRotation());
  EXPECT_TRUE(RotationLocked());

  delegate()->Unlock(content.get());
  EXPECT_FALSE(RotationLocked());
}

// Tests that a content::WebContents is able to change the orientation of the
// display after having locked rotation.
TEST_F(ScreenOrientationControllerTest, OrientationChanges) {
  scoped_ptr<content::WebContents> content(CreateWebContents());
  scoped_ptr<aura::Window> focus_window(CreateTestWindowInShellWithId(0));
  ASSERT_NE(nullptr, content->GetNativeView());
  ASSERT_EQ(gfx::Display::ROTATE_0, GetCurrentInternalDisplayRotation());
  ASSERT_FALSE(RotationLocked());

  AttachAndActivateWebContents(content.get(), focus_window.get());
  delegate()->Lock(content.get(), blink::WebScreenOrientationLockPortrait);
  EXPECT_EQ(gfx::Display::ROTATE_90, GetCurrentInternalDisplayRotation());
  EXPECT_TRUE(RotationLocked());

  delegate()->Lock(content.get(), blink::WebScreenOrientationLockLandscape);
  EXPECT_EQ(gfx::Display::ROTATE_0, GetCurrentInternalDisplayRotation());
}

// Tests that orientation can only be set by the first content::WebContents that
// has set a rotation lock.
TEST_F(ScreenOrientationControllerTest, SecondContentCannotChangeOrientation) {
  scoped_ptr<content::WebContents> content1(CreateWebContents());
  scoped_ptr<content::WebContents> content2(CreateSecondaryWebContents());
  scoped_ptr<aura::Window> focus_window1(CreateTestWindowInShellWithId(0));
  scoped_ptr<aura::Window> focus_window2(CreateTestWindowInShellWithId(1));
  ASSERT_NE(content1->GetNativeView(), content2->GetNativeView());

  AttachAndActivateWebContents(content1.get(), focus_window1.get());
  AttachWebContents(content2.get(), focus_window2.get());
  delegate()->Lock(content1.get(), blink::WebScreenOrientationLockLandscape);
  delegate()->Lock(content2.get(), blink::WebScreenOrientationLockPortrait);
  EXPECT_EQ(gfx::Display::ROTATE_0, GetCurrentInternalDisplayRotation());
}

// Tests that only the content::WebContents that set a rotation lock can perform
// an unlock.
TEST_F(ScreenOrientationControllerTest, SecondContentCannotUnlock) {
  scoped_ptr<content::WebContents> content1(CreateWebContents());
  scoped_ptr<content::WebContents> content2(CreateSecondaryWebContents());
  scoped_ptr<aura::Window> focus_window1(CreateTestWindowInShellWithId(0));
  scoped_ptr<aura::Window> focus_window2(CreateTestWindowInShellWithId(1));
  ASSERT_NE(content1->GetNativeView(), content2->GetNativeView());

  AttachAndActivateWebContents(content1.get(), focus_window1.get());
  AttachWebContents(content2.get(), focus_window2.get());
  delegate()->Lock(content1.get(), blink::WebScreenOrientationLockLandscape);
  delegate()->Unlock(content2.get());
  EXPECT_TRUE(RotationLocked());
}

// Tests that a rotation lock is applied only while the content::WebContents are
// a part of the active window.
TEST_F(ScreenOrientationControllerTest, ActiveWindowChangesUpdateLock) {
  scoped_ptr<content::WebContents> content(CreateWebContents());
  scoped_ptr<aura::Window> focus_window1(CreateTestWindowInShellWithId(0));
  scoped_ptr<aura::Window> focus_window2(CreateTestWindowInShellWithId(1));

  AttachAndActivateWebContents(content.get(), focus_window1.get());
  delegate()->Lock(content.get(), blink::WebScreenOrientationLockLandscape);
  ASSERT_TRUE(RotationLocked());

  aura::client::ActivationClient* activation_client =
      Shell::GetInstance()->activation_client();
  activation_client->ActivateWindow(focus_window2.get());
  EXPECT_FALSE(RotationLocked());

  activation_client->ActivateWindow(focus_window1.get());
  EXPECT_TRUE(RotationLocked());
}

// Tests that switching between windows with different orientation locks change
// the orientation.
TEST_F(ScreenOrientationControllerTest, ActiveWindowChangesUpdateOrientation) {
  scoped_ptr<content::WebContents> content1(CreateWebContents());
  scoped_ptr<content::WebContents> content2(CreateSecondaryWebContents());
  scoped_ptr<aura::Window> focus_window1(CreateTestWindowInShellWithId(0));
  scoped_ptr<aura::Window> focus_window2(CreateTestWindowInShellWithId(1));
  AttachAndActivateWebContents(content1.get(), focus_window1.get());
  AttachWebContents(content2.get(), focus_window2.get());

  delegate()->Lock(content1.get(), blink::WebScreenOrientationLockLandscape);
  delegate()->Lock(content2.get(), blink::WebScreenOrientationLockPortrait);
  EXPECT_EQ(gfx::Display::ROTATE_0, GetCurrentInternalDisplayRotation());

  aura::client::ActivationClient* activation_client =
      Shell::GetInstance()->activation_client();
  activation_client->ActivateWindow(focus_window2.get());
  EXPECT_TRUE(RotationLocked());
  EXPECT_EQ(gfx::Display::ROTATE_90, GetCurrentInternalDisplayRotation());

  activation_client->ActivateWindow(focus_window1.get());
  EXPECT_TRUE(RotationLocked());
  EXPECT_EQ(gfx::Display::ROTATE_0, GetCurrentInternalDisplayRotation());
}

// Tests that a rotation lock is removed when the setting window is hidden, and
// that it is reapplied when the window becomes visible.
TEST_F(ScreenOrientationControllerTest, VisibilityChangesLock) {
  scoped_ptr<content::WebContents> content(CreateWebContents());
  scoped_ptr<aura::Window> focus_window(CreateTestWindowInShellWithId(0));
  AttachAndActivateWebContents(content.get(), focus_window.get());
  delegate()->Lock(content.get(), blink::WebScreenOrientationLockLandscape);
  EXPECT_TRUE(RotationLocked());

  aura::Window* window = content->GetNativeView();
  window->Hide();
  EXPECT_FALSE(RotationLocked());

  window->Show();
  EXPECT_TRUE(RotationLocked());
}

// Tests that when a window is destroyed that its rotation lock is removed, and
// window activations no longer change the lock
TEST_F(ScreenOrientationControllerTest, WindowDestructionRemovesLock) {
  scoped_ptr<content::WebContents> content(CreateWebContents());
  scoped_ptr<aura::Window> focus_window1(CreateTestWindowInShellWithId(0));
  scoped_ptr<aura::Window> focus_window2(CreateTestWindowInShellWithId(1));

  AttachAndActivateWebContents(content.get(), focus_window1.get());
  delegate()->Lock(content.get(), blink::WebScreenOrientationLockLandscape);
  ASSERT_TRUE(RotationLocked());

  focus_window1->RemoveChild(content->GetNativeView());
  content.reset();
  EXPECT_FALSE(RotationLocked());

  aura::client::ActivationClient* activation_client =
      Shell::GetInstance()->activation_client();
  activation_client->ActivateWindow(focus_window2.get());
  EXPECT_FALSE(RotationLocked());

  activation_client->ActivateWindow(focus_window1.get());
  EXPECT_FALSE(RotationLocked());
}

// Tests that accelerometer readings in each of the screen angles will trigger a
// rotation of the internal display.
TEST_F(ScreenOrientationControllerTest, DisplayRotation) {
  EnableMaximizeMode(true);
  // Now test rotating in all directions.
  TriggerLidUpdate(gfx::Vector3dF(-kMeanGravity, 0.0f, 0.0f));
  EXPECT_EQ(gfx::Display::ROTATE_90, GetCurrentInternalDisplayRotation());
  TriggerLidUpdate(gfx::Vector3dF(0.0f, kMeanGravity, 0.0f));
  EXPECT_EQ(gfx::Display::ROTATE_180, GetCurrentInternalDisplayRotation());
  TriggerLidUpdate(gfx::Vector3dF(kMeanGravity, 0.0f, 0.0f));
  EXPECT_EQ(gfx::Display::ROTATE_270, GetCurrentInternalDisplayRotation());
  TriggerLidUpdate(gfx::Vector3dF(0.0f, -kMeanGravity, 0.0f));
  EXPECT_EQ(gfx::Display::ROTATE_0, GetCurrentInternalDisplayRotation());
}

// Tests that low angles are ignored by the accelerometer (i.e. when the device
// is almost laying flat).
TEST_F(ScreenOrientationControllerTest, RotationIgnoresLowAngles) {
  EnableMaximizeMode(true);
  TriggerLidUpdate(gfx::Vector3dF(0.0f, -kMeanGravity, -kMeanGravity));
  EXPECT_EQ(gfx::Display::ROTATE_0, GetCurrentInternalDisplayRotation());
  TriggerLidUpdate(gfx::Vector3dF(-2.0f, 0.0f, -kMeanGravity));
  EXPECT_EQ(gfx::Display::ROTATE_0, GetCurrentInternalDisplayRotation());
  TriggerLidUpdate(gfx::Vector3dF(0.0f, 2.0f, -kMeanGravity));
  EXPECT_EQ(gfx::Display::ROTATE_0, GetCurrentInternalDisplayRotation());
  TriggerLidUpdate(gfx::Vector3dF(2.0f, 0.0f, -kMeanGravity));
  EXPECT_EQ(gfx::Display::ROTATE_0, GetCurrentInternalDisplayRotation());
  TriggerLidUpdate(gfx::Vector3dF(0.0f, -2.0f, -kMeanGravity));
  EXPECT_EQ(gfx::Display::ROTATE_0, GetCurrentInternalDisplayRotation());
}

// Tests that the display will stick to the current orientation beyond the
// halfway point, preventing frequent updates back and forth.
TEST_F(ScreenOrientationControllerTest, RotationSticky) {
  EnableMaximizeMode(true);
  gfx::Vector3dF gravity(0.0f, -kMeanGravity, 0.0f);
  TriggerLidUpdate(gravity);
  EXPECT_EQ(gfx::Display::ROTATE_0, GetCurrentInternalDisplayRotation());

  // Turn past half-way point to next direction and rotation should remain
  // the same.
  float degrees = 50.0;
  gravity.set_x(-sin(degrees * kDegreesToRadians) * kMeanGravity);
  gravity.set_y(-cos(degrees * kDegreesToRadians) * kMeanGravity);
  TriggerLidUpdate(gravity);
  EXPECT_EQ(gfx::Display::ROTATE_0, GetCurrentInternalDisplayRotation());

  // Turn more and the screen should rotate.
  degrees = 70.0;
  gravity.set_x(-sin(degrees * kDegreesToRadians) * kMeanGravity);
  gravity.set_y(-cos(degrees * kDegreesToRadians) * kMeanGravity);
  TriggerLidUpdate(gravity);
  EXPECT_EQ(gfx::Display::ROTATE_90, GetCurrentInternalDisplayRotation());

  // Turn back just beyond the half-way point and the new rotation should
  // still be in effect.
  degrees = 40.0;
  gravity.set_x(-sin(degrees * kDegreesToRadians) * kMeanGravity);
  gravity.set_y(-cos(degrees * kDegreesToRadians) * kMeanGravity);
  TriggerLidUpdate(gravity);
  EXPECT_EQ(gfx::Display::ROTATE_90, GetCurrentInternalDisplayRotation());
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
  EXPECT_EQ(gfx::Display::ROTATE_0, GetCurrentInternalDisplayRotation());

  SetRotationLocked(false);
  TriggerLidUpdate(gravity);
  EXPECT_EQ(gfx::Display::ROTATE_90, GetCurrentInternalDisplayRotation());
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
  test::DisplayManagerTestApi().SetFirstDisplayAsInternalDisplay();

  message_center::MessageCenter* message_center =
      message_center::MessageCenter::Get();

  EXPECT_EQ(0u, message_center->NotificationCount());
  EXPECT_FALSE(message_center->HasPopupNotifications());

  // Make sure notifications are still displayed when
  // adjusting the screen rotation directly when in maximize mode
  ASSERT_NE(gfx::Display::ROTATE_270, GetCurrentInternalDisplayRotation());
  SetInternalDisplayRotation(gfx::Display::ROTATE_270);
  SetRotationLocked(false);
  EXPECT_EQ(gfx::Display::ROTATE_270, GetCurrentInternalDisplayRotation());
  EXPECT_EQ(1u, message_center->NotificationCount());
  EXPECT_TRUE(message_center->HasPopupNotifications());

  // Clear all notifications
  message_center->RemoveAllNotifications(false);
  EXPECT_EQ(0u, message_center->NotificationCount());
  EXPECT_FALSE(message_center->HasPopupNotifications());

  // Make sure notifications are blocked when adjusting the screen rotation
  // via the accelerometer while in maximize mode
  // Rotate the screen 90 degrees
  ASSERT_NE(gfx::Display::ROTATE_90, GetCurrentInternalDisplayRotation());
  TriggerLidUpdate(gfx::Vector3dF(-kMeanGravity, 0.0f, 0.0f));
  ASSERT_EQ(gfx::Display::ROTATE_90, GetCurrentInternalDisplayRotation());
  EXPECT_EQ(0u, message_center->NotificationCount());
  EXPECT_FALSE(message_center->HasPopupNotifications());

  // Make sure notifications are still displayed when
  // adjusting the screen rotation directly when not in maximize mode
  EnableMaximizeMode(false);
  // Reset the screen rotation.
  SetInternalDisplayRotation(gfx::Display::ROTATE_0);
  // Clear all notifications
  message_center->RemoveAllNotifications(false);
  ASSERT_NE(gfx::Display::ROTATE_180, GetCurrentInternalDisplayRotation());
  ASSERT_EQ(0u, message_center->NotificationCount());
  ASSERT_FALSE(message_center->HasPopupNotifications());
  SetInternalDisplayRotation(gfx::Display::ROTATE_180);
  EXPECT_EQ(gfx::Display::ROTATE_180, GetCurrentInternalDisplayRotation());
  EXPECT_EQ(1u, message_center->NotificationCount());
  EXPECT_TRUE(message_center->HasPopupNotifications());
}

// Tests that if a user has set a display rotation that it is restored upon
// exiting maximize mode.
TEST_F(ScreenOrientationControllerTest, ResetUserRotationUponExit) {
  test::DisplayManagerTestApi().SetFirstDisplayAsInternalDisplay();

  SetInternalDisplayRotation(gfx::Display::ROTATE_90);
  EnableMaximizeMode(true);

  TriggerLidUpdate(gfx::Vector3dF(0.0f, kMeanGravity, 0.0f));
  EXPECT_EQ(gfx::Display::ROTATE_180, GetCurrentInternalDisplayRotation());

  EnableMaximizeMode(false);
  EXPECT_EQ(gfx::Display::ROTATE_90, GetCurrentInternalDisplayRotation());
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
  EXPECT_EQ(gfx::Display::ROTATE_0, GetCurrentInternalDisplayRotation());
}

// Tests that when the orientation lock is set to Landscape, that rotation can
// be done between the two angles of the orientation.
TEST_F(ScreenOrientationControllerTest, LandscapeOrientationAllowsRotation) {
  scoped_ptr<content::WebContents> content(CreateWebContents());
  scoped_ptr<aura::Window> focus_window(CreateTestWindowInShellWithId(0));
  EnableMaximizeMode(true);

  AttachAndActivateWebContents(content.get(), focus_window.get());
  delegate()->Lock(content.get(), blink::WebScreenOrientationLockLandscape);
  EXPECT_EQ(gfx::Display::ROTATE_0, GetCurrentInternalDisplayRotation());
  EXPECT_TRUE(RotationLocked());

  // Inverse of orientation is allowed
  TriggerLidUpdate(gfx::Vector3dF(0.0f, kMeanGravity, 0.0f));
  EXPECT_EQ(gfx::Display::ROTATE_180, GetCurrentInternalDisplayRotation());

  // Display rotations between are not allowed
  TriggerLidUpdate(gfx::Vector3dF(kMeanGravity, 0.0f, 0.0f));
  EXPECT_EQ(gfx::Display::ROTATE_180, GetCurrentInternalDisplayRotation());
  TriggerLidUpdate(gfx::Vector3dF(-kMeanGravity, 0.0f, 0.0f));
  EXPECT_EQ(gfx::Display::ROTATE_180, GetCurrentInternalDisplayRotation());
}

// Tests that when the orientation lock is set to Portrait, that rotaiton can be
// done between the two angles of the orientation.
TEST_F(ScreenOrientationControllerTest, PortraitOrientationAllowsRotation) {
  scoped_ptr<content::WebContents> content(CreateWebContents());
  scoped_ptr<aura::Window> focus_window(CreateTestWindowInShellWithId(0));
  EnableMaximizeMode(true);

  AttachAndActivateWebContents(content.get(), focus_window.get());
  delegate()->Lock(content.get(), blink::WebScreenOrientationLockPortrait);
  EXPECT_EQ(gfx::Display::ROTATE_90, GetCurrentInternalDisplayRotation());
  EXPECT_TRUE(RotationLocked());

  // Inverse of orientation is allowed
  TriggerLidUpdate(gfx::Vector3dF(kMeanGravity, 0.0f, 0.0f));
  EXPECT_EQ(gfx::Display::ROTATE_270, GetCurrentInternalDisplayRotation());

  // Display rotations between are not allowed
  TriggerLidUpdate(gfx::Vector3dF(0.0f, kMeanGravity, 0.0f));
  EXPECT_EQ(gfx::Display::ROTATE_270, GetCurrentInternalDisplayRotation());
  TriggerLidUpdate(gfx::Vector3dF(0.0f, -kMeanGravity, 0.0f));
  EXPECT_EQ(gfx::Display::ROTATE_270, GetCurrentInternalDisplayRotation());
}

// Tests that for an orientation lock which does not allow rotation, that the
// display rotation remains constant.
TEST_F(ScreenOrientationControllerTest, OrientationLockDisallowsRotation) {
  scoped_ptr<content::WebContents> content(CreateWebContents());
  scoped_ptr<aura::Window> focus_window(CreateTestWindowInShellWithId(0));
  EnableMaximizeMode(true);

  AttachAndActivateWebContents(content.get(), focus_window.get());
  delegate()->Lock(content.get(),
                   blink::WebScreenOrientationLockPortraitPrimary);
  EXPECT_EQ(gfx::Display::ROTATE_90, GetCurrentInternalDisplayRotation());
  EXPECT_TRUE(RotationLocked());

  // Rotation does not change.
  TriggerLidUpdate(gfx::Vector3dF(kMeanGravity, 0.0f, 0.0f));
  EXPECT_EQ(gfx::Display::ROTATE_90, GetCurrentInternalDisplayRotation());
  TriggerLidUpdate(gfx::Vector3dF(0.0f, kMeanGravity, 0.0f));
  EXPECT_EQ(gfx::Display::ROTATE_90, GetCurrentInternalDisplayRotation());
  TriggerLidUpdate(gfx::Vector3dF(0.0f, -kMeanGravity, 0.0f));
  EXPECT_EQ(gfx::Display::ROTATE_90, GetCurrentInternalDisplayRotation());
}

// Tests that after a content::WebContents has applied an orientation lock which
// supports rotation, that a user rotation lock does not allow rotation.
TEST_F(ScreenOrientationControllerTest, UserRotationLockDisallowsRotation) {
  scoped_ptr<content::WebContents> content(CreateWebContents());
  scoped_ptr<aura::Window> focus_window(CreateTestWindowInShellWithId(0));
  EnableMaximizeMode(true);

  AttachAndActivateWebContents(content.get(), focus_window.get());
  delegate()->Lock(content.get(), blink::WebScreenOrientationLockLandscape);
  delegate()->Unlock(content.get());

  SetRotationLocked(true);
  EXPECT_TRUE(RotationLocked());
  EXPECT_EQ(gfx::Display::ROTATE_0, GetCurrentInternalDisplayRotation());

  TriggerLidUpdate(gfx::Vector3dF(0.0f, kMeanGravity, 0.0f));
  EXPECT_EQ(gfx::Display::ROTATE_0, GetCurrentInternalDisplayRotation());
}

// Tests that when MaximizeMode is triggered before the internal display is
// ready, that ScreenOrientationController still begins listening to events,
// which require an internal display to be acted upon.
TEST_F(ScreenOrientationControllerTest, InternalDisplayNotAvailableAtStartup) {
  test::DisplayManagerTestApi().SetFirstDisplayAsInternalDisplay();

  int64_t internal_display_id = gfx::Display::InternalDisplayId();
  gfx::Display::SetInternalDisplayId(gfx::Display::kInvalidDisplayID);

  EnableMaximizeMode(true);

  // Should not crash, even though there is no internal display.
  SetDisplayRotationById(internal_display_id, gfx::Display::ROTATE_180);
  EXPECT_FALSE(RotationLocked());

  // Should not crash, even though the invalid display id is requested.
  SetDisplayRotationById(gfx::Display::kInvalidDisplayID,
                         gfx::Display::ROTATE_180);
  EXPECT_FALSE(RotationLocked());

  // With an internal display now available, functionality should resume.
  gfx::Display::SetInternalDisplayId(internal_display_id);
  SetInternalDisplayRotation(gfx::Display::ROTATE_90);
  EXPECT_TRUE(RotationLocked());
}

// Verifies rotating an inactive Display is successful.
TEST_F(ScreenOrientationControllerTest, RotateInactiveDisplay) {
  const int64_t kInternalDisplayId = 9;
  const int64_t kExternalDisplayId = 10;
  const gfx::Display::Rotation kNewRotation = gfx::Display::ROTATE_180;

  const DisplayInfo internal_display_info =
      CreateDisplayInfo(kInternalDisplayId, gfx::Rect(0, 0, 500, 500));
  const DisplayInfo external_display_info =
      CreateDisplayInfo(kExternalDisplayId, gfx::Rect(1, 1, 500, 500));

  std::vector<DisplayInfo> display_info_list_two_active;
  display_info_list_two_active.push_back(internal_display_info);
  display_info_list_two_active.push_back(external_display_info);

  std::vector<DisplayInfo> display_info_list_one_active;
  display_info_list_one_active.push_back(external_display_info);

  // The DisplayInfo list with two active displays needs to be added first so
  // that the DisplayManager can track the |internal_display_info| as inactive
  // instead of non-existent.
  DisplayManager* display_manager = Shell::GetInstance()->display_manager();
  display_manager->UpdateDisplaysWith(display_info_list_two_active);
  display_manager->UpdateDisplaysWith(display_info_list_one_active);

  test::ScopedSetInternalDisplayId set_internal(kInternalDisplayId);

  ASSERT_NE(kNewRotation, display_manager->GetDisplayInfo(kInternalDisplayId)
                              .GetActiveRotation());

  Shell::GetInstance()->screen_orientation_controller()->SetDisplayRotation(
      kNewRotation, gfx::Display::ROTATION_SOURCE_ACTIVE);

  EXPECT_EQ(kNewRotation, display_manager->GetDisplayInfo(kInternalDisplayId)
                              .GetActiveRotation());
}

}  // namespace ash
