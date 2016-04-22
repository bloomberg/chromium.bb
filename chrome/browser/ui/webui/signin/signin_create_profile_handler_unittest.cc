// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/signin_create_profile_handler.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/signin/fake_signin_manager_builder.h"
#include "chrome/browser/signin/signin_error_controller_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/supervised_user/legacy/supervised_user_sync_service.h"
#include "chrome/browser/supervised_user/legacy/supervised_user_sync_service_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/signin/core/browser/fake_auth_status_provider.h"
#include "components/syncable_prefs/testing_pref_service_syncable.h"
#include "content/public/test/test_web_ui.h"
#include "sync/api/fake_sync_change_processor.h"
#include "sync/api/sync_data.h"
#include "sync/api/sync_error_factory_mock.h"
#include "sync/internal_api/public/attachments/attachment_service_proxy_for_test.h"
#include "sync/protocol/sync.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

// Gmock matchers and actions.
using testing::_;
using testing::Invoke;

namespace {

const char kTestProfileName[] = "test-profile-name";

const char kTestGaiaId1[] = "test-gaia-id-1";
const char kTestEmail1[] = "foo1@bar.com";

const char kTestGaiaId2[] = "test-gaia-id-2";
const char kTestEmail2[] = "foo2@bar.com";

const char kTestWebUIResponse[] = "cr.webUIListenerCallback";

const char kSupervisedUserId1[] = "test-supervised-id-1";
const char kSupervisedUserId2[] = "test-supervised-id-2";

const char kSupervisedUsername1[] = "test-supervised-username-1";
const char kSupervisedUsername2[] = "test-supervised-username-2";

const char kSupervisedUserAvatarName1[] = "chrome-avatar-index:0";
const char kSupervisedUserAvatarName2[] = "chrome-avatar-index:1";

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

class TestSigninCreateProfileHandler : public SigninCreateProfileHandler {
 public:
  explicit TestSigninCreateProfileHandler(
      content::WebUI* web_ui,
      TestingProfileManager* profile_manager)
          : profile_manager_(profile_manager) {
    set_web_ui(web_ui);
  }

  // Mock this method since it tries to create a profile asynchronously and the
  // test terminates before the callback gets called.
  MOCK_METHOD5(DoCreateProfile,
               void(const base::string16& name,
                    const std::string& icon_url,
                    bool create_shortcut,
                    const std::string& supervised_user_id,
                    Profile* custodian_profile));

  // Creates the profile synchronously, sets the appropriate flag and calls the
  // callback method to resume profile creation flow.
  void RealDoCreateProfile(const base::string16& name,
                           const std::string& icon_url,
                           bool create_shortcut,
                           const std::string& supervised_user_id,
                           Profile* custodian_profile) {
    // Create the profile synchronously.
    Profile* profile = profile_manager_->CreateTestingProfile(
        kTestProfileName,
        std::unique_ptr<syncable_prefs::TestingPrefServiceSyncable>(),
        name,
        0,
        supervised_user_id,
        TestingProfile::TestingFactories());

    // Set the flag used to track the state of the creation flow.
    profile_path_being_created_ = profile->GetPath();

    // Call the callback method to resume profile creation flow.
    SigninCreateProfileHandler::OnProfileCreated(
        create_shortcut,
        supervised_user_id,
        custodian_profile,
        profile,
        Profile::CREATE_STATUS_INITIALIZED);
  }

  // Mock this method to track when an attempt to open a new browser window for
  // the newly created/imported profile is made.
  MOCK_METHOD2(OpenNewWindowForProfile,
               void(Profile* profile, Profile::CreateStatus status));

  // We don't actually need to register supervised users in the test. Mock this
  // method to fake the registration part.
  MOCK_METHOD4(RegisterSupervisedUser,
               void(bool create_shortcut,
                    const std::string& supervised_user_id,
                    Profile* custodian_profile,
                    Profile* new_profile));

