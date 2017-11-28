// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accessibility/accessibility_controller.h"

#include "ash/ash_constants.h"
#include "ash/public/cpp/ash_pref_names.h"
#include "ash/session/session_controller.h"
#include "ash/shell.h"
#include "ash/system/accessibility_observer.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/test/ash_test_base.h"
#include "components/prefs/pref_service.h"

namespace ash {

class TestAccessibilityObserver : public AccessibilityObserver {
 public:
  TestAccessibilityObserver() = default;
  ~TestAccessibilityObserver() override = default;

  // AccessibilityObserver:
  void OnAccessibilityStatusChanged(
      AccessibilityNotificationVisibility notify) override {
    if (notify == A11Y_NOTIFICATION_NONE) {
      ++notification_none_changed_;
    } else if (notify == A11Y_NOTIFICATION_SHOW) {
      ++notification_show_changed_;
    }
  }

  int notification_none_changed_ = 0;
  int notification_show_changed_ = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestAccessibilityObserver);
};

using AccessibilityControllerTest = AshTestBase;

TEST_F(AccessibilityControllerTest, PrefsAreRegistered) {
  PrefService* prefs =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  EXPECT_TRUE(prefs->FindPreference(prefs::kAccessibilityHighContrastEnabled));
  EXPECT_TRUE(prefs->FindPreference(prefs::kAccessibilityLargeCursorEnabled));
  EXPECT_TRUE(prefs->FindPreference(prefs::kAccessibilityLargeCursorDipSize));
  EXPECT_TRUE(prefs->FindPreference(prefs::kAccessibilityMonoAudioEnabled));
  EXPECT_TRUE(
      prefs->FindPreference(prefs::kAccessibilityScreenMagnifierEnabled));
  EXPECT_TRUE(
      prefs->FindPreference(prefs::kAccessibilitySpokenFeedbackEnabled));
}

TEST_F(AccessibilityControllerTest, SetHighContrastEnabled) {
  AccessibilityController* controller =
      Shell::Get()->accessibility_controller();
  EXPECT_FALSE(controller->IsHighContrastEnabled());

  TestAccessibilityObserver observer;
  Shell::Get()->system_tray_notifier()->AddAccessibilityObserver(&observer);
  EXPECT_EQ(0, observer.notification_none_changed_);

  controller->SetHighContrastEnabled(true);
  EXPECT_TRUE(controller->IsHighContrastEnabled());
  EXPECT_EQ(1, observer.notification_none_changed_);

  controller->SetHighContrastEnabled(false);
  EXPECT_FALSE(controller->IsHighContrastEnabled());
  EXPECT_EQ(2, observer.notification_none_changed_);

  Shell::Get()->system_tray_notifier()->RemoveAccessibilityObserver(&observer);
}

TEST_F(AccessibilityControllerTest, SetLargeCursorEnabled) {
  AccessibilityController* controller =
      Shell::Get()->accessibility_controller();
  EXPECT_FALSE(controller->IsLargeCursorEnabled());

  TestAccessibilityObserver observer;
  Shell::Get()->system_tray_notifier()->AddAccessibilityObserver(&observer);
  EXPECT_EQ(0, observer.notification_none_changed_);

  controller->SetLargeCursorEnabled(true);
  EXPECT_TRUE(controller->IsLargeCursorEnabled());
  EXPECT_EQ(1, observer.notification_none_changed_);

  controller->SetLargeCursorEnabled(false);
  EXPECT_FALSE(controller->IsLargeCursorEnabled());
  EXPECT_EQ(2, observer.notification_none_changed_);

  Shell::Get()->system_tray_notifier()->RemoveAccessibilityObserver(&observer);
}

TEST_F(AccessibilityControllerTest, DisableLargeCursorResetsSize) {
  PrefService* prefs =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  EXPECT_EQ(kDefaultLargeCursorSize,
            prefs->GetInteger(prefs::kAccessibilityLargeCursorDipSize));

  // Simulate using chrome settings webui to turn on large cursor and set a
  // custom size.
  prefs->SetBoolean(prefs::kAccessibilityLargeCursorEnabled, true);
  prefs->SetInteger(prefs::kAccessibilityLargeCursorDipSize, 48);

  // Turning off large cursor resets the size to the default.
  prefs->SetBoolean(prefs::kAccessibilityLargeCursorEnabled, false);
  EXPECT_EQ(kDefaultLargeCursorSize,
            prefs->GetInteger(prefs::kAccessibilityLargeCursorDipSize));
}

