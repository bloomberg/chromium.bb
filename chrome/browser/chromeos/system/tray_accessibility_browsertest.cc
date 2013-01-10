// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/magnifier/magnification_controller.h"
#include "ash/shell.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/tray/tray_views.h"
#include "ash/system/tray_accessibility.h"
#include "ash/system/user/login_status.h"
#include "base/command_line.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/accessibility/accessibility_util.h"
#include "chrome/browser/chromeos/accessibility/magnification_manager.h"
#include "chrome/browser/chromeos/cros/cros_in_process_browser_test.h"
#include "chrome/browser/chromeos/login/helper.h"
#include "chrome/browser/chromeos/login/login_utils.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/login/user_manager_impl.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/widget/widget.h"

namespace chromeos {

namespace {
  ui::MouseEvent& dummyEvent = *((ui::MouseEvent*)0);
}

class TrayAccessibilityTest : public CrosInProcessBrowserTest {
 protected:
  TrayAccessibilityTest() {}
  virtual ~TrayAccessibilityTest() {}
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    command_line->AppendSwitch(switches::kLoginManager);
    command_line->AppendSwitchASCII(switches::kLoginProfile,
                                    TestingProfile::kTestUserProfileDir);
  }

  ash::internal::TrayAccessibility* tray() {
    return ash::Shell::GetInstance()->GetPrimarySystemTray()->
        GetTrayAccessibilityForTest();
  }

  bool IsTrayIconVisible() {
    return tray()->tray_icon_visible_;
  }

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
    tray()->detailed_menu_->ClickedOn(button);
  }

  void ClickHighContrastOnDetailMenu() {
    views::View* button = tray()->detailed_menu_->high_contrast_view_;
    EXPECT_TRUE(button);
    tray()->detailed_menu_->ClickedOn(button);
  }

  void ClickScreenMagnifierOnDetailMenu() {
    views::View* button = tray()->detailed_menu_->screen_magnifier_view_;
    EXPECT_TRUE(button);
    tray()->detailed_menu_->ClickedOn(button);
  }

  bool IsSpokenFeedbackEnabledOnDetailMenu() {
    return tray()->detailed_menu_->spoken_feedback_enabled_;
  }

  bool IsHighContrastEnabledOnDetailMenu() {
    return tray()->detailed_menu_->high_contrast_enabled_;
  }

  bool IsScreenMagnifierEnabledOnDetailMenu() {
    return tray()->detailed_menu_->screen_magnifier_enabled_;
  }
};

IN_PROC_BROWSER_TEST_F(TrayAccessibilityTest, LoginStatus) {
  EXPECT_EQ(ash::user::LOGGED_IN_NONE, GetLoginStatus());

  UserManager::Get()->UserLoggedIn("owner@invalid.domain", true);
  UserManager::Get()->SessionStarted();

  EXPECT_EQ(ash::user::LOGGED_IN_USER, GetLoginStatus());
}

