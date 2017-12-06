// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"

#include "ash/magnifier/magnification_controller.h"
#include "ash/public/cpp/accessibility_types.h"
#include "ash/public/cpp/ash_pref_names.h"
#include "ash/shell.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/accessibility/magnification_manager.h"
#include "chrome/browser/chromeos/login/helper.h"
#include "chrome/browser/chromeos/login/session/user_session_manager.h"
#include "chrome/browser/chromeos/preferences.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/extensions/api/braille_display_private/mock_braille_controller.h"
#include "chrome/browser/prefs/pref_service_syncable_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/chromeos_switches.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "components/signin/core/account_id/account_id.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "components/user_manager/user_names.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/chromeos/component_extension_ime_manager.h"
#include "ui/base/ime/chromeos/extension_ime_util.h"
#include "ui/base/ime/chromeos/input_method_manager.h"

using chromeos::input_method::InputMethodManager;
using chromeos::input_method::InputMethodUtil;
using chromeos::input_method::InputMethodDescriptors;
using content::BrowserThread;
using extensions::api::braille_display_private::BrailleObserver;
using extensions::api::braille_display_private::DisplayState;
using extensions::api::braille_display_private::KeyEvent;
using extensions::api::braille_display_private::MockBrailleController;

namespace chromeos {

namespace {

constexpr char kTestUserName[] = "owner@invalid.domain";
constexpr char kTestUserGaiaId[] = "9876543210";

constexpr int kTestAutoclickDelayMs = 2000;

// Test user name for supervised user. The domain part must be matched with
// user_manager::kSupervisedUserDomain.
constexpr char kTestSupervisedUserName[] = "test@locally-managed.localhost";

class MockAccessibilityObserver {
 public:
  MockAccessibilityObserver()
      : observed_(false), observed_enabled_(false), observed_type_(-1) {
    AccessibilityManager* accessibility_manager = AccessibilityManager::Get();
    CHECK(accessibility_manager);
    accessibility_subscription_ = accessibility_manager->RegisterCallback(
        base::Bind(&MockAccessibilityObserver::OnAccessibilityStatusChanged,
                   base::Unretained(this)));
  }

  virtual ~MockAccessibilityObserver() {}

  bool observed() const { return observed_; }
  bool observed_enabled() const { return observed_enabled_; }
  int observed_type() const { return observed_type_; }

  void reset() { observed_ = false; }

 private:
  void OnAccessibilityStatusChanged(
      const AccessibilityStatusEventDetails& details) {
    if (details.notification_type != ACCESSIBILITY_TOGGLE_SCREEN_MAGNIFIER) {
      observed_type_ = details.notification_type;
      observed_enabled_ = details.enabled;
      observed_ = true;
    }
  }

  bool observed_;
  bool observed_enabled_;
  int observed_type_;

  std::unique_ptr<AccessibilityStatusSubscription> accessibility_subscription_;

