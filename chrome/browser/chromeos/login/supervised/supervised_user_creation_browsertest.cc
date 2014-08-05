// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/compiler_specific.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/login/login_manager_test.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/supervised/supervised_user_authentication.h"
#include "chrome/browser/chromeos/login/supervised/supervised_user_test_base.h"
#include "chrome/browser/chromeos/login/ui/login_display_host_impl.h"
#include "chrome/browser/chromeos/login/ui/webui_login_view.h"
#include "chrome/browser/chromeos/login/users/supervised_user_manager.h"
#include "chrome/browser/chromeos/net/network_portal_detector_test_impl.h"
#include "chrome/browser/chromeos/settings/stub_cros_settings_provider.h"
#include "chrome/browser/supervised_user/supervised_user_constants.h"
#include "chrome/browser/supervised_user/supervised_user_registration_utility.h"
#include "chrome/browser/supervised_user/supervised_user_registration_utility_stub.h"
#include "chrome/browser/supervised_user/supervised_user_shared_settings_service.h"
#include "chrome/browser/supervised_user/supervised_user_shared_settings_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_sync_service.h"
#include "chrome/browser/supervised_user/supervised_user_sync_service_factory.h"
#include "chromeos/cryptohome/mock_async_method_caller.h"
#include "chromeos/cryptohome/mock_homedir_methods.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "sync/api/fake_sync_change_processor.h"
#include "sync/api/sync_change.h"
#include "sync/api/sync_error_factory_mock.h"
#include "sync/internal_api/public/attachments/attachment_service_proxy_for_test.h"
#include "sync/protocol/sync.pb.h"

using testing::_;
using chromeos::SupervisedUserTestBase;
using chromeos::kTestSupervisedUserDisplayName;
using chromeos::kTestManager;

namespace chromeos {

class SupervisedUserCreationTest : public SupervisedUserTestBase {
 public:
  SupervisedUserCreationTest() : SupervisedUserTestBase() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(SupervisedUserCreationTest);
};

class SupervisedUserTransactionCleanupTest : public SupervisedUserTestBase {
 public:
  SupervisedUserTransactionCleanupTest() : SupervisedUserTestBase() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(SupervisedUserTransactionCleanupTest);
};

class SupervisedUserOwnerCreationTest : public SupervisedUserTestBase {
 public:
  SupervisedUserOwnerCreationTest() : SupervisedUserTestBase() {}

  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    SupervisedUserTestBase::SetUpInProcessBrowserTestFixture();
    cros_settings_provider_.reset(new StubCrosSettingsProvider());
    cros_settings_provider_->Set(kDeviceOwner, base::StringValue(kTestManager));
  }

 private:
  scoped_ptr<StubCrosSettingsProvider> cros_settings_provider_;
  DISALLOW_COPY_AND_ASSIGN(SupervisedUserOwnerCreationTest);
};

class SupervisedUserTransactionCleanupTest2
    : public SupervisedUserTransactionCleanupTest {
 public:
  SupervisedUserTransactionCleanupTest2()
      : SupervisedUserTransactionCleanupTest() {}

  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    SupervisedUserTransactionCleanupTest::SetUpInProcessBrowserTestFixture();
    EXPECT_CALL(*mock_async_method_caller_, AsyncRemove(_, _)).Times(1);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SupervisedUserTransactionCleanupTest2);
};

IN_PROC_BROWSER_TEST_F(SupervisedUserCreationTest,
                       DISABLED_PRE_PRE_PRE_CreateAndRemoveSupervisedUser) {
  PrepareUsers();
}

IN_PROC_BROWSER_TEST_F(SupervisedUserCreationTest,
                       DISABLED_PRE_PRE_CreateAndRemoveSupervisedUser) {
  StartFlowLoginAsManager();
  FillNewUserData(kTestSupervisedUserDisplayName);
  StartUserCreation("supervised-user-creation-next-button",
                    kTestSupervisedUserDisplayName);
}

IN_PROC_BROWSER_TEST_F(SupervisedUserCreationTest,
                       DISABLED_PRE_CreateAndRemoveSupervisedUser) {
  SigninAsSupervisedUser(true, 0, kTestSupervisedUserDisplayName);
}

IN_PROC_BROWSER_TEST_F(SupervisedUserCreationTest,
                       DISABLED_CreateAndRemoveSupervisedUser) {
  RemoveSupervisedUser(3, 0, kTestSupervisedUserDisplayName);
}

IN_PROC_BROWSER_TEST_F(SupervisedUserOwnerCreationTest,
                       DISABLED_PRE_PRE_PRE_CreateAndRemoveSupervisedUser) {
  PrepareUsers();
}

IN_PROC_BROWSER_TEST_F(SupervisedUserOwnerCreationTest,
                       DISABLED_PRE_PRE_CreateAndRemoveSupervisedUser) {
  StartFlowLoginAsManager();
  FillNewUserData(kTestSupervisedUserDisplayName);
  StartUserCreation("supervised-user-creation-next-button",
                    kTestSupervisedUserDisplayName);
}

IN_PROC_BROWSER_TEST_F(SupervisedUserOwnerCreationTest,
                       DISABLED_PRE_CreateAndRemoveSupervisedUser) {
  SigninAsSupervisedUser(true, 0, kTestSupervisedUserDisplayName);
}

IN_PROC_BROWSER_TEST_F(SupervisedUserOwnerCreationTest,
                       DISABLED_CreateAndRemoveSupervisedUser) {
  RemoveSupervisedUser(3, 0, kTestSupervisedUserDisplayName);
}

IN_PROC_BROWSER_TEST_F(SupervisedUserTransactionCleanupTest,
                       DISABLED_PRE_PRE_CreateAndCancelSupervisedUser) {
  PrepareUsers();
}

IN_PROC_BROWSER_TEST_F(SupervisedUserTransactionCleanupTest,
                       DISABLED_PRE_CreateAndCancelSupervisedUser) {
  StartFlowLoginAsManager();
  FillNewUserData(kTestSupervisedUserDisplayName);

  EXPECT_CALL(*mock_homedir_methods_, MountEx(_, _, _, _)).Times(1);
  EXPECT_CALL(*mock_homedir_methods_, AddKeyEx(_, _, _, _, _)).Times(1);

  JSEval("$('supervised-user-creation-next-button').click()");

  testing::Mock::VerifyAndClearExpectations(mock_homedir_methods_);

  EXPECT_TRUE(registration_utility_stub_->register_was_called());
  EXPECT_EQ(registration_utility_stub_->display_name(),
            base::UTF8ToUTF16(kTestSupervisedUserDisplayName));

  std::string user_id = registration_utility_stub_->supervised_user_id();

  // Make sure user is already in list.
  ASSERT_EQ(3UL, UserManager::Get()->GetUsers().size());

  // We wait for token now. Press cancel button at this point.
  JSEval("$('cancel-add-user-button').click()");
}

IN_PROC_BROWSER_TEST_(
    SupervisedUserTransactionCleanupTest,
    DISABLED_CreateAndCancelSupervisedUser,
    SupervisedUserTransactionCleanupTest2,
    testing::internal::GetTypeId<SupervisedUserTransactionCleanupTest>()) {
  // Make sure there is no supervised user in list.
  ASSERT_EQ(2UL, UserManager::Get()->GetUsers().size());
}

}  // namespace chromeos
