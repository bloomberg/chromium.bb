// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/magnifier/magnification_controller.h"
#include "ash/shell.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/tray_accessibility.h"
#include "ash/system/user/login_status.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/prefs/pref_service.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/chromeos/accessibility/magnification_manager.h"
#include "chrome/browser/chromeos/login/helper.h"
#include "chrome/browser/chromeos/login/login_utils.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/users/user_manager.h"
#include "chrome/browser/chromeos/login/users/user_manager_impl.h"
#include "chrome/browser/extensions/api/braille_display_private/mock_braille_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/chromeos_switches.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/external_data_fetcher.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "content/public/test/test_utils.h"
#include "policy/policy_constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/controls/label.h"
#include "ui/views/widget/widget.h"

using extensions::api::braille_display_private::BrailleObserver;
using extensions::api::braille_display_private::DisplayState;
using extensions::api::braille_display_private::MockBrailleController;
using testing::Return;
using testing::_;
using testing::WithParamInterface;

namespace chromeos {

enum PrefSettingMechanism {
  PREF_SERVICE,
  POLICY,
};

void SetMagnifierEnabled(bool enabled) {
  MagnificationManager::Get()->SetMagnifierEnabled(enabled);
}

class TrayAccessibilityTest
    : public InProcessBrowserTest,
      public WithParamInterface<PrefSettingMechanism> {
 protected:
  TrayAccessibilityTest() {}
  virtual ~TrayAccessibilityTest() {}

  // The profile which should be used by these tests.
  Profile* GetProfile() { return ProfileManager::GetActiveUserProfile(); }

  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    EXPECT_CALL(provider_, IsInitializationComplete(_))
        .WillRepeatedly(Return(true));
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(&provider_);
    AccessibilityManager::SetBrailleControllerForTest(&braille_controller_);
  }

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    command_line->AppendSwitch(switches::kLoginManager);
    command_line->AppendSwitchASCII(switches::kLoginProfile,
                                    TestingProfile::kTestUserProfileDir);
  }

  virtual void SetUpOnMainThread() OVERRIDE {
    AccessibilityManager::Get()->SetProfileForTest(GetProfile());
    MagnificationManager::Get()->SetProfileForTest(GetProfile());
  }

  virtual void RunTestOnMainThreadLoop() OVERRIDE {
    // Need to mark oobe completed to show detailed views.
    StartupUtils::MarkOobeCompleted();
    InProcessBrowserTest::RunTestOnMainThreadLoop();
  }

  virtual void TearDownOnMainThread() OVERRIDE {
    AccessibilityManager::SetBrailleControllerForTest(NULL);
  }

  void SetShowAccessibilityOptionsInSystemTrayMenu(bool value) {
    if (GetParam() == PREF_SERVICE) {
      PrefService* prefs = GetProfile()->GetPrefs();
      prefs->SetBoolean(prefs::kShouldAlwaysShowAccessibilityMenu, value);
    } else if (GetParam() == POLICY) {
      policy::PolicyMap policy_map;
      policy_map.Set(policy::key::kShowAccessibilityOptionsInSystemTrayMenu,
                     policy::POLICY_LEVEL_MANDATORY,
                     policy::POLICY_SCOPE_USER,
                     new base::FundamentalValue(value),
                     NULL);
      provider_.UpdateChromePolicy(policy_map);
      base::RunLoop().RunUntilIdle();
    } else {
      FAIL() << "Unknown test parameterization";
    }
  }

  ash::TrayAccessibility* tray() {
    return ash::Shell::GetInstance()->GetPrimarySystemTray()->
        GetTrayAccessibilityForTest();
  }

  const ash::TrayAccessibility* tray() const {
    return ash::Shell::GetInstance()
        ->GetPrimarySystemTray()
        ->GetTrayAccessibilityForTest();
  }

  bool IsTrayIconVisible() const { return tray()->tray_icon_visible_; }

  views::View* CreateMenuItem() {
    return tray()->CreateDefaultView(GetLoginStatus());
  }

  void DestroyMenuItem() {
    return tray()->DestroyDefaultView();
  }

  bool CanCreateMenuItem() {
    views::View* menu_item_view = CreateMenuItem();
    DestroyMenuItem();
    return menu_item_view != NULL;
  }

  void SetLoginStatus(ash::user::LoginStatus status) {
    tray()->UpdateAfterLoginStatusChange(status);
  }

  ash::user::LoginStatus GetLoginStatus() {
    return tray()->login_;
  }

  bool CreateDetailedMenu() {
    tray()->PopupDetailedView(0, false);
    return tray()->detailed_menu_ != NULL;
  }

  void CloseDetailMenu() {
    CHECK(tray()->detailed_menu_);
    tray()->DestroyDetailedView();
    tray()->detailed_menu_ = NULL;
  }

  void ClickSpokenFeedbackOnDetailMenu() {
    views::View* button = tray()->detailed_menu_->spoken_feedback_view_;
    ASSERT_TRUE(button);
    tray()->detailed_menu_->OnViewClicked(button);
  }

  void ClickHighContrastOnDetailMenu() {
    views::View* button = tray()->detailed_menu_->high_contrast_view_;
    ASSERT_TRUE(button);
    tray()->detailed_menu_->OnViewClicked(button);
  }

  void ClickScreenMagnifierOnDetailMenu() {
    views::View* button = tray()->detailed_menu_->screen_magnifier_view_;
    ASSERT_TRUE(button);
    tray()->detailed_menu_->OnViewClicked(button);
  }

  void ClickAutoclickOnDetailMenu() {
    views::View* button = tray()->detailed_menu_->autoclick_view_;
    ASSERT_TRUE(button);
    tray()->detailed_menu_->OnViewClicked(button);
  }

  void ClickVirtualKeyboardOnDetailMenu() {
    views::View* button = tray()->detailed_menu_->virtual_keyboard_view_;
    ASSERT_TRUE(button);
    tray()->detailed_menu_->OnViewClicked(button);
  }

  bool IsSpokenFeedbackEnabledOnDetailMenu() const {
    return tray()->detailed_menu_->spoken_feedback_enabled_;
  }

  bool IsHighContrastEnabledOnDetailMenu() const {
    return tray()->detailed_menu_->high_contrast_enabled_;
  }

  bool IsScreenMagnifierEnabledOnDetailMenu() const {
    return tray()->detailed_menu_->screen_magnifier_enabled_;
  }

  bool IsLargeCursorEnabledOnDetailMenu() const {
    return tray()->detailed_menu_->large_cursor_enabled_;
  }

  bool IsAutoclickEnabledOnDetailMenu() const {
    return tray()->detailed_menu_->autoclick_enabled_;
  }

  bool IsVirtualKeyboardEnabledOnDetailMenu() const {
    return tray()->detailed_menu_->virtual_keyboard_enabled_;
  }

  bool IsSpokenFeedbackMenuShownOnDetailMenu() const {
    return tray()->detailed_menu_->spoken_feedback_view_;
  }

  bool IsHighContrastMenuShownOnDetailMenu() const {
    return tray()->detailed_menu_->high_contrast_view_;
  }

  bool IsScreenMagnifierMenuShownOnDetailMenu() const {
    return tray()->detailed_menu_->screen_magnifier_view_;
  }

  bool IsLargeCursorMenuShownOnDetailMenu() const {
    return tray()->detailed_menu_->large_cursor_view_;
  }

  bool IsAutoclickMenuShownOnDetailMenu() const {
    return tray()->detailed_menu_->autoclick_view_;
  }

  bool IsVirtualKeyboardMenuShownOnDetailMenu() const {
    return tray()->detailed_menu_->virtual_keyboard_view_;
  }

  bool IsNotificationShown() const {
    return (tray()->detailed_popup_ &&
            !tray()->detailed_popup_->GetWidget()->IsClosed());
  }

  base::string16 GetNotificationText() const {
    if (IsNotificationShown())
      return tray()->detailed_popup_->label_for_test()->text();
    else
      return base::string16();
  }

  void SetBrailleConnected(bool connected) {
    braille_controller_.SetAvailable(connected);
    braille_controller_.GetObserver()->OnBrailleDisplayStateChanged(
        *braille_controller_.GetDisplayState());
  }

  policy::MockConfigurationPolicyProvider provider_;
  MockBrailleController braille_controller_;
};

