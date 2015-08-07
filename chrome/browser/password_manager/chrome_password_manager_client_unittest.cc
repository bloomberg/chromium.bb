// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/chrome_password_manager_client.h"

#include "base/command_line.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/testing_pref_service.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/profile_sync_service_mock.h"
#include "chrome/common/channel_info.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_pref_service_syncable.h"
#include "chrome/test/base/testing_profile.h"
#include "components/autofill/content/common/autofill_messages.h"
#include "components/password_manager/content/browser/password_manager_internals_service_factory.h"
#include "components/password_manager/content/common/credential_manager_messages.h"
#include "components/password_manager/core/browser/log_receiver.h"
#include "components/password_manager/core/browser/password_manager_internals_service.h"
#include "components/password_manager/core/browser/store_result_filter.h"
#include "components/password_manager/core/common/credential_manager_types.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/password_manager/core/common/password_manager_switches.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/mock_render_process_host.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserContext;
using content::WebContents;
using testing::Return;
using testing::_;

namespace {

const char kTestText[] = "abcd1234";

class MockLogReceiver : public password_manager::LogReceiver {
 public:
  MOCK_METHOD1(LogSavePasswordProgress, void(const std::string&));
};

// TODO(vabr): Get rid of the mocked client in the client's own test, see
// http://crbug.com/474577.
class MockChromePasswordManagerClient : public ChromePasswordManagerClient {
 public:
  MOCK_CONST_METHOD0(DidLastPageLoadEncounterSSLErrors, bool());
  MOCK_CONST_METHOD2(IsSyncAccountCredential,
                     bool(const std::string& username,
                          const std::string& origin));

  explicit MockChromePasswordManagerClient(content::WebContents* web_contents)
      : ChromePasswordManagerClient(web_contents, nullptr) {
    ON_CALL(*this, DidLastPageLoadEncounterSSLErrors())
        .WillByDefault(testing::Return(false));
  }
  ~MockChromePasswordManagerClient() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(MockChromePasswordManagerClient);
};

}  // namespace

class ChromePasswordManagerClientTest : public ChromeRenderViewHostTestHarness {
 public:
  ChromePasswordManagerClientTest();

  void SetUp() override;

  TestingPrefServiceSyncable* prefs() {
    return profile()->GetTestingPrefService();
  }

 protected:
  ChromePasswordManagerClient* GetClient();

  // If the test IPC sink contains an AutofillMsg_SetLoggingState message, then
  // copies its argument into |activation_flag| and returns true. Otherwise
  // returns false.
  bool WasLoggingActivationMessageSent(bool* activation_flag);

  password_manager::PasswordManagerInternalsService* service_;

  testing::StrictMock<MockLogReceiver> receiver_;
  TestingPrefServiceSimple prefs_;
};

ChromePasswordManagerClientTest::ChromePasswordManagerClientTest()
    : service_(nullptr) {
}

void ChromePasswordManagerClientTest::SetUp() {
  ChromeRenderViewHostTestHarness::SetUp();
  prefs_.registry()->RegisterBooleanPref(
      password_manager::prefs::kPasswordManagerSavingEnabled, true);
  ChromePasswordManagerClient::CreateForWebContentsWithAutofillClient(
      web_contents(), nullptr);
  service_ = password_manager::PasswordManagerInternalsServiceFactory::
      GetForBrowserContext(profile());
  ASSERT_TRUE(service_);
}

ChromePasswordManagerClient* ChromePasswordManagerClientTest::GetClient() {
  return ChromePasswordManagerClient::FromWebContents(web_contents());
}

bool ChromePasswordManagerClientTest::WasLoggingActivationMessageSent(
    bool* activation_flag) {
  const uint32 kMsgID = AutofillMsg_SetLoggingState::ID;
  const IPC::Message* message =
      process()->sink().GetFirstMessageMatching(kMsgID);
  if (!message)
    return false;
  base::Tuple<bool> param;
  AutofillMsg_SetLoggingState::Read(message, &param);
  *activation_flag = base::get<0>(param);
  process()->sink().ClearMessages();
  return true;
}

TEST_F(ChromePasswordManagerClientTest, LogSavePasswordProgressNoReceiver) {
  ChromePasswordManagerClient* client = GetClient();

  EXPECT_CALL(receiver_, LogSavePasswordProgress(kTestText)).Times(0);
  // Before attaching the receiver, no text should be passed.
  client->LogSavePasswordProgress(kTestText);
  EXPECT_FALSE(client->IsLoggingActive());
}

