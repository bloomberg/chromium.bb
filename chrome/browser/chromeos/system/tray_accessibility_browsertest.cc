// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login_status.h"
#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/ash_pref_names.h"
#include "ash/shell.h"
#include "ash/shell_test_api.h"
#include "ash/system/accessibility/tray_accessibility.h"
#include "ash/system/tray/hover_highlight_view.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/tray/system_tray_test_api.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/chromeos/accessibility/magnification_manager.h"
#include "chrome/browser/chromeos/login/helper.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/session_controller_client.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/chromeos_switches.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/external_data_fetcher.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/label.h"
#include "ui/views/widget/widget.h"

using testing::Return;
using testing::_;
using testing::WithParamInterface;

namespace chromeos {

enum PrefSettingMechanism {
  PREF_SERVICE,
  POLICY,
};

namespace {

////////////////////////////////////////////////////////////////////////////////
// Changing accessibility settings may change preferences, so these helpers spin
// the message loop to ensure ash sees the change.

void SetMagnifierEnabled(bool enabled) {
  MagnificationManager::Get()->SetMagnifierEnabled(enabled);
  base::RunLoop().RunUntilIdle();
}

void EnableSpokenFeedback(bool enabled) {
  AccessibilityManager::Get()->EnableSpokenFeedback(enabled);
  base::RunLoop().RunUntilIdle();
}

void EnableSelectToSpeak(bool enabled) {
  AccessibilityManager::Get()->SetSelectToSpeakEnabled(enabled);
  base::RunLoop().RunUntilIdle();
}

void EnableHighContrast(bool enabled) {
  AccessibilityManager::Get()->EnableHighContrast(enabled);
  base::RunLoop().RunUntilIdle();
}

void EnableAutoclick(bool enabled) {
  AccessibilityManager::Get()->EnableAutoclick(enabled);
  base::RunLoop().RunUntilIdle();
}

void EnableVirtualKeyboard(bool enabled) {
  AccessibilityManager::Get()->EnableVirtualKeyboard(enabled);
  base::RunLoop().RunUntilIdle();
}

void EnableLargeCursor(bool enabled) {
  AccessibilityManager::Get()->EnableLargeCursor(enabled);
  base::RunLoop().RunUntilIdle();
}

void EnableMonoAudio(bool enabled) {
  AccessibilityManager::Get()->EnableMonoAudio(enabled);
  base::RunLoop().RunUntilIdle();
}

void SetCaretHighlightEnabled(bool enabled) {
  AccessibilityManager::Get()->SetCaretHighlightEnabled(enabled);
  base::RunLoop().RunUntilIdle();
}

void SetCursorHighlightEnabled(bool enabled) {
  AccessibilityManager::Get()->SetCursorHighlightEnabled(enabled);
  base::RunLoop().RunUntilIdle();
}

void SetFocusHighlightEnabled(bool enabled) {
  AccessibilityManager::Get()->SetFocusHighlightEnabled(enabled);
  base::RunLoop().RunUntilIdle();
}

void EnableStickyKeys(bool enabled) {
  AccessibilityManager::Get()->EnableStickyKeys(enabled);
  base::RunLoop().RunUntilIdle();
}

}  // namespace

// Uses InProcessBrowserTest instead of OobeBaseTest because most of the tests
// don't need to test the login screen.
class TrayAccessibilityTest
    : public InProcessBrowserTest,
      public WithParamInterface<PrefSettingMechanism> {
 public:
  TrayAccessibilityTest()
      : disable_animations_(
            ui::ScopedAnimationDurationScaleMode::ZERO_DURATION) {}
  ~TrayAccessibilityTest() override = default;

  // The profile which should be used by these tests.
  Profile* GetProfile() { return ProfileManager::GetActiveUserProfile(); }

  void SetUpInProcessBrowserTestFixture() override {
    InProcessBrowserTest::SetUpInProcessBrowserTestFixture();
    EXPECT_CALL(provider_, IsInitializationComplete(_))
        .WillRepeatedly(Return(true));
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(&provider_);
  }

  void SetShowAccessibilityOptionsInSystemTrayMenu(bool value) {
    if (GetParam() == PREF_SERVICE) {
      PrefService* prefs = GetProfile()->GetPrefs();
      prefs->SetBoolean(ash::prefs::kShouldAlwaysShowAccessibilityMenu, value);
      // Prefs are sent to ash asynchronously.
      base::RunLoop().RunUntilIdle();
    } else if (GetParam() == POLICY) {
      policy::PolicyMap policy_map;
      policy_map.Set(policy::key::kShowAccessibilityOptionsInSystemTrayMenu,
                     policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                     policy::POLICY_SOURCE_CLOUD,
                     std::make_unique<base::Value>(value), nullptr);
      provider_.UpdateChromePolicy(policy_map);
      base::RunLoop().RunUntilIdle();
    } else {
      FAIL() << "Unknown test parameterization";
    }
  }

  static ash::TrayAccessibility* tray() {
    return ash::SystemTrayTestApi(ash::Shell::Get()->GetPrimarySystemTray())
        .tray_accessibility();
  }

  views::View* CreateMenuItem() {
    return tray()->CreateDefaultView(GetLoginStatus());
  }

  void DestroyMenuItem() { return tray()->OnDefaultViewDestroyed(); }

  bool CanCreateMenuItem() {
    views::View* menu_item_view = CreateMenuItem();
    DestroyMenuItem();
    return menu_item_view != nullptr;
  }

  void SetLoginStatus(ash::LoginStatus status) {
    tray()->UpdateAfterLoginStatusChange(status);
  }

  ash::LoginStatus GetLoginStatus() { return tray()->login_; }

  bool CreateDetailedMenu() {
    tray()->ShowDetailedView(0);
    return tray()->detailed_menu_ != nullptr;
  }

  ash::tray::AccessibilityDetailedView* GetDetailedMenu() {
    return tray()->detailed_menu_;
  }

  void CloseDetailMenu() {
    ASSERT_TRUE(tray()->detailed_menu_);
    tray()->OnDetailedViewDestroyed();
    EXPECT_FALSE(tray()->detailed_menu_);
  }

  void ClickAutoclickOnDetailMenu() {
    ash::HoverHighlightView* view = tray()->detailed_menu_->autoclick_view_;
    tray()->detailed_menu_->OnViewClicked(view);
    base::RunLoop().RunUntilIdle();
  }

  bool IsAutoclickEnabledOnDetailMenu() const {
    return tray()->detailed_menu_->autoclick_enabled_;
  }

  // Disable animations so that tray icons hide immediately.
  ui::ScopedAnimationDurationScaleMode disable_animations_;

  policy::MockConfigurationPolicyProvider provider_;
};

IN_PROC_BROWSER_TEST_P(TrayAccessibilityTest, ShowMenu) {
  // TODO(tetsui): Restore after AccessibilityManager is moved to ash.
  // https://crbug.com/850014
  if (ash::features::IsSystemTrayUnifiedEnabled())
    return;

  SetShowAccessibilityOptionsInSystemTrayMenu(false);

  // Confirms that the menu is hidden.
  EXPECT_FALSE(CanCreateMenuItem());

  // Toggling spoken feedback changes the visibility of the menu.
  EnableSpokenFeedback(true);
  EXPECT_TRUE(CanCreateMenuItem());
  EnableSpokenFeedback(false);
  EXPECT_FALSE(CanCreateMenuItem());

  // Toggling high contrast changes the visibility of the menu.
  EnableHighContrast(true);
  EXPECT_TRUE(CanCreateMenuItem());
  EnableHighContrast(false);
  EXPECT_FALSE(CanCreateMenuItem());

  // Toggling screen magnifier changes the visibility of the menu.
  SetMagnifierEnabled(true);
  EXPECT_TRUE(CanCreateMenuItem());
  SetMagnifierEnabled(false);
  EXPECT_FALSE(CanCreateMenuItem());

  // Toggling autoclick changes the visibility of the menu.
  EnableAutoclick(true);
  EXPECT_TRUE(CanCreateMenuItem());
  EnableAutoclick(false);
  EXPECT_FALSE(CanCreateMenuItem());

  // Toggling virtual keyboard changes the visibility of the menu.
  EnableVirtualKeyboard(true);
  EXPECT_TRUE(CanCreateMenuItem());
  EnableVirtualKeyboard(false);
  EXPECT_FALSE(CanCreateMenuItem());

  // Toggling large mouse cursor changes the visibility of the menu.
  EnableLargeCursor(true);
  EXPECT_TRUE(CanCreateMenuItem());
  EnableLargeCursor(false);
  EXPECT_FALSE(CanCreateMenuItem());

  // Toggling mono audio changes the visibility of the menu.
  EnableMonoAudio(true);
  EXPECT_TRUE(CanCreateMenuItem());
  EnableMonoAudio(false);
  EXPECT_FALSE(CanCreateMenuItem());

  // Toggling caret highlight changes the visibility of the menu.
  SetCaretHighlightEnabled(true);
  EXPECT_TRUE(CanCreateMenuItem());
  SetCaretHighlightEnabled(false);
  EXPECT_FALSE(CanCreateMenuItem());

  // Toggling highlight mouse cursor changes the visibility of the menu.
  SetCursorHighlightEnabled(true);
  EXPECT_TRUE(CanCreateMenuItem());
  SetCursorHighlightEnabled(false);
  EXPECT_FALSE(CanCreateMenuItem());

  // Toggling highlight keyboard focus changes the visibility of the menu.
  SetFocusHighlightEnabled(true);
  EXPECT_TRUE(CanCreateMenuItem());
  SetFocusHighlightEnabled(false);
  EXPECT_FALSE(CanCreateMenuItem());

  // Toggling sticky keys changes the visibility of the menu.
  EnableStickyKeys(true);
  EXPECT_TRUE(CanCreateMenuItem());
  EnableStickyKeys(false);
  EXPECT_FALSE(CanCreateMenuItem());

  // Toggling select-to-speak dragging changes the visibility of the menu.
  EnableSelectToSpeak(true);
  EXPECT_TRUE(CanCreateMenuItem());
  EnableSelectToSpeak(false);
  EXPECT_FALSE(CanCreateMenuItem());

  // Enabling all accessibility features.
  SetMagnifierEnabled(true);
  EXPECT_TRUE(CanCreateMenuItem());
  EnableHighContrast(true);
  EXPECT_TRUE(CanCreateMenuItem());
  EnableSpokenFeedback(true);
  EXPECT_TRUE(CanCreateMenuItem());
  EnableSelectToSpeak(true);
  EXPECT_TRUE(CanCreateMenuItem());
  EnableAutoclick(true);
  EXPECT_TRUE(CanCreateMenuItem());
  EnableVirtualKeyboard(true);
  EXPECT_TRUE(CanCreateMenuItem());
  EnableLargeCursor(true);
  EXPECT_TRUE(CanCreateMenuItem());
  EnableMonoAudio(true);
  EXPECT_TRUE(CanCreateMenuItem());
  SetCaretHighlightEnabled(true);
  EXPECT_TRUE(CanCreateMenuItem());
  SetCursorHighlightEnabled(true);
  EXPECT_TRUE(CanCreateMenuItem());
  SetFocusHighlightEnabled(true);
  EXPECT_TRUE(CanCreateMenuItem());
  EnableStickyKeys(true);
  EXPECT_TRUE(CanCreateMenuItem());
  EnableVirtualKeyboard(false);
  EXPECT_TRUE(CanCreateMenuItem());
  EnableAutoclick(false);
  EXPECT_TRUE(CanCreateMenuItem());
  EnableSpokenFeedback(false);
  EXPECT_TRUE(CanCreateMenuItem());
  EnableSelectToSpeak(false);
  EXPECT_TRUE(CanCreateMenuItem());
  EnableHighContrast(false);
  EXPECT_TRUE(CanCreateMenuItem());
  SetMagnifierEnabled(false);
  EXPECT_TRUE(CanCreateMenuItem());
  EnableLargeCursor(false);
  EXPECT_TRUE(CanCreateMenuItem());
  EnableMonoAudio(false);
  EXPECT_TRUE(CanCreateMenuItem());
  SetCaretHighlightEnabled(false);
  EXPECT_TRUE(CanCreateMenuItem());
  SetCursorHighlightEnabled(false);
  EXPECT_TRUE(CanCreateMenuItem());
  SetFocusHighlightEnabled(false);
  EXPECT_TRUE(CanCreateMenuItem());
  EnableStickyKeys(false);
  EXPECT_FALSE(CanCreateMenuItem());
}

IN_PROC_BROWSER_TEST_P(TrayAccessibilityTest, ShowMenuWithShowMenuOption) {
  // TODO(tetsui): Restore after AccessibilityManager is moved to ash.
  // https://crbug.com/850014
  if (ash::features::IsSystemTrayUnifiedEnabled())
    return;

  SetShowAccessibilityOptionsInSystemTrayMenu(true);

  // Confirms that the menu is visible.
  EXPECT_TRUE(CanCreateMenuItem());

  // The menu remains visible regardless of toggling spoken feedback.
  EnableSpokenFeedback(true);
  EXPECT_TRUE(CanCreateMenuItem());
  EnableSpokenFeedback(false);
  EXPECT_TRUE(CanCreateMenuItem());

  // The menu remains visible regardless of toggling high contrast.
  EnableHighContrast(true);
  EXPECT_TRUE(CanCreateMenuItem());
  EnableHighContrast(false);
  EXPECT_TRUE(CanCreateMenuItem());

  // The menu remains visible regardless of toggling screen magnifier.
  SetMagnifierEnabled(true);
  EXPECT_TRUE(CanCreateMenuItem());
  SetMagnifierEnabled(false);
  EXPECT_TRUE(CanCreateMenuItem());

  // The menu remains visible regardless of toggling autoclick.
  EnableAutoclick(true);
  EXPECT_TRUE(CanCreateMenuItem());
  EnableAutoclick(false);
  EXPECT_TRUE(CanCreateMenuItem());

  // The menu remains visible regardless of toggling on-screen keyboard.
  EnableVirtualKeyboard(true);
  EXPECT_TRUE(CanCreateMenuItem());
  EnableVirtualKeyboard(false);
  EXPECT_TRUE(CanCreateMenuItem());

  // The menu remains visible regardless of toggling large mouse cursor.
  EnableLargeCursor(true);
  EXPECT_TRUE(CanCreateMenuItem());
  EnableLargeCursor(false);
  EXPECT_TRUE(CanCreateMenuItem());

  // The menu remains visible regardless of toggling mono audio.
  EnableMonoAudio(true);
  EXPECT_TRUE(CanCreateMenuItem());
  EnableMonoAudio(false);
  EXPECT_TRUE(CanCreateMenuItem());

  // The menu remains visible regardless of toggling caret highlight.
  SetCaretHighlightEnabled(true);
  EXPECT_TRUE(CanCreateMenuItem());
  SetCaretHighlightEnabled(false);
  EXPECT_TRUE(CanCreateMenuItem());

  // The menu remains visible regardless of toggling highlight mouse cursor.
  SetCursorHighlightEnabled(true);
  EXPECT_TRUE(CanCreateMenuItem());
  SetCursorHighlightEnabled(false);
  EXPECT_TRUE(CanCreateMenuItem());

  // The menu remains visible regardless of toggling highlight keyboard focus.
  SetFocusHighlightEnabled(true);
  EXPECT_TRUE(CanCreateMenuItem());
  SetFocusHighlightEnabled(false);
  EXPECT_TRUE(CanCreateMenuItem());

  // The menu remains visible regardless of the toggling sticky keys.
  EnableStickyKeys(true);
  EXPECT_TRUE(CanCreateMenuItem());
  EnableStickyKeys(false);
  EXPECT_TRUE(CanCreateMenuItem());

  // The menu remains visible regardless of toggling select-to-speak.
  EnableSelectToSpeak(true);
  EXPECT_TRUE(CanCreateMenuItem());
  EnableSelectToSpeak(false);
  EXPECT_TRUE(CanCreateMenuItem());

  // Enabling all accessibility features.
  SetMagnifierEnabled(true);
  EXPECT_TRUE(CanCreateMenuItem());
  EnableHighContrast(true);
  EXPECT_TRUE(CanCreateMenuItem());
  EnableSpokenFeedback(true);
  EXPECT_TRUE(CanCreateMenuItem());
  EnableSelectToSpeak(true);
  EXPECT_TRUE(CanCreateMenuItem());
  EnableAutoclick(true);
  EXPECT_TRUE(CanCreateMenuItem());
  EnableVirtualKeyboard(true);
  EXPECT_TRUE(CanCreateMenuItem());
  EnableLargeCursor(true);
  EXPECT_TRUE(CanCreateMenuItem());
  EnableMonoAudio(true);
  EXPECT_TRUE(CanCreateMenuItem());
  SetCaretHighlightEnabled(true);
  EXPECT_TRUE(CanCreateMenuItem());
  SetCursorHighlightEnabled(true);
  EXPECT_TRUE(CanCreateMenuItem());
  SetFocusHighlightEnabled(true);
  EXPECT_TRUE(CanCreateMenuItem());
  EnableStickyKeys(true);
  EXPECT_TRUE(CanCreateMenuItem());
  EnableVirtualKeyboard(false);
  EXPECT_TRUE(CanCreateMenuItem());
  EnableAutoclick(false);
  EXPECT_TRUE(CanCreateMenuItem());
  EnableSpokenFeedback(false);
  EXPECT_TRUE(CanCreateMenuItem());
  EnableSelectToSpeak(false);
  EXPECT_TRUE(CanCreateMenuItem());
  EnableHighContrast(false);
  EXPECT_TRUE(CanCreateMenuItem());
  SetMagnifierEnabled(false);
  EXPECT_TRUE(CanCreateMenuItem());
  EnableLargeCursor(false);
  EXPECT_TRUE(CanCreateMenuItem());
  EnableMonoAudio(false);
  EXPECT_TRUE(CanCreateMenuItem());
  SetCaretHighlightEnabled(false);
  EXPECT_TRUE(CanCreateMenuItem());
  SetCursorHighlightEnabled(false);
  EXPECT_TRUE(CanCreateMenuItem());
  SetFocusHighlightEnabled(false);
  EXPECT_TRUE(CanCreateMenuItem());
  EnableStickyKeys(false);
  EXPECT_TRUE(CanCreateMenuItem());

  SetShowAccessibilityOptionsInSystemTrayMenu(false);

  // Confirms that the menu is invisible.
  EXPECT_FALSE(CanCreateMenuItem());
}

IN_PROC_BROWSER_TEST_P(TrayAccessibilityTest, ShowMenuWithShowOnLoginScreen) {
  // TODO(tetsui): Restore after AccessibilityManager is moved to ash.
  // https://crbug.com/850014
  if (ash::features::IsSystemTrayUnifiedEnabled())
    return;

  SetLoginStatus(ash::LoginStatus::NOT_LOGGED_IN);

  // Confirms that the menu is visible.
  EXPECT_TRUE(CanCreateMenuItem());

  // The menu remains visible regardless of toggling spoken feedback.
  EnableSpokenFeedback(true);
  EXPECT_TRUE(CanCreateMenuItem());
  EnableSpokenFeedback(false);
  EXPECT_TRUE(CanCreateMenuItem());

  // The menu remains visible regardless of toggling high contrast.
  EnableHighContrast(true);
  EXPECT_TRUE(CanCreateMenuItem());
  EnableHighContrast(false);
  EXPECT_TRUE(CanCreateMenuItem());

  // The menu remains visible regardless of toggling screen magnifier.
  SetMagnifierEnabled(true);
  EXPECT_TRUE(CanCreateMenuItem());
  SetMagnifierEnabled(false);
  EXPECT_TRUE(CanCreateMenuItem());

  // The menu remains visible regardless of toggling on-screen keyboard.
  EnableVirtualKeyboard(true);
  EXPECT_TRUE(CanCreateMenuItem());
  EnableVirtualKeyboard(false);
  EXPECT_TRUE(CanCreateMenuItem());

  // The menu remains visible regardless of toggling large mouse cursor.
  EnableLargeCursor(true);
  EXPECT_TRUE(CanCreateMenuItem());
  EnableLargeCursor(false);
  EXPECT_TRUE(CanCreateMenuItem());

  // The menu remains visible regardless of toggling mono audio.
  EnableMonoAudio(true);
  EXPECT_TRUE(CanCreateMenuItem());
  EnableMonoAudio(false);
  EXPECT_TRUE(CanCreateMenuItem());

  // The menu remains visible regardless of toggling caret highlight.
  SetCaretHighlightEnabled(true);
  EXPECT_TRUE(CanCreateMenuItem());
  SetCaretHighlightEnabled(false);
  EXPECT_TRUE(CanCreateMenuItem());

  // The menu remains visible regardless of toggling highlight mouse cursor.
  SetCursorHighlightEnabled(true);
  EXPECT_TRUE(CanCreateMenuItem());
  SetCursorHighlightEnabled(false);
  EXPECT_TRUE(CanCreateMenuItem());

  // The menu remains visible regardless of toggling highlight keyboard focus.
  SetFocusHighlightEnabled(true);
  EXPECT_TRUE(CanCreateMenuItem());
  SetFocusHighlightEnabled(false);
  EXPECT_TRUE(CanCreateMenuItem());

  // The menu remains visible regardless of toggling sticky keys.
  EnableStickyKeys(true);
  EXPECT_TRUE(CanCreateMenuItem());
  EnableStickyKeys(false);
  EXPECT_TRUE(CanCreateMenuItem());

  // Enabling all accessibility features.
  SetMagnifierEnabled(true);
  EXPECT_TRUE(CanCreateMenuItem());
  EnableHighContrast(true);
  EXPECT_TRUE(CanCreateMenuItem());
  EnableSpokenFeedback(true);
  EXPECT_TRUE(CanCreateMenuItem());
  EnableVirtualKeyboard(true);
  EXPECT_TRUE(CanCreateMenuItem());
  EnableLargeCursor(true);
  EXPECT_TRUE(CanCreateMenuItem());
  EnableMonoAudio(true);
  EXPECT_TRUE(CanCreateMenuItem());
  SetCaretHighlightEnabled(true);
  EXPECT_TRUE(CanCreateMenuItem());
  SetCursorHighlightEnabled(true);
  EXPECT_TRUE(CanCreateMenuItem());
  SetFocusHighlightEnabled(true);
  EXPECT_TRUE(CanCreateMenuItem());
  EnableStickyKeys(true);
  EXPECT_TRUE(CanCreateMenuItem());
  EnableVirtualKeyboard(false);
  EXPECT_TRUE(CanCreateMenuItem());
  EnableSpokenFeedback(false);
  EXPECT_TRUE(CanCreateMenuItem());
  EnableHighContrast(false);
  EXPECT_TRUE(CanCreateMenuItem());
  SetMagnifierEnabled(false);
  EXPECT_TRUE(CanCreateMenuItem());
  EnableLargeCursor(false);
  EXPECT_TRUE(CanCreateMenuItem());
  EnableMonoAudio(false);
  EXPECT_TRUE(CanCreateMenuItem());
  SetCaretHighlightEnabled(false);
  EXPECT_TRUE(CanCreateMenuItem());
  SetCursorHighlightEnabled(false);
  EXPECT_TRUE(CanCreateMenuItem());
  SetFocusHighlightEnabled(false);
  EXPECT_TRUE(CanCreateMenuItem());
  EnableStickyKeys(false);
  EXPECT_TRUE(CanCreateMenuItem());

  SetShowAccessibilityOptionsInSystemTrayMenu(true);

  // Confirms that the menu remains visible.
  EXPECT_TRUE(CanCreateMenuItem());

  SetShowAccessibilityOptionsInSystemTrayMenu(false);

  // Confirms that the menu remains visible.
  EXPECT_TRUE(CanCreateMenuItem());
}

// TODO: Move to ash_unittests.
IN_PROC_BROWSER_TEST_P(TrayAccessibilityTest, KeepMenuVisibilityOnLockScreen) {
  // TODO(tetsui): Restore after AccessibilityManager is moved to ash.
  // https://crbug.com/850014
  if (ash::features::IsSystemTrayUnifiedEnabled())
    return;

  // Enables high contrast mode.
  EnableHighContrast(true);
  EXPECT_TRUE(CanCreateMenuItem());

  // Locks the screen.
  SetLoginStatus(ash::LoginStatus::LOCKED);
  EXPECT_TRUE(CanCreateMenuItem());

  // Disables high contrast mode.
  EnableHighContrast(false);

  // Confirms that the menu is still visible.
  EXPECT_TRUE(CanCreateMenuItem());
}

// Verify that the accessiblity system detailed menu remains open when an item
// is selected or deselected.
// TODO: Move to ash_unittests.
IN_PROC_BROWSER_TEST_P(TrayAccessibilityTest, DetailMenuRemainsOpen) {
  // TODO(tetsui): Restore after AccessibilityManager is moved to ash.
  // https://crbug.com/850014
  if (ash::features::IsSystemTrayUnifiedEnabled())
    return;

  EXPECT_TRUE(CreateDetailedMenu());

  ClickAutoclickOnDetailMenu();
  EXPECT_TRUE(IsAutoclickEnabledOnDetailMenu());
  {
    base::RunLoop run_loop;
    run_loop.RunUntilIdle();
  }
  EXPECT_TRUE(GetDetailedMenu());

  ClickAutoclickOnDetailMenu();
  EXPECT_FALSE(IsAutoclickEnabledOnDetailMenu());
  {
    base::RunLoop run_loop;
    run_loop.RunUntilIdle();
  }
  EXPECT_TRUE(GetDetailedMenu());
}

INSTANTIATE_TEST_CASE_P(TrayAccessibilityTestInstance,
                        TrayAccessibilityTest,
                        testing::Values(PREF_SERVICE,
                                        POLICY));

}  // namespace chromeos
