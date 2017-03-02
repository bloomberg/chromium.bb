// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <sys/types.h>

#include <set>
#include <string>

#include "ash/shell.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/input_method/input_method_manager_impl.h"
#include "chrome/browser/chromeos/login/login_manager_test.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/ui/user_adding_screen.h"
#include "chrome/browser/chromeos/preferences.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/stub_cros_settings_provider.h"
#include "chrome/browser/chromeos/system/fake_input_device_settings.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager_chromeos.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/chromeos_switches.h"
#include "components/feedback/tracing_manager.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/pref_store.h"
#include "components/prefs/writeable_pref_store.h"
#include "components/user_manager/user_manager.h"
#include "content/public/common/service_manager_connection.h"
#include "content/public/test/test_utils.h"
#include "services/preferences/public/cpp/pref_client_store.h"
#include "services/preferences/public/interfaces/preferences.mojom.h"
#include "services/service_manager/public/cpp/connector.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/chromeos/fake_ime_keyboard.h"
#include "ui/events/event_utils.h"

namespace chromeos {

namespace {

const char* const kTestUsers[] = {"test-user1@gmail.com",
                                  "test-user2@gmail.com"};

}  // namespace

class PreferencesTest : public LoginManagerTest {
 public:
  PreferencesTest()
      : LoginManagerTest(true), input_settings_(nullptr), keyboard_(nullptr) {
    for (size_t i = 0; i < arraysize(kTestUsers); ++i) {
      test_users_.push_back(AccountId::FromUserEmail(kTestUsers[i]));
    }
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    LoginManagerTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kStubCrosSettings);
  }

  void SetUpOnMainThread() override {
    LoginManagerTest::SetUpOnMainThread();
    input_settings_ = system::InputDeviceSettings::Get()->GetFakeInterface();
    EXPECT_NE(nullptr, input_settings_);
    keyboard_ = new input_method::FakeImeKeyboard();
    static_cast<input_method::InputMethodManagerImpl*>(
        input_method::InputMethodManager::Get())
        ->SetImeKeyboardForTesting(keyboard_);
    CrosSettings::Get()->SetString(kDeviceOwner, test_users_[0].GetUserEmail());
  }

  // Sets set of preferences in given |prefs|. Value of prefernece depends of
  // |variant| value. For opposite |variant| values all preferences receive
  // different values.
  void SetPrefs(PrefService* prefs, bool variant) {
    prefs->SetBoolean(prefs::kTapToClickEnabled, variant);
    prefs->SetBoolean(prefs::kPrimaryMouseButtonRight, !variant);
    prefs->SetBoolean(prefs::kTapDraggingEnabled, variant);
    prefs->SetBoolean(prefs::kEnableTouchpadThreeFingerClick, !variant);
    prefs->SetBoolean(prefs::kNaturalScroll, variant);
    prefs->SetInteger(prefs::kMouseSensitivity, !variant);
    prefs->SetInteger(prefs::kTouchpadSensitivity, variant);
    prefs->SetBoolean(prefs::kTouchHudProjectionEnabled, !variant);
    prefs->SetBoolean(prefs::kLanguageXkbAutoRepeatEnabled, variant);
    prefs->SetInteger(prefs::kLanguageXkbAutoRepeatDelay, variant ? 100 : 500);
    prefs->SetInteger(prefs::kLanguageXkbAutoRepeatInterval, variant ? 1 : 4);
    prefs->SetString(prefs::kLanguagePreloadEngines,
                     variant ? "xkb:us::eng,xkb:us:dvorak:eng"
                             : "xkb:us::eng,xkb:ru::rus");
  }

  void CheckSettingsCorrespondToPrefs(PrefService* prefs) {
    EXPECT_EQ(prefs->GetBoolean(prefs::kTapToClickEnabled),
              input_settings_->current_touchpad_settings().GetTapToClick());
    EXPECT_EQ(prefs->GetBoolean(prefs::kPrimaryMouseButtonRight),
              input_settings_->current_mouse_settings()
                  .GetPrimaryButtonRight());
    EXPECT_EQ(prefs->GetBoolean(prefs::kTapDraggingEnabled),
              input_settings_->current_touchpad_settings().GetTapDragging());
    EXPECT_EQ(prefs->GetBoolean(prefs::kEnableTouchpadThreeFingerClick),
              input_settings_->current_touchpad_settings()
                  .GetThreeFingerClick());
    EXPECT_EQ(prefs->GetInteger(prefs::kMouseSensitivity),
              input_settings_->current_mouse_settings().GetSensitivity());
    EXPECT_EQ(prefs->GetInteger(prefs::kTouchpadSensitivity),
              input_settings_->current_touchpad_settings().GetSensitivity());
    EXPECT_EQ(prefs->GetBoolean(prefs::kTouchHudProjectionEnabled),
              ash::Shell::GetInstance()->is_touch_hud_projection_enabled());
    EXPECT_EQ(prefs->GetBoolean(prefs::kLanguageXkbAutoRepeatEnabled),
              keyboard_->auto_repeat_is_enabled_);
    input_method::AutoRepeatRate rate = keyboard_->last_auto_repeat_rate_;
    EXPECT_EQ(prefs->GetInteger(prefs::kLanguageXkbAutoRepeatDelay),
              (int)rate.initial_delay_in_ms);
    EXPECT_EQ(prefs->GetInteger(prefs::kLanguageXkbAutoRepeatInterval),
              (int)rate.repeat_interval_in_ms);
    EXPECT_EQ(prefs->GetString(prefs::kLanguageCurrentInputMethod),
              input_method::InputMethodManager::Get()
                  ->GetActiveIMEState()
                  ->GetCurrentInputMethod()
                  .id());
  }