TEST_F(ChromePasswordManagerClientTest, LogSavePasswordProgressAttachReceiver) {
  ChromePasswordManagerClient* client = GetClient();
  EXPECT_FALSE(client->IsLoggingActive());

  // After attaching the logger, text should be passed.
  service_->RegisterReceiver(&receiver_);
  EXPECT_TRUE(client->IsLoggingActive());
  EXPECT_CALL(receiver_, LogSavePasswordProgress(kTestText)).Times(1);
  client->LogSavePasswordProgress(kTestText);
  service_->UnregisterReceiver(&receiver_);
  EXPECT_FALSE(client->IsLoggingActive());
}

TEST_F(ChromePasswordManagerClientTest, LogSavePasswordProgressDetachReceiver) {
  ChromePasswordManagerClient* client = GetClient();

  service_->RegisterReceiver(&receiver_);
  EXPECT_TRUE(client->IsLoggingActive());
  service_->UnregisterReceiver(&receiver_);
  EXPECT_FALSE(client->IsLoggingActive());

  // After detaching the logger, no text should be passed.
  EXPECT_CALL(receiver_, LogSavePasswordProgress(kTestText)).Times(0);
  client->LogSavePasswordProgress(kTestText);
}

TEST_F(ChromePasswordManagerClientTest, LogSavePasswordProgressNotifyRenderer) {
  ChromePasswordManagerClient* client = GetClient();
  bool logging_active = false;

  // Initially, the logging should be off, so no IPC messages.
  EXPECT_FALSE(WasLoggingActivationMessageSent(&logging_active));

  service_->RegisterReceiver(&receiver_);
  EXPECT_TRUE(client->IsLoggingActive());
  EXPECT_TRUE(WasLoggingActivationMessageSent(&logging_active));
  EXPECT_TRUE(logging_active);

  service_->UnregisterReceiver(&receiver_);
  EXPECT_FALSE(client->IsLoggingActive());
  EXPECT_TRUE(WasLoggingActivationMessageSent(&logging_active));
  EXPECT_FALSE(logging_active);
}

TEST_F(ChromePasswordManagerClientTest, AnswerToPingsAboutLoggingState_Active) {
  service_->RegisterReceiver(&receiver_);

  process()->sink().ClearMessages();

  // Ping the client for logging activity update.
  AutofillHostMsg_PasswordAutofillAgentConstructed msg(0);
  static_cast<content::WebContentsObserver*>(GetClient())->OnMessageReceived(
      msg, web_contents()->GetMainFrame());

  bool logging_active = false;
  EXPECT_TRUE(WasLoggingActivationMessageSent(&logging_active));
  EXPECT_TRUE(logging_active);

  service_->UnregisterReceiver(&receiver_);
}

TEST_F(ChromePasswordManagerClientTest,
       AnswerToPingsAboutLoggingState_Inactive) {
  process()->sink().ClearMessages();

  // Ping the client for logging activity update.
  AutofillHostMsg_PasswordAutofillAgentConstructed msg(0);
  static_cast<content::WebContentsObserver*>(GetClient())->OnMessageReceived(
      msg, web_contents()->GetMainFrame());

  bool logging_active = true;
  EXPECT_TRUE(WasLoggingActivationMessageSent(&logging_active));
  EXPECT_FALSE(logging_active);
}

TEST_F(ChromePasswordManagerClientTest,
       IsAutomaticPasswordSavingEnabledDefaultBehaviourTest) {
  EXPECT_FALSE(GetClient()->IsAutomaticPasswordSavingEnabled());
}

TEST_F(ChromePasswordManagerClientTest,
       IsAutomaticPasswordSavingEnabledWhenFlagIsSetTest) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      password_manager::switches::kEnableAutomaticPasswordSaving);
  if (chrome::GetChannel() == version_info::Channel::UNKNOWN)
    EXPECT_TRUE(GetClient()->IsAutomaticPasswordSavingEnabled());
  else
    EXPECT_FALSE(GetClient()->IsAutomaticPasswordSavingEnabled());
}

TEST_F(ChromePasswordManagerClientTest, LogToAReceiver) {
  ChromePasswordManagerClient* client = GetClient();
  service_->RegisterReceiver(&receiver_);
  EXPECT_TRUE(client->IsLoggingActive());

  EXPECT_CALL(receiver_, LogSavePasswordProgress(kTestText)).Times(1);
  client->LogSavePasswordProgress(kTestText);

  service_->UnregisterReceiver(&receiver_);
  EXPECT_FALSE(client->IsLoggingActive());
}

