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
#include "components/prefs/pref_service.h"

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

}  // namespace
}  // namespace ash