IN_PROC_BROWSER_TEST_F(TrayAccessibilityTest, ShowTrayIcon) {
  SetLoginStatus(ash::user::LOGGED_IN_NONE);

  // Confirms that the icon is invisible before login.
  EXPECT_FALSE(IsTrayIconVisible());

  UserManager::Get()->UserLoggedIn("owner@invalid.domain", true);
  UserManager::Get()->SessionStarted();

  // Confirms that the icon is invisible just after login.
  EXPECT_FALSE(IsTrayIconVisible());

  // Toggling spoken feedback changes the visibillity of the icon.
  accessibility::EnableSpokenFeedback(true, NULL, ash::A11Y_NOTIFICATION_NONE);
  EXPECT_TRUE(IsTrayIconVisible());
  accessibility::EnableSpokenFeedback(false, NULL, ash::A11Y_NOTIFICATION_NONE);
  EXPECT_FALSE(IsTrayIconVisible());

  // Toggling high contrast the visibillity of the icon.
  accessibility::EnableHighContrast(true);
  EXPECT_TRUE(IsTrayIconVisible());
  accessibility::EnableHighContrast(false);
  EXPECT_FALSE(IsTrayIconVisible());

  // Toggling magnifier the visibillity of the icon.
  MagnificationManager::Get()->SetMagnifier(ash::MAGNIFIER_FULL);
  EXPECT_TRUE(IsTrayIconVisible());
  MagnificationManager::Get()->SetMagnifier(ash::MAGNIFIER_OFF);
  EXPECT_FALSE(IsTrayIconVisible());

  // Enabling all accessibility features.
  MagnificationManager::Get()->SetMagnifier(ash::MAGNIFIER_FULL);
  EXPECT_TRUE(IsTrayIconVisible());
  accessibility::EnableHighContrast(true);
  EXPECT_TRUE(IsTrayIconVisible());
  accessibility::EnableSpokenFeedback(true, NULL, ash::A11Y_NOTIFICATION_NONE);
  EXPECT_TRUE(IsTrayIconVisible());
  accessibility::EnableSpokenFeedback(false, NULL, ash::A11Y_NOTIFICATION_NONE);
  EXPECT_TRUE(IsTrayIconVisible());
  accessibility::EnableHighContrast(false);
  EXPECT_TRUE(IsTrayIconVisible());
  MagnificationManager::Get()->SetMagnifier(ash::MAGNIFIER_OFF);
  EXPECT_FALSE(IsTrayIconVisible());

  // Confirms that prefs::kShouldAlwaysShowAccessibilityMenu doesn't affect
  // the icon on the tray.
  Profile* profile = ProfileManager::GetDefaultProfile();
  PrefService* prefs = profile->GetPrefs();
  prefs->SetBoolean(prefs::kShouldAlwaysShowAccessibilityMenu, true);
  prefs->CommitPendingWrite();
  accessibility::EnableHighContrast(true);
  EXPECT_TRUE(IsTrayIconVisible());
  accessibility::EnableHighContrast(false);
  EXPECT_FALSE(IsTrayIconVisible());
}

IN_PROC_BROWSER_TEST_F(TrayAccessibilityTest, ShowMenu) {
  // Login
  UserManager::Get()->UserLoggedIn("owner@invalid.domain", true);
  UserManager::Get()->SessionStarted();

  // Sets prefs::kShouldAlwaysShowAccessibilityMenu = false.
  Profile* profile = ProfileManager::GetDefaultProfile();
  PrefService* prefs = profile->GetPrefs();
  prefs->SetBoolean(prefs::kShouldAlwaysShowAccessibilityMenu, false);
  prefs->CommitPendingWrite();

  // Confirms that the menu is hidden.
  EXPECT_FALSE(CanCreateMenuItem());

  // Toggling spoken feedback changes the visibillity of the menu.
  accessibility::EnableSpokenFeedback(true, NULL, ash::A11Y_NOTIFICATION_NONE);
  EXPECT_TRUE(CanCreateMenuItem());
  accessibility::EnableSpokenFeedback(false, NULL, ash::A11Y_NOTIFICATION_NONE);
  EXPECT_FALSE(CanCreateMenuItem());

  // Toggling high contrast changes the visibillity of the menu.
  accessibility::EnableHighContrast(true);
  EXPECT_TRUE(CanCreateMenuItem());
  accessibility::EnableHighContrast(false);
  EXPECT_FALSE(CanCreateMenuItem());

  // Toggling screen magnifier changes the visibillity of the menu.
  MagnificationManager::Get()->SetMagnifier(ash::MAGNIFIER_FULL);
  EXPECT_TRUE(CanCreateMenuItem());
  MagnificationManager::Get()->SetMagnifier(ash::MAGNIFIER_OFF);
  EXPECT_FALSE(CanCreateMenuItem());

  // Enabling all accessibility features.
  MagnificationManager::Get()->SetMagnifier(ash::MAGNIFIER_FULL);
  EXPECT_TRUE(CanCreateMenuItem());
  accessibility::EnableHighContrast(true);
  EXPECT_TRUE(CanCreateMenuItem());
  accessibility::EnableSpokenFeedback(true, NULL, ash::A11Y_NOTIFICATION_NONE);
  EXPECT_TRUE(CanCreateMenuItem());
  accessibility::EnableSpokenFeedback(false, NULL, ash::A11Y_NOTIFICATION_NONE);
  EXPECT_TRUE(CanCreateMenuItem());
  accessibility::EnableHighContrast(false);
  EXPECT_TRUE(CanCreateMenuItem());
  MagnificationManager::Get()->SetMagnifier(ash::MAGNIFIER_OFF);
  EXPECT_FALSE(CanCreateMenuItem());
}