TEST_F(ChromePasswordManagerClientTest,
       IsPasswordManagementEnabledForCurrentPage) {
  ChromePasswordManagerClient* client = GetClient();
  NavigateAndCommit(
      GURL("https://accounts.google.com/ServiceLogin?continue="
           "https://passwords.google.com/settings&rart=123"));
  EXPECT_FALSE(client->IsPasswordManagementEnabledForCurrentPage());

  // Password site is inaccesible via HTTP, but because of HSTS the following
  // link should still continue to https://passwords.google.com.
  NavigateAndCommit(
      GURL("https://accounts.google.com/ServiceLogin?continue="
           "http://passwords.google.com/settings&rart=123"));
  EXPECT_FALSE(client->IsPasswordManagementEnabledForCurrentPage());
  EXPECT_FALSE(client->IsSavingEnabledForCurrentPage());

  // Specifying default port still passes.
  NavigateAndCommit(
      GURL("https://accounts.google.com/ServiceLogin?continue="
           "https://passwords.google.com:443/settings&rart=123"));
  EXPECT_FALSE(client->IsPasswordManagementEnabledForCurrentPage());
  EXPECT_FALSE(client->IsSavingEnabledForCurrentPage());

  // Encoded URL is considered the same.
  NavigateAndCommit(
      GURL("https://accounts.google.com/ServiceLogin?continue="
           "https://passwords.%67oogle.com/settings&rart=123"));
  EXPECT_FALSE(client->IsPasswordManagementEnabledForCurrentPage());
  EXPECT_FALSE(client->IsSavingEnabledForCurrentPage());

  // Make sure testing sites are disabled as well.
  NavigateAndCommit(
      GURL("https://accounts.google.com/Login?continue="
           "https://passwords-ac-testing.corp.google.com/settings&rart=456"));
  EXPECT_FALSE(client->IsSavingEnabledForCurrentPage());
  EXPECT_FALSE(client->IsPasswordManagementEnabledForCurrentPage());

  // Fully qualified domain name is considered a different hostname by GURL.
  // Ideally this would not be the case, but this quirk can be avoided by
  // verification on the server. This test is simply documentation of this
  // behavior.
  NavigateAndCommit(
      GURL("https://accounts.google.com/ServiceLogin?continue="
           "https://passwords.google.com./settings&rart=123"));
  EXPECT_TRUE(client->IsPasswordManagementEnabledForCurrentPage());

  // Not a transactional reauth page.
  NavigateAndCommit(
      GURL("https://accounts.google.com/ServiceLogin?continue="
           "https://passwords.google.com/settings"));
  EXPECT_TRUE(client->IsPasswordManagementEnabledForCurrentPage());

  // Should be enabled for other transactional reauth pages.
  NavigateAndCommit(
      GURL("https://accounts.google.com/ServiceLogin?continue="
           "https://mail.google.com&rart=234"));
  EXPECT_TRUE(client->IsPasswordManagementEnabledForCurrentPage());

  // Reauth pages are only on accounts.google.com
  NavigateAndCommit(
      GURL("https://other.site.com/ServiceLogin?continue="
           "https://passwords.google.com&rart=234"));
  EXPECT_TRUE(client->IsPasswordManagementEnabledForCurrentPage());
}

TEST_F(ChromePasswordManagerClientTest, GetPasswordSyncState) {
  ChromePasswordManagerClient* client = GetClient();

  ProfileSyncServiceMock* mock_sync_service =
      static_cast<ProfileSyncServiceMock*>(
          ProfileSyncServiceFactory::GetInstance()->SetTestingFactoryAndUse(
              profile(), ProfileSyncServiceMock::BuildMockProfileSyncService));

  syncer::ModelTypeSet active_types;
  active_types.Put(syncer::PASSWORDS);
  EXPECT_CALL(*mock_sync_service, HasSyncSetupCompleted())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*mock_sync_service, IsSyncActive()).WillRepeatedly(Return(true));
  EXPECT_CALL(*mock_sync_service, GetActiveDataTypes())
      .WillRepeatedly(Return(active_types));
  EXPECT_CALL(*mock_sync_service, IsUsingSecondaryPassphrase())
      .WillRepeatedly(Return(false));

  // Passwords are syncing and custom passphrase isn't used.
  EXPECT_EQ(password_manager::SYNCING_NORMAL_ENCRYPTION,
            client->GetPasswordSyncState());

  // Again, using a custom passphrase.
  EXPECT_CALL(*mock_sync_service, IsUsingSecondaryPassphrase())
      .WillRepeatedly(Return(true));

  EXPECT_EQ(password_manager::SYNCING_WITH_CUSTOM_PASSPHRASE,
            client->GetPasswordSyncState());

  // Report correctly if we aren't syncing passwords.
  active_types.Remove(syncer::PASSWORDS);
  active_types.Put(syncer::BOOKMARKS);
  EXPECT_CALL(*mock_sync_service, GetActiveDataTypes())
      .WillRepeatedly(Return(active_types));

  EXPECT_EQ(password_manager::NOT_SYNCING_PASSWORDS,
            client->GetPasswordSyncState());

  // Again, without a custom passphrase.
  EXPECT_CALL(*mock_sync_service, IsUsingSecondaryPassphrase())
      .WillRepeatedly(Return(false));

  EXPECT_EQ(password_manager::NOT_SYNCING_PASSWORDS,
            client->GetPasswordSyncState());
}

