// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/input_method/input_method_persistence.h"
#include "chrome/browser/chromeos/language_preferences.h"
#include "chrome/browser/chromeos/login/login_manager_test.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/test/oobe_screen_waiter.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/ui/webui/chromeos/login/signin_screen_handler.h"
#include "chrome/common/pref_names.h"
#include "chromeos/chromeos_switches.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/test_utils.h"

namespace chromeos {

namespace {

const char kTestUser1[] = "test-user1@gmail.com";
const char kTestUser2[] = "test-user2@gmail.com";
const char kTestUser3[] = "test-user3@gmail.com";

void Append_en_US_InputMethods(std::vector<std::string>* out) {
  out->push_back("xkb:us::eng");
  out->push_back("xkb:us:intl:eng");
  out->push_back("xkb:us:altgr-intl:eng");
  out->push_back("xkb:us:dvorak:eng");
  out->push_back("xkb:us:dvp:eng");
  out->push_back("xkb:us:colemak:eng");
  out->push_back("xkb:us:workman:eng");
  out->push_back("xkb:us:workman-intl:eng");
  chromeos::input_method::InputMethodManager::Get()->MigrateInputMethods(out);
}

class FocusPODWaiter {
 public:
  FocusPODWaiter() : focused_(false), runner_(new content::MessageLoopRunner) {
    GetOobeUI()
        ->signin_screen_handler_for_test()
        ->SetFocusPODCallbackForTesting(
            base::Bind(&FocusPODWaiter::OnFocusPOD, base::Unretained(this)));
  }
  ~FocusPODWaiter() {
    GetOobeUI()
        ->signin_screen_handler_for_test()
        ->SetFocusPODCallbackForTesting(base::Closure());
  }

  void OnFocusPOD() {
    focused_ = true;
    if (runner_.get())
      base::MessageLoopForUI::current()->PostTask(
          FROM_HERE,
          base::Bind(&FocusPODWaiter::ExitMessageLoop, base::Unretained(this)));
  }

  void ExitMessageLoop() { runner_->Quit(); }

  void Wait() {
    if (focused_)
      return;
    runner_->Run();
    GetOobeUI()
        ->signin_screen_handler_for_test()
        ->SetFocusPODCallbackForTesting(base::Closure());
    runner_ = NULL;
  }

 private:
  OobeUI* GetOobeUI() {
    OobeUI* oobe_ui = LoginDisplayHost::default_host()->GetOobeUI();
    CHECK(oobe_ui);
    return oobe_ui;
  }

  bool focused_;

  scoped_refptr<content::MessageLoopRunner> runner_;
};

}  // anonymous namespace

class LoginUIKeyboardTest : public chromeos::LoginManagerTest {
 public:
  LoginUIKeyboardTest() : LoginManagerTest(false) {}
  ~LoginUIKeyboardTest() override {}

  void SetUpOnMainThread() override {
    user_input_methods.push_back("xkb:fr::fra");
    user_input_methods.push_back("xkb:de::ger");

    chromeos::input_method::InputMethodManager::Get()->MigrateInputMethods(
        &user_input_methods);

    LoginManagerTest::SetUpOnMainThread();
  }

  // Should be called from PRE_ test so that local_state is saved to disk, and
  // reloaded in the main test.
  void InitUserLRUInputMethod() {
    PrefService* local_state = g_browser_process->local_state();

    input_method::SetUserLRUInputMethodPreferenceForTesting(
        kTestUser1, user_input_methods[0], local_state);
    input_method::SetUserLRUInputMethodPreferenceForTesting(
        kTestUser2, user_input_methods[1], local_state);
  }

 protected:
  std::vector<std::string> user_input_methods;
};

IN_PROC_BROWSER_TEST_F(LoginUIKeyboardTest, PRE_CheckPODScreenDefault) {
  RegisterUser(kTestUser1);
  RegisterUser(kTestUser2);

  StartupUtils::MarkOobeCompleted();
}

// Check default IME initialization, when there is no IME configuration in
// local_state.
IN_PROC_BROWSER_TEST_F(LoginUIKeyboardTest, CheckPODScreenDefault) {
  js_checker().ExpectEQ("$('pod-row').pods.length", 2);

  std::vector<std::string> expected_input_methods;
  Append_en_US_InputMethods(&expected_input_methods);

  EXPECT_EQ(expected_input_methods,
            input_method::InputMethodManager::Get()
                ->GetActiveIMEState()
                ->GetActiveInputMethodIds());
}

IN_PROC_BROWSER_TEST_F(LoginUIKeyboardTest, PRE_CheckPODScreenWithUsers) {
  RegisterUser(kTestUser1);
  RegisterUser(kTestUser2);

  InitUserLRUInputMethod();

  StartupUtils::MarkOobeCompleted();
}

IN_PROC_BROWSER_TEST_F(LoginUIKeyboardTest, CheckPODScreenWithUsers) {
  js_checker().ExpectEQ("$('pod-row').pods.length", 2);

  EXPECT_EQ(user_input_methods[0],
            input_method::InputMethodManager::Get()
                ->GetActiveIMEState()
                ->GetCurrentInputMethod()
                .id());

  std::vector<std::string> expected_input_methods;
  Append_en_US_InputMethods(&expected_input_methods);
  // Active IM for the first user (active user POD).
  expected_input_methods.push_back(user_input_methods[0]);

  EXPECT_EQ(expected_input_methods,
            input_method::InputMethodManager::Get()
                ->GetActiveIMEState()
                ->GetActiveInputMethodIds());

  FocusPODWaiter waiter;
  js_checker().Evaluate("$('pod-row').focusPod($('pod-row').pods[1])");
  waiter.Wait();

  EXPECT_EQ(user_input_methods[1],
            input_method::InputMethodManager::Get()
                ->GetActiveIMEState()
                ->GetCurrentInputMethod()
                .id());
}

class LoginUIKeyboardTestWithUsersAndOwner : public chromeos::LoginManagerTest {
 public:
  LoginUIKeyboardTestWithUsersAndOwner() : LoginManagerTest(false) {}
  ~LoginUIKeyboardTestWithUsersAndOwner() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    LoginManagerTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kStubCrosSettings);