IN_PROC_BROWSER_TEST_F(TrayAccessibilityTest, ShowMenuWithShowMenuOption) {
  // Login
  UserManager::Get()->UserLoggedIn("owner@invalid.domain", true);
  UserManager::Get()->SessionStarted();

  // Sets prefs::kShouldAlwaysShowAccessibilityMenu = true.
  Profile* profile = ProfileManager::GetDefaultProfile();
  PrefService* prefs = profile->GetPrefs();
  prefs->SetBoolean(prefs::kShouldAlwaysShowAccessibilityMenu, true);
  prefs->CommitPendingWrite();

  // Confirms that the menu is visible.
  EXPECT_TRUE(CanCreateMenuItem());

  // The menu is keeping visible regardless of toggling spoken feedback.
  accessibility::EnableSpokenFeedback(true, NULL, ash::A11Y_NOTIFICATION_NONE);
  EXPECT_TRUE(CanCreateMenuItem());
  accessibility::EnableSpokenFeedback(false, NULL, ash::A11Y_NOTIFICATION_NONE);
  EXPECT_TRUE(CanCreateMenuItem());

  // The menu is keeping visible regardless of toggling high contrast.
  accessibility::EnableHighContrast(true);
  EXPECT_TRUE(CanCreateMenuItem());
  accessibility::EnableHighContrast(false);
  EXPECT_TRUE(CanCreateMenuItem());

  // The menu is keeping visible regardless of toggling screen magnifier.
  MagnificationManager::Get()->SetMagnifier(ash::MAGNIFIER_FULL);
  EXPECT_TRUE(CanCreateMenuItem());
  MagnificationManager::Get()->SetMagnifier(ash::MAGNIFIER_OFF);
  EXPECT_TRUE(CanCreateMenuItem());

  // Enabling all accessibility features.
  MagnificationManager::Get()->SetMagnifier(ash::MAGNIFIER_FULL);
  EXPECT_TRUE(CanCreateMenuItem());
  accessibility::EnableHighContrast(true);
  EXPECT_TRUE(CanCreateMenuItem());
  accessibility::EnableSpokenFeedback(true, NULL, ash::A11Y_NOTIFICATION_NONE);
  EXPECT_TRUE(CanCreateMenuItem());
  accessibility::EnableSpokenFeedback(false, NULL, ash::A11Y_NOTIFICATION_NONE);
  EXPECT_TRUE(CanCreateMenuItem());
  accessibility::EnableHighContrast(false);
  EXPECT_TRUE(CanCreateMenuItem());
  MagnificationManager::Get()->SetMagnifier(ash::MAGNIFIER_OFF);
  EXPECT_TRUE(CanCreateMenuItem());

  // Sets prefs::kShouldAlwaysShowAccessibilityMenu = true.
  prefs->SetBoolean(prefs::kShouldAlwaysShowAccessibilityMenu, false);

  // Confirms that the menu is invisible.
  EXPECT_FALSE(CanCreateMenuItem());
}