  // Calls the callback method to resume profile creation flow.
  void RealRegisterSupervisedUser(bool create_shortcut,
                                  const std::string& supervised_user_id,
                                  Profile* custodian_profile,
                                  Profile* new_profile) {
    // Call the callback method to resume profile creation flow.
    SigninCreateProfileHandler::OnSupervisedUserRegistered(
        create_shortcut,
        new_profile,
        GoogleServiceAuthError(GoogleServiceAuthError::NONE));
  }

 private:
  TestingProfileManager* profile_manager_;
  DISALLOW_COPY_AND_ASSIGN(TestSigninCreateProfileHandler);
};

class SigninCreateProfileHandlerTest : public BrowserWithTestWindowTest {
 public:
  SigninCreateProfileHandlerTest()
      : web_ui_(new content::TestWebUI) {}

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();

    profile_manager_.reset(
        new TestingProfileManager(TestingBrowserProcess::GetGlobal()));
    ASSERT_TRUE(profile_manager_->SetUp());

    handler_.reset(new TestSigninCreateProfileHandler(web_ui(),
                                                      profile_manager()));

    TestingProfile::TestingFactories factories;
    factories.push_back(std::make_pair(SigninManagerFactory::GetInstance(),
                                       BuildFakeSigninManagerBase));
    custodian_ = profile_manager_.get()->CreateTestingProfile(
        "custodian-profile",
        std::unique_ptr<syncable_prefs::TestingPrefServiceSyncable>(),
        base::UTF8ToUTF16("custodian-profile"),
        0,
        std::string(),
        factories);

    // Authenticate the custodian profile.
    fake_signin_manager_ = static_cast<FakeSigninManagerForTesting*>(
        SigninManagerFactory::GetForProfile(custodian_));
    fake_signin_manager_->SetAuthenticatedAccountInfo(kTestGaiaId1,
                                                      kTestEmail1);

    // Add supervised users to the custodian profile.
    SupervisedUserSyncService* sync_service_ =
        SupervisedUserSyncServiceFactory::GetForProfile(custodian_);
    syncer::SyncDataList sync_data;
    sync_data.push_back(CreateSyncData(kSupervisedUserId1,
                                       kSupervisedUsername1,
                                       kSupervisedUserAvatarName1));
    sync_data.push_back(CreateSyncData(kSupervisedUserId2,
                                       kSupervisedUsername2,
                                       kSupervisedUserAvatarName2));
    syncer::SyncMergeResult result = sync_service_->MergeDataAndStartSyncing(
        syncer::SUPERVISED_USERS,
        sync_data,
        std::unique_ptr<syncer::SyncChangeProcessor>(
            new syncer::FakeSyncChangeProcessor()),
        std::unique_ptr<syncer::SyncErrorFactory>(
            new syncer::SyncErrorFactoryMock()));
    EXPECT_FALSE(result.error().IsSet());
    EXPECT_EQ(2u, sync_service_->GetSupervisedUsers()->size());

    // The second supervised user exists on the device.
    profile_manager()->CreateTestingProfile(
        kSupervisedUsername2,
        std::unique_ptr<syncable_prefs::PrefServiceSyncable>(),
        base::UTF8ToUTF16(kSupervisedUsername2),
        0,
        kSupervisedUserId2,  // supervised_user_id
        TestingProfile::TestingFactories());

    EXPECT_EQ(2u,
        profile_manager()->profile_attributes_storage()->GetNumberOfProfiles());
  }

  void TearDown() override {
    profile_manager_.reset();
    handler_.reset();
    BrowserWithTestWindowTest::TearDown();
  }

  content::TestWebUI* web_ui() {
    return web_ui_.get();
  }

  TestSigninCreateProfileHandler* handler() {
    return handler_.get();
  }

  TestingProfileManager* profile_manager() {
    return profile_manager_.get();
  }

  TestingProfile* custodian() {
    return custodian_;
  }

  FakeSigninManagerForTesting* signin_manager() {
    return fake_signin_manager_;
  }

 private:
  std::unique_ptr<content::TestWebUI> web_ui_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  TestingProfile* custodian_;
  FakeSigninManagerForTesting* fake_signin_manager_;
  std::unique_ptr<TestSigninCreateProfileHandler> handler_;
};

