// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>


#include "base/compiler_specific.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/login/login_display_host_impl.h"
#include "chrome/browser/chromeos/login/login_manager_test.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/webui_login_view.h"
#include "chrome/browser/chromeos/net/network_portal_detector_test_impl.h"
#include "chrome/browser/chromeos/settings/stub_cros_settings_provider.h"
#include "chrome/browser/managed_mode/managed_user_registration_utility.h"
#include "chrome/browser/managed_mode/managed_user_registration_utility_stub.h"
#include "chromeos/cryptohome/mock_async_method_caller.h"
#include "chromeos/cryptohome/mock_homedir_methods.h"
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

class SupervisedUserTest : public chromeos::LoginManagerTest {
 protected:
  SupervisedUserTest() : LoginManagerTest(true),
                         mock_async_method_caller_(NULL),
                         mock_homedir_methods_(NULL),
                         network_portal_detector_(NULL),
                         registration_utility_stub_(NULL) {
  }

  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    LoginManagerTest::SetUpInProcessBrowserTestFixture();
    mock_async_method_caller_ = new cryptohome::MockAsyncMethodCaller;
    mock_async_method_caller_->SetUp(true, cryptohome::MOUNT_ERROR_NONE);
    cryptohome::AsyncMethodCaller::InitializeForTesting(
        mock_async_method_caller_);

    mock_homedir_methods_ = new cryptohome::MockHomedirMethods;
    mock_homedir_methods_->SetUp(true, cryptohome::MOUNT_ERROR_NONE);
    cryptohome::HomedirMethods::InitializeForTesting(mock_homedir_methods_);

    registration_utility_stub_ = new ManagedUserRegistrationUtilityStub();
    scoped_utility_.reset(
        new ScopedTestingManagedUserRegistrationUtility(
            registration_utility_stub_));

    // Setup network portal detector to return online state for both
    // ethernet and wifi networks. Ethernet is an active network by
    // default.
    network_portal_detector_ = new NetworkPortalDetectorTestImpl();
    NetworkPortalDetector::InitializeForTesting(network_portal_detector_);
    NetworkPortalDetector::CaptivePortalState online_state;
    online_state.status = NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE;
    online_state.response_code = 204;
    network_portal_detector_->SetDefaultNetworkPathForTesting(
        kStubEthernetServicePath);
    network_portal_detector_->SetDetectionResultsForTesting(
        kStubEthernetServicePath, online_state);
  }

  virtual void CleanUpOnMainThread() OVERRIDE {
    LoginManagerTest::CleanUpOnMainThread();
    scoped_utility_.reset(NULL);
  }

  virtual void TearDown() OVERRIDE {
    cryptohome::AsyncMethodCaller::Shutdown();
    cryptohome::HomedirMethods::Shutdown();
    mock_homedir_methods_ = NULL;
    mock_async_method_caller_ = NULL;
    LoginManagerTest::TearDown();
  }

  virtual void TearDownInProcessBrowserTestFixture() OVERRIDE {
    NetworkPortalDetector::Shutdown();
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

  void PrepareUsers();
  void CreateSupervisedUser();
  void SigninAsSupervisedUser();
  void RemoveSupervisedUser();
  void LogInAsManagerAndFillUserData();

 protected:
   cryptohome::MockAsyncMethodCaller* mock_async_method_caller_;
   cryptohome::MockHomedirMethods* mock_homedir_methods_;
   NetworkPortalDetectorTestImpl* network_portal_detector_;
   ManagedUserRegistrationUtilityStub* registration_utility_stub_;
   scoped_ptr<ScopedTestingManagedUserRegistrationUtility> scoped_utility_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SupervisedUserTest);
};

void SupervisedUserTest::PrepareUsers() {
  RegisterUser(kTestManager);
  RegisterUser(kTestOtherUser);
  chromeos::StartupUtils::MarkOobeCompleted();
}