  DISALLOW_COPY_AND_ASSIGN(MockAccessibilityObserver);
};

void SetLargeCursorEnabled(bool enabled) {
  return AccessibilityManager::Get()->EnableLargeCursor(enabled);
}

bool IsLargeCursorEnabled() {
  return AccessibilityManager::Get()->IsLargeCursorEnabled();
}

bool ShouldShowAccessibilityMenu() {
  return AccessibilityManager::Get()->ShouldShowAccessibilityMenu();
}

void SetHighContrastEnabled(bool enabled) {
  return AccessibilityManager::Get()->EnableHighContrast(enabled);
}

bool IsHighContrastEnabled() {
  return AccessibilityManager::Get()->IsHighContrastEnabled();
}

void SetSpokenFeedbackEnabled(bool enabled) {
  return AccessibilityManager::Get()->EnableSpokenFeedback(
      enabled, ash::A11Y_NOTIFICATION_NONE);
}

bool IsSpokenFeedbackEnabled() {
  return AccessibilityManager::Get()->IsSpokenFeedbackEnabled();
}

void SetAutoclickEnabled(bool enabled) {
  return AccessibilityManager::Get()->EnableAutoclick(enabled);
}

bool IsAutoclickEnabled() {
  return AccessibilityManager::Get()->IsAutoclickEnabled();
}

void SetAutoclickDelay(int delay_ms) {
  return AccessibilityManager::Get()->SetAutoclickDelay(delay_ms);
}

int GetAutoclickDelay() {
  return AccessibilityManager::Get()->GetAutoclickDelay();
}

void SetVirtualKeyboardEnabled(bool enabled) {
  return AccessibilityManager::Get()->EnableVirtualKeyboard(enabled);
}

bool IsVirtualKeyboardEnabled() {
  return AccessibilityManager::Get()->IsVirtualKeyboardEnabled();
}

void SetMonoAudioEnabled(bool enabled) {
  return AccessibilityManager::Get()->EnableMonoAudio(enabled);
}

bool IsMonoAudioEnabled() {
  return AccessibilityManager::Get()->IsMonoAudioEnabled();
}

Profile* GetProfile() {
  // ProfileManager::GetActiveUserProfile() does not load user profile
  // implicitly any more. Use ProfileHelper::GetProfileByUserIdHashForTest() to
  // do an explicit load.
  Profile* const profile = ProfileHelper::GetProfileByUserIdHashForTest(
      user_manager::UserManager::Get()->GetActiveUser()->username_hash());

  DCHECK(profile);
  return profile;
}

PrefService* GetPrefs() {
  return GetProfile()->GetPrefs();
}

// Simulates how UserSessionManager starts a user session by loading user
// profile and marking session as started.
void StartUserSession(const AccountId& account_id) {
  ProfileHelper::GetProfileByUserIdHashForTest(
      user_manager::UserManager::Get()->FindUser(account_id)->username_hash());

  session_manager::SessionManager::Get()->SessionStarted();
}

void SetAlwaysShowMenuEnabled(bool enabled) {
  GetPrefs()->SetBoolean(ash::prefs::kShouldAlwaysShowAccessibilityMenu,
                         enabled);
}

void SetLargeCursorEnabledPref(bool enabled) {
  GetPrefs()->SetBoolean(ash::prefs::kAccessibilityLargeCursorEnabled, enabled);
}

void SetHighContrastEnabledPref(bool enabled) {
  GetPrefs()->SetBoolean(ash::prefs::kAccessibilityHighContrastEnabled,
                         enabled);
}

void SetSpokenFeedbackEnabledPref(bool enabled) {
  GetPrefs()->SetBoolean(ash::prefs::kAccessibilitySpokenFeedbackEnabled,
                         enabled);
}

void SetAutoclickEnabledPref(bool enabled) {
  GetPrefs()->SetBoolean(ash::prefs::kAccessibilityAutoclickEnabled, enabled);
}

void SetAutoclickDelayPref(int delay_ms) {
  GetPrefs()->SetInteger(ash::prefs::kAccessibilityAutoclickDelayMs, delay_ms);
}

void SetVirtualKeyboardEnabledPref(bool enabled) {
  GetPrefs()->SetBoolean(ash::prefs::kAccessibilityVirtualKeyboardEnabled,
                         enabled);
}

void SetMonoAudioEnabledPref(bool enabled) {
  GetPrefs()->SetBoolean(ash::prefs::kAccessibilityMonoAudioEnabled, enabled);
}

bool GetLargeCursorEnabledFromPref() {
  return GetPrefs()->GetBoolean(ash::prefs::kAccessibilityLargeCursorEnabled);
}

bool GetHighContrastEnabledFromPref() {
  return GetPrefs()->GetBoolean(ash::prefs::kAccessibilityHighContrastEnabled);
}

bool GetSpokenFeedbackEnabledFromPref() {
  return GetPrefs()->GetBoolean(
      ash::prefs::kAccessibilitySpokenFeedbackEnabled);
}

bool GetAutoclickEnabledFromPref() {
  return GetPrefs()->GetBoolean(ash::prefs::kAccessibilityAutoclickEnabled);
}

int GetAutoclickDelayFromPref() {
  return GetPrefs()->GetInteger(ash::prefs::kAccessibilityAutoclickDelayMs);
}

bool GetMonoAudioEnabledFromPref() {
  return GetPrefs()->GetBoolean(ash::prefs::kAccessibilityMonoAudioEnabled);
}

bool IsBrailleImeActive() {
  InputMethodManager* imm = InputMethodManager::Get();
  std::unique_ptr<InputMethodDescriptors> descriptors =
      imm->GetActiveIMEState()->GetActiveInputMethods();
  for (InputMethodDescriptors::const_iterator i = descriptors->begin();
       i != descriptors->end(); ++i) {
    if (i->id() == extension_ime_util::kBrailleImeEngineId)
      return true;
  }
  return false;
}

bool IsBrailleImeCurrent() {
  InputMethodManager* imm = InputMethodManager::Get();
  return imm->GetActiveIMEState()->GetCurrentInputMethod().id() ==
         extension_ime_util::kBrailleImeEngineId;
}

}  // anonymous namespace

class AccessibilityManagerTest : public InProcessBrowserTest {
 protected:
  AccessibilityManagerTest() : default_autoclick_delay_(0) {}
  ~AccessibilityManagerTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(chromeos::switches::kLoginManager);
    command_line->AppendSwitchASCII(chromeos::switches::kLoginProfile,
                                    TestingProfile::kTestUserProfileDir);
  }