IN_PROC_BROWSER_TEST_F(TrayAccessibilityTest, ShowMenuWithShowOnLoginScreen) {
  SetLoginStatus(ash::user::LOGGED_IN_NONE);

  // Confirms that the menu is visible.
  EXPECT_TRUE(CanCreateMenuItem());

  // The menu is keeping visible regardless of toggling spoken feedback.
  accessibility::EnableSpokenFeedback(true, NULL, ash::A11Y_NOTIFICATION_NONE);
  EXPECT_TRUE(CanCreateMenuItem());
  accessibility::EnableSpokenFeedback(false, NULL, ash::A11Y_NOTIFICATION_NONE);
  EXPECT_TRUE(CanCreateMenuItem());

  // The menu is keeping visible regardless of toggling high contrast.
  accessibility::EnableHighContrast(true);
  EXPECT_TRUE(CanCreateMenuItem());
  accessibility::EnableHighContrast(false);
  EXPECT_TRUE(CanCreateMenuItem());

  // The menu is keeping visible regardless of toggling screen magnifier.
  MagnificationManager::Get()->SetMagnifier(ash::MAGNIFIER_FULL);
  EXPECT_TRUE(CanCreateMenuItem());
  MagnificationManager::Get()->SetMagnifier(ash::MAGNIFIER_OFF);
  EXPECT_TRUE(CanCreateMenuItem());

  // Enabling all accessibility features.
  MagnificationManager::Get()->SetMagnifier(ash::MAGNIFIER_FULL);
  EXPECT_TRUE(CanCreateMenuItem());
  accessibility::EnableHighContrast(true);
  EXPECT_TRUE(CanCreateMenuItem());
  accessibility::EnableSpokenFeedback(true, NULL, ash::A11Y_NOTIFICATION_NONE);
  EXPECT_TRUE(CanCreateMenuItem());
  accessibility::EnableSpokenFeedback(false, NULL, ash::A11Y_NOTIFICATION_NONE);
  EXPECT_TRUE(CanCreateMenuItem());
  accessibility::EnableHighContrast(false);
  EXPECT_TRUE(CanCreateMenuItem());
  MagnificationManager::Get()->SetMagnifier(ash::MAGNIFIER_OFF);
  EXPECT_TRUE(CanCreateMenuItem());

  // Sets prefs::kShouldAlwaysShowAccessibilityMenu = true.
  Profile* profile = ProfileManager::GetDefaultProfile();
  PrefService* prefs = profile->GetPrefs();
  prefs->SetBoolean(prefs::kShouldAlwaysShowAccessibilityMenu, true);
  prefs->CommitPendingWrite();

  // Confirms that the menu is keeping visible.
  EXPECT_TRUE(CanCreateMenuItem());

  // Sets prefs::kShouldAlwaysShowAccessibilityMenu = false.
  prefs->SetBoolean(prefs::kShouldAlwaysShowAccessibilityMenu, false);
  prefs->CommitPendingWrite();

  // Confirms that the menu is keeping visible.
  EXPECT_TRUE(CanCreateMenuItem());
}

IN_PROC_BROWSER_TEST_F(TrayAccessibilityTest, ClickDetailMenu) {
  // Confirms that the check item toggles the spoken feedback.
  EXPECT_FALSE(accessibility::IsSpokenFeedbackEnabled());

  EXPECT_TRUE(CreateDetailedMenu());
  ClickSpokenFeedbackOnDetailMenu();
  EXPECT_TRUE(accessibility::IsSpokenFeedbackEnabled());

  EXPECT_TRUE(CreateDetailedMenu());
  ClickSpokenFeedbackOnDetailMenu();
  EXPECT_FALSE(accessibility::IsSpokenFeedbackEnabled());

  // Confirms that the check item toggles the high contrast.
  EXPECT_FALSE(accessibility::IsHighContrastEnabled());

  EXPECT_TRUE(CreateDetailedMenu());
  ClickHighContrastOnDetailMenu();
  EXPECT_TRUE(accessibility::IsHighContrastEnabled());

  EXPECT_TRUE(CreateDetailedMenu());
  ClickHighContrastOnDetailMenu();
  EXPECT_FALSE(accessibility::IsHighContrastEnabled());

  // Confirms that the check item toggles the magnifier.
  EXPECT_FALSE(accessibility::IsHighContrastEnabled());

  EXPECT_EQ(ash::MAGNIFIER_OFF,
            MagnificationManager::Get()->GetMagnifierType());
  EXPECT_TRUE(CreateDetailedMenu());
  ClickScreenMagnifierOnDetailMenu();
  EXPECT_EQ(ash::MAGNIFIER_FULL,
            MagnificationManager::Get()->GetMagnifierType());

  EXPECT_TRUE(CreateDetailedMenu());
  ClickScreenMagnifierOnDetailMenu();
  EXPECT_EQ(ash::MAGNIFIER_OFF,
            MagnificationManager::Get()->GetMagnifierType());
}

