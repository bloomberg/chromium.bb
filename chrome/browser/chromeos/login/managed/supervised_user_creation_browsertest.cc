// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>


#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/login/login_display_host_impl.h"
#include "chrome/browser/chromeos/login/login_manager_test.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/webui_login_view.h"
#include "chrome/browser/chromeos/net/network_portal_detector_stub.h"
#include "chrome/browser/managed_mode/managed_user_registration_utility.h"
#include "chrome/browser/managed_mode/managed_user_registration_utility_stub.h"
#include "chrome/common/chrome_switches.h"
#include "chromeos/cryptohome/mock_async_method_caller.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"

using testing::_;

namespace chromeos {

namespace {

const char kStubEthernetServicePath[] = "eth0";

const char kTestManager[] = "test-manager@gmail.com";
const char kTestOtherUser[] = "test-user@gmail.com";

const char kTestManagerPassword[] = "password";
const char kSupervisedUserDisplayName[] = "John Doe";
const char kSupervisedUserPassword[] = "simplepassword";

}  // namespace

class SupervisedUserCreationTest : public chromeos::LoginManagerTest {
 protected:
  SupervisedUserCreationTest() : LoginManagerTest(true),
                                 mock_async_method_caller_(NULL),
                                 network_portal_detector_stub_(NULL),
                                 registration_utility_stub_(NULL) {
  }

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    LoginManagerTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(::switches::kEnableManagedUsers);
  }

  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    LoginManagerTest::SetUpInProcessBrowserTestFixture();
    mock_async_method_caller_ = new cryptohome::MockAsyncMethodCaller;
    mock_async_method_caller_->SetUp(true, cryptohome::MOUNT_ERROR_NONE);
    cryptohome::AsyncMethodCaller::InitializeForTesting(
        mock_async_method_caller_);

    registration_utility_stub_ = new ManagedUserRegistrationUtilityStub();
    scoped_utility_.reset(
        new ScopedTestingManagedUserRegistrationUtility(
            registration_utility_stub_));

    // Setup network portal detector to return online state for both
    // ethernet and wifi networks. Ethernet is an active network by
    // default.
    network_portal_detector_stub_ =
        static_cast<NetworkPortalDetectorStub*>(
            NetworkPortalDetector::GetInstance());
    NetworkPortalDetector::CaptivePortalState online_state;
    online_state.status = NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE;
    online_state.response_code = 204;
    network_portal_detector_stub_->SetDefaultNetworkPathForTesting(
        kStubEthernetServicePath);
    network_portal_detector_stub_->SetDetectionResultsForTesting(
        kStubEthernetServicePath, online_state);
  }

  virtual void CleanUpOnMainThread() OVERRIDE {
    LoginManagerTest::CleanUpOnMainThread();
    scoped_utility_.reset(NULL);
  }

  virtual void TearDown() OVERRIDE {
    cryptohome::AsyncMethodCaller::Shutdown();
    mock_async_method_caller_ = NULL;
    LoginManagerTest::TearDown();
  }

  void JSEval(const std::string& script) {
    EXPECT_TRUE(content::ExecuteScript(web_contents(), script));
  }

  void JSExpectAsync(const std::string& function) {
    bool result;
    EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
        web_contents(),
        "(" + function + ")(function() {"
        "  window.domAutomationController.send(true);"
        "});",
        &result));
    EXPECT_TRUE(result);
  }

  void JSSetTextField(const std::string& element_selector,
                      const std::string& value) {
    std::string function;
    function.append("document.querySelector('");
    function.append(element_selector);
    function.append("').value = '");
    function.append(value);
    function.append("'");
    JSEval(function);
  }

 protected:
   cryptohome::MockAsyncMethodCaller* mock_async_method_caller_;
   NetworkPortalDetectorStub* network_portal_detector_stub_;
   ManagedUserRegistrationUtilityStub* registration_utility_stub_;
   scoped_ptr<ScopedTestingManagedUserRegistrationUtility> scoped_utility_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SupervisedUserCreationTest);
};

IN_PROC_BROWSER_TEST_F(SupervisedUserCreationTest,
    PRE_PRE_PRE_CreateAndRemoveSupervisedUser) {
  RegisterUser(kTestManager);
  RegisterUser(kTestOtherUser);
  chromeos::StartupUtils::MarkOobeCompleted();
}

