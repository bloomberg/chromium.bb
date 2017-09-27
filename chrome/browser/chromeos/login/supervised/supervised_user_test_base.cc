// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/supervised/supervised_user_test_base.h"

#include <string>

#include "base/compiler_specific.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/login/login_manager_test.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/supervised/supervised_user_authentication.h"
#include "chrome/browser/chromeos/login/ui/login_display_host_impl.h"
#include "chrome/browser/chromeos/login/ui/webui_login_view.h"
#include "chrome/browser/chromeos/login/users/chrome_user_manager.h"
#include "chrome/browser/chromeos/login/users/supervised_user_manager.h"
#include "chrome/browser/chromeos/login/users/supervised_user_manager_impl.h"
#include "chrome/browser/chromeos/net/network_portal_detector_test_impl.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/settings/stub_cros_settings_provider.h"
#include "chrome/browser/profiles/profile_impl.h"
#include "chrome/browser/supervised_user/legacy/supervised_user_registration_utility.h"
#include "chrome/browser/supervised_user/legacy/supervised_user_registration_utility_stub.h"
#include "chrome/browser/supervised_user/legacy/supervised_user_shared_settings_service.h"
#include "chrome/browser/supervised_user/legacy/supervised_user_shared_settings_service_factory.h"
#include "chrome/browser/supervised_user/legacy/supervised_user_sync_service.h"
#include "chrome/browser/supervised_user/legacy/supervised_user_sync_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_constants.h"
#include "chromeos/cryptohome/mock_async_method_caller.h"
#include "chromeos/cryptohome/mock_homedir_methods.h"
#include "chromeos/login/auth/key.h"
#include "chromeos/login/auth/user_context.h"
#include "components/sync/model/attachments/attachment_service_proxy_for_test.h"
#include "components/sync/model/fake_sync_change_processor.h"
#include "components/sync/model/sync_change.h"
#include "components/sync/model/sync_error_factory_mock.h"
#include "components/sync/protocol/sync.pb.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"

using testing::_;
using base::StringPrintf;