  void CheckLocalStateCorrespondsToPrefs(PrefService* prefs) {
    PrefService* local_state = g_browser_process->local_state();
    EXPECT_EQ(local_state->GetBoolean(prefs::kOwnerTapToClickEnabled),
              prefs->GetBoolean(prefs::kTapToClickEnabled));
    EXPECT_EQ(local_state->GetBoolean(prefs::kOwnerPrimaryMouseButtonRight),
              prefs->GetBoolean(prefs::kPrimaryMouseButtonRight));
  }

  void DisableAnimations() {
    // Disable animations for user transitions.
    chrome::MultiUserWindowManagerChromeOS* manager =
        static_cast<chrome::MultiUserWindowManagerChromeOS*>(
            chrome::MultiUserWindowManager::GetInstance());
    manager->SetAnimationSpeedForTest(
        chrome::MultiUserWindowManagerChromeOS::ANIMATION_SPEED_DISABLED);
  }

  std::vector<AccountId> test_users_;

 private:
  system::InputDeviceSettings::FakeInterface* input_settings_;
  input_method::FakeImeKeyboard* keyboard_;

  DISALLOW_COPY_AND_ASSIGN(PreferencesTest);
};

class PreferencesServiceBrowserTest : public InProcessBrowserTest {
 public:
  PreferencesServiceBrowserTest() {}

 protected:
  static service_manager::Connector* connector() {
    return content::ServiceManagerConnection::GetForProcess()->GetConnector();
  }

  void WaitForPrefChange(PrefStore* store, const std::string& key) {
    base::RunLoop run_loop;
    TestPrefObserver observer(key, run_loop.QuitClosure());
    store->AddObserver(&observer);
    run_loop.Run();
    store->RemoveObserver(&observer);
  }

  bool GetIntegerPrefValue(PrefStore* store,
                           const std::string& key,
                           int* out_value) {
    const base::Value* value = nullptr;
    if (!store->GetValue(key, &value))
      return false;
    return value->GetAsInteger(out_value);
  }

 private:
  class TestPrefObserver : public PrefStore::Observer {
   public:
    TestPrefObserver(const std::string& pref_name,
                     const base::Closure& callback)
        : pref_name_(pref_name), callback_(callback) {}

    ~TestPrefObserver() override {}

    // PrefStore::Observer:
    void OnPrefValueChanged(const std::string& key) override {
      if (key == pref_name_)
        callback_.Run();
    }

    void OnInitializationCompleted(bool success) override {
      ASSERT_TRUE(success);
    }

   private:
    const std::string pref_name_;
    const base::Closure callback_;

    DISALLOW_COPY_AND_ASSIGN(TestPrefObserver);
  };

  DISALLOW_COPY_AND_ASSIGN(PreferencesServiceBrowserTest);
};

IN_PROC_BROWSER_TEST_F(PreferencesTest, PRE_MultiProfiles) {
  RegisterUser(test_users_[0].GetUserEmail());
  RegisterUser(test_users_[1].GetUserEmail());
  chromeos::StartupUtils::MarkOobeCompleted();
}

