// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray_accessibility.h"

#include "ash/accessibility/accessibility_controller.h"
#include "ash/public/cpp/ash_pref_names.h"
#include "ash/session/session_controller.h"
#include "ash/shell.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/system/tray/system_tray_test_api.h"
#include "ash/test/ash_test_base.h"
#include "base/strings/utf_string_conversions.h"
#include "components/prefs/pref_service.h"
#include "ui/message_center/message_center.h"

using message_center::MessageCenter;

namespace ash {
namespace {

// Simulates changing the large cursor setting via menu.
void SetLargeCursorEnabledFromMenu(bool enabled) {
  Shell::Get()->accessibility_controller()->SetLargeCursorEnabled(enabled);
}

// Simulates changing the large cursor setting via webui settings.
void SetLargeCursorEnabledFromSettings(bool enabled) {
  Shell::Get()
      ->session_controller()
      ->GetLastActiveUserPrefService()
      ->SetBoolean(prefs::kAccessibilityLargeCursorEnabled, enabled);
}

using TrayAccessibilityTest = AshTestBase;

// Tests that the icon becomes visible when the tray menu toggles a feature.
TEST_F(TrayAccessibilityTest, VisibilityFromMenu) {
  SystemTray* tray = GetPrimarySystemTray();
  TrayAccessibility* tray_item = SystemTrayTestApi(tray).tray_accessibility();

  // By default the icon isn't visible.
  EXPECT_FALSE(tray_item->tray_view()->visible());

  // Turning on an accessibility feature shows the icon.
  SetLargeCursorEnabledFromMenu(true);
  EXPECT_TRUE(tray_item->tray_view()->visible());

  // Turning off all accessibility features hides the icon.
  SetLargeCursorEnabledFromMenu(false);
  EXPECT_FALSE(tray_item->tray_view()->visible());
}

// Tests that the icon becomes visible when webui settings toggles a feature.
TEST_F(TrayAccessibilityTest, VisibilityFromSettings) {
  SystemTray* tray = GetPrimarySystemTray();
  TrayAccessibility* tray_item = SystemTrayTestApi(tray).tray_accessibility();

  // By default the icon isn't visible.
  EXPECT_FALSE(tray_item->tray_view()->visible());

  // Turning on an accessibility pref shows the icon.
  SetLargeCursorEnabledFromSettings(true);
  EXPECT_TRUE(tray_item->tray_view()->visible());

  // Turning off all accessibility prefs hides the icon.
  SetLargeCursorEnabledFromSettings(false);
  EXPECT_FALSE(tray_item->tray_view()->visible());
}

TEST_F(TrayAccessibilityTest, ShowNotificationOnSpokenFeedback) {
  const base::string16 kChromeVoxEnabledTitle =
      base::ASCIIToUTF16("ChromeVox enabled");
  const base::string16 kChromeVoxEnabled =
      base::ASCIIToUTF16("Press Ctrl + Alt + Z to disable spoken feedback.");
  AccessibilityController* controller =
      Shell::Get()->accessibility_controller();

  // Enabling spoken feedback should show the notification.
  controller->SetSpokenFeedbackEnabled(true, A11Y_NOTIFICATION_SHOW);
  message_center::NotificationList::Notifications notifications =
      MessageCenter::Get()->GetVisibleNotifications();
  ASSERT_EQ(1u, notifications.size());
  EXPECT_EQ(kChromeVoxEnabledTitle, (*notifications.begin())->title());
  EXPECT_EQ(kChromeVoxEnabled, (*notifications.begin())->message());

  // Disabling spoken feedback should not show any notification.
  controller->SetSpokenFeedbackEnabled(false, A11Y_NOTIFICATION_SHOW);
  notifications = MessageCenter::Get()->GetVisibleNotifications();
  EXPECT_EQ(0u, notifications.size());
}

TEST_F(TrayAccessibilityTest, ShowNotificationOnBrailleDisplayStateChanged) {
  const base::string16 kBrailleConnected =
      base::ASCIIToUTF16("Braille display connected.");
  const base::string16 kChromeVoxEnabled =
      base::ASCIIToUTF16("Press Ctrl + Alt + Z to disable spoken feedback.");
  const base::string16 kBrailleConnectedAndChromeVoxEnabledTitle =
      base::ASCIIToUTF16("Braille and ChromeVox are enabled");
  AccessibilityController* controller =
      Shell::Get()->accessibility_controller();

  controller->SetSpokenFeedbackEnabled(true, A11Y_NOTIFICATION_SHOW);
  EXPECT_TRUE(controller->IsSpokenFeedbackEnabled());
  // Connecting a braille display when spoken feedback is already enabled
  // should only show the message about the braille display.
  controller->BrailleDisplayStateChanged(true);
  message_center::NotificationList::Notifications notifications =
      MessageCenter::Get()->GetVisibleNotifications();
  ASSERT_EQ(1u, notifications.size());
  EXPECT_EQ(base::string16(), (*notifications.begin())->title());
  EXPECT_EQ(kBrailleConnected, (*notifications.begin())->message());

  // Neither disconnecting a braille display, nor disabling spoken feedback
  // should show any notification.
  controller->BrailleDisplayStateChanged(false);
  EXPECT_TRUE(controller->IsSpokenFeedbackEnabled());
  notifications = MessageCenter::Get()->GetVisibleNotifications();
  EXPECT_EQ(0u, notifications.size());
  controller->SetSpokenFeedbackEnabled(false, A11Y_NOTIFICATION_SHOW);
  notifications = MessageCenter::Get()->GetVisibleNotifications();
  EXPECT_EQ(0u, notifications.size());
  EXPECT_FALSE(controller->IsSpokenFeedbackEnabled());

  // Connecting a braille display should enable spoken feedback and show
  // both messages.
  controller->BrailleDisplayStateChanged(true);
  EXPECT_TRUE(controller->IsSpokenFeedbackEnabled());
  notifications = MessageCenter::Get()->GetVisibleNotifications();
  EXPECT_EQ(kBrailleConnectedAndChromeVoxEnabledTitle,
            (*notifications.begin())->title());
  EXPECT_EQ(kChromeVoxEnabled, (*notifications.begin())->message());
}

}  // namespace
}  // namespace ash