TEST_F(SigninCreateProfileHandlerTest, ReturnDefaultProfileNameAndIcons) {
  // Request default profile information.
  base::ListValue list_args;
  handler()->RequestDefaultProfileIcons(&list_args);

  // Expect two JS callbacks. One with profile avatar icons and the other with
  // the default profile name.
  EXPECT_EQ(2U, web_ui()->call_data().size());

  EXPECT_EQ(kTestWebUIResponse, web_ui()->call_data()[0]->function_name());

  std::string callback_name;
  ASSERT_TRUE(web_ui()->call_data()[0]->arg1()->GetAsString(&callback_name));
  EXPECT_EQ("profile-icons-received", callback_name);

  const base::ListValue* profile_icons;
  ASSERT_TRUE(web_ui()->call_data()[0]->arg2()->GetAsList(&profile_icons));
  EXPECT_NE(0U, profile_icons->GetSize());

  EXPECT_EQ(kTestWebUIResponse, web_ui()->call_data()[1]->function_name());

  ASSERT_TRUE(web_ui()->call_data()[1]->arg1()->GetAsString(&callback_name));
  EXPECT_EQ("profile-defaults-received", callback_name);

  const base::DictionaryValue* profile_info;
  ASSERT_TRUE(web_ui()->call_data()[1]->arg2()->GetAsDictionary(&profile_info));
  std::string profile_name;
  ASSERT_TRUE(profile_info->GetString("name", &profile_name));
  EXPECT_NE("", profile_name);
}

TEST_F(SigninCreateProfileHandlerTest, ReturnSignedInProfiles) {
  // Create two test profiles.
  Profile* profile_1 = profile_manager()->CreateTestingProfile("profile_1");
  ASSERT_TRUE(profile_1);
  Profile* profile_2 = profile_manager()->CreateTestingProfile("profile_2");
  ASSERT_TRUE(profile_2);

  // Set Auth Info only for profile_2.
  ProfileAttributesEntry* entry;
  ASSERT_TRUE(profile_manager()->profile_attributes_storage()->
      GetProfileAttributesWithPath(profile_2->GetPath(), &entry));
  entry->SetAuthInfo(kTestGaiaId2, base::UTF8ToUTF16(kTestEmail2));

  // Request a list of signed in profiles.
  base::ListValue list_args;
  handler()->RequestSignedInProfiles(&list_args);

  // Expect a JS callback with a list containing profile_2.
  EXPECT_EQ(1U, web_ui()->call_data().size());

  EXPECT_EQ(kTestWebUIResponse, web_ui()->call_data()[0]->function_name());

  std::string callback_name;
  ASSERT_TRUE(web_ui()->call_data()[0]->arg1()->GetAsString(&callback_name));
  EXPECT_EQ("signedin-users-received", callback_name);

  const base::ListValue* signed_in_profiles;
  ASSERT_TRUE(web_ui()->call_data()[0]->arg2()->GetAsList(&signed_in_profiles));
  EXPECT_EQ(1U, signed_in_profiles->GetSize());

  const base::DictionaryValue* signed_in_profile;
  ASSERT_TRUE(signed_in_profiles->GetDictionary(0, &signed_in_profile));
  std::string user_name;
  ASSERT_TRUE(signed_in_profile->GetString("username", &user_name));
  EXPECT_EQ(kTestEmail2, user_name);
  base::string16 profile_path;
  ASSERT_TRUE(signed_in_profile->GetString("profilePath", &profile_path));
  EXPECT_EQ(profile_2->GetPath().AsUTF16Unsafe(), profile_path);
}