IN_PROC_BROWSER_TEST_F(SupervisedUserCreationTest,
    PRE_PRE_CreateAndRemoveSupervisedUser) {
  // Create supervised user.

  // Navigate to supervised user creation screen.
  JSEval("chrome.send('showLocallyManagedUserCreationScreen')");

  // Read intro and proceed.
  JSExpect("$('managed-user-creation').currentPage_ == 'intro'");

  JSEval("$('managed-user-creation-start-button').click()");

  // Check that both users appear as managers, and test-manager@gmail.com is
  // the first one. As no manager is selected, 'next' button is disabled
  JSExpect("$('managed-user-creation').currentPage_ == 'manager'");

  JSExpect(std::string("document.querySelectorAll(")
      .append("'#managed-user-creation-managers-pane .manager-pod.focused')")
      .append(".length == 0"));
  JSExpect("$('managed-user-creation-next-button').disabled");

  JSExpect("$('managed-user-creation').managerList_.pods.length == 2");
  JSExpect(std::string("document.querySelectorAll(")
      .append("'#managed-user-creation-managers-pane .manager-pod')")
      .append(".length == 2"));
  JSExpect(std::string("document.querySelectorAll(")
      .append("'#managed-user-creation-managers-pane .manager-pod')")
      .append("[0].user.emailAddress == '").append(kTestManager).append("'"));
  // Select the first user as manager, and enter password.
  JSEval(std::string("supervisedUserManagerPod = ")
      .append("$('managed-user-creation').managerList_.pods[0]"));
  JSEval(std::string("$('managed-user-creation').managerList_")
      .append(".selectPod(supervisedUserManagerPod)"));
  JSExpect("$('managed-user-creation-next-button').disabled");
  JSSetTextField(
      "#managed-user-creation .manager-pod.focused input",
      kTestManagerPassword);

  JSEval("$('managed-user-creation').updateNextButtonForManager_()");

  // Next button is now enabled.
  JSExpect("!$('managed-user-creation-next-button').disabled");
  SetExpectedCredentials(kTestManager, kTestManagerPassword);
  content::WindowedNotificationObserver login_observer(
      chrome::NOTIFICATION_LOGIN_USER_PROFILE_PREPARED,
      content::NotificationService::AllSources());
  // Log in as manager.
  JSEval("$('managed-user-creation-next-button').click()");
  login_observer.Wait();
  // OAuth token is valid.
  UserManager::Get()->
      SaveUserOAuthStatus(kTestManager, User::OAUTH2_TOKEN_STATUS_VALID);
  base::RunLoop().RunUntilIdle();
  // Enter managed user details.
  JSExpect("$('managed-user-creation').currentPage_ == 'username'");

  JSExpect("$('managed-user-creation-next-button').disabled");
  JSSetTextField(
      "#managed-user-creation-name",
      kSupervisedUserDisplayName);
  JSEval("$('managed-user-creation').checkUserName_()");

  base::RunLoop().RunUntilIdle();

  JSSetTextField(
      "#managed-user-creation-password",
      kSupervisedUserPassword);
  JSSetTextField(
      "#managed-user-creation-password-confirm",
      kSupervisedUserPassword);

  JSEval("$('managed-user-creation').updateNextButtonForUser_()");
  JSExpect("!$('managed-user-creation-next-button').disabled");

  EXPECT_CALL(*mock_async_method_caller_, AsyncMount(_, _, _, _))
      .Times(1);
  EXPECT_CALL(*mock_async_method_caller_, AsyncGetSanitizedUsername(_, _))
      .Times(1);
  EXPECT_CALL(*mock_async_method_caller_, AsyncAddKey(_, _, _, _))
      .Times(1);

  JSEval("$('managed-user-creation-next-button').click()");

  testing::Mock::VerifyAndClearExpectations(mock_async_method_caller_);

  EXPECT_TRUE(registration_utility_stub_->register_was_called());
  EXPECT_EQ(registration_utility_stub_->display_name(),
            UTF8ToUTF16(kSupervisedUserDisplayName));

  registration_utility_stub_->RunSuccessCallback("token");

  // Token writing moves control to BlockingPool and back.
  base::RunLoop().RunUntilIdle();
  content::BrowserThread::GetBlockingPool()->FlushForTesting();
  base::RunLoop().RunUntilIdle();

  JSExpect("$('managed-user-creation').currentPage_ == 'created'");
  JSEval("$('managed-user-creation-gotit-button').click()");
}

IN_PROC_BROWSER_TEST_F(SupervisedUserCreationTest,
    PRE_CreateAndRemoveSupervisedUser) {
  // Log in as supervised user, make sure that everything works.
  ASSERT_EQ(3UL, UserManager::Get()->GetUsers().size());
  // Created supervised user have to be first in a list.
  const User* user = UserManager::Get()->GetUsers().at(0);
  ASSERT_EQ(UTF8ToUTF16(kSupervisedUserDisplayName), user->display_name());
  LoginUser(user->email());
}

IN_PROC_BROWSER_TEST_F(SupervisedUserCreationTest,
    CreateAndRemoveSupervisedUser) {
  // Remove supervised user.

  ASSERT_EQ(3UL, UserManager::Get()->GetUsers().size());
  // Created supervised user have to be first in a list.
  const User* user = UserManager::Get()->GetUsers().at(0);
  ASSERT_EQ(UTF8ToUTF16(kSupervisedUserDisplayName), user->display_name());

  // Open pod menu.
  JSExpect("!$('pod-row').pods[0].isActionBoxMenuActive");
  JSEval("$('pod-row').pods[0].querySelector('.action-box-button').click()");
  JSExpect("$('pod-row').pods[0].isActionBoxMenuActive");

  // Select "Remove user" element.
  JSExpect("$('pod-row').pods[0].actionBoxRemoveUserWarningElement.hidden");
  JSEval(std::string("$('pod-row').pods[0].")
      .append("querySelector('.action-box-menu-remove').click()"));
  JSExpect("!$('pod-row').pods[0].actionBoxRemoveUserWarningElement.hidden");

  // Confirm deletion.
  JSEval(std::string("$('pod-row').pods[0].")
      .append("querySelector('.remove-warning-button').click()"));

  // Make sure there is no supervised user in list.
  ASSERT_EQ(2UL, UserManager::Get()->GetUsers().size());
}

}  // namespace chromeos
