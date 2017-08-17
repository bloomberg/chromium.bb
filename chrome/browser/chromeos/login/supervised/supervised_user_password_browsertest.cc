// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/login/login_manager_test.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/supervised/supervised_user_authentication.h"
#include "chrome/browser/chromeos/login/supervised/supervised_user_test_base.h"
#include "chrome/browser/chromeos/login/ui/login_display_host_impl.h"
#include "chrome/browser/chromeos/login/ui/webui_login_view.h"
#include "chrome/browser/chromeos/login/users/chrome_user_manager.h"
#include "chrome/browser/chromeos/login/users/supervised_user_manager.h"
#include "chrome/browser/chromeos/net/network_portal_detector_test_impl.h"
#include "chrome/browser/chromeos/settings/stub_cros_settings_provider.h"
#include "chrome/browser/supervised_user/supervised_user_constants.h"
#include "chrome/common/chrome_features.h"
#include "chromeos/cryptohome/mock_async_method_caller.h"
#include "chromeos/cryptohome/mock_homedir_methods.h"
#include "components/sync/model/attachments/attachment_service_proxy_for_test.h"
#include "components/sync/model/fake_sync_change_processor.h"
#include "components/sync/model/sync_change.h"
#include "components/sync/model/sync_error_factory_mock.h"
#include "components/sync/protocol/sync.pb.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"

using testing::_;
using chromeos::SupervisedUserTestBase;
using chromeos::kTestSupervisedUserDisplayName;
using chromeos::kTestManager;

namespace chromeos {

class SupervisedUserPasswordTest : public SupervisedUserTestBase {
 public:
  SupervisedUserPasswordTest() : SupervisedUserTestBase() {}