void SupervisedUserTest::LogInAsManagerAndFillUserData() {
  // Create supervised user.

  // Navigate to supervised user creation screen.
  JSEval("chrome.send('showLocallyManagedUserCreationScreen')");

  // Read intro and proceed.
  JSExpect("$('managed-user-creation').currentPage_ == 'intro'");

  JSEval("$('managed-user-creation-start-button').click()");

  // Check that both users appear as managers, and test-manager@gmail.com is
  // the first one.
  JSExpect("$('managed-user-creation').currentPage_ == 'manager'");

  JSExpect(std::string("document.querySelectorAll(")
      .append("'#managed-user-creation-managers-pane .manager-pod.focused')")
      .append(".length == 1"));

  JSExpect("$('managed-user-creation').managerList_.pods.length == 2");
  JSExpect(std::string("document.querySelectorAll(")
      .append("'#managed-user-creation-managers-pane .manager-pod')")
      .append(".length == 2"));
  JSExpect(std::string("document.querySelectorAll(")
      .append("'#managed-user-creation-managers-pane .manager-pod')")
      .append("[0].user.emailAddress == '").append(kTestManager).append("'"));
  // Select the first user as manager, and enter password.
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
}

void SupervisedUserTest::CreateSupervisedUser() {
  LogInAsManagerAndFillUserData();

  EXPECT_CALL(*mock_homedir_methods_, MountEx(_, _, _, _)).Times(1);
  EXPECT_CALL(*mock_homedir_methods_, AddKeyEx(_, _, _, _, _)).Times(1);

  JSEval("$('managed-user-creation-next-button').click()");

  testing::Mock::VerifyAndClearExpectations(mock_homedir_methods_);

  EXPECT_TRUE(registration_utility_stub_->register_was_called());
  EXPECT_EQ(registration_utility_stub_->display_name(),
            base::UTF8ToUTF16(kSupervisedUserDisplayName));

  registration_utility_stub_->RunSuccessCallback("token");

  // Token writing moves control to BlockingPool and back.
  base::RunLoop().RunUntilIdle();
  content::BrowserThread::GetBlockingPool()->FlushForTesting();
  base::RunLoop().RunUntilIdle();

  JSExpect("$('managed-user-creation').currentPage_ == 'created'");
  JSEval("$('managed-user-creation-gotit-button').click()");
}

void SupervisedUserTest::SigninAsSupervisedUser() {
  EXPECT_CALL(*mock_homedir_methods_, MountEx(_, _, _, _)).Times(1);
  // Log in as supervised user, make sure that everything works.
  ASSERT_EQ(3UL, UserManager::Get()->GetUsers().size());
  // Created supervised user have to be first in a list.
  const User* user = UserManager::Get()->GetUsers().at(0);
  ASSERT_EQ(base::UTF8ToUTF16(kSupervisedUserDisplayName),
            user->display_name());
  LoginUser(user->email());
  testing::Mock::VerifyAndClearExpectations(mock_homedir_methods_);
}

void SupervisedUserTest::RemoveSupervisedUser() {
  // Remove supervised user.

  ASSERT_EQ(3UL, UserManager::Get()->GetUsers().size());
  // Created supervised user have to be first in a list.
  const User* user = UserManager::Get()->GetUsers().at(0);
  ASSERT_EQ(base::UTF8ToUTF16(kSupervisedUserDisplayName),
            user->display_name());

  // Open pod menu.
  JSExpect("!$('pod-row').pods[0].isActionBoxMenuActive");
  JSEval("$('pod-row').pods[0].querySelector('.action-box-button').click()");
  JSExpect("$('pod-row').pods[0].isActionBoxMenuActive");

  // Select "Remove user" element.
  JSExpect("$('pod-row').pods[0].actionBoxRemoveUserWarningElement.hidden");
  JSEval(std::string("$('pod-row').pods[0].")
      .append("querySelector('.action-box-menu-remove').click()"));
  JSExpect("!$('pod-row').pods[0].actionBoxRemoveUserWarningElement.hidden");

  EXPECT_CALL(*mock_async_method_caller_, AsyncRemove(_, _)).Times(1);

  // Confirm deletion.
  JSEval(std::string("$('pod-row').pods[0].")
      .append("querySelector('.remove-warning-button').click()"));

  // Make sure there is no supervised user in list.
  ASSERT_EQ(2UL, UserManager::Get()->GetUsers().size());
}