namespace chromeos {

namespace {

const char kCurrentPage[] = "$('supervised-user-creation').currentPage_";

const char kStubEthernetGuid[] = "eth0";

}

SupervisedUsersSyncTestAdapter::SupervisedUsersSyncTestAdapter(Profile* profile)
    : processor_(), next_sync_data_id_(0) {
  service_ = SupervisedUserSyncServiceFactory::GetForProfile(profile);
  processor_ = new syncer::FakeSyncChangeProcessor();
  service_->MergeDataAndStartSyncing(
      syncer::SUPERVISED_USERS, syncer::SyncDataList(),
      std::unique_ptr<syncer::SyncChangeProcessor>(processor_),
      std::unique_ptr<syncer::SyncErrorFactory>(
          new syncer::SyncErrorFactoryMock));
}

std::unique_ptr<::sync_pb::ManagedUserSpecifics>
SupervisedUsersSyncTestAdapter::GetFirstChange() {
  std::unique_ptr<::sync_pb::ManagedUserSpecifics> result(
      new ::sync_pb::ManagedUserSpecifics);
  CHECK(HasChanges())
      << "GetFirstChange() should only be callled if HasChanges() is true";
  const syncer::SyncData& data = processor_->changes().front().sync_data();
  EXPECT_EQ(syncer::SUPERVISED_USERS, data.GetDataType());
  result->CopyFrom(data.GetSpecifics().managed_user());
  return result;
}

void SupervisedUsersSyncTestAdapter::AddChange(
    const ::sync_pb::ManagedUserSpecifics& proto,
    bool update) {
  sync_pb::EntitySpecifics specifics;

  specifics.mutable_managed_user()->CopyFrom(proto);

  syncer::SyncData change_data = syncer::SyncData::CreateRemoteData(
      ++next_sync_data_id_,
      specifics,
      base::Time(),
      syncer::AttachmentIdList(),
      syncer::AttachmentServiceProxyForTest::Create());
  syncer::SyncChange change(FROM_HERE,
                            update ? syncer::SyncChange::ACTION_UPDATE
                                   : syncer::SyncChange::ACTION_ADD,
                            change_data);

  syncer::SyncChangeList change_list;
  change_list.push_back(change);

  service_->ProcessSyncChanges(FROM_HERE, change_list);
}

SupervisedUsersSharedSettingsSyncTestAdapter::
    SupervisedUsersSharedSettingsSyncTestAdapter(Profile* profile)
    : processor_(), next_sync_data_id_(0) {
  service_ =
      SupervisedUserSharedSettingsServiceFactory::GetForBrowserContext(profile);
  processor_ = new syncer::FakeSyncChangeProcessor();
  service_->MergeDataAndStartSyncing(
      syncer::SUPERVISED_USER_SHARED_SETTINGS, syncer::SyncDataList(),
      std::unique_ptr<syncer::SyncChangeProcessor>(processor_),
      std::unique_ptr<syncer::SyncErrorFactory>(
          new syncer::SyncErrorFactoryMock));
}

std::unique_ptr<::sync_pb::ManagedUserSharedSettingSpecifics>
SupervisedUsersSharedSettingsSyncTestAdapter::GetFirstChange() {
  std::unique_ptr<::sync_pb::ManagedUserSharedSettingSpecifics> result(
      new ::sync_pb::ManagedUserSharedSettingSpecifics);
  CHECK(HasChanges())
      << "GetFirstChange() should only be callled if HasChanges() is true";
  const syncer::SyncData& data = processor_->changes().front().sync_data();
  EXPECT_EQ(syncer::SUPERVISED_USER_SHARED_SETTINGS, data.GetDataType());
  result->CopyFrom(data.GetSpecifics().managed_user_shared_setting());
  return result;
}

void SupervisedUsersSharedSettingsSyncTestAdapter::AddChange(
    const ::sync_pb::ManagedUserSharedSettingSpecifics& proto,
    bool update) {
  sync_pb::EntitySpecifics specifics;

  specifics.mutable_managed_user_shared_setting()->CopyFrom(proto);

  syncer::SyncData change_data = syncer::SyncData::CreateRemoteData(
      ++next_sync_data_id_,
      specifics,
      base::Time(),
      syncer::AttachmentIdList(),
      syncer::AttachmentServiceProxyForTest::Create());
  syncer::SyncChange change(FROM_HERE,
                            update ? syncer::SyncChange::ACTION_UPDATE
                                   : syncer::SyncChange::ACTION_ADD,
                            change_data);

  syncer::SyncChangeList change_list;
  change_list.push_back(change);

  service_->ProcessSyncChanges(FROM_HERE, change_list);
}

void SupervisedUsersSharedSettingsSyncTestAdapter::AddChange(
    const std::string& mu_id,
    const std::string& key,
    const base::Value& value,
    bool acknowledged,
    bool update) {
  syncer::SyncData data =
      SupervisedUserSharedSettingsService::CreateSyncDataForSetting(
          mu_id, key, value, acknowledged);
  AddChange(data.GetSpecifics().managed_user_shared_setting(), update);
}

SupervisedUserTestBase::SupervisedUserTestBase()
    : LoginManagerTest(true),
      mock_async_method_caller_(NULL),
      mock_homedir_methods_(NULL),
      network_portal_detector_(NULL),
      registration_utility_stub_(NULL) {
}

SupervisedUserTestBase::~SupervisedUserTestBase() {
}

void SupervisedUserTestBase::SetUpInProcessBrowserTestFixture() {
  LoginManagerTest::SetUpInProcessBrowserTestFixture();
  mock_async_method_caller_ = new cryptohome::MockAsyncMethodCaller;
  mock_async_method_caller_->SetUp(true, cryptohome::MOUNT_ERROR_NONE);
  cryptohome::AsyncMethodCaller::InitializeForTesting(
      mock_async_method_caller_);

  mock_homedir_methods_ = new cryptohome::MockHomedirMethods;
  mock_homedir_methods_->SetUp(true, cryptohome::MOUNT_ERROR_NONE);
  cryptohome::HomedirMethods::InitializeForTesting(mock_homedir_methods_);

  registration_utility_stub_ = new SupervisedUserRegistrationUtilityStub();
  scoped_utility_.reset(new ScopedTestingSupervisedUserRegistrationUtility(
      registration_utility_stub_));

  // Setup network portal detector to return online state for both
  // ethernet and wifi networks. Ethernet is an active network by
  // default.
  network_portal_detector_ = new NetworkPortalDetectorTestImpl();
  network_portal_detector::InitializeForTesting(network_portal_detector_);
  NetworkPortalDetector::CaptivePortalState online_state;
  online_state.status = NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE;
  online_state.response_code = 204;
  network_portal_detector_->SetDefaultNetworkForTesting(kStubEthernetGuid);
  network_portal_detector_->SetDetectionResultsForTesting(kStubEthernetGuid,
                                                          online_state);
}

void SupervisedUserTestBase::TearDown() {
  cryptohome::AsyncMethodCaller::Shutdown();
  cryptohome::HomedirMethods::Shutdown();
  mock_homedir_methods_ = NULL;
  mock_async_method_caller_ = NULL;
  LoginManagerTest::TearDown();
}

void SupervisedUserTestBase::TearDownInProcessBrowserTestFixture() {
  network_portal_detector::Shutdown();
}

void SupervisedUserTestBase::JSEval(const std::string& script) {
  EXPECT_TRUE(content::ExecuteScript(web_contents(), script)) << script;
}

void SupervisedUserTestBase::JSEvalOrExitBrowser(const std::string& script) {
  ignore_result(content::ExecuteScript(web_contents(), script));
}

void SupervisedUserTestBase::JSExpectAsync(const std::string& function) {
  bool result;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      web_contents(),
      StringPrintf(
          "(%s)(function() { window.domAutomationController.send(true); });",
          function.c_str()),
      &result)) << function;
  EXPECT_TRUE(result);
}

