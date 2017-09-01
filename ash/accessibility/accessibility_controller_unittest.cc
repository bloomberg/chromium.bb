// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accessibility/accessibility_controller.h"

#include "ash/public/cpp/ash_pref_names.h"
#include "ash/session/session_controller.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "components/prefs/pref_service.h"

namespace ash {

using AccessibilityControllerTest = AshTestBase;

TEST_F(AccessibilityControllerTest, PrefsAreRegistered) {
  PrefService* prefs =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  EXPECT_TRUE(prefs->FindPreference(prefs::kAccessibilityLargeCursorEnabled));
}

TEST_F(AccessibilityControllerTest, SetLargeCursorEnabled) {
  AccessibilityController* controller =
      Shell::Get()->accessibility_controller();
  EXPECT_FALSE(controller->IsLargeCursorEnabled());
  controller->SetLargeCursorEnabled(true);
  EXPECT_TRUE(controller->IsLargeCursorEnabled());
  controller->SetLargeCursorEnabled(false);
  EXPECT_FALSE(controller->IsLargeCursorEnabled());
}

}  // namespace ash