class SupervisedUserCreationTest : public SupervisedUserTest {
 public:
  SupervisedUserCreationTest() : SupervisedUserTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(SupervisedUserCreationTest);
};

class SupervisedUserTransactionCleanupTest : public SupervisedUserTest {
 public:
  SupervisedUserTransactionCleanupTest() : SupervisedUserTest () {}

 private:
  DISALLOW_COPY_AND_ASSIGN(SupervisedUserTransactionCleanupTest);
};

class SupervisedUserOwnerCreationTest : public SupervisedUserTest {
 public:
  SupervisedUserOwnerCreationTest() : SupervisedUserTest() {}

  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    SupervisedUserTest::SetUpInProcessBrowserTestFixture();
    cros_settings_provider_.reset(new StubCrosSettingsProvider());
    cros_settings_provider_->Set(kDeviceOwner, base::StringValue(kTestManager));
  }

 private:
  scoped_ptr<StubCrosSettingsProvider> cros_settings_provider_;
  DISALLOW_COPY_AND_ASSIGN(SupervisedUserOwnerCreationTest);
};

IN_PROC_BROWSER_TEST_F(SupervisedUserCreationTest,
    PRE_PRE_PRE_CreateAndRemoveSupervisedUser) {
  PrepareUsers();
}

IN_PROC_BROWSER_TEST_F(SupervisedUserCreationTest,
    PRE_PRE_CreateAndRemoveSupervisedUser) {
  CreateSupervisedUser();
}

IN_PROC_BROWSER_TEST_F(SupervisedUserCreationTest,
    PRE_CreateAndRemoveSupervisedUser) {
  SigninAsSupervisedUser();
}

IN_PROC_BROWSER_TEST_F(SupervisedUserCreationTest,
    CreateAndRemoveSupervisedUser) {
  RemoveSupervisedUser();
}

IN_PROC_BROWSER_TEST_F(SupervisedUserOwnerCreationTest,
    PRE_PRE_PRE_CreateAndRemoveSupervisedUser) {
  PrepareUsers();
}

IN_PROC_BROWSER_TEST_F(SupervisedUserOwnerCreationTest,
    PRE_PRE_CreateAndRemoveSupervisedUser) {
  CreateSupervisedUser();
}

IN_PROC_BROWSER_TEST_F(SupervisedUserOwnerCreationTest,
    PRE_CreateAndRemoveSupervisedUser) {
  SigninAsSupervisedUser();
}

IN_PROC_BROWSER_TEST_F(SupervisedUserOwnerCreationTest,
    CreateAndRemoveSupervisedUser) {
  RemoveSupervisedUser();
}

IN_PROC_BROWSER_TEST_F(SupervisedUserTransactionCleanupTest,
    PRE_PRE_CreateAndCancelSupervisedUser) {
  PrepareUsers();
}

IN_PROC_BROWSER_TEST_F(SupervisedUserTransactionCleanupTest,
    PRE_CreateAndCancelSupervisedUser) {
  LogInAsManagerAndFillUserData();

  EXPECT_CALL(*mock_homedir_methods_, MountEx(_, _, _, _)).Times(1);
  EXPECT_CALL(*mock_homedir_methods_, AddKeyEx(_, _, _, _, _)).Times(1);

  JSEval("$('managed-user-creation-next-button').click()");

  testing::Mock::VerifyAndClearExpectations(mock_homedir_methods_);

  EXPECT_TRUE(registration_utility_stub_->register_was_called());
  EXPECT_EQ(registration_utility_stub_->display_name(),
            base::UTF8ToUTF16(kSupervisedUserDisplayName));

  std::string user_id = registration_utility_stub_->managed_user_id();
  // Make sure user is already in list.
  ASSERT_EQ(3UL, UserManager::Get()->GetUsers().size());
  // We wait for token now. Press cancel button at this point.
  JSEval("$('cancel-add-user-button').click()");
}

IN_PROC_BROWSER_TEST_F(SupervisedUserTransactionCleanupTest,
    CreateAndCancelSupervisedUser) {
  // Make sure there is no supervised user in list.
  ASSERT_EQ(2UL, UserManager::Get()->GetUsers().size());
}

}  // namespace chromeos