void SupervisedUserTestBase::JSSetTextField(const std::string& element_selector,
                                         const std::string& value) {
  std::string function =
      StringPrintf("document.querySelector('%s').value = '%s'",
                   element_selector.c_str(),
                   value.c_str());
  JSEval(function);
}

void SupervisedUserTestBase::PrepareUsers() {
  RegisterUser(kTestManager);
  RegisterUser(kTestOtherUser);
  chromeos::StartupUtils::MarkOobeCompleted();
}

void SupervisedUserTestBase::StartFlowLoginAsManager() {
  // Navigate to supervised user creation screen.
  JSEval("chrome.send('showSupervisedUserCreationScreen')");

  // Read intro and proceed.
  JSExpect(StringPrintf("%s == 'intro'", kCurrentPage));

  JSEval("$('supervised-user-creation-start-button').click()");

  // Check that both users appear as managers, and test-manager@gmail.com is
  // the first one.
  JSExpect(StringPrintf("%s == 'manager'", kCurrentPage));

  std::string manager_pods =
      "document.querySelectorAll('#supervised-user-creation-managers-pane "
      ".manager-pod')";
  std::string selected_manager_pods =
      "document.querySelectorAll('#supervised-user-creation-managers-pane "
      ".manager-pod.focused')";

  int managers_on_device = 2;

  JSExpect(StringPrintf("%s.length == 1", selected_manager_pods.c_str()));

  JSExpect(StringPrintf(
      "$('supervised-user-creation').managerList_.pods.length == %d",
      managers_on_device));
  JSExpect(StringPrintf(
      "%s.length == %d", manager_pods.c_str(), managers_on_device));
  JSExpect(StringPrintf("%s[%d].user.emailAddress == '%s'",
                        manager_pods.c_str(),
                        0,
                        kTestManager));

  // Select the first user as manager, and enter password.
  JSExpect("$('supervised-user-creation-next-button').disabled");
  JSSetTextField("#supervised-user-creation .manager-pod.focused input",
                 kTestManagerPassword);

  JSEval("$('supervised-user-creation').updateNextButtonForManager_()");

  // Next button is now enabled.
  JSExpect("!$('supervised-user-creation-next-button').disabled");
  UserContext user_context(AccountId::FromUserEmailGaiaId(
      kTestManager, GetGaiaIDForUserID(kTestManager)));
  user_context.SetKey(Key(kTestManagerPassword));
  SetExpectedCredentials(user_context);
  content::WindowedNotificationObserver login_observer(
      chrome::NOTIFICATION_LOGIN_USER_PROFILE_PREPARED,
      content::NotificationService::AllSources());

  // Log in as manager.
  JSEval("$('supervised-user-creation-next-button').click()");
  login_observer.Wait();

  // OAuth token is valid.
  user_manager::UserManager::Get()->SaveUserOAuthStatus(
      AccountId::FromUserEmail(kTestManager),
      user_manager::User::OAUTH2_TOKEN_STATUS_VALID);
  base::RunLoop().RunUntilIdle();

  // Check the page have changed.
  JSExpect(StringPrintf("%s == 'username'", kCurrentPage));
}