TEST_F(SigninCreateProfileHandlerTest, CreateProfile) {
  // Expect the call to create the profile.
  EXPECT_CALL(*handler(), DoCreateProfile(_, _, _, _, _))
      .WillOnce(Invoke(handler(),
                       &TestSigninCreateProfileHandler::RealDoCreateProfile));

  // Expect no calls to register a supervised user.
  EXPECT_CALL(*handler(), RegisterSupervisedUser(_, _, _, _)).Times(0);

  // Expect a new browser window for the new profile to be opened.
  EXPECT_CALL(*handler(), OpenNewWindowForProfile(_, _));

  // Create a non-supervised profile.
  base::ListValue list_args;
  list_args.Append(new base::StringValue(kTestProfileName));
  list_args.Append(new base::StringValue(profiles::GetDefaultAvatarIconUrl(0)));
  list_args.Append(new base::FundamentalValue(false));  // create_shortcut
  list_args.Append(new base::FundamentalValue(false));  // is_supervised
  handler()->CreateProfile(&list_args);

  // Expect a JS callbacks with the new profile information.
  EXPECT_EQ(1U, web_ui()->call_data().size());

  EXPECT_EQ(kTestWebUIResponse, web_ui()->call_data()[0]->function_name());

  std::string callback_name;
  ASSERT_TRUE(web_ui()->call_data()[0]->arg1()->GetAsString(&callback_name));
  EXPECT_EQ("create-profile-success", callback_name);

  const base::DictionaryValue* profile;
  ASSERT_TRUE(web_ui()->call_data()[0]->arg2()->GetAsDictionary(&profile));
  std::string profile_name;
  ASSERT_TRUE(profile->GetString("name", &profile_name));
  EXPECT_NE("", profile_name);
  std::string profile_path;
  ASSERT_TRUE(profile->GetString("filePath", &profile_path));
  EXPECT_NE("", profile_path);
  bool is_supervised;
  ASSERT_TRUE(profile->GetBoolean("isSupervised", &is_supervised));
  ASSERT_FALSE(is_supervised);
}

TEST_F(SigninCreateProfileHandlerTest, CreateSupervisedUser) {
  // Expect the call to create the profile.
  EXPECT_CALL(*handler(), DoCreateProfile(_, _, _, _, _))
      .WillOnce(Invoke(handler(),
                       &TestSigninCreateProfileHandler::RealDoCreateProfile));

  // Expect the call to register the supervised user.
  EXPECT_CALL(*handler(), RegisterSupervisedUser(_, _, _, _))
      .WillOnce(Invoke(
            handler(),
            &TestSigninCreateProfileHandler::RealRegisterSupervisedUser));

  // Expect no new browser window for the new supervised profile to be opened.
  EXPECT_CALL(*handler(), OpenNewWindowForProfile(_, _)).Times(0);

  // Create a supervised profile.
  base::ListValue list_args;
  list_args.Clear();
  list_args.Append(new base::StringValue(kSupervisedUsername1));
  list_args.Append(new base::StringValue(profiles::GetDefaultAvatarIconUrl(0)));
  list_args.Append(new base::FundamentalValue(false));  // create_shortcut
  list_args.Append(new base::FundamentalValue(true));  // is_supervised
  list_args.Append(new base::StringValue(""));  // supervised_user_id
  list_args.Append(new base::StringValue(custodian()->GetPath().value()));
  handler()->CreateProfile(&list_args);

  // Expect a JS callbacks with the new profile information.
  EXPECT_EQ(1U, web_ui()->call_data().size());

  EXPECT_EQ(kTestWebUIResponse, web_ui()->call_data()[0]->function_name());

  std::string callback_name;
  ASSERT_TRUE(web_ui()->call_data()[0]->arg1()->GetAsString(&callback_name));
  EXPECT_EQ("create-profile-success", callback_name);

  const base::DictionaryValue* profile;
  ASSERT_TRUE(web_ui()->call_data()[0]->arg2()->GetAsDictionary(&profile));
  std::string profile_name;
  ASSERT_TRUE(profile->GetString("name", &profile_name));
  EXPECT_NE("", profile_name);
  std::string profile_path;
  ASSERT_TRUE(profile->GetString("filePath", &profile_path));
  EXPECT_NE("", profile_path);
  bool is_supervised;
  ASSERT_TRUE(profile->GetBoolean("isSupervised", &is_supervised));
  ASSERT_TRUE(is_supervised);
}