IN_PROC_BROWSER_TEST_P(TrayAccessibilityTest, LoginStatus) {
  EXPECT_EQ(ash::user::LOGGED_IN_NONE, GetLoginStatus());

  UserManager::Get()->UserLoggedIn(
      "owner@invalid.domain", "owner@invalid.domain", true);
  UserManager::Get()->SessionStarted();

  EXPECT_EQ(ash::user::LOGGED_IN_USER, GetLoginStatus());
}

// http://crbug.com/396342
IN_PROC_BROWSER_TEST_P(TrayAccessibilityTest, DISABLED_ShowTrayIcon) {
  SetLoginStatus(ash::user::LOGGED_IN_NONE);

  // Confirms that the icon is invisible before login.
  EXPECT_FALSE(IsTrayIconVisible());

  UserManager::Get()->UserLoggedIn(
      "owner@invalid.domain", "owner@invalid.domain", true);
  UserManager::Get()->SessionStarted();

  // Confirms that the icon is invisible just after login.
  EXPECT_FALSE(IsTrayIconVisible());

  // Toggling spoken feedback changes the visibillity of the icon.
  AccessibilityManager::Get()->EnableSpokenFeedback(
      true, ash::A11Y_NOTIFICATION_NONE);
  EXPECT_TRUE(IsTrayIconVisible());
  AccessibilityManager::Get()->EnableSpokenFeedback(
      false, ash::A11Y_NOTIFICATION_NONE);
  EXPECT_FALSE(IsTrayIconVisible());

  // Toggling high contrast the visibillity of the icon.
  AccessibilityManager::Get()->EnableHighContrast(true);
  EXPECT_TRUE(IsTrayIconVisible());
  AccessibilityManager::Get()->EnableHighContrast(false);
  EXPECT_FALSE(IsTrayIconVisible());

  // Toggling magnifier the visibility of the icon.
  SetMagnifierEnabled(true);
  EXPECT_TRUE(IsTrayIconVisible());
  SetMagnifierEnabled(false);
  EXPECT_FALSE(IsTrayIconVisible());

  // Toggling the virtual keyboard setting changes the visibility of the a11y
  // icon.
  AccessibilityManager::Get()->EnableVirtualKeyboard(true);
  EXPECT_TRUE(IsTrayIconVisible());
  AccessibilityManager::Get()->EnableVirtualKeyboard(false);
  EXPECT_FALSE(IsTrayIconVisible());

  // Enabling all accessibility features.
  SetMagnifierEnabled(true);
  EXPECT_TRUE(IsTrayIconVisible());
  AccessibilityManager::Get()->EnableHighContrast(true);
  EXPECT_TRUE(IsTrayIconVisible());
  AccessibilityManager::Get()->EnableSpokenFeedback(
      true, ash::A11Y_NOTIFICATION_NONE);
  EXPECT_TRUE(IsTrayIconVisible());
  AccessibilityManager::Get()->EnableVirtualKeyboard(true);
  EXPECT_TRUE(IsTrayIconVisible());
  AccessibilityManager::Get()->EnableSpokenFeedback(
      false, ash::A11Y_NOTIFICATION_NONE);
  EXPECT_TRUE(IsTrayIconVisible());
  AccessibilityManager::Get()->EnableHighContrast(false);
  EXPECT_TRUE(IsTrayIconVisible());
  SetMagnifierEnabled(false);
  EXPECT_TRUE(IsTrayIconVisible());
  AccessibilityManager::Get()->EnableVirtualKeyboard(false);
  EXPECT_FALSE(IsTrayIconVisible());

  // Confirms that prefs::kShouldAlwaysShowAccessibilityMenu doesn't affect
  // the icon on the tray.
  SetShowAccessibilityOptionsInSystemTrayMenu(true);
  AccessibilityManager::Get()->EnableHighContrast(true);
  EXPECT_TRUE(IsTrayIconVisible());
  AccessibilityManager::Get()->EnableHighContrast(false);
  EXPECT_FALSE(IsTrayIconVisible());
}