  void SetUpInProcessBrowserTestFixture() override {
    AccessibilityManager::SetBrailleControllerForTest(&braille_controller_);
  }

  void SetUpOnMainThread() override {
    // Sets the login-screen profile.
    AccessibilityManager::Get()->SetProfileForTest(
        ProfileHelper::GetSigninProfile());
    default_autoclick_delay_ = GetAutoclickDelay();
  }

  void TearDownOnMainThread() override {
    AccessibilityManager::SetBrailleControllerForTest(NULL);
  }

  void SetBrailleDisplayAvailability(bool available) {
    braille_controller_.SetAvailable(available);
    braille_controller_.GetObserver()->OnBrailleDisplayStateChanged(
        *braille_controller_.GetDisplayState());
  }

  int default_autoclick_delay() const { return default_autoclick_delay_; }

  int default_autoclick_delay_;

  content::NotificationRegistrar registrar_;

  MockBrailleController braille_controller_;

  const AccountId test_account_id_ =
      AccountId::FromUserEmailGaiaId(kTestUserName, kTestUserGaiaId);

  DISALLOW_COPY_AND_ASSIGN(AccessibilityManagerTest);
};

IN_PROC_BROWSER_TEST_F(AccessibilityManagerTest, Login) {
  // Confirms that a11y features are disabled on the login screen.
  EXPECT_FALSE(IsLargeCursorEnabled());
  EXPECT_FALSE(IsSpokenFeedbackEnabled());
  EXPECT_FALSE(IsHighContrastEnabled());
  EXPECT_FALSE(IsAutoclickEnabled());
  EXPECT_FALSE(IsVirtualKeyboardEnabled());
  EXPECT_FALSE(IsMonoAudioEnabled());
  EXPECT_EQ(default_autoclick_delay(), GetAutoclickDelay());

  // Logs in.
  auto* session_manager = session_manager::SessionManager::Get();
  session_manager->CreateSession(test_account_id_, kTestUserName, false);

  // Confirms that the features still disabled just after login.
  EXPECT_FALSE(IsLargeCursorEnabled());
  EXPECT_FALSE(IsSpokenFeedbackEnabled());
  EXPECT_FALSE(IsHighContrastEnabled());
  EXPECT_FALSE(IsAutoclickEnabled());
  EXPECT_FALSE(IsVirtualKeyboardEnabled());
  EXPECT_FALSE(IsMonoAudioEnabled());
  EXPECT_EQ(default_autoclick_delay(), GetAutoclickDelay());

  StartUserSession(test_account_id_);

  // Confirms that the features are still disabled just after login.
  EXPECT_FALSE(IsLargeCursorEnabled());
  EXPECT_FALSE(IsSpokenFeedbackEnabled());
  EXPECT_FALSE(IsHighContrastEnabled());
  EXPECT_FALSE(IsAutoclickEnabled());
  EXPECT_FALSE(IsVirtualKeyboardEnabled());
  EXPECT_FALSE(IsMonoAudioEnabled());
  EXPECT_EQ(default_autoclick_delay(), GetAutoclickDelay());

  // Enables large cursor.
  SetLargeCursorEnabled(true);
  // Confirms that large cursor is enabled.
  EXPECT_TRUE(IsLargeCursorEnabled());

  // Enables spoken feedback.
  SetSpokenFeedbackEnabled(true);
  // Confirms that the spoken feedback is enabled.
  EXPECT_TRUE(IsSpokenFeedbackEnabled());

  // Enables high contrast.
  SetHighContrastEnabled(true);
  // Confirms that high cotrast is enabled.
  EXPECT_TRUE(IsHighContrastEnabled());

  // Enables autoclick.
  SetAutoclickEnabled(true);
  // Confirms that autoclick is enabled.
  EXPECT_TRUE(IsAutoclickEnabled());

  // Test that autoclick delay is set properly.
  SetAutoclickDelay(kTestAutoclickDelayMs);
  EXPECT_EQ(kTestAutoclickDelayMs, GetAutoclickDelay());

  // Enable on-screen keyboard
  SetVirtualKeyboardEnabled(true);
  // Confirm that the on-screen keyboard option is enabled.
  EXPECT_TRUE(IsVirtualKeyboardEnabled());

  // Enable mono audio output
  SetMonoAudioEnabled(true);
  // Confirm that mono audio output option is enabled.
  EXPECT_TRUE(IsMonoAudioEnabled());
}