IN_PROC_BROWSER_TEST_F(PreferencesTest, MultiProfiles) {
  user_manager::UserManager* user_manager = user_manager::UserManager::Get();

  // Add first user and init its preferences. Check that corresponding
  // settings has been changed.
  LoginUser(test_users_[0].GetUserEmail());
  const user_manager::User* user1 = user_manager->FindUser(test_users_[0]);
  PrefService* prefs1 =
      ProfileHelper::Get()->GetProfileByUserUnsafe(user1)->GetPrefs();
  SetPrefs(prefs1, false);
  content::RunAllPendingInMessageLoop();
  CheckSettingsCorrespondToPrefs(prefs1);

  // Add second user and init its prefs with different values.
  UserAddingScreen::Get()->Start();
  content::RunAllPendingInMessageLoop();
  DisableAnimations();
  AddUser(test_users_[1].GetUserEmail());
  content::RunAllPendingInMessageLoop();
  const user_manager::User* user2 = user_manager->FindUser(test_users_[1]);
  EXPECT_TRUE(user2->is_active());
  PrefService* prefs2 =
      ProfileHelper::Get()->GetProfileByUserUnsafe(user2)->GetPrefs();
  SetPrefs(prefs2, true);

  // Check that settings were changed accordingly.
  EXPECT_TRUE(user2->is_active());
  CheckSettingsCorrespondToPrefs(prefs2);

  // Check that changing prefs of the active user doesn't affect prefs of the
  // inactive user.
  std::unique_ptr<base::DictionaryValue> prefs_backup =
      prefs1->GetPreferenceValues();
  SetPrefs(prefs2, false);
  CheckSettingsCorrespondToPrefs(prefs2);
  EXPECT_TRUE(prefs_backup->Equals(prefs1->GetPreferenceValues().get()));
  SetPrefs(prefs2, true);
  CheckSettingsCorrespondToPrefs(prefs2);
  EXPECT_TRUE(prefs_backup->Equals(prefs1->GetPreferenceValues().get()));

  // Check that changing prefs of the inactive user doesn't affect prefs of the
  // active user.
  prefs_backup = prefs2->GetPreferenceValues();
  SetPrefs(prefs1, true);
  CheckSettingsCorrespondToPrefs(prefs2);
  EXPECT_TRUE(prefs_backup->Equals(prefs2->GetPreferenceValues().get()));
  SetPrefs(prefs1, false);
  CheckSettingsCorrespondToPrefs(prefs2);
  EXPECT_TRUE(prefs_backup->Equals(prefs2->GetPreferenceValues().get()));

  // Check that changing non-owner prefs doesn't change corresponding local
  // state prefs and vice versa.
  EXPECT_EQ(user_manager->GetOwnerAccountId(), test_users_[0]);
  CheckLocalStateCorrespondsToPrefs(prefs1);
  prefs2->SetBoolean(prefs::kTapToClickEnabled,
                     !prefs1->GetBoolean(prefs::kTapToClickEnabled));
  CheckLocalStateCorrespondsToPrefs(prefs1);
  prefs1->SetBoolean(prefs::kTapToClickEnabled,
                     !prefs1->GetBoolean(prefs::kTapToClickEnabled));
  CheckLocalStateCorrespondsToPrefs(prefs1);

  // Switch user back.
  user_manager->SwitchActiveUser(test_users_[0]);
  CheckSettingsCorrespondToPrefs(prefs1);
  CheckLocalStateCorrespondsToPrefs(prefs1);
}

IN_PROC_BROWSER_TEST_F(PreferencesServiceBrowserTest, Basic) {
  constexpr int kInitialValue = 1;
  PrefService* pref_service = browser()->profile()->GetPrefs();
  pref_service->SetInteger(prefs::kMouseSensitivity, kInitialValue);

  prefs::mojom::PreferencesServiceFactoryPtr factory_a;
  connector()->BindInterface(prefs::mojom::kServiceName, &factory_a);
  scoped_refptr<preferences::PrefClientStore> pref_store_a =
      new preferences::PrefClientStore(std::move(factory_a));
  pref_store_a->Subscribe({prefs::kMouseSensitivity});
  WaitForPrefChange(pref_store_a.get(), prefs::kMouseSensitivity);

  prefs::mojom::PreferencesServiceFactoryPtr factory_b;
  connector()->BindInterface(prefs::mojom::kServiceName, &factory_b);
  scoped_refptr<preferences::PrefClientStore> pref_store_b =
      new preferences::PrefClientStore(std::move(factory_b));
  pref_store_b->Subscribe({prefs::kMouseSensitivity});
  WaitForPrefChange(pref_store_b.get(), prefs::kMouseSensitivity);

  int value_a = 0;
  ASSERT_TRUE(GetIntegerPrefValue(pref_store_a.get(), prefs::kMouseSensitivity,
                                  &value_a));
  EXPECT_EQ(kInitialValue, value_a);

  int value_b = 0;
  ASSERT_TRUE(GetIntegerPrefValue(pref_store_b.get(), prefs::kMouseSensitivity,
                                  &value_b));
  EXPECT_EQ(kInitialValue, value_b);

  const int kTestValue = 42;
  pref_store_a->SetValue(prefs::kMouseSensitivity,
                         base::MakeUnique<base::Value>(kTestValue),
                         WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
  WaitForPrefChange(pref_store_b.get(), prefs::kMouseSensitivity);
  ASSERT_TRUE(GetIntegerPrefValue(pref_store_b.get(), prefs::kMouseSensitivity,
                                  &value_b));
  EXPECT_EQ(kTestValue, value_b);
}

}  // namespace chromeos