// http://crbug.com/396342
IN_PROC_BROWSER_TEST_P(TrayAccessibilityTest, DISABLED_ShowMenu) {
  // Login
  UserManager::Get()->UserLoggedIn(
      "owner@invalid.domain", "owner@invalid.domain", true);
  UserManager::Get()->SessionStarted();

  SetShowAccessibilityOptionsInSystemTrayMenu(false);

  // Confirms that the menu is hidden.
  EXPECT_FALSE(CanCreateMenuItem());

  // Toggling spoken feedback changes the visibillity of the menu.
  AccessibilityManager::Get()->EnableSpokenFeedback(
      true, ash::A11Y_NOTIFICATION_NONE);
  EXPECT_TRUE(CanCreateMenuItem());
  AccessibilityManager::Get()->EnableSpokenFeedback(
      false, ash::A11Y_NOTIFICATION_NONE);
  EXPECT_FALSE(CanCreateMenuItem());

  // Toggling high contrast changes the visibillity of the menu.
  AccessibilityManager::Get()->EnableHighContrast(true);
  EXPECT_TRUE(CanCreateMenuItem());
  AccessibilityManager::Get()->EnableHighContrast(false);
  EXPECT_FALSE(CanCreateMenuItem());

  // Toggling screen magnifier changes the visibility of the menu.
  SetMagnifierEnabled(true);
  EXPECT_TRUE(CanCreateMenuItem());
  SetMagnifierEnabled(false);
  EXPECT_FALSE(CanCreateMenuItem());

  // Toggling autoclick changes the visibility of the menu.
  AccessibilityManager::Get()->EnableAutoclick(true);
  EXPECT_TRUE(CanCreateMenuItem());
  AccessibilityManager::Get()->EnableAutoclick(false);
  EXPECT_FALSE(CanCreateMenuItem());

  // Toggling virtual keyboard changes the visibility of the menu.
  AccessibilityManager::Get()->EnableVirtualKeyboard(true);
  EXPECT_TRUE(CanCreateMenuItem());
  AccessibilityManager::Get()->EnableVirtualKeyboard(false);
  EXPECT_FALSE(CanCreateMenuItem());

  // Enabling all accessibility features.
  SetMagnifierEnabled(true);
  EXPECT_TRUE(CanCreateMenuItem());
  AccessibilityManager::Get()->EnableHighContrast(true);
  EXPECT_TRUE(CanCreateMenuItem());
  AccessibilityManager::Get()->EnableSpokenFeedback(
      true, ash::A11Y_NOTIFICATION_NONE);
  EXPECT_TRUE(CanCreateMenuItem());
  AccessibilityManager::Get()->EnableAutoclick(true);
  EXPECT_TRUE(CanCreateMenuItem());
  AccessibilityManager::Get()->EnableVirtualKeyboard(true);
  EXPECT_TRUE(CanCreateMenuItem());
  AccessibilityManager::Get()->EnableVirtualKeyboard(false);
  EXPECT_TRUE(CanCreateMenuItem());
  AccessibilityManager::Get()->EnableAutoclick(false);
  EXPECT_TRUE(CanCreateMenuItem());
  AccessibilityManager::Get()->EnableSpokenFeedback(
      false, ash::A11Y_NOTIFICATION_NONE);
  EXPECT_TRUE(CanCreateMenuItem());
  AccessibilityManager::Get()->EnableHighContrast(false);
  EXPECT_TRUE(CanCreateMenuItem());
  SetMagnifierEnabled(false);
  EXPECT_FALSE(CanCreateMenuItem());
}