IN_PROC_BROWSER_TEST_F(TrayAccessibilityTest, CheckMarksOnDetailMenu) {
  // At first, all of the check is unchecked.
  EXPECT_TRUE(CreateDetailedMenu());
  EXPECT_FALSE(IsSpokenFeedbackEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighContrastEnabledOnDetailMenu());
  EXPECT_FALSE(IsScreenMagnifierEnabledOnDetailMenu());
  CloseDetailMenu();

  // Enabling spoken feedback.
  accessibility::EnableSpokenFeedback(true, NULL, ash::A11Y_NOTIFICATION_NONE);
  EXPECT_TRUE(CreateDetailedMenu());
  EXPECT_TRUE(IsSpokenFeedbackEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighContrastEnabledOnDetailMenu());
  EXPECT_FALSE(IsScreenMagnifierEnabledOnDetailMenu());
  CloseDetailMenu();

  // Disabling spoken feedback.
  accessibility::EnableSpokenFeedback(false, NULL, ash::A11Y_NOTIFICATION_NONE);
  EXPECT_TRUE(CreateDetailedMenu());
  EXPECT_FALSE(IsSpokenFeedbackEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighContrastEnabledOnDetailMenu());
  EXPECT_FALSE(IsScreenMagnifierEnabledOnDetailMenu());
  CloseDetailMenu();

  // Enabling high contrast.
  accessibility::EnableHighContrast(true);
  EXPECT_TRUE(CreateDetailedMenu());
  EXPECT_FALSE(IsSpokenFeedbackEnabledOnDetailMenu());
  EXPECT_TRUE(IsHighContrastEnabledOnDetailMenu());
  EXPECT_FALSE(IsScreenMagnifierEnabledOnDetailMenu());
  CloseDetailMenu();

  // Disabling high contrast.
  accessibility::EnableHighContrast(false);
  EXPECT_TRUE(CreateDetailedMenu());
  EXPECT_FALSE(IsSpokenFeedbackEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighContrastEnabledOnDetailMenu());
  EXPECT_FALSE(IsScreenMagnifierEnabledOnDetailMenu());
  CloseDetailMenu();

  // Enabling full screen magnifier.
  MagnificationManager::Get()->SetMagnifier(ash::MAGNIFIER_FULL);
  EXPECT_TRUE(CreateDetailedMenu());
  EXPECT_FALSE(IsSpokenFeedbackEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighContrastEnabledOnDetailMenu());
  EXPECT_TRUE(IsScreenMagnifierEnabledOnDetailMenu());
  CloseDetailMenu();

  // Disabling screen magnifier.
  MagnificationManager::Get()->SetMagnifier(ash::MAGNIFIER_OFF);
  EXPECT_TRUE(CreateDetailedMenu());
  EXPECT_FALSE(IsSpokenFeedbackEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighContrastEnabledOnDetailMenu());
  EXPECT_FALSE(IsScreenMagnifierEnabledOnDetailMenu());
  CloseDetailMenu();

  // Enabling all of the a11y features.
  accessibility::EnableSpokenFeedback(true, NULL, ash::A11Y_NOTIFICATION_NONE);
  accessibility::EnableHighContrast(true);
  MagnificationManager::Get()->SetMagnifier(ash::MAGNIFIER_FULL);
  EXPECT_TRUE(CreateDetailedMenu());
  EXPECT_TRUE(IsSpokenFeedbackEnabledOnDetailMenu());
  EXPECT_TRUE(IsHighContrastEnabledOnDetailMenu());
  EXPECT_TRUE(IsScreenMagnifierEnabledOnDetailMenu());
  CloseDetailMenu();

  // Disabling all of the a11y features.
  accessibility::EnableSpokenFeedback(false, NULL, ash::A11Y_NOTIFICATION_NONE);
  accessibility::EnableHighContrast(false);
  MagnificationManager::Get()->SetMagnifier(ash::MAGNIFIER_OFF);
  EXPECT_TRUE(CreateDetailedMenu());
  EXPECT_FALSE(IsSpokenFeedbackEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighContrastEnabledOnDetailMenu());
  EXPECT_FALSE(IsScreenMagnifierEnabledOnDetailMenu());
  CloseDetailMenu();
}

}  // namespace chromeos