IN_PROC_BROWSER_TEST_F(AccessibilityManagerTest, BrailleOnLoginScreen) {
  EXPECT_FALSE(IsSpokenFeedbackEnabled());

  // Signal the accessibility manager that a braille display was connected.
  SetBrailleDisplayAvailability(true);
  // Confirms that the spoken feedback is enabled.
  EXPECT_TRUE(IsSpokenFeedbackEnabled());
}

IN_PROC_BROWSER_TEST_F(AccessibilityManagerTest, TypePref) {
  // Logs in.
  auto* session_manager = session_manager::SessionManager::Get();
  session_manager->CreateSession(test_account_id_, kTestUserName, false);
  StartUserSession(test_account_id_);

  // Confirms that the features are disabled just after login.
  EXPECT_FALSE(IsLargeCursorEnabled());
  EXPECT_FALSE(IsSpokenFeedbackEnabled());
  EXPECT_FALSE(IsHighContrastEnabled());
  EXPECT_FALSE(IsAutoclickEnabled());
  EXPECT_EQ(default_autoclick_delay(), GetAutoclickDelay());
  EXPECT_FALSE(IsVirtualKeyboardEnabled());
  EXPECT_FALSE(IsMonoAudioEnabled());

  // Sets the pref as true to enable the large cursor.
  SetLargeCursorEnabledPref(true);
  // Confirms that the large cursor is enabled.
  EXPECT_TRUE(IsLargeCursorEnabled());

  // Sets the pref as true to enable the spoken feedback.
  SetSpokenFeedbackEnabledPref(true);
  // Confirms that the spoken feedback is enabled.
  EXPECT_TRUE(IsSpokenFeedbackEnabled());

  // Sets the pref as true to enable high contrast mode.
  SetHighContrastEnabledPref(true);
  // Confirms that the high contrast mode is enabled.
  EXPECT_TRUE(IsHighContrastEnabled());

  // Sets the pref as true to enable autoclick.
  SetAutoclickEnabledPref(true);
  // Confirms that autoclick is enabled.
  EXPECT_TRUE(IsAutoclickEnabled());

  // Set autoclick delay pref.
  SetAutoclickDelayPref(kTestAutoclickDelayMs);
  // Confirm that the correct value is set.
  EXPECT_EQ(kTestAutoclickDelayMs, GetAutoclickDelay());

  // Sets the on-screen keyboard pref.
  SetVirtualKeyboardEnabledPref(true);
  // Confirm that the on-screen keyboard option is enabled.
  EXPECT_TRUE(IsVirtualKeyboardEnabled());

  // Sets mono audio output pref.
  SetMonoAudioEnabledPref(true);
  // Confirm that mono audio output is enabled.
  EXPECT_TRUE(IsMonoAudioEnabled());

  SetLargeCursorEnabledPref(false);
  EXPECT_FALSE(IsLargeCursorEnabled());

  SetSpokenFeedbackEnabledPref(false);
  EXPECT_FALSE(IsSpokenFeedbackEnabled());

  SetHighContrastEnabledPref(false);
  EXPECT_FALSE(IsHighContrastEnabled());

  SetAutoclickEnabledPref(false);
  EXPECT_FALSE(IsAutoclickEnabled());

  SetVirtualKeyboardEnabledPref(false);
  EXPECT_FALSE(IsVirtualKeyboardEnabled());

  SetMonoAudioEnabledPref(false);
  EXPECT_FALSE(IsMonoAudioEnabled());
}