TEST_F(SigninCreateProfileHandlerTest, ImportSupervisedUser) {
  // Expect the call to create the profile.
  EXPECT_CALL(*handler(), DoCreateProfile(_,  _,  _,  _,  _))
      .WillOnce(Invoke(handler(),
                       &TestSigninCreateProfileHandler::RealDoCreateProfile));

  // Expect the call to register the supervised user.
  EXPECT_CALL(*handler(), RegisterSupervisedUser(_, _, _, _))
      .WillOnce(Invoke(
            handler(),
            &TestSigninCreateProfileHandler::RealRegisterSupervisedUser));

  // Expect a new browser window for the new profile to be opened.
  EXPECT_CALL(*handler(), OpenNewWindowForProfile(_, _));

  // Import a supervised profile.
  base::ListValue list_args;
  list_args.Clear();
  list_args.Append(new base::StringValue(kSupervisedUsername1));
  list_args.Append(new base::StringValue(profiles::GetDefaultAvatarIconUrl(0)));
  list_args.Append(new base::FundamentalValue(false));   // create_shortcut
  list_args.Append(new base::FundamentalValue(true));  // is_supervised
  list_args.Append(
      new base::StringValue(kSupervisedUserId1));  // supervised_user_id
  list_args.Append(new base::StringValue(custodian()->GetPath().value()));
  handler()->CreateProfile(&list_args);

  // Expect a JS callbacks with the new profile information.
  EXPECT_EQ(1U, web_ui()->call_data().size());

  EXPECT_EQ(kTestWebUIResponse, web_ui()->call_data()[0]->function_name());

  std::string callback_name;
  ASSERT_TRUE(web_ui()->call_data()[0]->arg1()->GetAsString(&callback_name));
  EXPECT_EQ("create-profile-success", callback_name);

  const base::DictionaryValue* profile;
  ASSERT_TRUE(web_ui()->call_data()[0]->arg2()->GetAsDictionary(&profile));
  std::string profile_name;
  ASSERT_TRUE(profile->GetString("name", &profile_name));
  EXPECT_NE("", profile_name);
  std::string profile_path;
  ASSERT_TRUE(profile->GetString("filePath", &profile_path));
  EXPECT_NE("", profile_path);
  bool is_supervised;
  ASSERT_TRUE(profile->GetBoolean("isSupervised", &is_supervised));
  ASSERT_TRUE(is_supervised);
}

TEST_F(SigninCreateProfileHandlerTest, ImportSupervisedUserAlreadyOnDevice) {
  // Import a supervised profile whose already on the current device.
  base::ListValue list_args;
  list_args.Clear();
  list_args.Append(new base::StringValue(kSupervisedUsername2));
  list_args.Append(new base::StringValue(profiles::GetDefaultAvatarIconUrl(0)));
  list_args.Append(new base::FundamentalValue(false));
  list_args.Append(new base::FundamentalValue(true));
  list_args.Append(new base::StringValue(kSupervisedUserId2));
  list_args.Append(new base::StringValue(custodian()->GetPath().value()));
  handler()->CreateProfile(&list_args);

  // Expect a JS callbacks containing an error message.
  EXPECT_EQ(1U, web_ui()->call_data().size());

  EXPECT_EQ(kTestWebUIResponse, web_ui()->call_data()[0]->function_name());

  std::string callback_name;
  ASSERT_TRUE(web_ui()->call_data()[0]->arg1()->GetAsString(&callback_name));
  EXPECT_EQ("create-profile-error", callback_name);

  base::string16 expected_error_message = l10n_util::GetStringUTF16(
      IDS_LEGACY_SUPERVISED_USER_IMPORT_LOCAL_ERROR);
  base::string16 error_message;
  ASSERT_TRUE(web_ui()->call_data()[0]->arg2()->GetAsString(&error_message));
  EXPECT_EQ(expected_error_message, error_message);
}