TEST_F(AccessibilityControllerTest, SetMonoAudioEnabled) {
  AccessibilityController* controller =
      Shell::Get()->accessibility_controller();
  EXPECT_FALSE(controller->IsMonoAudioEnabled());

  TestAccessibilityObserver observer;
  Shell::Get()->system_tray_notifier()->AddAccessibilityObserver(&observer);
  EXPECT_EQ(0, observer.notification_none_changed_);

  controller->SetMonoAudioEnabled(true);
  EXPECT_TRUE(controller->IsMonoAudioEnabled());
  EXPECT_EQ(1, observer.notification_none_changed_);

  controller->SetMonoAudioEnabled(false);
  EXPECT_FALSE(controller->IsMonoAudioEnabled());
  EXPECT_EQ(2, observer.notification_none_changed_);

  Shell::Get()->system_tray_notifier()->RemoveAccessibilityObserver(&observer);
}

TEST_F(AccessibilityControllerTest, SetSpokenFeedbackEnabled) {
  AccessibilityController* controller =
      Shell::Get()->accessibility_controller();
  EXPECT_FALSE(controller->IsSpokenFeedbackEnabled());

  TestAccessibilityObserver observer;
  Shell::Get()->system_tray_notifier()->AddAccessibilityObserver(&observer);
  EXPECT_EQ(0, observer.notification_none_changed_);
  EXPECT_EQ(0, observer.notification_show_changed_);

  controller->SetSpokenFeedbackEnabled(true, A11Y_NOTIFICATION_SHOW);
  EXPECT_TRUE(controller->IsSpokenFeedbackEnabled());
  EXPECT_EQ(0, observer.notification_none_changed_);
  EXPECT_EQ(1, observer.notification_show_changed_);

  controller->SetSpokenFeedbackEnabled(false, A11Y_NOTIFICATION_NONE);
  EXPECT_FALSE(controller->IsSpokenFeedbackEnabled());
  EXPECT_EQ(1, observer.notification_none_changed_);
  EXPECT_EQ(1, observer.notification_show_changed_);

  Shell::Get()->system_tray_notifier()->RemoveAccessibilityObserver(&observer);
}

using AccessibilityControllerSigninTest = NoSessionAshTestBase;

TEST_F(AccessibilityControllerSigninTest, SigninScreenPrefs) {
  AccessibilityController* accessibility =
      Shell::Get()->accessibility_controller();

  SessionController* session = Shell::Get()->session_controller();
  EXPECT_EQ(session_manager::SessionState::LOGIN_PRIMARY,
            session->GetSessionState());
  EXPECT_FALSE(accessibility->IsLargeCursorEnabled());

  // Verify that toggling prefs at the signin screen changes the signin setting.
  PrefService* signin_prefs = session->GetSigninScreenPrefService();
  using prefs::kAccessibilityLargeCursorEnabled;
  EXPECT_FALSE(signin_prefs->GetBoolean(kAccessibilityLargeCursorEnabled));
  accessibility->SetLargeCursorEnabled(true);
  EXPECT_TRUE(accessibility->IsLargeCursorEnabled());
  EXPECT_TRUE(signin_prefs->GetBoolean(kAccessibilityLargeCursorEnabled));

  // Verify that toggling prefs after signin changes the user setting.
  SimulateUserLogin("user1@test.com");
  PrefService* user_prefs = session->GetLastActiveUserPrefService();
  EXPECT_NE(signin_prefs, user_prefs);
  EXPECT_FALSE(accessibility->IsLargeCursorEnabled());
  EXPECT_FALSE(user_prefs->GetBoolean(kAccessibilityLargeCursorEnabled));
  accessibility->SetLargeCursorEnabled(true);
  EXPECT_TRUE(accessibility->IsLargeCursorEnabled());
  EXPECT_TRUE(user_prefs->GetBoolean(kAccessibilityLargeCursorEnabled));
}

}  // namespace ash