// http://crbug.com/396318
IN_PROC_BROWSER_TEST_P(TrayAccessibilityTest,
    DISABLED_ShowMenuWithShowMenuOption) {
  // Login
  UserManager::Get()->UserLoggedIn(
      "owner@invalid.domain", "owner@invalid.domain", true);
  UserManager::Get()->SessionStarted();

  SetShowAccessibilityOptionsInSystemTrayMenu(true);

  // Confirms that the menu is visible.
  EXPECT_TRUE(CanCreateMenuItem());

  // The menu remains visible regardless of toggling spoken feedback.
  AccessibilityManager::Get()->EnableSpokenFeedback(
      true, ash::A11Y_NOTIFICATION_NONE);
  EXPECT_TRUE(CanCreateMenuItem());
  AccessibilityManager::Get()->EnableSpokenFeedback(
      false, ash::A11Y_NOTIFICATION_NONE);
  EXPECT_TRUE(CanCreateMenuItem());

  // The menu remains visible regardless of toggling high contrast.
  AccessibilityManager::Get()->EnableHighContrast(true);
  EXPECT_TRUE(CanCreateMenuItem());
  AccessibilityManager::Get()->EnableHighContrast(false);
  EXPECT_TRUE(CanCreateMenuItem());

  // The menu remains visible regardless of toggling screen magnifier.
  SetMagnifierEnabled(true);
  EXPECT_TRUE(CanCreateMenuItem());
  SetMagnifierEnabled(false);
  EXPECT_TRUE(CanCreateMenuItem());

  // The menu remains visible regardless of toggling autoclick.
  AccessibilityManager::Get()->EnableAutoclick(true);
  EXPECT_TRUE(CanCreateMenuItem());
  AccessibilityManager::Get()->EnableAutoclick(false);
  EXPECT_TRUE(CanCreateMenuItem());

  // The menu remains visible regardless of toggling on-screen keyboard.
  AccessibilityManager::Get()->EnableVirtualKeyboard(true);
  EXPECT_TRUE(CanCreateMenuItem());
  AccessibilityManager::Get()->EnableVirtualKeyboard(false);
  EXPECT_TRUE(CanCreateMenuItem());

  // Enabling all accessibility features.
  SetMagnifierEnabled(true);
  EXPECT_TRUE(CanCreateMenuItem());
  AccessibilityManager::Get()->EnableHighContrast(true);
  EXPECT_TRUE(CanCreateMenuItem());
  AccessibilityManager::Get()->EnableSpokenFeedback(
      true, ash::A11Y_NOTIFICATION_NONE);
  EXPECT_TRUE(CanCreateMenuItem());
  AccessibilityManager::Get()->EnableAutoclick(true);
  EXPECT_TRUE(CanCreateMenuItem());
  AccessibilityManager::Get()->EnableVirtualKeyboard(true);
  EXPECT_TRUE(CanCreateMenuItem());
  AccessibilityManager::Get()->EnableVirtualKeyboard(false);
  EXPECT_TRUE(CanCreateMenuItem());
  AccessibilityManager::Get()->EnableAutoclick(false);
  EXPECT_TRUE(CanCreateMenuItem());
  AccessibilityManager::Get()->EnableSpokenFeedback(
      false, ash::A11Y_NOTIFICATION_NONE);
  EXPECT_TRUE(CanCreateMenuItem());
  AccessibilityManager::Get()->EnableHighContrast(false);
  EXPECT_TRUE(CanCreateMenuItem());
  SetMagnifierEnabled(false);
  EXPECT_TRUE(CanCreateMenuItem());

  SetShowAccessibilityOptionsInSystemTrayMenu(false);

  // Confirms that the menu is invisible.
  EXPECT_FALSE(CanCreateMenuItem());
}