void SupervisedUserTestBase::FillNewUserData(const std::string& display_name) {
  JSExpect("$('supervised-user-creation-next-button').disabled");
  JSSetTextField("#supervised-user-creation-name", display_name);
  JSEval("$('supervised-user-creation').checkUserName_()");

  base::RunLoop().RunUntilIdle();

  JSSetTextField("#supervised-user-creation-password",
                 kTestSupervisedUserPassword);
  JSSetTextField("#supervised-user-creation-password-confirm",
                 kTestSupervisedUserPassword);

  JSEval("$('supervised-user-creation').updateNextButtonForUser_()");
  JSExpect("!$('supervised-user-creation-next-button').disabled");
}

void SupervisedUserTestBase::StartUserCreation(
    const std::string& button_id,
    const std::string& expected_display_name) {
  base::RunLoop mount_wait_loop, add_key_wait_loop;
  mock_homedir_methods_->set_mount_callback(mount_wait_loop.QuitClosure());
  mock_homedir_methods_->set_add_key_callback(add_key_wait_loop.QuitClosure());
  EXPECT_CALL(*mock_homedir_methods_, MountEx(_, _, _, _)).Times(1);
  EXPECT_CALL(*mock_homedir_methods_, AddKeyEx(_, _, _, _, _)).Times(1);

  JSEval(std::string("$('").append(button_id).append("').click()"));

  mount_wait_loop.Run();
  add_key_wait_loop.Run();
  ::testing::Mock::VerifyAndClearExpectations(mock_homedir_methods_);
  mock_homedir_methods_->set_mount_callback(base::Closure());
  mock_homedir_methods_->set_add_key_callback(base::Closure());

  EXPECT_TRUE(registration_utility_stub_->register_was_called());
  EXPECT_EQ(base::UTF8ToUTF16(expected_display_name),
            registration_utility_stub_->display_name());

  registration_utility_stub_->RunSuccessCallback("token");

  // Token writing moves control to BlockingPool and back.
  content::RunAllTasksUntilIdle();

  JSExpect(StringPrintf("%s == 'created'", kCurrentPage));
  JSEvalOrExitBrowser("$('supervised-user-creation-gotit-button').click()");

  // TODO(achuith): There should probably be a wait for a specific event.
  content::RunAllPendingInMessageLoop();
}

