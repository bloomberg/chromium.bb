// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/signin_supervised_user_import_handler.h"

#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/signin/fake_signin_manager_builder.h"
#include "chrome/browser/signin/signin_error_controller_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/supervised_user/legacy/supervised_user_sync_service.h"
#include "chrome/browser/supervised_user/legacy/supervised_user_sync_service_factory.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/signin/core/browser/fake_auth_status_provider.h"
#include "components/sync/model/attachments/attachment_id.h"
#include "components/sync/model/attachments/attachment_service_proxy_for_test.h"
#include "components/sync/model/fake_sync_change_processor.h"
#include "components/sync/model/sync_error_factory_mock.h"
#include "components/sync/protocol/sync.pb.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "content/public/test/test_web_ui.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

const char kTestGaiaId[] = "test-gaia-id";
const char kTestEmail[] = "foo@bar.com";

const char kTestWebUIResponse[] = "cr.webUIResponse";
const char kTestCallbackId[] = "test-callback-id";

const char kSupervisedUserId[] = "test-supervised-id";
const char kSupervisedUsername[] = "test-supervised-username";
const char kSupervisedUserAvatarName[] = "chrome-avatar-index:0";
const char kSupervisedUserAvatarUrl[] = "chrome://theme/IDR_PROFILE_AVATAR_0";

syncer::SyncData CreateSyncData(const std::string& id,
                                const std::string& name,
                                const std::string& chrome_avatar) {
  sync_pb::EntitySpecifics specifics;
  specifics.mutable_managed_user()->set_id(id);
  specifics.mutable_managed_user()->set_name(name);
  specifics.mutable_managed_user()->set_acknowledged(true);
  specifics.mutable_managed_user()->set_chrome_avatar(chrome_avatar);

  return syncer::SyncData::CreateRemoteData(
      1,
      specifics,
      base::Time(),
      syncer::AttachmentIdList(),
      syncer::AttachmentServiceProxyForTest::Create());
}

}  // namespace

class TestSigninSupervisedUserImportHandler :
    public SigninSupervisedUserImportHandler {
 public:
  explicit TestSigninSupervisedUserImportHandler(content::WebUI* web_ui) {
    set_web_ui(web_ui);
  }
};

class SigninSupervisedUserImportHandlerTest : public BrowserWithTestWindowTest {
 public:
  SigninSupervisedUserImportHandlerTest() : web_ui_(new content::TestWebUI) {}

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    handler_.reset(new TestSigninSupervisedUserImportHandler(web_ui()));

    // Build a test profile.
    profile_manager_.reset(
        new TestingProfileManager(TestingBrowserProcess::GetGlobal()));
    ASSERT_TRUE(profile_manager_->SetUp());

    TestingProfile::TestingFactories factories;
    factories.push_back(std::make_pair(SigninManagerFactory::GetInstance(),
                                       BuildFakeSigninManagerBase));
    profile_ = profile_manager_.get()->CreateTestingProfile(
        "test-profile",
        std::unique_ptr<sync_preferences::PrefServiceSyncable>(),
        base::UTF8ToUTF16("test-profile"), 0, std::string(), factories);

    // Authenticate the test profile.
    fake_signin_manager_ = static_cast<FakeSigninManagerForTesting*>(
        SigninManagerFactory::GetForProfile(profile_));
    fake_signin_manager_->SetAuthenticatedAccountInfo(kTestGaiaId, kTestEmail);

    // Add supervised users to the profile.
    SupervisedUserSyncService* sync_service_ =
        SupervisedUserSyncServiceFactory::GetForProfile(profile_);
    syncer::SyncDataList sync_data;
    sync_data.push_back(CreateSyncData(kSupervisedUserId,
                                       kSupervisedUsername,
                                       kSupervisedUserAvatarName));
    syncer::SyncMergeResult result = sync_service_->MergeDataAndStartSyncing(
        syncer::SUPERVISED_USERS,
        sync_data,
        std::unique_ptr<syncer::SyncChangeProcessor>(
            new syncer::FakeSyncChangeProcessor()),
        std::unique_ptr<syncer::SyncErrorFactory>(
            new syncer::SyncErrorFactoryMock()));
    EXPECT_FALSE(result.error().IsSet());
    EXPECT_EQ(1u, sync_service_->GetSupervisedUsers()->size());
  }

  void TearDown() override {
    profile_manager_.reset();
    handler_.reset();
    web_ui_.reset();
    BrowserWithTestWindowTest::TearDown();
  }

  content::TestWebUI* web_ui() {
    return web_ui_.get();
  }

  TestSigninSupervisedUserImportHandler* handler() {
    return handler_.get();
  }

  TestingProfileManager* profile_manager() {
    return profile_manager_.get();
  }

  TestingProfile* profile() {
    return profile_;
  }

  FakeSigninManagerForTesting* signin_manager() {
    return fake_signin_manager_;
  }

  void VerifyResponse(size_t expected_total_calls,
                      const std::string& expected_callback_id,
                      bool expected_fulfilled) {
    EXPECT_EQ(expected_total_calls, web_ui()->call_data().size());

    EXPECT_EQ(kTestWebUIResponse, web_ui()->call_data()[0]->function_name());

    std::string callback_id;
    ASSERT_TRUE(web_ui()->call_data()[0]->arg1()->GetAsString(&callback_id));
    EXPECT_EQ(expected_callback_id, callback_id);

    bool fulfilled;
    ASSERT_TRUE(web_ui()->call_data()[0]->arg2()->GetAsBoolean(&fulfilled));
    EXPECT_EQ(expected_fulfilled, fulfilled);
  }

 private:
  std::unique_ptr<content::TestWebUI> web_ui_;
  std::unique_ptr<TestSigninSupervisedUserImportHandler> handler_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  TestingProfile* profile_;
  FakeSigninManagerForTesting* fake_signin_manager_;
};