IN_PROC_BROWSER_TEST_F(AccessibilityManagerTest, ResumeSavedPref) {
  // Loads the profile of the user.
  auto* session_manager = session_manager::SessionManager::Get();
  session_manager->CreateSession(test_account_id_, kTestUserName, false);

  // Sets the pref to enable large cursor before login.
  SetLargeCursorEnabledPref(true);
  EXPECT_FALSE(IsLargeCursorEnabled());

  // Sets the pref to enable spoken feedback before login.
  SetSpokenFeedbackEnabledPref(true);
  EXPECT_FALSE(IsSpokenFeedbackEnabled());

  // Sets the pref to enable high contrast before login.
  SetHighContrastEnabledPref(true);
  EXPECT_FALSE(IsHighContrastEnabled());

  // Sets the pref to enable autoclick before login.
  SetAutoclickEnabledPref(true);
  EXPECT_FALSE(IsAutoclickEnabled());

  // Sets the autoclick delay pref before login but the
  // initial value should not change.
  SetAutoclickDelayPref(kTestAutoclickDelayMs);
  EXPECT_EQ(default_autoclick_delay(), GetAutoclickDelay());

  // Sets the pref to enable the on-screen keyboard before login.
  SetVirtualKeyboardEnabledPref(true);
  EXPECT_FALSE(IsVirtualKeyboardEnabled());

  // Sets the pref to enable mono audio output before login.
  SetMonoAudioEnabledPref(true);
  EXPECT_FALSE(IsMonoAudioEnabled());

  // Logs in.
  StartUserSession(test_account_id_);

  // Confirms that features are enabled by restoring from pref just after login.
  EXPECT_TRUE(IsLargeCursorEnabled());
  EXPECT_TRUE(IsSpokenFeedbackEnabled());
  EXPECT_TRUE(IsHighContrastEnabled());
  EXPECT_TRUE(IsAutoclickEnabled());
  EXPECT_EQ(kTestAutoclickDelayMs, GetAutoclickDelay());
  EXPECT_TRUE(IsVirtualKeyboardEnabled());
  EXPECT_TRUE(IsMonoAudioEnabled());
}

IN_PROC_BROWSER_TEST_F(AccessibilityManagerTest,
                       ChangingTypeInvokesNotification) {
  MockAccessibilityObserver observer;

  // Logs in.
  auto* session_manager = session_manager::SessionManager::Get();
  session_manager->CreateSession(test_account_id_, kTestUserName, false);
  StartUserSession(test_account_id_);

  EXPECT_FALSE(observer.observed());
  observer.reset();

  SetSpokenFeedbackEnabled(true);
  EXPECT_TRUE(observer.observed());
  EXPECT_TRUE(observer.observed_enabled());
  EXPECT_EQ(observer.observed_type(), ACCESSIBILITY_TOGGLE_SPOKEN_FEEDBACK);
  EXPECT_TRUE(IsSpokenFeedbackEnabled());

  observer.reset();
  SetSpokenFeedbackEnabled(false);
  EXPECT_TRUE(observer.observed());
  EXPECT_FALSE(observer.observed_enabled());
  EXPECT_EQ(observer.observed_type(), ACCESSIBILITY_TOGGLE_SPOKEN_FEEDBACK);
  EXPECT_FALSE(IsSpokenFeedbackEnabled());

  observer.reset();
  SetHighContrastEnabled(true);
  EXPECT_TRUE(observer.observed());
  EXPECT_TRUE(observer.observed_enabled());
  EXPECT_EQ(observer.observed_type(), ACCESSIBILITY_TOGGLE_HIGH_CONTRAST_MODE);
  EXPECT_TRUE(IsHighContrastEnabled());

  observer.reset();
  SetHighContrastEnabled(false);
  EXPECT_TRUE(observer.observed());
  EXPECT_FALSE(observer.observed_enabled());
  EXPECT_EQ(observer.observed_type(), ACCESSIBILITY_TOGGLE_HIGH_CONTRAST_MODE);
  EXPECT_FALSE(IsHighContrastEnabled());

  observer.reset();
  SetVirtualKeyboardEnabled(true);
  EXPECT_TRUE(observer.observed());
  EXPECT_TRUE(observer.observed_enabled());
  EXPECT_EQ(observer.observed_type(), ACCESSIBILITY_TOGGLE_VIRTUAL_KEYBOARD);
  EXPECT_TRUE(IsVirtualKeyboardEnabled());

  observer.reset();
  SetVirtualKeyboardEnabled(false);
  EXPECT_TRUE(observer.observed());
  EXPECT_FALSE(observer.observed_enabled());
  EXPECT_EQ(observer.observed_type(), ACCESSIBILITY_TOGGLE_VIRTUAL_KEYBOARD);
  EXPECT_FALSE(IsVirtualKeyboardEnabled());

  observer.reset();
  SetMonoAudioEnabled(true);
  EXPECT_TRUE(observer.observed());
  EXPECT_TRUE(observer.observed_enabled());
  EXPECT_EQ(observer.observed_type(), ACCESSIBILITY_TOGGLE_MONO_AUDIO);
  EXPECT_TRUE(IsMonoAudioEnabled());

  observer.reset();
  SetMonoAudioEnabled(false);
  EXPECT_TRUE(observer.observed());
  EXPECT_FALSE(observer.observed_enabled());
  EXPECT_EQ(observer.observed_type(), ACCESSIBILITY_TOGGLE_MONO_AUDIO);
  EXPECT_FALSE(IsMonoAudioEnabled());
}

