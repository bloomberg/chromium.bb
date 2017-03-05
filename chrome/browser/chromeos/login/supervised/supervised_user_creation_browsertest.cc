// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "ash/common/system/status_area_widget.h"
#include "ash/common/system/web_notification/web_notification_tray.h"
#include "ash/test/status_area_widget_test_helper.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/login/login_manager_test.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/supervised/supervised_user_authentication.h"
#include "chrome/browser/chromeos/login/supervised/supervised_user_test_base.h"
#include "chrome/browser/chromeos/login/test/oobe_screen_waiter.h"
#include "chrome/browser/chromeos/login/ui/login_display_host_impl.h"
#include "chrome/browser/chromeos/login/ui/webui_login_view.h"
#include "chrome/browser/chromeos/login/users/supervised_user_manager.h"
#include "chrome/browser/chromeos/net/network_portal_detector_test_impl.h"
#include "chrome/browser/chromeos/settings/stub_cros_settings_provider.h"
#include "chrome/browser/supervised_user/legacy/supervised_user_registration_utility.h"
#include "chrome/browser/supervised_user/legacy/supervised_user_registration_utility_stub.h"
#include "chrome/browser/supervised_user/supervised_user_constants.h"
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

namespace {

bool GetWebNotificationTrayVisibility() {
  return ash::StatusAreaWidgetTestHelper::GetStatusAreaWidget()
      ->web_notification_tray()
      ->visible();
}

}  // anonymous namespace

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

  void SetUpInProcessBrowserTestFixture() override {
    SupervisedUserTestBase::SetUpInProcessBrowserTestFixture();
    cros_settings_provider_.reset(new StubCrosSettingsProvider());
    cros_settings_provider_->Set(kDeviceOwner, base::StringValue(kTestManager));
  }

 private:
  std::unique_ptr<StubCrosSettingsProvider> cros_settings_provider_;
  DISALLOW_COPY_AND_ASSIGN(SupervisedUserOwnerCreationTest);
};

class SupervisedUserTransactionCleanupTest2
    : public SupervisedUserTransactionCleanupTest {
 public:
  SupervisedUserTransactionCleanupTest2()
      : SupervisedUserTransactionCleanupTest() {}

  void SetUpInProcessBrowserTestFixture() override {
    SupervisedUserTransactionCleanupTest::SetUpInProcessBrowserTestFixture();
    EXPECT_CALL(*mock_async_method_caller_, AsyncRemove(_, _)).Times(1);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SupervisedUserTransactionCleanupTest2);
};

IN_PROC_BROWSER_TEST_F(SupervisedUserCreationTest,
                       PRE_PRE_PRE_CreateAndRemoveSupervisedUser) {
  PrepareUsers();
}

IN_PROC_BROWSER_TEST_F(SupervisedUserCreationTest,
                       PRE_PRE_CreateAndRemoveSupervisedUser) {
  StartFlowLoginAsManager();
  FillNewUserData(kTestSupervisedUserDisplayName);
  StartUserCreation("supervised-user-creation-next-button",
                    kTestSupervisedUserDisplayName);
}

IN_PROC_BROWSER_TEST_F(SupervisedUserCreationTest,
                       PRE_CreateAndRemoveSupervisedUser) {
  SigninAsSupervisedUser(true, 0, kTestSupervisedUserDisplayName);
}

IN_PROC_BROWSER_TEST_F(SupervisedUserCreationTest,
                       CreateAndRemoveSupervisedUser) {
  RemoveSupervisedUser(3, 0, kTestSupervisedUserDisplayName);
}

IN_PROC_BROWSER_TEST_F(SupervisedUserCreationTest,
                       PRE_PRE_CheckPodForSupervisedUser) {
  PrepareUsers();
}

IN_PROC_BROWSER_TEST_F(SupervisedUserCreationTest,
                       PRE_CheckPodForSupervisedUser) {
  StartFlowLoginAsManager();
  FillNewUserData(kTestSupervisedUserDisplayName);
  StartUserCreation("supervised-user-creation-next-button",
                    kTestSupervisedUserDisplayName);
}

IN_PROC_BROWSER_TEST_F(SupervisedUserCreationTest, CheckPodForSupervisedUser) {
  OobeScreenWaiter(OobeScreen::SCREEN_ACCOUNT_PICKER).Wait();

  // Open pod menu
  JSEval("$('pod-row').pods[0].querySelector('.action-box-button').click()");
  JSExpect("$('pod-row').pods[0].actionBoxMenuTitleEmailElement.hidden");
}

IN_PROC_BROWSER_TEST_F(SupervisedUserOwnerCreationTest,
                       PRE_PRE_PRE_CreateAndRemoveSupervisedUser) {
  PrepareUsers();
}