IN_PROC_BROWSER_TEST_P(TrayAccessibilityTest, ShowMenuWithShowOnLoginScreen) {
  SetLoginStatus(ash::user::LOGGED_IN_NONE);

  // Confirms that the menu is visible.
  EXPECT_TRUE(CanCreateMenuItem());

  // The menu remains visible regardless of toggling spoken feedback.
  AccessibilityManager::Get()->EnableSpokenFeedback(
      true, ash::A11Y_NOTIFICATION_NONE);
  EXPECT_TRUE(CanCreateMenuItem());
  AccessibilityManager::Get()->EnableSpokenFeedback(
      false, ash::A11Y_NOTIFICATION_NONE);
  EXPECT_TRUE(CanCreateMenuItem());

  // The menu remains visible regardless of toggling high contrast.
  AccessibilityManager::Get()->EnableHighContrast(true);
  EXPECT_TRUE(CanCreateMenuItem());
  AccessibilityManager::Get()->EnableHighContrast(false);
  EXPECT_TRUE(CanCreateMenuItem());

  // The menu remains visible regardless of toggling screen magnifier.
  SetMagnifierEnabled(true);
  EXPECT_TRUE(CanCreateMenuItem());
  SetMagnifierEnabled(false);
  EXPECT_TRUE(CanCreateMenuItem());

  // The menu remains visible regardless of toggling on-screen keyboard.
  AccessibilityManager::Get()->EnableVirtualKeyboard(true);
  EXPECT_TRUE(CanCreateMenuItem());
  AccessibilityManager::Get()->EnableVirtualKeyboard(false);
  EXPECT_TRUE(CanCreateMenuItem());

  // Enabling all accessibility features.
  SetMagnifierEnabled(true);
  EXPECT_TRUE(CanCreateMenuItem());
  AccessibilityManager::Get()->EnableHighContrast(true);
  EXPECT_TRUE(CanCreateMenuItem());
  AccessibilityManager::Get()->EnableSpokenFeedback(
      true, ash::A11Y_NOTIFICATION_NONE);
  EXPECT_TRUE(CanCreateMenuItem());
  AccessibilityManager::Get()->EnableVirtualKeyboard(true);
  EXPECT_TRUE(CanCreateMenuItem());
  AccessibilityManager::Get()->EnableVirtualKeyboard(false);
  EXPECT_TRUE(CanCreateMenuItem());
  AccessibilityManager::Get()->EnableSpokenFeedback(
      false, ash::A11Y_NOTIFICATION_NONE);
  EXPECT_TRUE(CanCreateMenuItem());
  AccessibilityManager::Get()->EnableHighContrast(false);
  EXPECT_TRUE(CanCreateMenuItem());
  SetMagnifierEnabled(false);
  EXPECT_TRUE(CanCreateMenuItem());

  SetShowAccessibilityOptionsInSystemTrayMenu(true);

  // Confirms that the menu remains visible.
  EXPECT_TRUE(CanCreateMenuItem());

  SetShowAccessibilityOptionsInSystemTrayMenu(false);

  // Confirms that the menu remains visible.
  EXPECT_TRUE(CanCreateMenuItem());
}

IN_PROC_BROWSER_TEST_P(TrayAccessibilityTest, ShowNotification) {
  const base::string16 BRAILLE_CONNECTED =
      base::ASCIIToUTF16("Braille display connected.");
  const base::string16 CHROMEVOX_ENABLED = base::ASCIIToUTF16(
      "ChromeVox (spoken feedback) is enabled.\nPress Ctrl+Alt+Z to disable.");
  const base::string16 BRAILLE_CONNECTED_AND_CHROMEVOX_ENABLED(
      BRAILLE_CONNECTED + base::ASCIIToUTF16(" ") + CHROMEVOX_ENABLED);

  EXPECT_FALSE(AccessibilityManager::Get()->IsSpokenFeedbackEnabled());

  // Enabling spoken feedback should show the notification.
  AccessibilityManager::Get()->EnableSpokenFeedback(
      true, ash::A11Y_NOTIFICATION_SHOW);
  EXPECT_EQ(CHROMEVOX_ENABLED, GetNotificationText());

  // Connecting a braille display when spoken feedback is already enabled
  // should only show the message about the braille display.
  SetBrailleConnected(true);
  EXPECT_EQ(BRAILLE_CONNECTED, GetNotificationText());

  // Neither disconnecting a braille display, nor disabling spoken feedback
  // should show any notification.
  SetBrailleConnected(false);
  EXPECT_TRUE(AccessibilityManager::Get()->IsSpokenFeedbackEnabled());
  EXPECT_FALSE(IsNotificationShown());
  AccessibilityManager::Get()->EnableSpokenFeedback(
      false, ash::A11Y_NOTIFICATION_SHOW);
  EXPECT_FALSE(IsNotificationShown());
  EXPECT_FALSE(AccessibilityManager::Get()->IsSpokenFeedbackEnabled());

  // Connecting a braille display should enable spoken feedback and show
  // both messages.
  SetBrailleConnected(true);
  EXPECT_TRUE(AccessibilityManager::Get()->IsSpokenFeedbackEnabled());
  EXPECT_EQ(BRAILLE_CONNECTED_AND_CHROMEVOX_ENABLED, GetNotificationText());
}

IN_PROC_BROWSER_TEST_P(TrayAccessibilityTest, KeepMenuVisibilityOnLockScreen) {
  // Enables high contrast mode.
  AccessibilityManager::Get()->EnableHighContrast(true);
  EXPECT_TRUE(CanCreateMenuItem());

  // Locks the screen.
  SetLoginStatus(ash::user::LOGGED_IN_LOCKED);
  EXPECT_TRUE(CanCreateMenuItem());

  // Disables high contrast mode.
  AccessibilityManager::Get()->EnableHighContrast(false);

  // Confirms that the menu is still visible.
  EXPECT_TRUE(CanCreateMenuItem());
}

#if defined(OS_CHROMEOS)
#define MAYBE_ClickDetailMenu DISABLED_ClickDetailMenu
#else
#define MAYBE_ClickDetailMenu ClickDetailMenu
#endif