TEST_F(SigninCreateProfileHandlerTest, CustodianNotAuthenticated) {
  // Stop Sync before signing out.
  SupervisedUserSyncService* sync_service_ =
      SupervisedUserSyncServiceFactory::GetForProfile(custodian());
  sync_service_->StopSyncing(syncer::SUPERVISED_USERS);

  // Sign out the custodian.
  signin_manager()->ForceSignOut();

  // Create a supervised profile.
  base::ListValue list_args;
  list_args.Clear();
  list_args.Append(new base::StringValue(kSupervisedUsername1));
  list_args.Append(new base::StringValue(profiles::GetDefaultAvatarIconUrl(0)));
  list_args.Append(new base::FundamentalValue(false));  // create_shortcut
  list_args.Append(new base::FundamentalValue(true));  // is_supervised
  list_args.Append(new base::StringValue(""));  // supervised_user_id
  list_args.Append(new base::StringValue(custodian()->GetPath().value()));
  handler()->CreateProfile(&list_args);

  // Expect a JS callbacks containing an error message.
  EXPECT_EQ(1U, web_ui()->call_data().size());

  EXPECT_EQ(kTestWebUIResponse, web_ui()->call_data()[0]->function_name());

  std::string callback_name;
  ASSERT_TRUE(web_ui()->call_data()[0]->arg1()->GetAsString(&callback_name));
  EXPECT_EQ("create-profile-error", callback_name);

  base::string16 expected_error_message = l10n_util::GetStringUTF16(
      IDS_PROFILES_CREATE_CUSTODIAN_ACCOUNT_DETAILS_OUT_OF_DATE_ERROR);
  base::string16 error_message;
  ASSERT_TRUE(web_ui()->call_data()[0]->arg2()->GetAsString(&error_message));
  EXPECT_EQ(expected_error_message, error_message);
}

TEST_F(SigninCreateProfileHandlerTest, CustodianHasAuthError) {
  // Set an Auth Error for the custodian.
  const GoogleServiceAuthError error(
      GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS);
  FakeAuthStatusProvider provider(
      SigninErrorControllerFactory::GetForProfile(custodian()));
  provider.SetAuthError(kTestGaiaId1, error);

  // Create a supervised profile.
  base::ListValue list_args;
  list_args.Clear();
  list_args.Append(new base::StringValue(kSupervisedUsername1));
  list_args.Append(new base::StringValue(profiles::GetDefaultAvatarIconUrl(0)));
  list_args.Append(new base::FundamentalValue(false));  // create_shortcut
  list_args.Append(new base::FundamentalValue(true));  // is_supervised
  list_args.Append(new base::StringValue(""));  // supervised_user_id
  list_args.Append(new base::StringValue(custodian()->GetPath().value()));
  handler()->CreateProfile(&list_args);

  // Expect a JS callbacks containing an error message.
  EXPECT_EQ(1U, web_ui()->call_data().size());

  EXPECT_EQ(kTestWebUIResponse, web_ui()->call_data()[0]->function_name());

  std::string callback_name;
  ASSERT_TRUE(web_ui()->call_data()[0]->arg1()->GetAsString(&callback_name));
  EXPECT_EQ("create-profile-error", callback_name);

  base::string16 expected_error_message = l10n_util::GetStringUTF16(
      IDS_PROFILES_CREATE_CUSTODIAN_ACCOUNT_DETAILS_OUT_OF_DATE_ERROR);
  base::string16 error_message;
  ASSERT_TRUE(web_ui()->call_data()[0]->arg2()->GetAsString(&error_message));
  EXPECT_EQ(expected_error_message, error_message);
}

TEST_F(SigninCreateProfileHandlerTest, NotAllowedToCreateSupervisedUser) {
  // Custodian is not permitted to create supervised users.
  custodian()->GetPrefs()->SetBoolean(prefs::kSupervisedUserCreationAllowed,
                                      false);

  // Create a supervised profile.
  base::ListValue list_args;
  list_args.Clear();
  list_args.Append(new base::StringValue(kSupervisedUsername1));
  list_args.Append(new base::StringValue(profiles::GetDefaultAvatarIconUrl(0)));
  list_args.Append(new base::FundamentalValue(false));  // create_shortcut
  list_args.Append(new base::FundamentalValue(true));  // is_supervised
  list_args.Append(new base::StringValue(""));  // supervised_user_id
  list_args.Append(new base::StringValue(custodian()->GetPath().value()));
  handler()->CreateProfile(&list_args);

  // Expect nothing to happen.
  EXPECT_EQ(0U, web_ui()->call_data().size());
}
