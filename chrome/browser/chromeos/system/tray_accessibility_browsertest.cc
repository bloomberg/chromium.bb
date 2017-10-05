// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accessibility/accessibility_controller.h"
#include "ash/accessibility_types.h"
#include "ash/login_status.h"
#include "ash/magnifier/magnification_controller.h"
#include "ash/public/cpp/ash_pref_names.h"
#include "ash/shell.h"
#include "ash/shell_test_api.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/tray/system_tray_test_api.h"
#include "ash/system/tray_accessibility.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/chromeos/accessibility/magnification_manager.h"
#include "chrome/browser/chromeos/login/helper.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/extensions/api/braille_display_private/mock_braille_controller.h"
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
#include "ui/message_center/message_center.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/label.h"
#include "ui/views/widget/widget.h"

using extensions::api::braille_display_private::BrailleObserver;
using extensions::api::braille_display_private::DisplayState;
using extensions::api::braille_display_private::MockBrailleController;
using message_center::MessageCenter;
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

// Simulates how UserSessionManager creates and starts a user session.
void CreateAndStartUserSession(const AccountId& account_id) {
  using session_manager::SessionManager;

  const std::string user_id_hash =
      ProfileHelper::GetUserIdHashByUserIdForTesting(account_id.GetUserEmail());

  SessionManager::Get()->CreateSession(account_id, user_id_hash);
  Profile* profile = ProfileHelper::GetProfileByUserIdHashForTest(user_id_hash);
  ash::Shell::Get()->accessibility_controller()->SetPrefServiceForTest(
      profile->GetPrefs());
  SessionManager::Get()->SessionStarted();
  // Flush to ensure the session state reaches ash and updates login status.
  SessionControllerClient::FlushForTesting();
}