  void SetUpInProcessBrowserTestFixture() override {
    SupervisedUserTestBase::SetUpInProcessBrowserTestFixture();
    scoped_feature_list_.InitAndEnableFeature(
        features::kSupervisedUserCreation);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(SupervisedUserPasswordTest);
};

class SupervisedUserPasswordManagerTest : public SupervisedUserTestBase {
 public:
  SupervisedUserPasswordManagerTest() : SupervisedUserTestBase() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(SupervisedUserPasswordManagerTest);
};

IN_PROC_BROWSER_TEST_F(SupervisedUserPasswordTest,
                       PRE_PRE_PRE_PasswordChangeFromUserTest) {
  PrepareUsers();
}

IN_PROC_BROWSER_TEST_F(SupervisedUserPasswordTest,
                       PRE_PRE_PasswordChangeFromUserTest) {
  StartFlowLoginAsManager();
  FillNewUserData(kTestSupervisedUserDisplayName);
  StartUserCreation("supervised-user-creation-next-button",
                    kTestSupervisedUserDisplayName);
}

// Supervised user signs in, get sync notification about password update, and
// schedules password migration.
IN_PROC_BROWSER_TEST_F(SupervisedUserPasswordTest,
                       PRE_PasswordChangeFromUserTest) {
  SigninAsSupervisedUser(true, 0, kTestSupervisedUserDisplayName);

  const user_manager::User* user =
      user_manager::UserManager::Get()->GetUsers().at(0);
  std::string sync_id =
      ChromeUserManager::Get()->GetSupervisedUserManager()->GetUserSyncId(
          user->GetAccountId().GetUserEmail());
  base::DictionaryValue password;
  password.SetKey(
      kSchemaVersion,
      base::Value(SupervisedUserAuthentication::SCHEMA_SALT_HASHED));
  password.SetKey(kPasswordRevision, base::Value(2));

  password.SetKey(kPasswordSignature, base::Value("signature"));
  password.SetKey(kEncryptedPassword, base::Value("new-encrypted-password"));

  shared_settings_adapter_->AddChange(
      sync_id, supervised_users::kChromeOSPasswordData, password, true, false);
  content::RunAllPendingInMessageLoop();
}

// Supervised user signs in for second time, and actual password migration takes
// place.
IN_PROC_BROWSER_TEST_F(SupervisedUserPasswordTest,
                       PasswordChangeFromUserTest) {
  EXPECT_CALL(*mock_homedir_methods_, MountEx(_, _, _, _)).Times(1);
  EXPECT_CALL(*mock_homedir_methods_, UpdateKeyEx(_, _, _, _, _)).Times(1);
  SigninAsSupervisedUser(false, 0, kTestSupervisedUserDisplayName);
  testing::Mock::VerifyAndClearExpectations(mock_homedir_methods_);
}

IN_PROC_BROWSER_TEST_F(SupervisedUserPasswordTest,
                       PRE_PRE_PRE_PasswordChangeFromManagerTest) {
  PrepareUsers();
}

IN_PROC_BROWSER_TEST_F(SupervisedUserPasswordTest,
                       PRE_PRE_PasswordChangeFromManagerTest) {
  StartFlowLoginAsManager();
  FillNewUserData(kTestSupervisedUserDisplayName);
  StartUserCreation("supervised-user-creation-next-button",
                    kTestSupervisedUserDisplayName);
}

// Manager signs in, gets sync notification about supervised user password
// update, and performs migration.
IN_PROC_BROWSER_TEST_F(SupervisedUserPasswordTest,
                       PRE_PasswordChangeFromManagerTest) {
  const user_manager::User* supervised_user =
      user_manager::UserManager::Get()->GetUsers().at(0);

  SigninAsManager(1);

  EXPECT_CALL(*mock_homedir_methods_, AddKeyEx(_, _, _, _, _)).Times(1);

  std::string sync_id =
      ChromeUserManager::Get()->GetSupervisedUserManager()->GetUserSyncId(
          supervised_user->GetAccountId().GetUserEmail());

  ::sync_pb::ManagedUserSpecifics managed_user_proto;

  managed_user_proto.set_id(sync_id);
  managed_user_proto.set_name(kTestSupervisedUserDisplayName);
  managed_user_proto.set_acknowledged(true);
  managed_user_proto.set_master_key("master key");
  managed_user_proto.set_password_signature_key("signature_key");
  managed_user_proto.set_password_encryption_key("encryption_key");

  supervised_users_adapter_->AddChange(managed_user_proto, false);
  content::RunAllPendingInMessageLoop();

  base::DictionaryValue password;
  password.SetKey(
      kSchemaVersion,
      base::Value(SupervisedUserAuthentication::SCHEMA_SALT_HASHED));
  password.SetKey(kPasswordRevision, base::Value(2));

  password.SetKey(kPasswordSignature, base::Value("signature"));
  password.SetKey(kEncryptedPassword, base::Value("new-encrypted-password"));
  shared_settings_adapter_->AddChange(
      sync_id, supervised_users::kChromeOSPasswordData, password, true, false);
  content::RunAllPendingInMessageLoop();

  testing::Mock::VerifyAndClearExpectations(mock_homedir_methods_);
}

// After that supervised user signs in, and no password change happens.
IN_PROC_BROWSER_TEST_F(SupervisedUserPasswordTest,
                       PasswordChangeFromManagerTest) {
  EXPECT_CALL(*mock_homedir_methods_, MountEx(_, _, _, _)).Times(1);
  EXPECT_CALL(*mock_homedir_methods_, UpdateKeyEx(_, _, _, _, _)).Times(0);
  SigninAsSupervisedUser(false, 1, kTestSupervisedUserDisplayName);
  testing::Mock::VerifyAndClearExpectations(mock_homedir_methods_);
}

IN_PROC_BROWSER_TEST_F(
    SupervisedUserPasswordTest,
    PRE_PRE_PRE_PRE_PasswordChangeUserAndManagerTest) {
  PrepareUsers();
}

IN_PROC_BROWSER_TEST_F(SupervisedUserPasswordTest,
                       PRE_PRE_PRE_PasswordChangeUserAndManagerTest) {
  StartFlowLoginAsManager();
  FillNewUserData(kTestSupervisedUserDisplayName);
  StartUserCreation("supervised-user-creation-next-button",
                    kTestSupervisedUserDisplayName);
}

// Supervised user signs in, get sync notification about password update, and
// schedules password migration.
IN_PROC_BROWSER_TEST_F(SupervisedUserPasswordTest,
                       PRE_PRE_PasswordChangeUserAndManagerTest) {
  SigninAsSupervisedUser(true, 0, kTestSupervisedUserDisplayName);

  const user_manager::User* user =
      user_manager::UserManager::Get()->GetUsers().at(0);
  std::string sync_id =
      ChromeUserManager::Get()->GetSupervisedUserManager()->GetUserSyncId(
          user->GetAccountId().GetUserEmail());
  base::DictionaryValue password;
  password.SetKey(
      kSchemaVersion,
      base::Value(SupervisedUserAuthentication::SCHEMA_SALT_HASHED));
  password.SetKey(kPasswordRevision, base::Value(2));

  password.SetKey(kPasswordSignature, base::Value("signature"));
  password.SetKey(kEncryptedPassword, base::Value("new-encrypted-password"));

  shared_settings_adapter_->AddChange(
      sync_id, supervised_users::kChromeOSPasswordData, password, true, false);
  content::RunAllPendingInMessageLoop();
}

// After that manager signs in, and also detects password change. Manager
// performs the migration.
IN_PROC_BROWSER_TEST_F(SupervisedUserPasswordTest,
                       PRE_PasswordChangeUserAndManagerTest) {
  const user_manager::User* supervised_user =
      user_manager::UserManager::Get()->GetUsers().at(0);

  SigninAsManager(1);

  EXPECT_CALL(*mock_homedir_methods_, AddKeyEx(_, _, _, _, _)).Times(1);

  std::string sync_id =
      ChromeUserManager::Get()->GetSupervisedUserManager()->GetUserSyncId(
          supervised_user->GetAccountId().GetUserEmail());

  ::sync_pb::ManagedUserSpecifics managed_user_proto;

  managed_user_proto.set_id(sync_id);
  managed_user_proto.set_name(kTestSupervisedUserDisplayName);
  managed_user_proto.set_acknowledged(true);
  managed_user_proto.set_master_key("master key");
  managed_user_proto.set_password_signature_key("signature_key");
  managed_user_proto.set_password_encryption_key("encryption_key");

  supervised_users_adapter_->AddChange(managed_user_proto, false);
  content::RunAllPendingInMessageLoop();

  base::DictionaryValue password;
  password.SetKey(
      kSchemaVersion,
      base::Value(SupervisedUserAuthentication::SCHEMA_SALT_HASHED));
  password.SetKey(kPasswordRevision, base::Value(2));

  password.SetKey(kPasswordSignature, base::Value("signature"));
  password.SetKey(kEncryptedPassword, base::Value("new-encrypted-password"));
  shared_settings_adapter_->AddChange(
      sync_id, supervised_users::kChromeOSPasswordData, password, true, false);
  content::RunAllPendingInMessageLoop();

  testing::Mock::VerifyAndClearExpectations(mock_homedir_methods_);
}

// When supervised user signs in, password is already migrated, so no migration
// should be attempted.
IN_PROC_BROWSER_TEST_F(SupervisedUserPasswordTest,
                       PasswordChangeUserAndManagerTest) {
  EXPECT_CALL(*mock_homedir_methods_, MountEx(_, _, _, _)).Times(1);
  EXPECT_CALL(*mock_homedir_methods_, UpdateKeyEx(_, _, _, _, _)).Times(0);
  SigninAsSupervisedUser(false, 1, kTestSupervisedUserDisplayName);
  testing::Mock::VerifyAndClearExpectations(mock_homedir_methods_);
}

}  // namespace chromeos