IN_PROC_BROWSER_TEST_F(SupervisedUserOwnerCreationTest,
                       PRE_PRE_CreateAndRemoveSupervisedUser) {
  StartFlowLoginAsManager();
  FillNewUserData(kTestSupervisedUserDisplayName);
  StartUserCreation("supervised-user-creation-next-button",
                    kTestSupervisedUserDisplayName);
}

IN_PROC_BROWSER_TEST_F(SupervisedUserOwnerCreationTest,
                       PRE_CreateAndRemoveSupervisedUser) {
  SigninAsSupervisedUser(true, 0, kTestSupervisedUserDisplayName);
}

IN_PROC_BROWSER_TEST_F(SupervisedUserOwnerCreationTest,
                       CreateAndRemoveSupervisedUser) {
  RemoveSupervisedUser(3, 0, kTestSupervisedUserDisplayName);
}

IN_PROC_BROWSER_TEST_F(SupervisedUserTransactionCleanupTest,
                       PRE_PRE_CreateAndCancelSupervisedUser) {
  PrepareUsers();
}

IN_PROC_BROWSER_TEST_F(SupervisedUserTransactionCleanupTest,
                       PRE_CreateAndCancelSupervisedUser) {
  StartFlowLoginAsManager();
  FillNewUserData(kTestSupervisedUserDisplayName);

  base::RunLoop mount_wait_loop, add_key_wait_loop;
  mock_homedir_methods_->set_mount_callback(mount_wait_loop.QuitClosure());
  mock_homedir_methods_->set_add_key_callback(add_key_wait_loop.QuitClosure());
  EXPECT_CALL(*mock_homedir_methods_, MountEx(_, _, _, _)).Times(1);
  EXPECT_CALL(*mock_homedir_methods_, AddKeyEx(_, _, _, _, _)).Times(1);

  JSEval("$('supervised-user-creation-next-button').click()");

  mount_wait_loop.Run();
  add_key_wait_loop.Run();
  testing::Mock::VerifyAndClearExpectations(mock_homedir_methods_);
  mock_homedir_methods_->set_mount_callback(base::Closure());
  mock_homedir_methods_->set_add_key_callback(base::Closure());

  EXPECT_TRUE(registration_utility_stub_->register_was_called());
  EXPECT_EQ(registration_utility_stub_->display_name(),
            base::UTF8ToUTF16(kTestSupervisedUserDisplayName));

  std::string user_id = registration_utility_stub_->supervised_user_id();

  // Make sure user is already in list.
  ASSERT_EQ(3UL, user_manager::UserManager::Get()->GetUsers().size());

  // We wait for token now. Press cancel button at this point.
  JSEvalOrExitBrowser("$('supervised-user-creation').cancel()");

  // TODO(achuith): There should probably be a wait for a specific event.
  content::RunAllPendingInMessageLoop();
}

IN_PROC_BROWSER_TEST_F(SupervisedUserCreationTest,
                       PRE_PRE_PRE_CheckNoNotificationTray) {
  PrepareUsers();
}

IN_PROC_BROWSER_TEST_F(SupervisedUserCreationTest,
                       PRE_PRE_CheckNoNotificationTray) {
  // Before sign-in, the tray should not be visible.
  EXPECT_FALSE(GetWebNotificationTrayVisibility());

  StartFlowLoginAsManager();

  // On supervised user creation flow, the tray should not be visible.
  EXPECT_FALSE(GetWebNotificationTrayVisibility());

  FillNewUserData(kTestSupervisedUserDisplayName);
  StartUserCreation("supervised-user-creation-next-button",
                    kTestSupervisedUserDisplayName);
}

IN_PROC_BROWSER_TEST_F(SupervisedUserCreationTest,
                       PRE_CheckNoNotificationTray) {
  SigninAsSupervisedUser(true, 0, kTestSupervisedUserDisplayName);

  // After sign-in, the tray should be visible.
  EXPECT_TRUE(GetWebNotificationTrayVisibility());
}

IN_PROC_BROWSER_TEST_F(SupervisedUserCreationTest, CheckNoNotificationTray) {
  RemoveSupervisedUser(3, 0, kTestSupervisedUserDisplayName);
}

IN_PROC_BROWSER_TEST_(
    SupervisedUserTransactionCleanupTest,
    CreateAndCancelSupervisedUser,
    SupervisedUserTransactionCleanupTest2,
    testing::internal::GetTypeId<SupervisedUserTransactionCleanupTest>()) {
  // Make sure there is no supervised user in list.
  ASSERT_EQ(2UL, user_manager::UserManager::Get()->GetUsers().size());
}

}  // namespace chromeos