void SupervisedUserTestBase::SigninAsSupervisedUser(
    bool check_homedir_calls,
    int user_index,
    const std::string& expected_display_name) {
  if (check_homedir_calls)
    EXPECT_CALL(*mock_homedir_methods_, MountEx(_, _, _, _)).Times(1);

  // Log in as supervised user, make sure that everything works.
  ASSERT_EQ(3UL, user_manager::UserManager::Get()->GetUsers().size());

  // Created supervised user have to be first in a list.
  const user_manager::User* user =
      user_manager::UserManager::Get()->GetUsers().at(user_index);
  ASSERT_EQ(base::UTF8ToUTF16(expected_display_name), user->display_name());

  // Clean first run flag before logging in.
  static_cast<SupervisedUserManagerImpl*>(
      ChromeUserManager::Get()->GetSupervisedUserManager())
      ->CheckForFirstRun(user->GetAccountId().GetUserEmail());

  LoginUser(user->GetAccountId().GetUserEmail());
  if (check_homedir_calls)
    ::testing::Mock::VerifyAndClearExpectations(mock_homedir_methods_);
  Profile* profile = ProfileHelper::Get()->GetProfileByUserUnsafe(user);
  shared_settings_adapter_.reset(
      new SupervisedUsersSharedSettingsSyncTestAdapter(profile));

  // Check ChromeOS preference is initialized.
  EXPECT_TRUE(
      static_cast<ProfileImpl*>(profile)->chromeos_preferences_);
}

void SupervisedUserTestBase::SigninAsManager(int user_index) {
  // Log in as supervised user, make sure that everything works.
  ASSERT_EQ(3UL, user_manager::UserManager::Get()->GetUsers().size());

  // Created supervised user have to be first in a list.
  const user_manager::User* user =
      user_manager::UserManager::Get()->GetUsers().at(user_index);
  LoginUser(user->GetAccountId().GetUserEmail());
  Profile* profile = ProfileHelper::Get()->GetProfileByUserUnsafe(user);
  shared_settings_adapter_.reset(
      new SupervisedUsersSharedSettingsSyncTestAdapter(profile));
  supervised_users_adapter_.reset(new SupervisedUsersSyncTestAdapter(profile));
}

void SupervisedUserTestBase::RemoveSupervisedUser(
    size_t original_user_count,
    int user_index,
    const std::string& expected_display_name) {
  // Remove supervised user.
  ASSERT_EQ(original_user_count,
            user_manager::UserManager::Get()->GetUsers().size());

  // Created supervised user have to be first in a list.
  const user_manager::User* user =
      user_manager::UserManager::Get()->GetUsers().at(user_index);
  ASSERT_EQ(base::UTF8ToUTF16(expected_display_name), user->display_name());

  // Open pod menu.
  JSExpect(
      StringPrintf("!$('pod-row').pods[%d].isActionBoxMenuActive", user_index));
  JSEval(StringPrintf(
      "$('pod-row').pods[%d].querySelector('.action-box-button').click()",
      user_index));
  JSExpect(
      StringPrintf("$('pod-row').pods[%d].isActionBoxMenuActive", user_index));

  // Select "Remove user" element.
  JSExpect(StringPrintf(
      "$('pod-row').pods[%d].actionBoxRemoveUserWarningElement.hidden",
      user_index));
  JSEval(StringPrintf(
      "$('pod-row').pods[%d].querySelector('.action-box-menu-remove').click()",
      user_index));
  JSExpect(StringPrintf(
      "!$('pod-row').pods[%d].actionBoxRemoveUserWarningElement.hidden",
      user_index));

  EXPECT_CALL(*mock_async_method_caller_, AsyncRemove(_, _)).Times(1);

  // Confirm deletion.
  JSEval(StringPrintf(
      "$('pod-row').pods[%d].querySelector('.remove-warning-button').click()",
      user_index));

  // Make sure there is no supervised user in list.
  ASSERT_EQ(original_user_count - 1,
            user_manager::UserManager::Get()->GetUsers().size());
}

}  // namespace chromeos