TEST_F(SigninSupervisedUserImportHandlerTest, NotAuthenticated) {
  // Stop Sync before signing out.
  SupervisedUserSyncService* sync_service_ =
      SupervisedUserSyncServiceFactory::GetForProfile(profile());
  sync_service_->StopSyncing(syncer::SUPERVISED_USERS);

  // Sign out the user.
  signin_manager()->ForceSignOut();

  // Test the JS -> C++ -> JS callback path.
  base::ListValue list_args;
  list_args.AppendString(kTestCallbackId);
  list_args.AppendString(profile()->GetPath().AsUTF16Unsafe());
  handler()->GetExistingSupervisedUsers(&list_args);

  // Expect an error response.
  VerifyResponse(1U, kTestCallbackId, false);

  base::string16 expected_error_message = l10n_util::GetStringFUTF16(
      IDS_PROFILES_CREATE_CUSTODIAN_ACCOUNT_DETAILS_OUT_OF_DATE_ERROR,
      base::ASCIIToUTF16(profile()->GetProfileUserName()));
  base::string16 error_message;
  ASSERT_TRUE(web_ui()->call_data()[0]->arg3()->GetAsString(&error_message));
  EXPECT_EQ(expected_error_message, error_message);
}

TEST_F(SigninSupervisedUserImportHandlerTest, AuthError) {
  // Set Auth Error.
  const GoogleServiceAuthError error(
      GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS);
  FakeAuthStatusProvider provider(
      SigninErrorControllerFactory::GetForProfile(profile()));
  provider.SetAuthError(kTestGaiaId, error);

  // Test the JS -> C++ -> JS callback path.
  base::ListValue list_args;
  list_args.AppendString(kTestCallbackId);
  list_args.AppendString(profile()->GetPath().AsUTF16Unsafe());
  handler()->GetExistingSupervisedUsers(&list_args);

  // Expect an error response.
  VerifyResponse(1U, kTestCallbackId, false);

  base::string16 expected_error_message = l10n_util::GetStringFUTF16(
      IDS_PROFILES_CREATE_CUSTODIAN_ACCOUNT_DETAILS_OUT_OF_DATE_ERROR,
      base::ASCIIToUTF16(profile()->GetProfileUserName()));
  base::string16 error_message;
  ASSERT_TRUE(web_ui()->call_data()[0]->arg3()->GetAsString(&error_message));
  EXPECT_EQ(expected_error_message, error_message);
}

TEST_F(SigninSupervisedUserImportHandlerTest, CustodianIsSupervised) {
  // Build a supervised test profile.
  TestingProfile* profile_ = profile_manager()->CreateTestingProfile(
      "supervised-test-profile",
      std::unique_ptr<sync_preferences::PrefServiceSyncable>(),
      base::UTF8ToUTF16("supervised-test-profile"), 0,
      "12345",  // supervised_user_id
      TestingProfile::TestingFactories());

  // Test the JS -> C++ -> JS callback path.
  base::ListValue list_args;
  list_args.AppendString(kTestCallbackId);
  list_args.AppendString(profile_->GetPath().AsUTF16Unsafe());
  handler()->GetExistingSupervisedUsers(&list_args);

  // Expect to do nothing.
  EXPECT_EQ(0U, web_ui()->call_data().size());
}

TEST_F(SigninSupervisedUserImportHandlerTest, SendExistingSupervisedUsers) {
  // Test the JS -> C++ -> JS callback path.
  base::ListValue list_args;
  list_args.AppendString(kTestCallbackId);
  list_args.AppendString(profile()->GetPath().AsUTF16Unsafe());
  handler()->GetExistingSupervisedUsers(&list_args);

  // Expect a success response.
  VerifyResponse(1U, kTestCallbackId, true);

  const base::ListValue* supervised_users;
  ASSERT_TRUE(web_ui()->call_data()[0]->arg3()->GetAsList(&supervised_users));
  EXPECT_EQ(1U, supervised_users->GetSize());

  const base::DictionaryValue* supervised_user;
  ASSERT_TRUE(supervised_users->GetDictionary(0, &supervised_user));
  std::string id;
  ASSERT_TRUE(supervised_user->GetString("id", &id));
  EXPECT_EQ(kSupervisedUserId, id);
  std::string name;
  ASSERT_TRUE(supervised_user->GetString("name", &name));
  EXPECT_EQ(kSupervisedUsername, name);
  std::string iconURL;
  ASSERT_TRUE(supervised_user->GetString("iconURL", &iconURL));
  EXPECT_EQ(kSupervisedUserAvatarUrl, iconURL);
  bool onCurrentDevice;
  ASSERT_TRUE(supervised_user->GetBoolean("onCurrentDevice", &onCurrentDevice));
  ASSERT_FALSE(onCurrentDevice);
}