IN_PROC_BROWSER_TEST_F(AccessibilityManagerTest,
                       ChangingTypePrefInvokesNotification) {
  MockAccessibilityObserver observer;

  // Logs in.
  auto* session_manager = session_manager::SessionManager::Get();
  session_manager->CreateSession(test_account_id_, kTestUserName, false);
  StartUserSession(test_account_id_);

  EXPECT_FALSE(observer.observed());
  observer.reset();

  SetSpokenFeedbackEnabledPref(true);
  EXPECT_TRUE(observer.observed());
  EXPECT_TRUE(observer.observed_enabled());
  EXPECT_EQ(observer.observed_type(), ACCESSIBILITY_TOGGLE_SPOKEN_FEEDBACK);
  EXPECT_TRUE(IsSpokenFeedbackEnabled());

  observer.reset();
  SetSpokenFeedbackEnabledPref(false);
  EXPECT_TRUE(observer.observed());
  EXPECT_FALSE(observer.observed_enabled());
  EXPECT_EQ(observer.observed_type(), ACCESSIBILITY_TOGGLE_SPOKEN_FEEDBACK);
  EXPECT_FALSE(IsSpokenFeedbackEnabled());

  observer.reset();
  SetHighContrastEnabledPref(true);
  EXPECT_TRUE(observer.observed());
  EXPECT_TRUE(observer.observed_enabled());
  EXPECT_EQ(observer.observed_type(), ACCESSIBILITY_TOGGLE_HIGH_CONTRAST_MODE);
  EXPECT_TRUE(IsHighContrastEnabled());

  observer.reset();
  SetHighContrastEnabledPref(false);
  EXPECT_TRUE(observer.observed());
  EXPECT_FALSE(observer.observed_enabled());
  EXPECT_EQ(observer.observed_type(), ACCESSIBILITY_TOGGLE_HIGH_CONTRAST_MODE);
  EXPECT_FALSE(IsHighContrastEnabled());

  observer.reset();
  SetVirtualKeyboardEnabledPref(true);
  EXPECT_TRUE(observer.observed());
  EXPECT_TRUE(observer.observed_enabled());
  EXPECT_EQ(observer.observed_type(), ACCESSIBILITY_TOGGLE_VIRTUAL_KEYBOARD);
  EXPECT_TRUE(IsVirtualKeyboardEnabled());

  observer.reset();
  SetVirtualKeyboardEnabledPref(false);
  EXPECT_TRUE(observer.observed());
  EXPECT_FALSE(observer.observed_enabled());
  EXPECT_EQ(observer.observed_type(), ACCESSIBILITY_TOGGLE_VIRTUAL_KEYBOARD);
  EXPECT_FALSE(IsVirtualKeyboardEnabled());

  observer.reset();
  SetMonoAudioEnabledPref(true);
  EXPECT_TRUE(observer.observed());
  EXPECT_TRUE(observer.observed_enabled());
  EXPECT_EQ(observer.observed_type(), ACCESSIBILITY_TOGGLE_MONO_AUDIO);
  EXPECT_TRUE(IsMonoAudioEnabled());

  observer.reset();
  SetMonoAudioEnabledPref(false);
  EXPECT_TRUE(observer.observed());
  EXPECT_FALSE(observer.observed_enabled());
  EXPECT_EQ(observer.observed_type(), ACCESSIBILITY_TOGGLE_MONO_AUDIO);
  EXPECT_FALSE(IsMonoAudioEnabled());
}