IN_PROC_BROWSER_TEST_P(TrayAccessibilityTest, MAYBE_ClickDetailMenu) {
  // Confirms that the check item toggles the spoken feedback.
  EXPECT_FALSE(AccessibilityManager::Get()->IsSpokenFeedbackEnabled());

  EXPECT_TRUE(CreateDetailedMenu());
  ClickSpokenFeedbackOnDetailMenu();
  EXPECT_TRUE(AccessibilityManager::Get()->IsSpokenFeedbackEnabled());

  EXPECT_TRUE(CreateDetailedMenu());
  ClickSpokenFeedbackOnDetailMenu();
  EXPECT_FALSE(AccessibilityManager::Get()->IsSpokenFeedbackEnabled());

  // Confirms that the check item toggles the high contrast.
  EXPECT_FALSE(AccessibilityManager::Get()->IsHighContrastEnabled());

  EXPECT_TRUE(CreateDetailedMenu());
  ClickHighContrastOnDetailMenu();
  EXPECT_TRUE(AccessibilityManager::Get()->IsHighContrastEnabled());

  EXPECT_TRUE(CreateDetailedMenu());
  ClickHighContrastOnDetailMenu();
  EXPECT_FALSE(AccessibilityManager::Get()->IsHighContrastEnabled());

  // Confirms that the check item toggles the magnifier.
  EXPECT_FALSE(AccessibilityManager::Get()->IsHighContrastEnabled());

  EXPECT_FALSE(MagnificationManager::Get()->IsMagnifierEnabled());
  EXPECT_TRUE(CreateDetailedMenu());
  ClickScreenMagnifierOnDetailMenu();
  EXPECT_TRUE(MagnificationManager::Get()->IsMagnifierEnabled());

  EXPECT_TRUE(CreateDetailedMenu());
  ClickScreenMagnifierOnDetailMenu();
  EXPECT_FALSE(MagnificationManager::Get()->IsMagnifierEnabled());

  // Confirms that the check item toggles autoclick.
  EXPECT_FALSE(AccessibilityManager::Get()->IsAutoclickEnabled());

  EXPECT_TRUE(CreateDetailedMenu());
  ClickAutoclickOnDetailMenu();
  EXPECT_TRUE(AccessibilityManager::Get()->IsAutoclickEnabled());

  EXPECT_TRUE(CreateDetailedMenu());
  ClickAutoclickOnDetailMenu();
  EXPECT_FALSE(AccessibilityManager::Get()->IsAutoclickEnabled());

  // Confirms that the check item toggles on-screen keyboard.
  EXPECT_FALSE(AccessibilityManager::Get()->IsVirtualKeyboardEnabled());

  EXPECT_TRUE(CreateDetailedMenu());
  ClickVirtualKeyboardOnDetailMenu();
  EXPECT_TRUE(AccessibilityManager::Get()->IsVirtualKeyboardEnabled());

  EXPECT_TRUE(CreateDetailedMenu());
  ClickVirtualKeyboardOnDetailMenu();
  EXPECT_FALSE(AccessibilityManager::Get()->IsVirtualKeyboardEnabled());
}