class TrayAccessibilityTest
    : public InProcessBrowserTest,
      public WithParamInterface<PrefSettingMechanism> {
 protected:
  TrayAccessibilityTest() {}
  virtual ~TrayAccessibilityTest() {}

  // The profile which should be used by these tests.
  Profile* GetProfile() { return ProfileManager::GetActiveUserProfile(); }

  void SetUpInProcessBrowserTestFixture() override {
    EXPECT_CALL(provider_, IsInitializationComplete(_))
        .WillRepeatedly(Return(true));
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(&provider_);
    AccessibilityManager::SetBrailleControllerForTest(&braille_controller_);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kLoginManager);
    command_line->AppendSwitchASCII(switches::kLoginProfile,
                                    TestingProfile::kTestUserProfileDir);
  }

  void SetUpOnMainThread() override {
    AccessibilityManager::Get()->SetProfileForTest(GetProfile());
    MagnificationManager::Get()->SetProfileForTest(GetProfile());
    ash::Shell::Get()->accessibility_controller()->SetPrefServiceForTest(
        GetProfile()->GetPrefs());
    // Need to mark oobe completed to show detailed views.
    StartupUtils::MarkOobeCompleted();
  }

  void TearDownOnMainThread() override {
    AccessibilityManager::SetBrailleControllerForTest(nullptr);
  }

  void SetShowAccessibilityOptionsInSystemTrayMenu(bool value) {
    if (GetParam() == PREF_SERVICE) {
      PrefService* prefs = GetProfile()->GetPrefs();
      prefs->SetBoolean(ash::prefs::kShouldAlwaysShowAccessibilityMenu, value);
    } else if (GetParam() == POLICY) {
      policy::PolicyMap policy_map;
      policy_map.Set(policy::key::kShowAccessibilityOptionsInSystemTrayMenu,
                     policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                     policy::POLICY_SOURCE_CLOUD,
                     base::MakeUnique<base::Value>(value), nullptr);
      provider_.UpdateChromePolicy(policy_map);
      base::RunLoop().RunUntilIdle();
    } else {
      FAIL() << "Unknown test parameterization";
    }
  }

  ash::TrayAccessibility* tray() {
    return const_cast<ash::TrayAccessibility*>(
        const_cast<const TrayAccessibilityTest*>(this)->tray());
  }

  const ash::TrayAccessibility* tray() const {
    return ash::SystemTrayTestApi(ash::Shell::Get()->GetPrimarySystemTray())
        .tray_accessibility();
  }

  bool IsTrayIconVisible() const { return tray()->tray_icon_visible_; }

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

  void CloseDetailMenu() {
    ASSERT_TRUE(tray()->detailed_menu_);
    tray()->OnDetailedViewDestroyed();
    EXPECT_FALSE(tray()->detailed_menu_);
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

  void ClickLargeMouseCursorOnDetailMenu() {
    views::View* button = tray()->detailed_menu_->large_cursor_view_;
    ASSERT_TRUE(button);
    tray()->detailed_menu_->OnViewClicked(button);
  }

  void ClickMonoAudioOnDetailMenu() {
    views::View* button = tray()->detailed_menu_->mono_audio_view_;
    ASSERT_TRUE(button);
    tray()->detailed_menu_->OnViewClicked(button);
  }

  void ClickCaretHighlightOnDetailMenu() {
    views::View* button = tray()->detailed_menu_->caret_highlight_view_;
    ASSERT_TRUE(button);
    tray()->detailed_menu_->OnViewClicked(button);
  }

  void ClickHighlightMouseCursorOnDetailMenu() {
    views::View* button = tray()->detailed_menu_->highlight_mouse_cursor_view_;
    ASSERT_TRUE(button);
    tray()->detailed_menu_->OnViewClicked(button);
  }

  void ClickHighlishtKeyboardFocusOnDetailMenu() {
    views::View* button =
        tray()->detailed_menu_->highlight_keyboard_focus_view_;
    ASSERT_TRUE(button);
    tray()->detailed_menu_->OnViewClicked(button);
  }

  void ClickStickyKeysOnDetailMenu() {
    views::View* button = tray()->detailed_menu_->sticky_keys_view_;
    ASSERT_TRUE(button);
    tray()->detailed_menu_->OnViewClicked(button);
  }

  void ClickTapDraggingOnDetailMenu() {
    views::View* button = tray()->detailed_menu_->tap_dragging_view_;
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

  bool IsMonoAudioEnabledOnDetailMenu() const {
    return tray()->detailed_menu_->mono_audio_enabled_;
  }

  bool IsCaretHighlightEnabledOnDetailMenu() const {
    return tray()->detailed_menu_->caret_highlight_enabled_;
  }

  bool IsHighlightMouseCursorEnabledOnDetailMenu() const {
    return tray()->detailed_menu_->highlight_mouse_cursor_enabled_;
  }

  bool IsHighlightKeyboardFocusEnabledOnDetailMenu() const {
    return tray()->detailed_menu_->highlight_keyboard_focus_enabled_;
  }

  bool IsStickyKeysEnabledOnDetailMenu() const {
    return tray()->detailed_menu_->sticky_keys_enabled_;
  }

  bool IsTapDraggingEnabledOnDetailMenu() const {
    return tray()->detailed_menu_->tap_dragging_enabled_;
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

  bool IsMonoAudioMenuShownOnDetailMenu() const {
    return tray()->detailed_menu_->mono_audio_view_;
  }

  bool IsCaretHighlightMenuShownOnDetailMenu() const {
    return tray()->detailed_menu_->caret_highlight_view_;
  }

  bool IsHighlightMouseCursorMenuShownOnDetailMenu() const {
    return tray()->detailed_menu_->highlight_mouse_cursor_view_;
  }

  bool IsHighlightKeyboardFocusMenuShownOnDetailMenu() const {
    return tray()->detailed_menu_->highlight_keyboard_focus_view_;
  }

  bool IsStickyKeysMenuShownOnDetailMenu() const {
    return tray()->detailed_menu_->sticky_keys_view_;
  }

  bool IsTapDraggingMenuShownOnDetailMenu() const {
    return tray()->detailed_menu_->tap_dragging_view_;
  }

  // In material design we show the help button but theme it as disabled if
  // it is not possible to load the help page.
  bool IsHelpAvailableOnDetailMenu() const {
    return tray()->detailed_menu_->help_view_->state() ==
           views::Button::STATE_NORMAL;
  }

  // In material design we show the settings button but theme it as disabled if
  // it is not possible to load the settings page.
  bool IsSettingsAvailableOnDetailMenu() const {
    return tray()->detailed_menu_->settings_view_->state() ==
           views::Button::STATE_NORMAL;
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
  EXPECT_EQ(ash::LoginStatus::NOT_LOGGED_IN, GetLoginStatus());

  CreateAndStartUserSession(AccountId::FromUserEmail("owner@invalid.domain"));

  EXPECT_EQ(ash::LoginStatus::USER, GetLoginStatus());
}

IN_PROC_BROWSER_TEST_P(TrayAccessibilityTest, ShowTrayIcon) {
  SetLoginStatus(ash::LoginStatus::NOT_LOGGED_IN);

  // Confirms that the icon is invisible before login.
  EXPECT_FALSE(IsTrayIconVisible());

  CreateAndStartUserSession(AccountId::FromUserEmail("owner@invalid.domain"));

  // Confirms that the icon is invisible just after login.
  EXPECT_FALSE(IsTrayIconVisible());

  // Toggling spoken feedback changes the visibility of the icon.
  AccessibilityManager::Get()->EnableSpokenFeedback(
      true, ash::A11Y_NOTIFICATION_NONE);
  EXPECT_TRUE(IsTrayIconVisible());
  AccessibilityManager::Get()->EnableSpokenFeedback(
      false, ash::A11Y_NOTIFICATION_NONE);
  EXPECT_FALSE(IsTrayIconVisible());

  // Toggling high contrast changes the visibility of the icon.
  AccessibilityManager::Get()->EnableHighContrast(true);
  EXPECT_TRUE(IsTrayIconVisible());
  AccessibilityManager::Get()->EnableHighContrast(false);
  EXPECT_FALSE(IsTrayIconVisible());

  // Toggling magnifier changes the visibility of the icon.
  SetMagnifierEnabled(true);
  EXPECT_TRUE(IsTrayIconVisible());
  SetMagnifierEnabled(false);
  EXPECT_FALSE(IsTrayIconVisible());

  // Toggling automatic clicks changes the visibility of the icon.
  AccessibilityManager::Get()->EnableAutoclick(true);
  EXPECT_TRUE(IsTrayIconVisible());
  AccessibilityManager::Get()->EnableAutoclick(false);
  EXPECT_FALSE(IsTrayIconVisible());

  // Toggling the virtual keyboard setting changes the visibility of the a11y
  // icon.
  AccessibilityManager::Get()->EnableVirtualKeyboard(true);
  EXPECT_TRUE(IsTrayIconVisible());
  AccessibilityManager::Get()->EnableVirtualKeyboard(false);
  EXPECT_FALSE(IsTrayIconVisible());

  // Toggling large cursor changes the visibility of the icon.
  AccessibilityManager::Get()->EnableLargeCursor(true);
  EXPECT_TRUE(IsTrayIconVisible());
  AccessibilityManager::Get()->EnableLargeCursor(false);
  EXPECT_FALSE(IsTrayIconVisible());

  // Toggling mono audio changes the visibility of the icon.
  AccessibilityManager::Get()->EnableMonoAudio(true);
  EXPECT_TRUE(IsTrayIconVisible());
  AccessibilityManager::Get()->EnableMonoAudio(false);
  EXPECT_FALSE(IsTrayIconVisible());

  // Toggling caret highlight changes the visibility of the icon.
  AccessibilityManager::Get()->SetCaretHighlightEnabled(true);
  EXPECT_TRUE(IsTrayIconVisible());
  AccessibilityManager::Get()->SetCaretHighlightEnabled(false);
  EXPECT_FALSE(IsTrayIconVisible());

  // Toggling highlight mouse cursor changes the visibility of the icon.
  AccessibilityManager::Get()->SetCursorHighlightEnabled(true);
  EXPECT_TRUE(IsTrayIconVisible());
  AccessibilityManager::Get()->SetCursorHighlightEnabled(false);
  EXPECT_FALSE(IsTrayIconVisible());

  // Toggling highlight keyboard focus changes the visibility of the icon.
  AccessibilityManager::Get()->SetFocusHighlightEnabled(true);
  EXPECT_TRUE(IsTrayIconVisible());
  AccessibilityManager::Get()->SetFocusHighlightEnabled(false);
  EXPECT_FALSE(IsTrayIconVisible());

  // Toggling sticky keys changes the visibility of the icon.
  AccessibilityManager::Get()->EnableStickyKeys(true);
  EXPECT_TRUE(IsTrayIconVisible());
  AccessibilityManager::Get()->EnableStickyKeys(false);
  EXPECT_FALSE(IsTrayIconVisible());

  // Toggling tap dragging changes the visibility of the icon.
  AccessibilityManager::Get()->EnableTapDragging(true);
  EXPECT_TRUE(IsTrayIconVisible());
  AccessibilityManager::Get()->EnableTapDragging(false);
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
  AccessibilityManager::Get()->EnableLargeCursor(true);
  EXPECT_TRUE(IsTrayIconVisible());
  AccessibilityManager::Get()->EnableMonoAudio(true);
  EXPECT_TRUE(IsTrayIconVisible());
  AccessibilityManager::Get()->SetCaretHighlightEnabled(true);
  EXPECT_TRUE(IsTrayIconVisible());
  AccessibilityManager::Get()->SetCursorHighlightEnabled(true);
  EXPECT_TRUE(IsTrayIconVisible());
  AccessibilityManager::Get()->SetFocusHighlightEnabled(true);
  EXPECT_TRUE(IsTrayIconVisible());
  AccessibilityManager::Get()->EnableStickyKeys(true);
  EXPECT_TRUE(IsTrayIconVisible());
  AccessibilityManager::Get()->EnableTapDragging(true);
  EXPECT_TRUE(IsTrayIconVisible());
  AccessibilityManager::Get()->EnableSpokenFeedback(
      false, ash::A11Y_NOTIFICATION_NONE);
  EXPECT_TRUE(IsTrayIconVisible());
  AccessibilityManager::Get()->EnableHighContrast(false);
  EXPECT_TRUE(IsTrayIconVisible());
  SetMagnifierEnabled(false);
  EXPECT_TRUE(IsTrayIconVisible());
  AccessibilityManager::Get()->EnableVirtualKeyboard(false);
  EXPECT_TRUE(IsTrayIconVisible());
  AccessibilityManager::Get()->EnableLargeCursor(false);
  EXPECT_TRUE(IsTrayIconVisible());
  AccessibilityManager::Get()->EnableMonoAudio(false);
  EXPECT_TRUE(IsTrayIconVisible());
  AccessibilityManager::Get()->SetCaretHighlightEnabled(false);
  EXPECT_TRUE(IsTrayIconVisible());
  AccessibilityManager::Get()->SetCursorHighlightEnabled(false);
  EXPECT_TRUE(IsTrayIconVisible());
  AccessibilityManager::Get()->SetFocusHighlightEnabled(false);
  EXPECT_TRUE(IsTrayIconVisible());
  AccessibilityManager::Get()->EnableStickyKeys(false);
  EXPECT_TRUE(IsTrayIconVisible());
  AccessibilityManager::Get()->EnableTapDragging(false);
  EXPECT_FALSE(IsTrayIconVisible());

  // Confirms that ash::prefs::kShouldAlwaysShowAccessibilityMenu doesn't affect
  // the icon on the tray.
  SetShowAccessibilityOptionsInSystemTrayMenu(true);
  AccessibilityManager::Get()->EnableHighContrast(true);
  EXPECT_TRUE(IsTrayIconVisible());
  AccessibilityManager::Get()->EnableHighContrast(false);
  EXPECT_FALSE(IsTrayIconVisible());
}

IN_PROC_BROWSER_TEST_P(TrayAccessibilityTest, ShowMenu) {
  // Login
  CreateAndStartUserSession(AccountId::FromUserEmail("owner@invalid.domain"));

  SetShowAccessibilityOptionsInSystemTrayMenu(false);

  // Confirms that the menu is hidden.
  EXPECT_FALSE(CanCreateMenuItem());

  // Toggling spoken feedback changes the visibility of the menu.
  AccessibilityManager::Get()->EnableSpokenFeedback(
      true, ash::A11Y_NOTIFICATION_NONE);
  EXPECT_TRUE(CanCreateMenuItem());
  AccessibilityManager::Get()->EnableSpokenFeedback(
      false, ash::A11Y_NOTIFICATION_NONE);
  EXPECT_FALSE(CanCreateMenuItem());

  // Toggling high contrast changes the visibility of the menu.
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

  // Toggling large mouse cursor changes the visibility of the menu.
  AccessibilityManager::Get()->EnableLargeCursor(true);
  EXPECT_TRUE(CanCreateMenuItem());
  AccessibilityManager::Get()->EnableLargeCursor(false);
  EXPECT_FALSE(CanCreateMenuItem());

  // Toggling mono audio changes the visibility of the menu.
  AccessibilityManager::Get()->EnableMonoAudio(true);
  EXPECT_TRUE(CanCreateMenuItem());
  AccessibilityManager::Get()->EnableMonoAudio(false);
  EXPECT_FALSE(CanCreateMenuItem());

  // Toggling caret highlight changes the visibility of the menu.
  AccessibilityManager::Get()->SetCaretHighlightEnabled(true);
  EXPECT_TRUE(CanCreateMenuItem());
  AccessibilityManager::Get()->SetCaretHighlightEnabled(false);
  EXPECT_FALSE(CanCreateMenuItem());

  // Toggling highlight mouse cursor changes the visibility of the menu.
  AccessibilityManager::Get()->SetCursorHighlightEnabled(true);
  EXPECT_TRUE(CanCreateMenuItem());
  AccessibilityManager::Get()->SetCursorHighlightEnabled(false);
  EXPECT_FALSE(CanCreateMenuItem());

  // Toggling highlight keyboard focus changes the visibility of the menu.
  AccessibilityManager::Get()->SetFocusHighlightEnabled(true);
  EXPECT_TRUE(CanCreateMenuItem());
  AccessibilityManager::Get()->SetFocusHighlightEnabled(false);
  EXPECT_FALSE(CanCreateMenuItem());

  // Toggling sticky keys changes the visibility of the menu.
  AccessibilityManager::Get()->EnableStickyKeys(true);
  EXPECT_TRUE(CanCreateMenuItem());
  AccessibilityManager::Get()->EnableStickyKeys(false);
  EXPECT_FALSE(CanCreateMenuItem());

  // Toggling tap dragging changes the visibility of the menu.
  AccessibilityManager::Get()->EnableTapDragging(true);
  EXPECT_TRUE(CanCreateMenuItem());
  AccessibilityManager::Get()->EnableTapDragging(false);
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
  AccessibilityManager::Get()->EnableLargeCursor(true);
  EXPECT_TRUE(CanCreateMenuItem());
  AccessibilityManager::Get()->EnableMonoAudio(true);
  EXPECT_TRUE(CanCreateMenuItem());
  AccessibilityManager::Get()->SetCaretHighlightEnabled(true);
  EXPECT_TRUE(CanCreateMenuItem());
  AccessibilityManager::Get()->SetCursorHighlightEnabled(true);
  EXPECT_TRUE(CanCreateMenuItem());
  AccessibilityManager::Get()->SetFocusHighlightEnabled(true);
  EXPECT_TRUE(CanCreateMenuItem());
  AccessibilityManager::Get()->EnableStickyKeys(true);
  EXPECT_TRUE(CanCreateMenuItem());
  AccessibilityManager::Get()->EnableTapDragging(true);
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
  AccessibilityManager::Get()->EnableLargeCursor(false);
  EXPECT_TRUE(CanCreateMenuItem());
  AccessibilityManager::Get()->EnableMonoAudio(false);
  EXPECT_TRUE(CanCreateMenuItem());
  AccessibilityManager::Get()->SetCaretHighlightEnabled(false);
  EXPECT_TRUE(CanCreateMenuItem());
  AccessibilityManager::Get()->SetCursorHighlightEnabled(false);
  EXPECT_TRUE(CanCreateMenuItem());
  AccessibilityManager::Get()->SetFocusHighlightEnabled(false);
  EXPECT_TRUE(CanCreateMenuItem());
  AccessibilityManager::Get()->EnableStickyKeys(false);
  EXPECT_TRUE(CanCreateMenuItem());
  AccessibilityManager::Get()->EnableTapDragging(false);
  EXPECT_FALSE(CanCreateMenuItem());
}

IN_PROC_BROWSER_TEST_P(TrayAccessibilityTest, ShowMenuWithShowMenuOption) {
  // Login
  CreateAndStartUserSession(AccountId::FromUserEmail("owner@invalid.domain"));

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

  // The menu remains visible regardless of toggling large mouse cursor.
  AccessibilityManager::Get()->EnableLargeCursor(true);
  EXPECT_TRUE(CanCreateMenuItem());
  AccessibilityManager::Get()->EnableLargeCursor(false);
  EXPECT_TRUE(CanCreateMenuItem());

  // The menu remains visible regardless of toggling mono audio.
  AccessibilityManager::Get()->EnableMonoAudio(true);
  EXPECT_TRUE(CanCreateMenuItem());
  AccessibilityManager::Get()->EnableMonoAudio(false);
  EXPECT_TRUE(CanCreateMenuItem());

  // The menu remains visible regardless of toggling caret highlight.
  AccessibilityManager::Get()->SetCaretHighlightEnabled(true);
  EXPECT_TRUE(CanCreateMenuItem());
  AccessibilityManager::Get()->SetCaretHighlightEnabled(false);
  EXPECT_TRUE(CanCreateMenuItem());

  // The menu remains visible regardless of toggling highlight mouse cursor.
  AccessibilityManager::Get()->SetCursorHighlightEnabled(true);
  EXPECT_TRUE(CanCreateMenuItem());
  AccessibilityManager::Get()->SetCursorHighlightEnabled(false);
  EXPECT_TRUE(CanCreateMenuItem());

  // The menu remains visible regardless of toggling highlight keyboard focus.
  AccessibilityManager::Get()->SetFocusHighlightEnabled(true);
  EXPECT_TRUE(CanCreateMenuItem());
  AccessibilityManager::Get()->SetFocusHighlightEnabled(false);
  EXPECT_TRUE(CanCreateMenuItem());

  // The menu remains visible regardless of the toggling sticky keys.
  AccessibilityManager::Get()->EnableStickyKeys(true);
  EXPECT_TRUE(CanCreateMenuItem());
  AccessibilityManager::Get()->EnableStickyKeys(false);
  EXPECT_TRUE(CanCreateMenuItem());

  // The menu remains visible regardless of the toggling tap dragging.
  AccessibilityManager::Get()->EnableTapDragging(true);
  EXPECT_TRUE(CanCreateMenuItem());
  AccessibilityManager::Get()->EnableTapDragging(false);
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
  AccessibilityManager::Get()->EnableLargeCursor(true);
  EXPECT_TRUE(CanCreateMenuItem());
  AccessibilityManager::Get()->EnableMonoAudio(true);
  EXPECT_TRUE(CanCreateMenuItem());
  AccessibilityManager::Get()->SetCaretHighlightEnabled(true);
  EXPECT_TRUE(CanCreateMenuItem());
  AccessibilityManager::Get()->SetCursorHighlightEnabled(true);
  EXPECT_TRUE(CanCreateMenuItem());
  AccessibilityManager::Get()->SetFocusHighlightEnabled(true);
  EXPECT_TRUE(CanCreateMenuItem());
  AccessibilityManager::Get()->EnableStickyKeys(true);
  EXPECT_TRUE(CanCreateMenuItem());
  AccessibilityManager::Get()->EnableTapDragging(true);
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
  AccessibilityManager::Get()->EnableLargeCursor(false);
  EXPECT_TRUE(CanCreateMenuItem());
  AccessibilityManager::Get()->EnableMonoAudio(false);
  EXPECT_TRUE(CanCreateMenuItem());
  AccessibilityManager::Get()->SetCaretHighlightEnabled(false);
  EXPECT_TRUE(CanCreateMenuItem());
  AccessibilityManager::Get()->SetCursorHighlightEnabled(false);
  EXPECT_TRUE(CanCreateMenuItem());
  AccessibilityManager::Get()->SetFocusHighlightEnabled(false);
  EXPECT_TRUE(CanCreateMenuItem());
  AccessibilityManager::Get()->EnableStickyKeys(false);
  EXPECT_TRUE(CanCreateMenuItem());
  AccessibilityManager::Get()->EnableTapDragging(false);
  EXPECT_TRUE(CanCreateMenuItem());

  SetShowAccessibilityOptionsInSystemTrayMenu(false);

  // Confirms that the menu is invisible.
  EXPECT_FALSE(CanCreateMenuItem());
}

IN_PROC_BROWSER_TEST_P(TrayAccessibilityTest, ShowMenuWithShowOnLoginScreen) {
  SetLoginStatus(ash::LoginStatus::NOT_LOGGED_IN);

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

  // The menu remains visible regardless of toggling large mouse cursor.
  AccessibilityManager::Get()->EnableLargeCursor(true);
  EXPECT_TRUE(CanCreateMenuItem());
  AccessibilityManager::Get()->EnableLargeCursor(false);
  EXPECT_TRUE(CanCreateMenuItem());

  // The menu remains visible regardless of toggling mono audio.
  AccessibilityManager::Get()->EnableMonoAudio(true);
  EXPECT_TRUE(CanCreateMenuItem());
  AccessibilityManager::Get()->EnableMonoAudio(false);
  EXPECT_TRUE(CanCreateMenuItem());

  // The menu remains visible regardless of toggling caret highlight.
  AccessibilityManager::Get()->SetCaretHighlightEnabled(true);
  EXPECT_TRUE(CanCreateMenuItem());
  AccessibilityManager::Get()->SetCaretHighlightEnabled(false);
  EXPECT_TRUE(CanCreateMenuItem());

  // The menu remains visible regardless of toggling highlight mouse cursor.
  AccessibilityManager::Get()->SetCursorHighlightEnabled(true);
  EXPECT_TRUE(CanCreateMenuItem());
  AccessibilityManager::Get()->SetCursorHighlightEnabled(false);
  EXPECT_TRUE(CanCreateMenuItem());

  // The menu remains visible regardless of toggling highlight keyboard focus.
  AccessibilityManager::Get()->SetFocusHighlightEnabled(true);
  EXPECT_TRUE(CanCreateMenuItem());
  AccessibilityManager::Get()->SetFocusHighlightEnabled(false);
  EXPECT_TRUE(CanCreateMenuItem());

  // The menu remains visible regardless of toggling sticky keys.
  AccessibilityManager::Get()->EnableStickyKeys(true);
  EXPECT_TRUE(CanCreateMenuItem());
  AccessibilityManager::Get()->EnableStickyKeys(false);
  EXPECT_TRUE(CanCreateMenuItem());

  // The menu remains visible regardless of toggling tap dragging.
  AccessibilityManager::Get()->EnableTapDragging(true);
  EXPECT_TRUE(CanCreateMenuItem());
  AccessibilityManager::Get()->EnableTapDragging(false);
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
  AccessibilityManager::Get()->EnableLargeCursor(true);
  EXPECT_TRUE(CanCreateMenuItem());
  AccessibilityManager::Get()->EnableMonoAudio(true);
  EXPECT_TRUE(CanCreateMenuItem());
  AccessibilityManager::Get()->SetCaretHighlightEnabled(true);
  EXPECT_TRUE(CanCreateMenuItem());
  AccessibilityManager::Get()->SetCursorHighlightEnabled(true);
  EXPECT_TRUE(CanCreateMenuItem());
  AccessibilityManager::Get()->SetFocusHighlightEnabled(true);
  EXPECT_TRUE(CanCreateMenuItem());
  AccessibilityManager::Get()->EnableStickyKeys(true);
  EXPECT_TRUE(CanCreateMenuItem());
  AccessibilityManager::Get()->EnableTapDragging(true);
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
  AccessibilityManager::Get()->EnableLargeCursor(false);
  EXPECT_TRUE(CanCreateMenuItem());
  AccessibilityManager::Get()->EnableMonoAudio(false);
  EXPECT_TRUE(CanCreateMenuItem());
  AccessibilityManager::Get()->SetCaretHighlightEnabled(false);
  EXPECT_TRUE(CanCreateMenuItem());
  AccessibilityManager::Get()->SetCursorHighlightEnabled(false);
  EXPECT_TRUE(CanCreateMenuItem());
  AccessibilityManager::Get()->SetFocusHighlightEnabled(false);
  EXPECT_TRUE(CanCreateMenuItem());
  AccessibilityManager::Get()->EnableStickyKeys(false);
  EXPECT_TRUE(CanCreateMenuItem());
  AccessibilityManager::Get()->EnableTapDragging(false);
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
  const base::string16 CHROMEVOX_ENABLED_TITLE =
      base::ASCIIToUTF16("ChromeVox enabled");
  const base::string16 CHROMEVOX_ENABLED =
      base::ASCIIToUTF16("Press Ctrl + Alt + Z to disable spoken feedback.");
  const base::string16 BRAILLE_CONNECTED_AND_CHROMEVOX_ENABLED_TITLE =
      base::ASCIIToUTF16("Braille and ChromeVox are enabled");

  EXPECT_FALSE(AccessibilityManager::Get()->IsSpokenFeedbackEnabled());

  // Enabling spoken feedback should show the notification.
  AccessibilityManager::Get()->EnableSpokenFeedback(
      true, ash::A11Y_NOTIFICATION_SHOW);
  message_center::NotificationList::Notifications notifications =
      MessageCenter::Get()->GetVisibleNotifications();
  EXPECT_EQ(1u, notifications.size());
  EXPECT_EQ(CHROMEVOX_ENABLED_TITLE, (*notifications.begin())->title());
  EXPECT_EQ(CHROMEVOX_ENABLED, (*notifications.begin())->message());

  // Connecting a braille display when spoken feedback is already enabled
  // should only show the message about the braille display.
  SetBrailleConnected(true);
  notifications = MessageCenter::Get()->GetVisibleNotifications();
  EXPECT_EQ(1u, notifications.size());
  EXPECT_EQ(base::string16(), (*notifications.begin())->title());
  EXPECT_EQ(BRAILLE_CONNECTED, (*notifications.begin())->message());

  // Neither disconnecting a braille display, nor disabling spoken feedback
  // should show any notification.
  SetBrailleConnected(false);
  EXPECT_TRUE(AccessibilityManager::Get()->IsSpokenFeedbackEnabled());
  notifications = MessageCenter::Get()->GetVisibleNotifications();
  EXPECT_EQ(0u, notifications.size());
  AccessibilityManager::Get()->EnableSpokenFeedback(
      false, ash::A11Y_NOTIFICATION_SHOW);
  notifications = MessageCenter::Get()->GetVisibleNotifications();
  EXPECT_EQ(0u, notifications.size());
  EXPECT_FALSE(AccessibilityManager::Get()->IsSpokenFeedbackEnabled());

  // Connecting a braille display should enable spoken feedback and show
  // both messages.
  SetBrailleConnected(true);
  EXPECT_TRUE(AccessibilityManager::Get()->IsSpokenFeedbackEnabled());
  notifications = MessageCenter::Get()->GetVisibleNotifications();
  EXPECT_EQ(BRAILLE_CONNECTED_AND_CHROMEVOX_ENABLED_TITLE,
            (*notifications.begin())->title());
  EXPECT_EQ(CHROMEVOX_ENABLED, (*notifications.begin())->message());
}

IN_PROC_BROWSER_TEST_P(TrayAccessibilityTest, KeepMenuVisibilityOnLockScreen) {
  // Enables high contrast mode.
  AccessibilityManager::Get()->EnableHighContrast(true);
  EXPECT_TRUE(CanCreateMenuItem());

  // Locks the screen.
  SetLoginStatus(ash::LoginStatus::LOCKED);
  EXPECT_TRUE(CanCreateMenuItem());

  // Disables high contrast mode.
  AccessibilityManager::Get()->EnableHighContrast(false);

  // Confirms that the menu is still visible.
  EXPECT_TRUE(CanCreateMenuItem());
}

IN_PROC_BROWSER_TEST_P(TrayAccessibilityTest, ClickDetailMenu) {
  SetLoginStatus(ash::LoginStatus::USER);

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

  // Confirms that the check item toggles large mouse cursor.
  EXPECT_FALSE(AccessibilityManager::Get()->IsLargeCursorEnabled());

  EXPECT_TRUE(CreateDetailedMenu());
  ClickLargeMouseCursorOnDetailMenu();
  EXPECT_TRUE(AccessibilityManager::Get()->IsLargeCursorEnabled());

  EXPECT_TRUE(CreateDetailedMenu());
  ClickLargeMouseCursorOnDetailMenu();
  EXPECT_FALSE(AccessibilityManager::Get()->IsLargeCursorEnabled());

  // Confirms that the check item toggles mono audio.
  EXPECT_FALSE(AccessibilityManager::Get()->IsMonoAudioEnabled());

  EXPECT_TRUE(CreateDetailedMenu());
  ClickMonoAudioOnDetailMenu();
  EXPECT_TRUE(AccessibilityManager::Get()->IsMonoAudioEnabled());

  EXPECT_TRUE(CreateDetailedMenu());
  ClickMonoAudioOnDetailMenu();
  EXPECT_FALSE(AccessibilityManager::Get()->IsMonoAudioEnabled());

  // Confirms that the check item toggles caret highlight.
  EXPECT_FALSE(AccessibilityManager::Get()->IsCaretHighlightEnabled());

  EXPECT_TRUE(CreateDetailedMenu());
  ClickCaretHighlightOnDetailMenu();
  EXPECT_TRUE(AccessibilityManager::Get()->IsCaretHighlightEnabled());

  EXPECT_TRUE(CreateDetailedMenu());
  ClickCaretHighlightOnDetailMenu();
  EXPECT_FALSE(AccessibilityManager::Get()->IsCaretHighlightEnabled());

  // Confirms that the check item toggles highlight mouse cursor.
  EXPECT_FALSE(AccessibilityManager::Get()->IsCursorHighlightEnabled());

  EXPECT_TRUE(CreateDetailedMenu());
  ClickHighlightMouseCursorOnDetailMenu();
  EXPECT_TRUE(AccessibilityManager::Get()->IsCursorHighlightEnabled());

  EXPECT_TRUE(CreateDetailedMenu());
  ClickHighlightMouseCursorOnDetailMenu();
  EXPECT_FALSE(AccessibilityManager::Get()->IsCursorHighlightEnabled());

  // Confirms that the check item toggles highlight keyboard focus.
  EXPECT_FALSE(AccessibilityManager::Get()->IsFocusHighlightEnabled());

  EXPECT_TRUE(CreateDetailedMenu());
  ClickHighlishtKeyboardFocusOnDetailMenu();
  EXPECT_TRUE(AccessibilityManager::Get()->IsFocusHighlightEnabled());

  EXPECT_TRUE(CreateDetailedMenu());
  ClickHighlishtKeyboardFocusOnDetailMenu();
  EXPECT_FALSE(AccessibilityManager::Get()->IsFocusHighlightEnabled());

  // Confirms that the check item toggles sticky keys.
  EXPECT_FALSE(AccessibilityManager::Get()->IsStickyKeysEnabled());

  EXPECT_TRUE(CreateDetailedMenu());
  ClickStickyKeysOnDetailMenu();
  EXPECT_TRUE(AccessibilityManager::Get()->IsStickyKeysEnabled());

  EXPECT_TRUE(CreateDetailedMenu());
  ClickStickyKeysOnDetailMenu();
  EXPECT_FALSE(AccessibilityManager::Get()->IsStickyKeysEnabled());

  // Confirms that the check item toggles tap dragging.
  EXPECT_FALSE(AccessibilityManager::Get()->IsTapDraggingEnabled());

  EXPECT_TRUE(CreateDetailedMenu());
  ClickTapDraggingOnDetailMenu();
  EXPECT_TRUE(AccessibilityManager::Get()->IsTapDraggingEnabled());

  EXPECT_TRUE(CreateDetailedMenu());
  ClickTapDraggingOnDetailMenu();
  EXPECT_FALSE(AccessibilityManager::Get()->IsTapDraggingEnabled());
}

IN_PROC_BROWSER_TEST_P(TrayAccessibilityTest, CheckMarksOnDetailMenu) {
  SetLoginStatus(ash::LoginStatus::NOT_LOGGED_IN);

  // At first, all of the check is unchecked.
  EXPECT_TRUE(CreateDetailedMenu());
  EXPECT_FALSE(IsSpokenFeedbackEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighContrastEnabledOnDetailMenu());
  EXPECT_FALSE(IsScreenMagnifierEnabledOnDetailMenu());
  EXPECT_FALSE(IsLargeCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsAutoclickEnabledOnDetailMenu());
  EXPECT_FALSE(IsVirtualKeyboardEnabledOnDetailMenu());
  EXPECT_FALSE(IsMonoAudioEnabledOnDetailMenu());
  EXPECT_FALSE(IsCaretHighlightEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightMouseCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightKeyboardFocusEnabledOnDetailMenu());
  EXPECT_FALSE(IsStickyKeysEnabledOnDetailMenu());
  EXPECT_FALSE(IsTapDraggingEnabledOnDetailMenu());
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
  EXPECT_FALSE(IsMonoAudioEnabledOnDetailMenu());
  EXPECT_FALSE(IsCaretHighlightEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightMouseCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightKeyboardFocusEnabledOnDetailMenu());
  EXPECT_FALSE(IsStickyKeysEnabledOnDetailMenu());
  EXPECT_FALSE(IsTapDraggingEnabledOnDetailMenu());
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
  EXPECT_FALSE(IsMonoAudioEnabledOnDetailMenu());
  EXPECT_FALSE(IsCaretHighlightEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightMouseCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightKeyboardFocusEnabledOnDetailMenu());
  EXPECT_FALSE(IsStickyKeysEnabledOnDetailMenu());
  EXPECT_FALSE(IsTapDraggingEnabledOnDetailMenu());
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
  EXPECT_FALSE(IsMonoAudioEnabledOnDetailMenu());
  EXPECT_FALSE(IsCaretHighlightEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightMouseCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightKeyboardFocusEnabledOnDetailMenu());
  EXPECT_FALSE(IsStickyKeysEnabledOnDetailMenu());
  EXPECT_FALSE(IsTapDraggingEnabledOnDetailMenu());
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
  EXPECT_FALSE(IsMonoAudioEnabledOnDetailMenu());
  EXPECT_FALSE(IsCaretHighlightEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightMouseCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightKeyboardFocusEnabledOnDetailMenu());
  EXPECT_FALSE(IsStickyKeysEnabledOnDetailMenu());
  EXPECT_FALSE(IsTapDraggingEnabledOnDetailMenu());
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
  EXPECT_FALSE(IsMonoAudioEnabledOnDetailMenu());
  EXPECT_FALSE(IsCaretHighlightEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightMouseCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightKeyboardFocusEnabledOnDetailMenu());
  EXPECT_FALSE(IsStickyKeysEnabledOnDetailMenu());
  EXPECT_FALSE(IsTapDraggingEnabledOnDetailMenu());
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
  EXPECT_FALSE(IsMonoAudioEnabledOnDetailMenu());
  EXPECT_FALSE(IsCaretHighlightEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightMouseCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightKeyboardFocusEnabledOnDetailMenu());
  EXPECT_FALSE(IsStickyKeysEnabledOnDetailMenu());
  EXPECT_FALSE(IsTapDraggingEnabledOnDetailMenu());
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
  EXPECT_FALSE(IsMonoAudioEnabledOnDetailMenu());
  EXPECT_FALSE(IsCaretHighlightEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightMouseCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightKeyboardFocusEnabledOnDetailMenu());
  EXPECT_FALSE(IsStickyKeysEnabledOnDetailMenu());
  EXPECT_FALSE(IsTapDraggingEnabledOnDetailMenu());
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
  EXPECT_FALSE(IsMonoAudioEnabledOnDetailMenu());
  EXPECT_FALSE(IsCaretHighlightEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightMouseCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightKeyboardFocusEnabledOnDetailMenu());
  EXPECT_FALSE(IsStickyKeysEnabledOnDetailMenu());
  EXPECT_FALSE(IsTapDraggingEnabledOnDetailMenu());
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
  EXPECT_FALSE(IsMonoAudioEnabledOnDetailMenu());
  EXPECT_FALSE(IsCaretHighlightEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightMouseCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightKeyboardFocusEnabledOnDetailMenu());
  EXPECT_FALSE(IsStickyKeysEnabledOnDetailMenu());
  EXPECT_FALSE(IsTapDraggingEnabledOnDetailMenu());
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
  EXPECT_FALSE(IsMonoAudioEnabledOnDetailMenu());
  EXPECT_FALSE(IsCaretHighlightEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightMouseCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightKeyboardFocusEnabledOnDetailMenu());
  EXPECT_FALSE(IsStickyKeysEnabledOnDetailMenu());
  EXPECT_FALSE(IsTapDraggingEnabledOnDetailMenu());
  CloseDetailMenu();

  // Enabling mono audio.
  AccessibilityManager::Get()->EnableMonoAudio(true);
  EXPECT_TRUE(CreateDetailedMenu());
  EXPECT_FALSE(IsSpokenFeedbackEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighContrastEnabledOnDetailMenu());
  EXPECT_FALSE(IsScreenMagnifierEnabledOnDetailMenu());
  EXPECT_FALSE(IsLargeCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsAutoclickEnabledOnDetailMenu());
  EXPECT_FALSE(IsVirtualKeyboardEnabledOnDetailMenu());
  EXPECT_TRUE(IsMonoAudioEnabledOnDetailMenu());
  EXPECT_FALSE(IsCaretHighlightEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightMouseCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightKeyboardFocusEnabledOnDetailMenu());
  EXPECT_FALSE(IsStickyKeysEnabledOnDetailMenu());
  EXPECT_FALSE(IsTapDraggingEnabledOnDetailMenu());
  CloseDetailMenu();

  // Disabling mono audio.
  AccessibilityManager::Get()->EnableMonoAudio(false);
  EXPECT_TRUE(CreateDetailedMenu());
  EXPECT_FALSE(IsSpokenFeedbackEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighContrastEnabledOnDetailMenu());
  EXPECT_FALSE(IsScreenMagnifierEnabledOnDetailMenu());
  EXPECT_FALSE(IsLargeCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsAutoclickEnabledOnDetailMenu());
  EXPECT_FALSE(IsVirtualKeyboardEnabledOnDetailMenu());
  EXPECT_FALSE(IsMonoAudioEnabledOnDetailMenu());
  EXPECT_FALSE(IsCaretHighlightEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightMouseCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightKeyboardFocusEnabledOnDetailMenu());
  EXPECT_FALSE(IsStickyKeysEnabledOnDetailMenu());
  EXPECT_FALSE(IsTapDraggingEnabledOnDetailMenu());
  CloseDetailMenu();

  // Enabling caret highlight.
  AccessibilityManager::Get()->SetCaretHighlightEnabled(true);
  EXPECT_TRUE(CreateDetailedMenu());
  EXPECT_FALSE(IsSpokenFeedbackEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighContrastEnabledOnDetailMenu());
  EXPECT_FALSE(IsScreenMagnifierEnabledOnDetailMenu());
  EXPECT_FALSE(IsLargeCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsAutoclickEnabledOnDetailMenu());
  EXPECT_FALSE(IsVirtualKeyboardEnabledOnDetailMenu());
  EXPECT_FALSE(IsMonoAudioEnabledOnDetailMenu());
  EXPECT_TRUE(IsCaretHighlightEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightMouseCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightKeyboardFocusEnabledOnDetailMenu());
  EXPECT_FALSE(IsStickyKeysEnabledOnDetailMenu());
  EXPECT_FALSE(IsTapDraggingEnabledOnDetailMenu());
  CloseDetailMenu();

  // Disabling caret highlight.
  AccessibilityManager::Get()->SetCaretHighlightEnabled(false);
  EXPECT_TRUE(CreateDetailedMenu());
  EXPECT_FALSE(IsSpokenFeedbackEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighContrastEnabledOnDetailMenu());
  EXPECT_FALSE(IsScreenMagnifierEnabledOnDetailMenu());
  EXPECT_FALSE(IsLargeCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsAutoclickEnabledOnDetailMenu());
  EXPECT_FALSE(IsVirtualKeyboardEnabledOnDetailMenu());
  EXPECT_FALSE(IsMonoAudioEnabledOnDetailMenu());
  EXPECT_FALSE(IsCaretHighlightEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightMouseCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightKeyboardFocusEnabledOnDetailMenu());
  EXPECT_FALSE(IsStickyKeysEnabledOnDetailMenu());
  EXPECT_FALSE(IsTapDraggingEnabledOnDetailMenu());
  CloseDetailMenu();

  // Enabling highlight mouse cursor.
  AccessibilityManager::Get()->SetCursorHighlightEnabled(true);
  EXPECT_TRUE(CreateDetailedMenu());
  EXPECT_FALSE(IsSpokenFeedbackEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighContrastEnabledOnDetailMenu());
  EXPECT_FALSE(IsScreenMagnifierEnabledOnDetailMenu());
  EXPECT_FALSE(IsLargeCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsAutoclickEnabledOnDetailMenu());
  EXPECT_FALSE(IsVirtualKeyboardEnabledOnDetailMenu());
  EXPECT_FALSE(IsMonoAudioEnabledOnDetailMenu());
  EXPECT_FALSE(IsCaretHighlightEnabledOnDetailMenu());
  EXPECT_TRUE(IsHighlightMouseCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightKeyboardFocusEnabledOnDetailMenu());
  EXPECT_FALSE(IsStickyKeysEnabledOnDetailMenu());
  EXPECT_FALSE(IsTapDraggingEnabledOnDetailMenu());
  CloseDetailMenu();

  // Disabling highlight mouse cursor.
  AccessibilityManager::Get()->SetCursorHighlightEnabled(false);
  EXPECT_TRUE(CreateDetailedMenu());
  EXPECT_FALSE(IsSpokenFeedbackEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighContrastEnabledOnDetailMenu());
  EXPECT_FALSE(IsScreenMagnifierEnabledOnDetailMenu());
  EXPECT_FALSE(IsLargeCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsAutoclickEnabledOnDetailMenu());
  EXPECT_FALSE(IsVirtualKeyboardEnabledOnDetailMenu());
  EXPECT_FALSE(IsMonoAudioEnabledOnDetailMenu());
  EXPECT_FALSE(IsCaretHighlightEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightMouseCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightKeyboardFocusEnabledOnDetailMenu());
  EXPECT_FALSE(IsStickyKeysEnabledOnDetailMenu());
  EXPECT_FALSE(IsTapDraggingEnabledOnDetailMenu());
  CloseDetailMenu();

  // Enabling highlight keyboard focus.
  AccessibilityManager::Get()->SetFocusHighlightEnabled(true);
  EXPECT_TRUE(CreateDetailedMenu());
  EXPECT_FALSE(IsSpokenFeedbackEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighContrastEnabledOnDetailMenu());
  EXPECT_FALSE(IsScreenMagnifierEnabledOnDetailMenu());
  EXPECT_FALSE(IsLargeCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsAutoclickEnabledOnDetailMenu());
  EXPECT_FALSE(IsVirtualKeyboardEnabledOnDetailMenu());
  EXPECT_FALSE(IsMonoAudioEnabledOnDetailMenu());
  EXPECT_FALSE(IsCaretHighlightEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightMouseCursorEnabledOnDetailMenu());
  EXPECT_TRUE(IsHighlightKeyboardFocusEnabledOnDetailMenu());
  EXPECT_FALSE(IsStickyKeysEnabledOnDetailMenu());
  EXPECT_FALSE(IsTapDraggingEnabledOnDetailMenu());
  CloseDetailMenu();

  // Disabling highlight keyboard focus.
  AccessibilityManager::Get()->SetFocusHighlightEnabled(false);
  EXPECT_TRUE(CreateDetailedMenu());
  EXPECT_FALSE(IsSpokenFeedbackEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighContrastEnabledOnDetailMenu());
  EXPECT_FALSE(IsScreenMagnifierEnabledOnDetailMenu());
  EXPECT_FALSE(IsLargeCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsAutoclickEnabledOnDetailMenu());
  EXPECT_FALSE(IsVirtualKeyboardEnabledOnDetailMenu());
  EXPECT_FALSE(IsMonoAudioEnabledOnDetailMenu());
  EXPECT_FALSE(IsCaretHighlightEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightMouseCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightKeyboardFocusEnabledOnDetailMenu());
  EXPECT_FALSE(IsStickyKeysEnabledOnDetailMenu());
  EXPECT_FALSE(IsTapDraggingEnabledOnDetailMenu());
  CloseDetailMenu();

  // Enabling sticky keys.
  AccessibilityManager::Get()->EnableStickyKeys(true);
  EXPECT_TRUE(CreateDetailedMenu());
  EXPECT_FALSE(IsSpokenFeedbackEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighContrastEnabledOnDetailMenu());
  EXPECT_FALSE(IsScreenMagnifierEnabledOnDetailMenu());
  EXPECT_FALSE(IsLargeCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsAutoclickEnabledOnDetailMenu());
  EXPECT_FALSE(IsVirtualKeyboardEnabledOnDetailMenu());
  EXPECT_FALSE(IsMonoAudioEnabledOnDetailMenu());
  EXPECT_FALSE(IsCaretHighlightEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightMouseCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightKeyboardFocusEnabledOnDetailMenu());
  EXPECT_TRUE(IsStickyKeysEnabledOnDetailMenu());
  EXPECT_FALSE(IsTapDraggingEnabledOnDetailMenu());
  CloseDetailMenu();

  // Disabling sticky keys.
  AccessibilityManager::Get()->EnableStickyKeys(false);
  EXPECT_TRUE(CreateDetailedMenu());
  EXPECT_FALSE(IsSpokenFeedbackEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighContrastEnabledOnDetailMenu());
  EXPECT_FALSE(IsScreenMagnifierEnabledOnDetailMenu());
  EXPECT_FALSE(IsLargeCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsAutoclickEnabledOnDetailMenu());
  EXPECT_FALSE(IsVirtualKeyboardEnabledOnDetailMenu());
  EXPECT_FALSE(IsMonoAudioEnabledOnDetailMenu());
  EXPECT_FALSE(IsCaretHighlightEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightMouseCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightKeyboardFocusEnabledOnDetailMenu());
  EXPECT_FALSE(IsStickyKeysEnabledOnDetailMenu());
  EXPECT_FALSE(IsTapDraggingEnabledOnDetailMenu());
  CloseDetailMenu();

  // Enabling tap dragging.
  AccessibilityManager::Get()->EnableTapDragging(true);
  EXPECT_TRUE(CreateDetailedMenu());
  EXPECT_FALSE(IsSpokenFeedbackEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighContrastEnabledOnDetailMenu());
  EXPECT_FALSE(IsScreenMagnifierEnabledOnDetailMenu());
  EXPECT_FALSE(IsLargeCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsAutoclickEnabledOnDetailMenu());
  EXPECT_FALSE(IsVirtualKeyboardEnabledOnDetailMenu());
  EXPECT_FALSE(IsMonoAudioEnabledOnDetailMenu());
  EXPECT_FALSE(IsCaretHighlightEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightMouseCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightKeyboardFocusEnabledOnDetailMenu());
  EXPECT_FALSE(IsStickyKeysEnabledOnDetailMenu());
  EXPECT_TRUE(IsTapDraggingEnabledOnDetailMenu());
  CloseDetailMenu();

  // Disabling tap dragging.
  AccessibilityManager::Get()->EnableTapDragging(false);
  EXPECT_TRUE(CreateDetailedMenu());
  EXPECT_FALSE(IsSpokenFeedbackEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighContrastEnabledOnDetailMenu());
  EXPECT_FALSE(IsScreenMagnifierEnabledOnDetailMenu());
  EXPECT_FALSE(IsLargeCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsAutoclickEnabledOnDetailMenu());
  EXPECT_FALSE(IsVirtualKeyboardEnabledOnDetailMenu());
  EXPECT_FALSE(IsMonoAudioEnabledOnDetailMenu());
  EXPECT_FALSE(IsCaretHighlightEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightMouseCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightKeyboardFocusEnabledOnDetailMenu());
  EXPECT_FALSE(IsStickyKeysEnabledOnDetailMenu());
  EXPECT_FALSE(IsTapDraggingEnabledOnDetailMenu());
  CloseDetailMenu();

  // Enabling all of the a11y features.
  AccessibilityManager::Get()->EnableSpokenFeedback(
      true, ash::A11Y_NOTIFICATION_NONE);
  AccessibilityManager::Get()->EnableHighContrast(true);
  SetMagnifierEnabled(true);
  AccessibilityManager::Get()->EnableLargeCursor(true);
  AccessibilityManager::Get()->EnableVirtualKeyboard(true);
  AccessibilityManager::Get()->EnableAutoclick(true);
  AccessibilityManager::Get()->EnableMonoAudio(true);
  AccessibilityManager::Get()->SetCaretHighlightEnabled(true);
  AccessibilityManager::Get()->SetCursorHighlightEnabled(true);
  AccessibilityManager::Get()->SetFocusHighlightEnabled(true);
  AccessibilityManager::Get()->EnableStickyKeys(true);
  AccessibilityManager::Get()->EnableTapDragging(true);
  EXPECT_TRUE(CreateDetailedMenu());
  EXPECT_TRUE(IsSpokenFeedbackEnabledOnDetailMenu());
  EXPECT_TRUE(IsHighContrastEnabledOnDetailMenu());
  EXPECT_TRUE(IsScreenMagnifierEnabledOnDetailMenu());
  EXPECT_TRUE(IsLargeCursorEnabledOnDetailMenu());
  EXPECT_TRUE(IsAutoclickEnabledOnDetailMenu());
  EXPECT_TRUE(IsVirtualKeyboardEnabledOnDetailMenu());
  EXPECT_TRUE(IsMonoAudioEnabledOnDetailMenu());
  EXPECT_TRUE(IsCaretHighlightEnabledOnDetailMenu());
  EXPECT_TRUE(IsHighlightMouseCursorEnabledOnDetailMenu());
  // Focus highlighting can't be on when spoken feedback is on
  EXPECT_FALSE(IsHighlightKeyboardFocusEnabledOnDetailMenu());
  EXPECT_TRUE(IsStickyKeysEnabledOnDetailMenu());
  EXPECT_TRUE(IsTapDraggingEnabledOnDetailMenu());
  CloseDetailMenu();

  // Disabling all of the a11y features.
  AccessibilityManager::Get()->EnableSpokenFeedback(
      false, ash::A11Y_NOTIFICATION_NONE);
  AccessibilityManager::Get()->EnableHighContrast(false);
  SetMagnifierEnabled(false);
  AccessibilityManager::Get()->EnableLargeCursor(false);
  AccessibilityManager::Get()->EnableVirtualKeyboard(false);
  AccessibilityManager::Get()->EnableAutoclick(false);
  AccessibilityManager::Get()->EnableMonoAudio(false);
  AccessibilityManager::Get()->SetCaretHighlightEnabled(false);
  AccessibilityManager::Get()->SetCursorHighlightEnabled(false);
  AccessibilityManager::Get()->SetFocusHighlightEnabled(false);
  AccessibilityManager::Get()->EnableStickyKeys(false);
  AccessibilityManager::Get()->EnableTapDragging(false);
  EXPECT_TRUE(CreateDetailedMenu());
  EXPECT_FALSE(IsSpokenFeedbackEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighContrastEnabledOnDetailMenu());
  EXPECT_FALSE(IsScreenMagnifierEnabledOnDetailMenu());
  EXPECT_FALSE(IsLargeCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsAutoclickEnabledOnDetailMenu());
  EXPECT_FALSE(IsVirtualKeyboardEnabledOnDetailMenu());
  EXPECT_FALSE(IsMonoAudioEnabledOnDetailMenu());
  EXPECT_FALSE(IsCaretHighlightEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightMouseCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightKeyboardFocusEnabledOnDetailMenu());
  EXPECT_FALSE(IsStickyKeysEnabledOnDetailMenu());
  EXPECT_FALSE(IsTapDraggingEnabledOnDetailMenu());
  CloseDetailMenu();

  // Enabling autoclick.
  AccessibilityManager::Get()->EnableAutoclick(true);
  EXPECT_TRUE(CreateDetailedMenu());
  EXPECT_FALSE(IsSpokenFeedbackEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighContrastEnabledOnDetailMenu());
  EXPECT_FALSE(IsScreenMagnifierEnabledOnDetailMenu());
  EXPECT_FALSE(IsLargeCursorEnabledOnDetailMenu());
  EXPECT_TRUE(IsAutoclickEnabledOnDetailMenu());
  EXPECT_FALSE(IsVirtualKeyboardEnabledOnDetailMenu());
  EXPECT_FALSE(IsMonoAudioEnabledOnDetailMenu());
  EXPECT_FALSE(IsCaretHighlightEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightMouseCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightKeyboardFocusEnabledOnDetailMenu());
  EXPECT_FALSE(IsStickyKeysEnabledOnDetailMenu());
  EXPECT_FALSE(IsTapDraggingEnabledOnDetailMenu());
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
  EXPECT_FALSE(IsMonoAudioEnabledOnDetailMenu());
  EXPECT_FALSE(IsCaretHighlightEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightMouseCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightKeyboardFocusEnabledOnDetailMenu());
  EXPECT_FALSE(IsStickyKeysEnabledOnDetailMenu());
  EXPECT_FALSE(IsTapDraggingEnabledOnDetailMenu());
  CloseDetailMenu();
}

IN_PROC_BROWSER_TEST_P(TrayAccessibilityTest, CheckMenuVisibilityOnDetailMenu) {
  // Except help & settings, others should be kept the same
  // in LOGIN | NOT LOGIN | LOCKED. https://crbug.com/632107.
  SetLoginStatus(ash::LoginStatus::NOT_LOGGED_IN);
  EXPECT_TRUE(CreateDetailedMenu());
  EXPECT_TRUE(IsSpokenFeedbackMenuShownOnDetailMenu());
  EXPECT_TRUE(IsHighContrastMenuShownOnDetailMenu());
  EXPECT_TRUE(IsScreenMagnifierMenuShownOnDetailMenu());
  EXPECT_TRUE(IsAutoclickMenuShownOnDetailMenu());
  EXPECT_TRUE(IsVirtualKeyboardMenuShownOnDetailMenu());
  EXPECT_FALSE(IsHelpAvailableOnDetailMenu());
  EXPECT_FALSE(IsSettingsAvailableOnDetailMenu());
  EXPECT_TRUE(IsLargeCursorMenuShownOnDetailMenu());
  EXPECT_TRUE(IsMonoAudioMenuShownOnDetailMenu());
  EXPECT_TRUE(IsCaretHighlightMenuShownOnDetailMenu());
  EXPECT_TRUE(IsHighlightMouseCursorMenuShownOnDetailMenu());
  EXPECT_TRUE(IsHighlightKeyboardFocusMenuShownOnDetailMenu());
  EXPECT_TRUE(IsStickyKeysMenuShownOnDetailMenu());
  EXPECT_TRUE(IsTapDraggingMenuShownOnDetailMenu());
  CloseDetailMenu();

  // Simulate login.
  CreateAndStartUserSession(AccountId::FromUserEmail("owner@invalid.domain"));
  EXPECT_TRUE(CreateDetailedMenu());
  EXPECT_TRUE(IsSpokenFeedbackMenuShownOnDetailMenu());
  EXPECT_TRUE(IsHighContrastMenuShownOnDetailMenu());
  EXPECT_TRUE(IsScreenMagnifierMenuShownOnDetailMenu());
  EXPECT_TRUE(IsAutoclickMenuShownOnDetailMenu());
  EXPECT_TRUE(IsVirtualKeyboardMenuShownOnDetailMenu());
  EXPECT_TRUE(IsHelpAvailableOnDetailMenu());
  EXPECT_TRUE(IsSettingsAvailableOnDetailMenu());
  EXPECT_TRUE(IsLargeCursorMenuShownOnDetailMenu());
  EXPECT_TRUE(IsMonoAudioMenuShownOnDetailMenu());
  EXPECT_TRUE(IsCaretHighlightMenuShownOnDetailMenu());
  EXPECT_TRUE(IsHighlightMouseCursorMenuShownOnDetailMenu());
  EXPECT_TRUE(IsHighlightKeyboardFocusMenuShownOnDetailMenu());
  EXPECT_TRUE(IsStickyKeysMenuShownOnDetailMenu());
  EXPECT_TRUE(IsTapDraggingMenuShownOnDetailMenu());
  CloseDetailMenu();

  // Simulate screen lock.
  session_manager::SessionManager::Get()->SetSessionState(
      session_manager::SessionState::LOCKED);
  // Flush to ensure the session state reaches ash and updates login status.
  SessionControllerClient::FlushForTesting();
  EXPECT_TRUE(CreateDetailedMenu());
  EXPECT_TRUE(IsSpokenFeedbackMenuShownOnDetailMenu());
  EXPECT_TRUE(IsHighContrastMenuShownOnDetailMenu());
  EXPECT_TRUE(IsScreenMagnifierMenuShownOnDetailMenu());
  EXPECT_TRUE(IsAutoclickMenuShownOnDetailMenu());
  EXPECT_TRUE(IsVirtualKeyboardMenuShownOnDetailMenu());
  EXPECT_FALSE(IsHelpAvailableOnDetailMenu());
  EXPECT_FALSE(IsSettingsAvailableOnDetailMenu());
  EXPECT_TRUE(IsLargeCursorMenuShownOnDetailMenu());
  EXPECT_TRUE(IsMonoAudioMenuShownOnDetailMenu());
  EXPECT_TRUE(IsCaretHighlightMenuShownOnDetailMenu());
  EXPECT_TRUE(IsHighlightMouseCursorMenuShownOnDetailMenu());
  EXPECT_TRUE(IsHighlightKeyboardFocusMenuShownOnDetailMenu());
  EXPECT_TRUE(IsStickyKeysMenuShownOnDetailMenu());
  EXPECT_TRUE(IsTapDraggingMenuShownOnDetailMenu());
  CloseDetailMenu();

  // Simulate adding multiprofile user.
  session_manager::SessionManager::Get()->SetSessionState(
      session_manager::SessionState::LOGIN_SECONDARY);
  // Flush to ensure the session state reaches ash and updates login status.
  SessionControllerClient::FlushForTesting();
  EXPECT_TRUE(CreateDetailedMenu());
  EXPECT_TRUE(IsSpokenFeedbackMenuShownOnDetailMenu());
  EXPECT_TRUE(IsHighContrastMenuShownOnDetailMenu());
  EXPECT_TRUE(IsScreenMagnifierMenuShownOnDetailMenu());
  EXPECT_TRUE(IsAutoclickMenuShownOnDetailMenu());
  EXPECT_TRUE(IsVirtualKeyboardMenuShownOnDetailMenu());
  EXPECT_FALSE(IsHelpAvailableOnDetailMenu());
  EXPECT_FALSE(IsSettingsAvailableOnDetailMenu());
  EXPECT_TRUE(IsLargeCursorMenuShownOnDetailMenu());
  EXPECT_TRUE(IsMonoAudioMenuShownOnDetailMenu());
  EXPECT_TRUE(IsCaretHighlightMenuShownOnDetailMenu());
  EXPECT_TRUE(IsHighlightMouseCursorMenuShownOnDetailMenu());
  EXPECT_TRUE(IsHighlightKeyboardFocusMenuShownOnDetailMenu());
  EXPECT_TRUE(IsStickyKeysMenuShownOnDetailMenu());
  EXPECT_TRUE(IsTapDraggingMenuShownOnDetailMenu());
  CloseDetailMenu();
}

INSTANTIATE_TEST_CASE_P(TrayAccessibilityTestInstance,
                        TrayAccessibilityTest,
                        testing::Values(PREF_SERVICE,
                                        POLICY));

}  // namespace chromeos