class AccessibilityManagerUserTypeTest
    : public AccessibilityManagerTest,
      public ::testing::WithParamInterface<AccountId> {
 protected:
  AccessibilityManagerUserTypeTest() {}
  virtual ~AccessibilityManagerUserTypeTest() {}

  DISALLOW_COPY_AND_ASSIGN(AccessibilityManagerUserTypeTest);
};

// TODO(yoshiki): Enable a test for retail mode (i.e. RetailAccountId).
INSTANTIATE_TEST_CASE_P(
    UserTypeInstantiation,
    AccessibilityManagerUserTypeTest,
    ::testing::Values(AccountId::FromUserEmailGaiaId(kTestUserName,
                                                     kTestUserGaiaId),
                      user_manager::GuestAccountId(),
                      AccountId::FromUserEmail(kTestSupervisedUserName)));

IN_PROC_BROWSER_TEST_P(AccessibilityManagerUserTypeTest,
                       EnableOnLoginScreenAndLogin) {
  // Enables large cursor.
  SetLargeCursorEnabled(true);
  EXPECT_TRUE(IsLargeCursorEnabled());
  // Enables spoken feedback.
  SetSpokenFeedbackEnabled(true);
  EXPECT_TRUE(IsSpokenFeedbackEnabled());
  // Enables high contrast.
  SetHighContrastEnabled(true);
  EXPECT_TRUE(IsHighContrastEnabled());
  // Enables autoclick.
  SetAutoclickEnabled(true);
  EXPECT_TRUE(IsAutoclickEnabled());
  // Set autoclick delay.
  SetAutoclickDelay(kTestAutoclickDelayMs);
  EXPECT_EQ(kTestAutoclickDelayMs, GetAutoclickDelay());
  // Enables mono audio output.
  SetMonoAudioEnabled(true);
  EXPECT_TRUE(IsMonoAudioEnabled());

  // Logs in.
  const AccountId account_id = GetParam();
  auto* session_manager = session_manager::SessionManager::Get();
  session_manager->CreateSession(account_id, account_id.GetUserEmail(), false);

  // Confirms that the features are still enabled just after login.
  EXPECT_TRUE(IsLargeCursorEnabled());
  EXPECT_TRUE(IsSpokenFeedbackEnabled());
  EXPECT_TRUE(IsHighContrastEnabled());
  EXPECT_TRUE(IsAutoclickEnabled());
  EXPECT_EQ(kTestAutoclickDelayMs, GetAutoclickDelay());
  EXPECT_TRUE(IsMonoAudioEnabled());

  StartUserSession(account_id);

  // Confirms that the features keep enabled after session starts.
  EXPECT_TRUE(IsLargeCursorEnabled());
  EXPECT_TRUE(IsSpokenFeedbackEnabled());
  EXPECT_TRUE(IsHighContrastEnabled());
  EXPECT_TRUE(IsAutoclickEnabled());
  EXPECT_EQ(kTestAutoclickDelayMs, GetAutoclickDelay());
  EXPECT_TRUE(IsMonoAudioEnabled());

  // Confirms that the prefs have been copied to the user's profile.
  EXPECT_TRUE(GetLargeCursorEnabledFromPref());
  EXPECT_TRUE(GetSpokenFeedbackEnabledFromPref());
  EXPECT_TRUE(GetHighContrastEnabledFromPref());
  EXPECT_TRUE(GetAutoclickEnabledFromPref());
  EXPECT_EQ(kTestAutoclickDelayMs, GetAutoclickDelayFromPref());
  EXPECT_TRUE(GetMonoAudioEnabledFromPref());
}

