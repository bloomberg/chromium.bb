// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/test/active_directory_login_mixin.h"

#include "chrome/browser/chromeos/login/login_shelf_test_helper.h"
#include "chrome/browser/chromeos/login/test/js_checker.h"
#include "chrome/browser/chromeos/login/test/oobe_screen_waiter.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/ui/webui/chromeos/login/gaia_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chrome/browser/ui/webui/chromeos/login/signin_screen_handler.h"
#include "chromeos/dbus/auth_policy/fake_auth_policy_client.h"
#include "content/public/test/browser_test_utils.h"

namespace chromeos {

namespace {

constexpr char kGaiaSigninId[] = "signin-frame-dialog";
constexpr char kAdOfflineAuthId[] = "offline-ad-auth";

constexpr char kAdMachineInput[] = "machineNameInput";
constexpr char kAdMoreOptionsButton[] = "moreOptionsBtn";
constexpr char kAdUserInput[] = "userInput";
constexpr char kAdPasswordInput[] = "passwordInput";
constexpr char kAdCredsButton[] = "nextButton";
constexpr char kAdAutocompleteRealm[] = "$.userInput.querySelector('span')";

constexpr char kPasswordChangeId[] = "active-directory-password-change";
constexpr char kAdAnimatedPages[] = "animatedPages";
constexpr char kAdOldPasswordInput[] = "oldPassword";
constexpr char kAdNewPassword1Input[] = "newPassword1";
constexpr char kAdNewPassword2Input[] = "newPassword2";
constexpr char kPasswordChangeFormId[] = "inputForm";
constexpr char kFormButtonId[] = "button";

constexpr char kNavigationId[] = "navigation";
constexpr char kCloseButtonId[] = "closeButton";

}  // namespace

ActiveDirectoryLoginMixin::ActiveDirectoryLoginMixin(
    InProcessBrowserTestMixinHost* host)
    : InProcessBrowserTestMixin(host) {}

void ActiveDirectoryLoginMixin::SetUpInProcessBrowserTestFixture() {
  AuthPolicyClient::InitializeFake();
  FakeAuthPolicyClient::Get()->DisableOperationDelayForTesting();
}

void ActiveDirectoryLoginMixin::SetUpOnMainThread() {
  // Set the threshold to a max value to disable the offline message screen on
  // slow configurations like MSAN, where it otherwise triggers on every run.
  LoginDisplayHost::default_host()
      ->GetOobeUI()
      ->signin_screen_handler()
      ->SetOfflineTimeoutForTesting(base::TimeDelta::Max());

  message_queue_ = std::make_unique<content::DOMMessageQueue>();
  SetupActiveDirectoryJSNotifications();
}

void ActiveDirectoryLoginMixin::TriggerPasswordChangeScreen() {
  OobeScreenWaiter screen_waiter(
      OobeScreen::SCREEN_ACTIVE_DIRECTORY_PASSWORD_CHANGE);

  FakeAuthPolicyClient::Get()->set_auth_error(
      authpolicy::ERROR_PASSWORD_EXPIRED);
  SubmitActiveDirectoryCredentials("any_user@any_realm", "any_password");
  screen_waiter.Wait();
  TestPasswordChangeError(std::string());
}

void ActiveDirectoryLoginMixin::ClosePasswordChangeScreen() {
  test::OobeJS().TapOnPath({kPasswordChangeId, kNavigationId, kCloseButtonId});
}

void ActiveDirectoryLoginMixin::ExpectValid(const std::string& parent_id,
                                            const std::string& child_id,
                                            bool valid) {
  std::string js = test::GetOobeElementPath({parent_id, child_id}) + ".invalid";
  if (valid)
    test::OobeJS().ExpectFalse(js);
  else
    test::OobeJS().ExpectTrue(js);
}

// Checks if Active Directory login is visible.
void ActiveDirectoryLoginMixin::TestLoginVisible() {
  OobeScreenWaiter screen_waiter(GaiaView::kScreenId);
  screen_waiter.Wait();
  // Checks if Gaia signin is hidden.
  test::OobeJS().ExpectHidden(kGaiaSigninId);

  // Checks if Active Directory signin is visible.
  test::OobeJS().ExpectVisible(kAdOfflineAuthId);
  test::OobeJS().ExpectHiddenPath({kAdOfflineAuthId, kAdMachineInput});
  test::OobeJS().ExpectHiddenPath({kAdOfflineAuthId, kAdMoreOptionsButton});
  test::OobeJS().ExpectVisiblePath({kAdOfflineAuthId, kAdUserInput});
  test::OobeJS().ExpectVisiblePath({kAdOfflineAuthId, kAdPasswordInput});

  test::OobeJS().ExpectEQ(
      JSElement(kAdOfflineAuthId, kAdAutocompleteRealm) + ".innerText.trim()",
      autocomplete_realm_);

  EXPECT_TRUE(LoginShelfTestHelper().IsLoginShelfShown());
}

// Checks if Active Directory password change screen is shown.
void ActiveDirectoryLoginMixin::TestPasswordChangeVisible() {
  // Checks if Gaia signin is hidden.
  test::OobeJS().ExpectHidden(kGaiaSigninId);
  // Checks if Active Directory signin is visible.
  test::OobeJS().ExpectVisible(kPasswordChangeId);
  test::OobeJS().ExpectTrue(
      test::GetOobeElementPath({kPasswordChangeId, kAdAnimatedPages}) +
      ".selected == 0");
  test::OobeJS().ExpectVisiblePath(
      {kPasswordChangeId, kNavigationId, kCloseButtonId});
}

// Checks if user input is marked as invalid.
void ActiveDirectoryLoginMixin::TestUserError() {
  TestLoginVisible();
  ExpectValid(kAdOfflineAuthId, kAdUserInput, false);
}

void ActiveDirectoryLoginMixin::SetUserInput(const std::string& value) {
  test::OobeJS().TypeIntoPath(value, {kAdOfflineAuthId, kAdUserInput});
}

void ActiveDirectoryLoginMixin::TestUserInput(const std::string& value) {
  test::OobeJS().ExpectEQ(
      test::GetOobeElementPath({kAdOfflineAuthId, kAdUserInput}) + ".value",
      value);
}

// Checks if password input is marked as invalid.
void ActiveDirectoryLoginMixin::TestPasswordError() {
  TestLoginVisible();
  ExpectValid(kAdOfflineAuthId, kAdPasswordInput, false);
}

// Checks that machine, password and user inputs are valid.
void ActiveDirectoryLoginMixin::TestNoError() {
  TestLoginVisible();
  ExpectValid(kAdOfflineAuthId, kAdMachineInput, true);
  ExpectValid(kAdOfflineAuthId, kAdUserInput, true);
  ExpectValid(kAdOfflineAuthId, kAdPasswordInput, true);
}

// Checks if autocomplete domain is visible for the user input.
void ActiveDirectoryLoginMixin::TestDomainVisible() {
  test::OobeJS().ExpectTrue(
      "!" + JSElement(kAdOfflineAuthId, kAdAutocompleteRealm) + ".hidden");
}

// Checks if autocomplete domain is hidden for the user input.
void ActiveDirectoryLoginMixin::TestDomainHidden() {
  test::OobeJS().ExpectTrue(JSElement(kAdOfflineAuthId, kAdAutocompleteRealm) +
                            ".hidden");
}

void ActiveDirectoryLoginMixin::TestPasswordChangeNoErrors() {
  TestPasswordChangeError("");
}

void ActiveDirectoryLoginMixin::TestPasswordChangeOldPasswordError() {
  TestPasswordChangeError(kAdOldPasswordInput);
}

void ActiveDirectoryLoginMixin::TestPasswordChangeNewPasswordError() {
  TestPasswordChangeError(kAdNewPassword1Input);
}

void ActiveDirectoryLoginMixin::TestPasswordChangeConfirmNewPasswordError() {
  TestPasswordChangeError(kAdNewPassword2Input);
}

// Checks if Active Directory password change screen is shown. Also checks if
// |invalid_element| is invalidated and all the other elements are valid.
void ActiveDirectoryLoginMixin::TestPasswordChangeError(
    const std::string& invalid_element) {
  TestPasswordChangeVisible();
  for (const char* element :
       {kAdOldPasswordInput, kAdNewPassword1Input, kAdNewPassword2Input}) {
    std::string js_assertion =
        test::GetOobeElementPath({kPasswordChangeId, element}) + ".isInvalid";
    if (element != invalid_element)
      js_assertion = "!" + js_assertion;
    test::OobeJS().ExpectTrue(js_assertion);
  }
}

// Sets username and password for the Active Directory login and submits it.
void ActiveDirectoryLoginMixin::SubmitActiveDirectoryCredentials(
    const std::string& username,
    const std::string& password) {
  test::OobeJS().TypeIntoPath(username, {kAdOfflineAuthId, kAdUserInput});
  test::OobeJS().TypeIntoPath(password, {kAdOfflineAuthId, kAdPasswordInput});
  test::OobeJS().TapOnPath({kAdOfflineAuthId, kAdCredsButton});
}

// Sets username and password for the Active Directory login and submits it.
void ActiveDirectoryLoginMixin::SubmitActiveDirectoryPasswordChangeCredentials(
    const std::string& old_password,
    const std::string& new_password1,
    const std::string& new_password2) {
  test::OobeJS().TypeIntoPath(old_password,
                              {kPasswordChangeId, kAdOldPasswordInput});
  test::OobeJS().TypeIntoPath(new_password1,
                              {kPasswordChangeId, kAdNewPassword1Input});
  test::OobeJS().TypeIntoPath(new_password2,
                              {kPasswordChangeId, kAdNewPassword2Input});
  test::OobeJS().TapOnPath(
      {kPasswordChangeId, kPasswordChangeFormId, kFormButtonId});
}

void ActiveDirectoryLoginMixin::SetupActiveDirectoryJSNotifications() {
  test::OobeJS().Evaluate(
      "var testInvalidateAd = login.GaiaSigninScreen.invalidateAd;"
      "login.GaiaSigninScreen.invalidateAd = function(user, errorState) {"
      "  testInvalidateAd(user, errorState);"
      "  window.domAutomationController.send('ShowAuthError');"
      "}");
}

void ActiveDirectoryLoginMixin::WaitForAuthError() {
  const std::string& expected_message = "\"ShowAuthError\"";
  std::string message;
  do {
    ASSERT_TRUE(message_queue_->WaitForMessage(&message));
  } while (message != expected_message);
}

// Returns string representing element with id=|element_id| inside Active
// Directory login element.
std::string ActiveDirectoryLoginMixin::JSElement(const std::string& parent_id,
                                                 const std::string& selector) {
  return "document.querySelector('#" + parent_id + "')." + selector;
}

ActiveDirectoryLoginMixin::~ActiveDirectoryLoginMixin() = default;

}  // namespace chromeos