TEST_F(ChromePasswordManagerClientTest, IsOffTheRecordTest) {
  ChromePasswordManagerClient* client = GetClient();

  profile()->ForceIncognito(true);
  EXPECT_TRUE(client->IsOffTheRecord());

  profile()->ForceIncognito(false);
  EXPECT_FALSE(client->IsOffTheRecord());
}

TEST_F(ChromePasswordManagerClientTest,
       SavingDependsOnManagerEnabledPreference) {
  // Test that saving passwords depends on the password manager enabled
  // preference.
  ChromePasswordManagerClient* client = GetClient();
  prefs()->SetUserPref(password_manager::prefs::kPasswordManagerSavingEnabled,
                       new base::FundamentalValue(true));
  EXPECT_TRUE(client->IsSavingEnabledForCurrentPage());
  prefs()->SetUserPref(password_manager::prefs::kPasswordManagerSavingEnabled,
                       new base::FundamentalValue(false));
  EXPECT_FALSE(client->IsSavingEnabledForCurrentPage());
}

TEST_F(ChromePasswordManagerClientTest, IsSavingEnabledForCurrentPageTest) {
  scoped_ptr<MockChromePasswordManagerClient> client(
      new MockChromePasswordManagerClient(web_contents()));
  // Functionality disabled if there is SSL errors.
  EXPECT_CALL(*client, DidLastPageLoadEncounterSSLErrors())
      .WillRepeatedly(Return(true));
  EXPECT_FALSE(client->IsSavingEnabledForCurrentPage());

  // Functionality disabled if there are SSL errors and the manager itself is
  // disabled.
  prefs()->SetUserPref(password_manager::prefs::kPasswordManagerSavingEnabled,
                       new base::FundamentalValue(false));
  EXPECT_FALSE(client->IsSavingEnabledForCurrentPage());

  // Functionality disabled if there are no SSL errorsm, but the manager itself
  // is disabled.
  EXPECT_CALL(*client, DidLastPageLoadEncounterSSLErrors())
      .WillRepeatedly(Return(false));
  prefs()->SetUserPref(password_manager::prefs::kPasswordManagerSavingEnabled,
                       new base::FundamentalValue(false));
  EXPECT_FALSE(client->IsSavingEnabledForCurrentPage());

  // Functionality enabled if there are no SSL errors and the manager is
  // enabled.
  EXPECT_CALL(*client, DidLastPageLoadEncounterSSLErrors())
      .WillRepeatedly(Return(false));
  prefs()->SetUserPref(password_manager::prefs::kPasswordManagerSavingEnabled,
                       new base::FundamentalValue(true));
  EXPECT_TRUE(client->IsSavingEnabledForCurrentPage());

  // Functionality disabled in Incognito mode.
  profile()->ForceIncognito(true);
  EXPECT_FALSE(client->IsSavingEnabledForCurrentPage());

  // Functionality disabled in Incognito mode also when manager itself is
  // enabled.
  prefs()->SetUserPref(password_manager::prefs::kPasswordManagerSavingEnabled,
                       new base::FundamentalValue(true));
  EXPECT_FALSE(client->IsSavingEnabledForCurrentPage());
  profile()->ForceIncognito(false);
}

TEST_F(ChromePasswordManagerClientTest, GetLastCommittedEntryURL_Empty) {
  EXPECT_EQ(GURL::EmptyGURL(), GetClient()->GetLastCommittedEntryURL());
}

TEST_F(ChromePasswordManagerClientTest, GetLastCommittedEntryURL) {
  GURL kUrl(
      "https://accounts.google.com/ServiceLogin?continue="
      "https://passwords.google.com/settings&rart=123");
  NavigateAndCommit(kUrl);
  EXPECT_EQ(kUrl, GetClient()->GetLastCommittedEntryURL());
}

TEST_F(ChromePasswordManagerClientTest, CreateStoreResulFilter) {
  scoped_ptr<password_manager::StoreResultFilter> filter1 =
      GetClient()->CreateStoreResultFilter();
  scoped_ptr<password_manager::StoreResultFilter> filter2 =
      GetClient()->CreateStoreResultFilter();
  EXPECT_TRUE(filter1);
  EXPECT_TRUE(filter2);
  EXPECT_NE(filter1.get(), filter2.get());
}