IN_PROC_BROWSER_TEST_P(AccessibilityManagerUserTypeTest, BrailleWhenLoggedIn) {
  // Logs in.
  const AccountId account_id = GetParam();
  auto* session_manager = session_manager::SessionManager::Get();
  session_manager->CreateSession(account_id, account_id.GetUserEmail(), false);
  StartUserSession(account_id);
  // This object watches for IME preference changes and reflects those in
  // the IME framework state.
  chromeos::Preferences prefs;
  prefs.InitUserPrefsForTesting(
      PrefServiceSyncableFromProfile(GetProfile()),
      user_manager::UserManager::Get()->GetActiveUser(),
      UserSessionManager::GetInstance()->GetDefaultIMEState(GetProfile()));

  // Make sure we start in the expected state.
  EXPECT_FALSE(IsBrailleImeActive());
  EXPECT_FALSE(IsSpokenFeedbackEnabled());

  // Signal the accessibility manager that a braille display was connected.
  SetBrailleDisplayAvailability(true);

  // Now, both spoken feedback and the Braille IME should be enabled.
  EXPECT_TRUE(IsSpokenFeedbackEnabled());
  EXPECT_TRUE(IsBrailleImeActive());

  // Send a braille dots key event and make sure that the braille IME is
  // enabled.
  KeyEvent event;
  event.command = extensions::api::braille_display_private::KEY_COMMAND_DOTS;
  event.braille_dots.reset(new int(0));
  braille_controller_.GetObserver()->OnBrailleKeyEvent(event);
  EXPECT_TRUE(IsBrailleImeCurrent());

  // Unplug the display.  Spolken feedback remains on, but the Braille IME
  // should get deactivated.
  SetBrailleDisplayAvailability(false);
  EXPECT_TRUE(IsSpokenFeedbackEnabled());
  EXPECT_FALSE(IsBrailleImeActive());
  EXPECT_FALSE(IsBrailleImeCurrent());

  // Plugging in a display while spoken feedback is enabled should activate
  // the Braille IME.
  SetBrailleDisplayAvailability(true);
  EXPECT_TRUE(IsSpokenFeedbackEnabled());
  EXPECT_TRUE(IsBrailleImeActive());
}

IN_PROC_BROWSER_TEST_F(AccessibilityManagerTest, AccessibilityMenuVisibility) {
  // Log in.
  auto* session_manager = session_manager::SessionManager::Get();
  session_manager->CreateSession(test_account_id_, kTestUserName, false);
  StartUserSession(test_account_id_);

  // Confirms that the features are disabled.
  EXPECT_FALSE(IsLargeCursorEnabled());
  EXPECT_FALSE(IsSpokenFeedbackEnabled());
  EXPECT_FALSE(IsHighContrastEnabled());
  EXPECT_FALSE(IsAutoclickEnabled());
  EXPECT_FALSE(ShouldShowAccessibilityMenu());
  EXPECT_FALSE(IsVirtualKeyboardEnabled());
  EXPECT_FALSE(IsMonoAudioEnabled());

  // Check "should always show menu" pref.
  EXPECT_FALSE(ShouldShowAccessibilityMenu());
  SetAlwaysShowMenuEnabled(true);
  EXPECT_TRUE(ShouldShowAccessibilityMenu());
  SetAlwaysShowMenuEnabled(false);
  EXPECT_FALSE(ShouldShowAccessibilityMenu());

  // Check large cursor.
  SetLargeCursorEnabled(true);
  EXPECT_TRUE(ShouldShowAccessibilityMenu());
  SetLargeCursorEnabled(false);
  EXPECT_FALSE(ShouldShowAccessibilityMenu());

  // Check spoken feedback.
  SetSpokenFeedbackEnabled(true);
  EXPECT_TRUE(ShouldShowAccessibilityMenu());
  SetSpokenFeedbackEnabled(false);
  EXPECT_FALSE(ShouldShowAccessibilityMenu());

  // Check high contrast.
  SetHighContrastEnabled(true);
  EXPECT_TRUE(ShouldShowAccessibilityMenu());
  SetHighContrastEnabled(false);
  EXPECT_FALSE(ShouldShowAccessibilityMenu());

  // Check autoclick.
  SetAutoclickEnabled(true);
  EXPECT_TRUE(ShouldShowAccessibilityMenu());
  SetAutoclickEnabled(false);
  EXPECT_FALSE(ShouldShowAccessibilityMenu());

  // Check on-screen keyboard.
  SetVirtualKeyboardEnabled(true);
  EXPECT_TRUE(ShouldShowAccessibilityMenu());
  SetVirtualKeyboardEnabled(false);
  EXPECT_FALSE(ShouldShowAccessibilityMenu());

  // Check mono audio output.
  SetMonoAudioEnabled(true);
  EXPECT_TRUE(ShouldShowAccessibilityMenu());
  SetMonoAudioEnabled(false);
  EXPECT_FALSE(ShouldShowAccessibilityMenu());
}

}  // namespace chromeos
