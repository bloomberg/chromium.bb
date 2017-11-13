// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray_caps_lock.h"

#include "ash/accessibility/accessibility_controller.h"
#include "ash/accessibility/test_accessibility_controller_client.h"
#include "ash/shell.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/tray/system_tray_test_api.h"
#include "ash/system/web_notification/web_notification_tray.h"
#include "ash/test/ash_test_base.h"

namespace ash {

class TrayCapsLockTest : public AshTestBase {
 public:
  TrayCapsLockTest() = default;
  ~TrayCapsLockTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();
    WebNotificationTray::DisableAnimationsForTest(true);
  }

  void TearDown() override {
    WebNotificationTray::DisableAnimationsForTest(false);
    AshTestBase::TearDown();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TrayCapsLockTest);
};

// Tests that the icon becomes visible when the tray controller toggles it.
TEST_F(TrayCapsLockTest, Visibility) {
  SystemTray* tray = GetPrimarySystemTray();
  TrayCapsLock* caps_lock = SystemTrayTestApi(tray).tray_caps_lock();

  // By default the icon isn't visible.
  EXPECT_FALSE(caps_lock->tray_view()->visible());

  // Simulate turning on caps lock.
  caps_lock->OnCapsLockChanged(true);
  EXPECT_TRUE(caps_lock->tray_view()->visible());

  // Simulate turning off caps lock.
  caps_lock->OnCapsLockChanged(false);
  EXPECT_FALSE(caps_lock->tray_view()->visible());
}

// Tests that a11y alert is sent on toggling caps lock.
TEST_F(TrayCapsLockTest, A11yAlert) {
  SystemTray* tray = GetPrimarySystemTray();
  TrayCapsLock* caps_lock = SystemTrayTestApi(tray).tray_caps_lock();

  TestAccessibilityControllerClient client;
  AccessibilityController* controller =
      Shell::Get()->accessibility_controller();
  controller->SetClient(client.CreateInterfacePtrAndBind());

  // Simulate turning on caps lock.
  caps_lock->OnCapsLockChanged(true);
  controller->FlushMojoForTest();
  EXPECT_EQ(mojom::AccessibilityAlert::CAPS_ON, client.last_a11y_alert());

  // Simulate turning off caps lock.
  caps_lock->OnCapsLockChanged(false);
  controller->FlushMojoForTest();
  EXPECT_EQ(mojom::AccessibilityAlert::CAPS_OFF, client.last_a11y_alert());
}

}  // namespace ash