IN_PROC_BROWSER_TEST_P(TrayAccessibilityTest, CheckMarksOnDetailMenu) {
  SetLoginStatus(ash::user::LOGGED_IN_NONE);

  // At first, all of the check is unchecked.
  EXPECT_TRUE(CreateDetailedMenu());
  EXPECT_FALSE(IsSpokenFeedbackEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighContrastEnabledOnDetailMenu());
  EXPECT_FALSE(IsScreenMagnifierEnabledOnDetailMenu());
  EXPECT_FALSE(IsLargeCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsAutoclickEnabledOnDetailMenu());
  EXPECT_FALSE(IsVirtualKeyboardEnabledOnDetailMenu());
  CloseDetailMenu();

  // Enabling spoken feedback.
  AccessibilityManager::Get()->EnableSpokenFeedback(
      true, ash::A11Y_NOTIFICATION_NONE);
  EXPECT_TRUE(CreateDetailedMenu());
  EXPECT_TRUE(IsSpokenFeedbackEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighContrastEnabledOnDetailMenu());
  EXPECT_FALSE(IsScreenMagnifierEnabledOnDetailMenu());
  EXPECT_FALSE(IsLargeCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsAutoclickEnabledOnDetailMenu());
  EXPECT_FALSE(IsVirtualKeyboardEnabledOnDetailMenu());
  CloseDetailMenu();

  // Disabling spoken feedback.
  AccessibilityManager::Get()->EnableSpokenFeedback(
      false, ash::A11Y_NOTIFICATION_NONE);
  EXPECT_TRUE(CreateDetailedMenu());
  EXPECT_FALSE(IsSpokenFeedbackEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighContrastEnabledOnDetailMenu());
  EXPECT_FALSE(IsScreenMagnifierEnabledOnDetailMenu());
  EXPECT_FALSE(IsLargeCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsAutoclickEnabledOnDetailMenu());
  EXPECT_FALSE(IsVirtualKeyboardEnabledOnDetailMenu());
  CloseDetailMenu();

  // Enabling high contrast.
  AccessibilityManager::Get()->EnableHighContrast(true);
  EXPECT_TRUE(CreateDetailedMenu());
  EXPECT_FALSE(IsSpokenFeedbackEnabledOnDetailMenu());
  EXPECT_TRUE(IsHighContrastEnabledOnDetailMenu());
  EXPECT_FALSE(IsScreenMagnifierEnabledOnDetailMenu());
  EXPECT_FALSE(IsLargeCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsAutoclickEnabledOnDetailMenu());
  EXPECT_FALSE(IsVirtualKeyboardEnabledOnDetailMenu());
  CloseDetailMenu();

  // Disabling high contrast.
  AccessibilityManager::Get()->EnableHighContrast(false);
  EXPECT_TRUE(CreateDetailedMenu());
  EXPECT_FALSE(IsSpokenFeedbackEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighContrastEnabledOnDetailMenu());
  EXPECT_FALSE(IsScreenMagnifierEnabledOnDetailMenu());
  EXPECT_FALSE(IsLargeCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsAutoclickEnabledOnDetailMenu());
  EXPECT_FALSE(IsVirtualKeyboardEnabledOnDetailMenu());
  CloseDetailMenu();

  // Enabling full screen magnifier.
  SetMagnifierEnabled(true);
  EXPECT_TRUE(CreateDetailedMenu());
  EXPECT_FALSE(IsSpokenFeedbackEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighContrastEnabledOnDetailMenu());
  EXPECT_TRUE(IsScreenMagnifierEnabledOnDetailMenu());
  EXPECT_FALSE(IsLargeCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsAutoclickEnabledOnDetailMenu());
  EXPECT_FALSE(IsVirtualKeyboardEnabledOnDetailMenu());
  CloseDetailMenu();

  // Disabling screen magnifier.
  SetMagnifierEnabled(false);
  EXPECT_TRUE(CreateDetailedMenu());
  EXPECT_FALSE(IsSpokenFeedbackEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighContrastEnabledOnDetailMenu());
  EXPECT_FALSE(IsScreenMagnifierEnabledOnDetailMenu());
  EXPECT_FALSE(IsLargeCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsAutoclickEnabledOnDetailMenu());
  EXPECT_FALSE(IsVirtualKeyboardEnabledOnDetailMenu());
  CloseDetailMenu();

  // Enabling large cursor.
  AccessibilityManager::Get()->EnableLargeCursor(true);
  EXPECT_TRUE(CreateDetailedMenu());
  EXPECT_FALSE(IsSpokenFeedbackEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighContrastEnabledOnDetailMenu());
  EXPECT_FALSE(IsScreenMagnifierEnabledOnDetailMenu());
  EXPECT_TRUE(IsLargeCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsAutoclickEnabledOnDetailMenu());
  EXPECT_FALSE(IsVirtualKeyboardEnabledOnDetailMenu());
  CloseDetailMenu();

  // Disabling large cursor.
  AccessibilityManager::Get()->EnableLargeCursor(false);
  EXPECT_TRUE(CreateDetailedMenu());
  EXPECT_FALSE(IsSpokenFeedbackEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighContrastEnabledOnDetailMenu());
  EXPECT_FALSE(IsScreenMagnifierEnabledOnDetailMenu());
  EXPECT_FALSE(IsLargeCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsAutoclickEnabledOnDetailMenu());
  EXPECT_FALSE(IsVirtualKeyboardEnabledOnDetailMenu());
  CloseDetailMenu();

  // Enable on-screen keyboard.
  AccessibilityManager::Get()->EnableVirtualKeyboard(true);
  EXPECT_TRUE(CreateDetailedMenu());
  EXPECT_FALSE(IsSpokenFeedbackEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighContrastEnabledOnDetailMenu());
  EXPECT_FALSE(IsScreenMagnifierEnabledOnDetailMenu());
  EXPECT_FALSE(IsLargeCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsAutoclickEnabledOnDetailMenu());
  EXPECT_TRUE(IsVirtualKeyboardEnabledOnDetailMenu());
  CloseDetailMenu();

  // Disable on-screen keyboard.
  AccessibilityManager::Get()->EnableVirtualKeyboard(false);
  EXPECT_TRUE(CreateDetailedMenu());
  EXPECT_FALSE(IsSpokenFeedbackEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighContrastEnabledOnDetailMenu());
  EXPECT_FALSE(IsScreenMagnifierEnabledOnDetailMenu());
  EXPECT_FALSE(IsLargeCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsAutoclickEnabledOnDetailMenu());
  EXPECT_FALSE(IsVirtualKeyboardEnabledOnDetailMenu());
  CloseDetailMenu();

  // Enabling all of the a11y features.
  AccessibilityManager::Get()->EnableSpokenFeedback(
      true, ash::A11Y_NOTIFICATION_NONE);
  AccessibilityManager::Get()->EnableHighContrast(true);
  SetMagnifierEnabled(true);
  AccessibilityManager::Get()->EnableLargeCursor(true);
  AccessibilityManager::Get()->EnableVirtualKeyboard(true);
  EXPECT_TRUE(CreateDetailedMenu());
  EXPECT_TRUE(IsSpokenFeedbackEnabledOnDetailMenu());
  EXPECT_TRUE(IsHighContrastEnabledOnDetailMenu());
  EXPECT_TRUE(IsScreenMagnifierEnabledOnDetailMenu());
  EXPECT_TRUE(IsLargeCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsAutoclickEnabledOnDetailMenu());
  EXPECT_TRUE(IsVirtualKeyboardEnabledOnDetailMenu());
  CloseDetailMenu();

  // Disabling all of the a11y features.
  AccessibilityManager::Get()->EnableSpokenFeedback(
      false, ash::A11Y_NOTIFICATION_NONE);
  AccessibilityManager::Get()->EnableHighContrast(false);
  SetMagnifierEnabled(false);
  AccessibilityManager::Get()->EnableLargeCursor(false);
  AccessibilityManager::Get()->EnableVirtualKeyboard(false);
  EXPECT_TRUE(CreateDetailedMenu());
  EXPECT_FALSE(IsSpokenFeedbackEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighContrastEnabledOnDetailMenu());
  EXPECT_FALSE(IsScreenMagnifierEnabledOnDetailMenu());
  EXPECT_FALSE(IsLargeCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsAutoclickEnabledOnDetailMenu());
  EXPECT_FALSE(IsVirtualKeyboardEnabledOnDetailMenu());
  CloseDetailMenu();

  // Autoclick is disabled on login screen.
  SetLoginStatus(ash::user::LOGGED_IN_USER);

  // Enabling autoclick.
  AccessibilityManager::Get()->EnableAutoclick(true);
  EXPECT_TRUE(CreateDetailedMenu());
  EXPECT_FALSE(IsSpokenFeedbackEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighContrastEnabledOnDetailMenu());
  EXPECT_FALSE(IsScreenMagnifierEnabledOnDetailMenu());
  EXPECT_FALSE(IsLargeCursorEnabledOnDetailMenu());
  EXPECT_TRUE(IsAutoclickEnabledOnDetailMenu());
  EXPECT_FALSE(IsVirtualKeyboardEnabledOnDetailMenu());
  CloseDetailMenu();

  // Disabling autoclick.
  AccessibilityManager::Get()->EnableAutoclick(false);
  EXPECT_TRUE(CreateDetailedMenu());
  EXPECT_FALSE(IsSpokenFeedbackEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighContrastEnabledOnDetailMenu());
  EXPECT_FALSE(IsScreenMagnifierEnabledOnDetailMenu());
  EXPECT_FALSE(IsLargeCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsAutoclickEnabledOnDetailMenu());
  EXPECT_FALSE(IsVirtualKeyboardEnabledOnDetailMenu());
  CloseDetailMenu();
}