    LoginManagerTest::SetUpCommandLine(command_line);
  }

  void SetUpOnMainThread() override {
    user_input_methods.push_back("xkb:fr::fra");
    user_input_methods.push_back("xkb:de::ger");
    user_input_methods.push_back("xkb:pl::pol");

    chromeos::input_method::InputMethodManager::Get()->MigrateInputMethods(
        &user_input_methods);

    CrosSettings::Get()->SetString(kDeviceOwner, kTestUser3);

    LoginManagerTest::SetUpOnMainThread();
  }

  // Should be called from PRE_ test so that local_state is saved to disk, and
  // reloaded in the main test.
  void InitUserLRUInputMethod() {
    PrefService* local_state = g_browser_process->local_state();

    input_method::SetUserLRUInputMethodPreferenceForTesting(
        kTestUser1, user_input_methods[0], local_state);
    input_method::SetUserLRUInputMethodPreferenceForTesting(
        kTestUser2, user_input_methods[1], local_state);
    input_method::SetUserLRUInputMethodPreferenceForTesting(
        kTestUser3, user_input_methods[2], local_state);

    local_state->SetString(language_prefs::kPreferredKeyboardLayout,
                           user_input_methods[2]);
  }

  void CheckGaiaKeyboard();

 protected:
  std::vector<std::string> user_input_methods;
};

void LoginUIKeyboardTestWithUsersAndOwner::CheckGaiaKeyboard() {
  std::vector<std::string> expected_input_methods;
  // kPreferredKeyboardLayout is now set to last focused POD.
  expected_input_methods.push_back(user_input_methods[0]);
  // Locale default input methods (the first one also is hardware IM).
  Append_en_US_InputMethods(&expected_input_methods);

  EXPECT_EQ(expected_input_methods,
            input_method::InputMethodManager::Get()
                ->GetActiveIMEState()
                ->GetActiveInputMethodIds());
}

IN_PROC_BROWSER_TEST_F(LoginUIKeyboardTestWithUsersAndOwner,
                       PRE_CheckPODScreenKeyboard) {
  RegisterUser(kTestUser1);
  RegisterUser(kTestUser2);
  RegisterUser(kTestUser3);

  InitUserLRUInputMethod();

  StartupUtils::MarkOobeCompleted();
}

IN_PROC_BROWSER_TEST_F(LoginUIKeyboardTestWithUsersAndOwner,
                       CheckPODScreenKeyboard) {
  js_checker().ExpectEQ("$('pod-row').pods.length", 3);

  std::vector<std::string> expected_input_methods;
  // Owner input method.
  expected_input_methods.push_back(user_input_methods[2]);
  // Locale default input methods (the first one also is hardware IM).
  Append_en_US_InputMethods(&expected_input_methods);
  // Active IM for the first user (active user POD).
  expected_input_methods.push_back(user_input_methods[0]);

  EXPECT_EQ(expected_input_methods,
            input_method::InputMethodManager::Get()
                ->GetActiveIMEState()
                ->GetActiveInputMethodIds());

  // Switch to Gaia.
  js_checker().Evaluate("$('add-user-button').click()");
  OobeScreenWaiter(OobeDisplay::SCREEN_GAIA_SIGNIN).Wait();
  CheckGaiaKeyboard();

  // Switch back.
  js_checker().Evaluate("$('gaia-signin').cancel()");
  OobeScreenWaiter(OobeDisplay::SCREEN_ACCOUNT_PICKER).Wait();

  EXPECT_EQ(expected_input_methods,
            input_method::InputMethodManager::Get()
                ->GetActiveIMEState()
                ->GetActiveInputMethodIds());
}
}  // namespace chromeos