IN_PROC_BROWSER_TEST_P(TrayAccessibilityTest, CheckMenuVisibilityOnDetailMenu) {
  SetLoginStatus(ash::user::LOGGED_IN_NONE);
  EXPECT_TRUE(CreateDetailedMenu());
  EXPECT_TRUE(IsSpokenFeedbackMenuShownOnDetailMenu());
  EXPECT_TRUE(IsHighContrastMenuShownOnDetailMenu());
  EXPECT_TRUE(IsScreenMagnifierMenuShownOnDetailMenu());
  EXPECT_TRUE(IsLargeCursorMenuShownOnDetailMenu());
  EXPECT_FALSE(IsAutoclickMenuShownOnDetailMenu());
  EXPECT_TRUE(IsVirtualKeyboardMenuShownOnDetailMenu());
  CloseDetailMenu();

  SetLoginStatus(ash::user::LOGGED_IN_USER);
  EXPECT_TRUE(CreateDetailedMenu());
  EXPECT_TRUE(IsSpokenFeedbackMenuShownOnDetailMenu());
  EXPECT_TRUE(IsHighContrastMenuShownOnDetailMenu());
  EXPECT_TRUE(IsScreenMagnifierMenuShownOnDetailMenu());
  EXPECT_FALSE(IsLargeCursorMenuShownOnDetailMenu());
  EXPECT_TRUE(IsAutoclickMenuShownOnDetailMenu());
  EXPECT_TRUE(IsVirtualKeyboardMenuShownOnDetailMenu());
  CloseDetailMenu();

  SetLoginStatus(ash::user::LOGGED_IN_LOCKED);
  EXPECT_TRUE(CreateDetailedMenu());
  EXPECT_TRUE(IsSpokenFeedbackMenuShownOnDetailMenu());
  EXPECT_TRUE(IsHighContrastMenuShownOnDetailMenu());
  EXPECT_TRUE(IsScreenMagnifierMenuShownOnDetailMenu());
  EXPECT_FALSE(IsLargeCursorMenuShownOnDetailMenu());
  EXPECT_TRUE(IsAutoclickMenuShownOnDetailMenu());
  EXPECT_TRUE(IsVirtualKeyboardMenuShownOnDetailMenu());
  CloseDetailMenu();
}

INSTANTIATE_TEST_CASE_P(TrayAccessibilityTestInstance,
                        TrayAccessibilityTest,
                        testing::Values(PREF_SERVICE,
                                        POLICY));

}  // namespace chromeos
