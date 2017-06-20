// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/chrome_password_manager_client.h"

#include <stdint.h>

#include <string>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial.h"
#include "base/run_loop.h"
#include "base/strings/string16.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/profile_sync_test_util.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/autofill/content/common/autofill_agent.mojom.h"
#include "components/password_manager/content/browser/password_manager_internals_service_factory.h"
#include "components/password_manager/core/browser/credentials_filter.h"
#include "components/password_manager/core/browser/log_manager.h"
#include "components/password_manager/core/browser/log_receiver.h"
#include "components/password_manager/core/browser/log_router.h"
#include "components/password_manager/core/browser/password_manager_internals_service.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/common/credential_manager_types.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sessions/content/content_record_password_state.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/web_contents_tester.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/common/constants.h"
#endif

#if defined(SAFE_BROWSING_DB_LOCAL)
#include "components/safe_browsing/password_protection/password_protection_service.h"
#include "components/safe_browsing_db/database_manager.h"
#endif

using browser_sync::ProfileSyncServiceMock;
using content::BrowserContext;
using content::WebContents;
using sessions::GetPasswordStateFromNavigation;
using sessions::SerializedNavigationEntry;
using testing::Return;
using testing::_;

namespace {
#if defined(SAFE_BROWSING_DB_LOCAL)
class MockPasswordProtectionService
    : public safe_browsing::PasswordProtectionService {
 public:
  MockPasswordProtectionService()
      : safe_browsing::PasswordProtectionService(nullptr,
                                                 nullptr,
                                                 nullptr,
                                                 nullptr) {}

  ~MockPasswordProtectionService() override {}

  MOCK_METHOD3(FillReferrerChain,
               void(const GURL&,
                    int,
                    safe_browsing::LoginReputationClientRequest::Frame*));
  MOCK_METHOD0(IsExtendedReporting, bool());
  MOCK_METHOD0(IsIncognito, bool());
  MOCK_METHOD2(IsPingingEnabled, bool(const base::Feature&, RequestOutcome*));
  MOCK_METHOD0(IsHistorySyncEnabled, bool());
  MOCK_METHOD4(MaybeStartPasswordFieldOnFocusRequest,
               void(WebContents*, const GURL&, const GURL&, const GURL&));
  MOCK_METHOD4(MaybeStartProtectedPasswordEntryRequest,
               void(WebContents*, const GURL&, const std::string&, bool));
  MOCK_METHOD3(ShowPhishingInterstitial,
               void(const GURL&, const std::string&, content::WebContents*));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockPasswordProtectionService);
};
#endif

// TODO(vabr): Get rid of the mocked client in the client's own test, see
// http://crbug.com/474577.
class MockChromePasswordManagerClient : public ChromePasswordManagerClient {
 public:
  MOCK_CONST_METHOD0(DidLastPageLoadEncounterSSLErrors, bool());

  explicit MockChromePasswordManagerClient(content::WebContents* web_contents)
      : ChromePasswordManagerClient(web_contents, nullptr) {
    ON_CALL(*this, DidLastPageLoadEncounterSSLErrors())
        .WillByDefault(testing::Return(false));
#if defined(SAFE_BROWSING_DB_LOCAL)
    password_protection_service_ =
        base::MakeUnique<MockPasswordProtectionService>();
#endif
  }
  ~MockChromePasswordManagerClient() override {}

#if defined(SAFE_BROWSING_DB_LOCAL)
  safe_browsing::PasswordProtectionService* GetPasswordProtectionService()
      const override {
    return password_protection_service_.get();
  }

  MockPasswordProtectionService* password_protection_service() {
    return password_protection_service_.get();
  }
#endif

 private:
#if defined(SAFE_BROWSING_DB_LOCAL)
  std::unique_ptr<MockPasswordProtectionService> password_protection_service_;
#endif
  DISALLOW_COPY_AND_ASSIGN(MockChromePasswordManagerClient);
};

class DummyLogReceiver : public password_manager::LogReceiver {
 public:
  DummyLogReceiver() = default;

  void LogSavePasswordProgress(const std::string& text) override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(DummyLogReceiver);
};

class FakePasswordAutofillAgent
    : public autofill::mojom::PasswordAutofillAgent {
 public:
  FakePasswordAutofillAgent()
      : called_set_logging_state_(false),
        logging_state_active_(false),
        binding_(this) {}

  ~FakePasswordAutofillAgent() override {}

  void BindRequest(mojo::ScopedMessagePipeHandle handle) {
    binding_.Bind(
        autofill::mojom::PasswordAutofillAgentRequest(std::move(handle)));
  }

  bool called_set_logging_state() { return called_set_logging_state_; }

  bool logging_state_active() { return logging_state_active_; }

  void reset_data() {
    called_set_logging_state_ = false;
    logging_state_active_ = false;
  }

 private:
  // autofill::mojom::PasswordAutofillAgent:
  void FillPasswordForm(
      int key,
      const autofill::PasswordFormFillData& form_data) override {}

  void SetLoggingState(bool active) override {
    called_set_logging_state_ = true;
    logging_state_active_ = active;
  }

  void AutofillUsernameAndPasswordDataReceived(
      const autofill::FormsPredictionsMap& predictions) override {}

  void FindFocusedPasswordForm(
      FindFocusedPasswordFormCallback callback) override {}

  // Records whether SetLoggingState() gets called.
  bool called_set_logging_state_;
  // Records data received via SetLoggingState() call.
  bool logging_state_active_;

  mojo::Binding<autofill::mojom::PasswordAutofillAgent> binding_;
};

}  // namespace

class ChromePasswordManagerClientTest : public ChromeRenderViewHostTestHarness {
 public:
  ChromePasswordManagerClientTest()
      : field_trial_list_(nullptr), metrics_enabled_(false) {}
  void SetUp() override;
  void TearDown() override;

  sync_preferences::TestingPrefServiceSyncable* prefs() {
    return profile()->GetTestingPrefService();
  }

  // Caller does not own the returned pointer.
  ProfileSyncServiceMock* SetupBasicMockSync() {
    ProfileSyncServiceMock* mock_sync_service =
        static_cast<ProfileSyncServiceMock*>(
            ProfileSyncServiceFactory::GetInstance()->SetTestingFactoryAndUse(
                profile(), BuildMockProfileSyncService));

    EXPECT_CALL(*mock_sync_service, IsFirstSetupComplete())
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*mock_sync_service, IsSyncActive())
        .WillRepeatedly(Return(true));
    return mock_sync_service;
  }

  // Make a navigation entry that will accept an annotation.
  void SetupNavigationForAnnotation() {
    ProfileSyncServiceMock* mock_sync_service = SetupBasicMockSync();
    EXPECT_CALL(*mock_sync_service, IsUsingSecondaryPassphrase())
        .WillRepeatedly(Return(false));
    metrics_enabled_ = true;
    NavigateAndCommit(GURL("about:blank"));
  }

 protected:
  ChromePasswordManagerClient* GetClient();

  // If autofill::mojom::PasswordAutofillAgent::SetLoggingState() got called,
  // copies its argument into |activation_flag| and returns true. Otherwise
  // returns false.
  bool WasLoggingActivationMessageSent(bool* activation_flag);

  FakePasswordAutofillAgent fake_agent_;

  TestingPrefServiceSimple prefs_;
  base::FieldTrialList field_trial_list_;
  bool metrics_enabled_;
};

void ChromePasswordManagerClientTest::SetUp() {
  ChromeRenderViewHostTestHarness::SetUp();

  service_manager::InterfaceProvider* remote_interfaces =
      web_contents()->GetMainFrame()->GetRemoteInterfaces();
  service_manager::InterfaceProvider::TestApi test_api(remote_interfaces);
  test_api.SetBinderForName(autofill::mojom::PasswordAutofillAgent::Name_,
                            base::Bind(&FakePasswordAutofillAgent::BindRequest,
                                       base::Unretained(&fake_agent_)));

  prefs_.registry()->RegisterBooleanPref(
      password_manager::prefs::kCredentialsEnableService, true);
  ChromePasswordManagerClient::CreateForWebContentsWithAutofillClient(
      web_contents(), nullptr);

  // Connect our bool for testing.
  ChromeMetricsServiceAccessor::SetMetricsAndCrashReportingForTesting(
      &metrics_enabled_);
}

void ChromePasswordManagerClientTest::TearDown() {
  ChromeMetricsServiceAccessor::SetMetricsAndCrashReportingForTesting(nullptr);
  ChromeRenderViewHostTestHarness::TearDown();
}

ChromePasswordManagerClient* ChromePasswordManagerClientTest::GetClient() {
  return ChromePasswordManagerClient::FromWebContents(web_contents());
}

bool ChromePasswordManagerClientTest::WasLoggingActivationMessageSent(
    bool* activation_flag) {
  base::RunLoop().RunUntilIdle();
  if (!fake_agent_.called_set_logging_state())
    return false;

  if (activation_flag)
    *activation_flag = fake_agent_.logging_state_active();
  fake_agent_.reset_data();
  return true;
}

TEST_F(ChromePasswordManagerClientTest, LogSavePasswordProgressNotifyRenderer) {
  bool logging_active = true;
  // Ensure the existence of a driver, which will send the IPCs we listen for
  // below.
  NavigateAndCommit(GURL("about:blank"));

  // Initially, the logging should be off, so no IPC messages.
  EXPECT_TRUE(!WasLoggingActivationMessageSent(&logging_active) ||
              !logging_active)
      << "logging_active=" << logging_active;

  DummyLogReceiver log_receiver;
  password_manager::LogRouter* log_router = password_manager::
      PasswordManagerInternalsServiceFactory::GetForBrowserContext(profile());
  EXPECT_EQ(std::string(), log_router->RegisterReceiver(&log_receiver));
  EXPECT_TRUE(WasLoggingActivationMessageSent(&logging_active));
  EXPECT_TRUE(logging_active);

  log_router->UnregisterReceiver(&log_receiver);
  EXPECT_TRUE(WasLoggingActivationMessageSent(&logging_active));
  EXPECT_FALSE(logging_active);
}

TEST_F(ChromePasswordManagerClientTest, GetPasswordSyncState) {
  ProfileSyncServiceMock* mock_sync_service = SetupBasicMockSync();

  syncer::ModelTypeSet active_types;
  active_types.Put(syncer::PASSWORDS);
  EXPECT_CALL(*mock_sync_service, GetActiveDataTypes())
      .WillRepeatedly(Return(active_types));
  EXPECT_CALL(*mock_sync_service, IsUsingSecondaryPassphrase())
      .WillRepeatedly(Return(false));

  ChromePasswordManagerClient* client = GetClient();

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

TEST_F(ChromePasswordManagerClientTest, IsIncognitoTest) {
  ChromePasswordManagerClient* client = GetClient();

  profile()->ForceIncognito(true);
  EXPECT_TRUE(client->IsIncognito());

  profile()->ForceIncognito(false);
  EXPECT_FALSE(client->IsIncognito());
}

TEST_F(ChromePasswordManagerClientTest,
       SavingDependsOnManagerEnabledPreference) {
  // Test that saving passwords depends on the password manager enabled
  // preference.
  ChromePasswordManagerClient* client = GetClient();
  prefs()->SetUserPref(password_manager::prefs::kCredentialsEnableService,
                       base::MakeUnique<base::Value>(true));
  EXPECT_TRUE(client->IsSavingAndFillingEnabledForCurrentPage());
  prefs()->SetUserPref(password_manager::prefs::kCredentialsEnableService,
                       base::MakeUnique<base::Value>(false));
  EXPECT_FALSE(client->IsSavingAndFillingEnabledForCurrentPage());
}

TEST_F(ChromePasswordManagerClientTest, SavingAndFillingEnabledConditionsTest) {
  std::unique_ptr<WebContents> test_web_contents(
      content::WebContentsTester::CreateTestWebContents(
          web_contents()->GetBrowserContext(), nullptr));
  std::unique_ptr<MockChromePasswordManagerClient> client(
      new MockChromePasswordManagerClient(test_web_contents.get()));
  // Functionality disabled if there is SSL errors.
  EXPECT_CALL(*client, DidLastPageLoadEncounterSSLErrors())
      .WillRepeatedly(Return(true));
  EXPECT_FALSE(client->IsSavingAndFillingEnabledForCurrentPage());
  EXPECT_FALSE(client->IsFillingEnabledForCurrentPage());

  // Functionality disabled if there are SSL errors and the manager itself is
  // disabled.
  prefs()->SetUserPref(password_manager::prefs::kCredentialsEnableService,
                       base::MakeUnique<base::Value>(false));
  EXPECT_FALSE(client->IsSavingAndFillingEnabledForCurrentPage());
  EXPECT_FALSE(client->IsFillingEnabledForCurrentPage());

  // Functionality disabled if there are no SSL errors, but the manager itself
  // is disabled.
  EXPECT_CALL(*client, DidLastPageLoadEncounterSSLErrors())
      .WillRepeatedly(Return(false));
  prefs()->SetUserPref(password_manager::prefs::kCredentialsEnableService,
                       base::MakeUnique<base::Value>(false));
  EXPECT_FALSE(client->IsSavingAndFillingEnabledForCurrentPage());
  EXPECT_TRUE(client->IsFillingEnabledForCurrentPage());

  // Functionality enabled if there are no SSL errors and the manager is
  // enabled.
  EXPECT_CALL(*client, DidLastPageLoadEncounterSSLErrors())
      .WillRepeatedly(Return(false));
  prefs()->SetUserPref(password_manager::prefs::kCredentialsEnableService,
                       base::MakeUnique<base::Value>(true));
  EXPECT_TRUE(client->IsSavingAndFillingEnabledForCurrentPage());
  EXPECT_TRUE(client->IsFillingEnabledForCurrentPage());

  // Functionality disabled in Incognito mode.
  profile()->ForceIncognito(true);
  EXPECT_FALSE(client->IsSavingAndFillingEnabledForCurrentPage());
  EXPECT_TRUE(client->IsFillingEnabledForCurrentPage());

  // Functionality disabled in Incognito mode also when manager itself is
  // enabled.
  prefs()->SetUserPref(password_manager::prefs::kCredentialsEnableService,
                       base::MakeUnique<base::Value>(true));
  EXPECT_FALSE(client->IsSavingAndFillingEnabledForCurrentPage());
  EXPECT_TRUE(client->IsFillingEnabledForCurrentPage());
  profile()->ForceIncognito(false);
}

TEST_F(ChromePasswordManagerClientTest, SavingDependsOnAutomation) {
  // Test that saving passwords UI is disabled for automated tests.
  ChromePasswordManagerClient* client = GetClient();
  EXPECT_TRUE(client->IsSavingAndFillingEnabledForCurrentPage());
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableAutomation);
  EXPECT_FALSE(client->IsSavingAndFillingEnabledForCurrentPage());
}

TEST_F(ChromePasswordManagerClientTest, GetLastCommittedEntryURL_Empty) {
  EXPECT_EQ(GURL::EmptyGURL(), GetClient()->GetLastCommittedEntryURL());
}

TEST_F(ChromePasswordManagerClientTest, GetLastCommittedEntryURL) {
  GURL kUrl(
      "https://accounts.google.com/ServiceLogin?continue="
      "https://passwords.google.com/settings");
  NavigateAndCommit(kUrl);
  EXPECT_EQ(kUrl, GetClient()->GetLastCommittedEntryURL());
}

TEST_F(ChromePasswordManagerClientTest, WebUINoLogging) {
  // Make sure that logging is active.
  password_manager::LogRouter* log_router = password_manager::
      PasswordManagerInternalsServiceFactory::GetForBrowserContext(profile());
  DummyLogReceiver log_receiver;
  EXPECT_EQ(std::string(), log_router->RegisterReceiver(&log_receiver));

  // But then navigate to a WebUI, there the logging should not be active.
  NavigateAndCommit(GURL("about:password-manager-internals"));
  EXPECT_FALSE(GetClient()->GetLogManager()->IsLoggingActive());

  log_router->UnregisterReceiver(&log_receiver);
}

// Metrics enabled, syncing with non-custom passphrase: Do not annotate.
TEST_F(ChromePasswordManagerClientTest,
       AnnotateNavigationEntryWithMetricsNoCustom) {
  ProfileSyncServiceMock* mock_sync_service = SetupBasicMockSync();
  EXPECT_CALL(*mock_sync_service, IsUsingSecondaryPassphrase())
      .WillRepeatedly(Return(false));
  metrics_enabled_ = true;

  NavigateAndCommit(GURL("about:blank"));
  GetClient()->AnnotateNavigationEntry(true);

  EXPECT_EQ(
      SerializedNavigationEntry::HAS_PASSWORD_FIELD,
      GetPasswordStateFromNavigation(*controller().GetLastCommittedEntry()));
}

// Metrics disabled, syncing with non-custom passphrase: Do not annotate.
TEST_F(ChromePasswordManagerClientTest,
       AnnotateNavigationEntryNoMetricsNoCustom) {
  ProfileSyncServiceMock* mock_sync_service = SetupBasicMockSync();
  EXPECT_CALL(*mock_sync_service, IsUsingSecondaryPassphrase())
      .WillRepeatedly(Return(false));
  metrics_enabled_ = false;

  NavigateAndCommit(GURL("about:blank"));
  GetClient()->AnnotateNavigationEntry(true);

  EXPECT_EQ(
      SerializedNavigationEntry::PASSWORD_STATE_UNKNOWN,
      GetPasswordStateFromNavigation(*controller().GetLastCommittedEntry()));
}

// Metrics enabled, syncing with custom passphrase: Do not annotate.
TEST_F(ChromePasswordManagerClientTest,
       AnnotateNavigationEntryWithMetricsWithCustom) {
  ProfileSyncServiceMock* mock_sync_service = SetupBasicMockSync();
  EXPECT_CALL(*mock_sync_service, IsUsingSecondaryPassphrase())
      .WillRepeatedly(Return(true));
  metrics_enabled_ = true;

  NavigateAndCommit(GURL("about:blank"));
  GetClient()->AnnotateNavigationEntry(true);

  EXPECT_EQ(
      SerializedNavigationEntry::PASSWORD_STATE_UNKNOWN,
      GetPasswordStateFromNavigation(*controller().GetLastCommittedEntry()));
}

// Metrics disabled, syncing with custom passphrase: Do not annotate.
TEST_F(ChromePasswordManagerClientTest,
       AnnotateNavigationEntryNoMetricsWithCustom) {
  ProfileSyncServiceMock* mock_sync_service = SetupBasicMockSync();
  EXPECT_CALL(*mock_sync_service, IsUsingSecondaryPassphrase())
      .WillRepeatedly(Return(true));
  metrics_enabled_ = false;

  NavigateAndCommit(GURL("about:blank"));
  GetClient()->AnnotateNavigationEntry(true);

  EXPECT_EQ(
      SerializedNavigationEntry::PASSWORD_STATE_UNKNOWN,
      GetPasswordStateFromNavigation(*controller().GetLastCommittedEntry()));
}

// State transition: Unannotated
TEST_F(ChromePasswordManagerClientTest, AnnotateNavigationEntryUnannotated) {
  SetupNavigationForAnnotation();

  EXPECT_EQ(
      SerializedNavigationEntry::PASSWORD_STATE_UNKNOWN,
      GetPasswordStateFromNavigation(*controller().GetLastCommittedEntry()));
}

// State transition: unknown->false
TEST_F(ChromePasswordManagerClientTest, AnnotateNavigationEntryToFalse) {
  SetupNavigationForAnnotation();

  GetClient()->AnnotateNavigationEntry(false);
  EXPECT_EQ(
      SerializedNavigationEntry::NO_PASSWORD_FIELD,
      GetPasswordStateFromNavigation(*controller().GetLastCommittedEntry()));
}

// State transition: false->true
TEST_F(ChromePasswordManagerClientTest, AnnotateNavigationEntryToTrue) {
  SetupNavigationForAnnotation();

  GetClient()->AnnotateNavigationEntry(false);
  GetClient()->AnnotateNavigationEntry(true);
  EXPECT_EQ(
      SerializedNavigationEntry::HAS_PASSWORD_FIELD,
      GetPasswordStateFromNavigation(*controller().GetLastCommittedEntry()));
}

// State transition: true->false (retains true)
TEST_F(ChromePasswordManagerClientTest, AnnotateNavigationEntryTrueToFalse) {
  SetupNavigationForAnnotation();

  GetClient()->AnnotateNavigationEntry(true);
  GetClient()->AnnotateNavigationEntry(false);
  EXPECT_EQ(
      SerializedNavigationEntry::HAS_PASSWORD_FIELD,
      GetPasswordStateFromNavigation(*controller().GetLastCommittedEntry()));
}

// Handle missing ChromePasswordManagerClient instance in BindCredentialManager
// gracefully.
TEST_F(ChromePasswordManagerClientTest, BindCredentialManager_MissingInstance) {
  // Create a WebContent without tab helpers.
  std::unique_ptr<content::WebContents> web_contents(
      content::WebContents::Create(
          content::WebContents::CreateParams(profile())));
  // In particular, this WebContent should not have the
  // ChromePasswordManagerClient.
  ASSERT_FALSE(
      ChromePasswordManagerClient::FromWebContents(web_contents.get()));

  // This call should not crash.
  ChromePasswordManagerClient::BindCredentialManager(
      service_manager::BindSourceInfo(),
      password_manager::mojom::CredentialManagerRequest(),
      web_contents->GetMainFrame());
}

TEST_F(ChromePasswordManagerClientTest, CanShowBubbleOnURL) {
  struct TestCase {
    const char* scheme;
    bool can_show_bubble;
  } kTestCases[] = {
      {url::kHttpScheme, true},
      {url::kHttpsScheme, true},
      {url::kFtpScheme, true},
      {url::kDataScheme, true},
      {"feed", true},
      {url::kBlobScheme, true},
      {url::kFileSystemScheme, true},

      {"invalid-scheme-i-just-made-up", false},
#if BUILDFLAG(ENABLE_EXTENSIONS)
      {extensions::kExtensionScheme, false},
#endif
      {url::kAboutScheme, false},
      {content::kChromeDevToolsScheme, false},
      {content::kChromeUIScheme, false},
      {url::kJavaScriptScheme, false},
      {url::kMailToScheme, false},
      {content::kViewSourceScheme, false},
  };

  for (const TestCase& test_case : kTestCases) {
    // CanShowBubbleOnURL currently only depends on the scheme.
    GURL url(base::StringPrintf("%s://example.org", test_case.scheme));
    SCOPED_TRACE(url.possibly_invalid_spec());
    EXPECT_EQ(test_case.can_show_bubble,
              ChromePasswordManagerClient::CanShowBubbleOnURL(url));
  }
}

#if defined(SAFE_BROWSING_DB_LOCAL)
TEST_F(ChromePasswordManagerClientTest,
       VerifyMaybeStartPasswordFieldOnFocusRequestCalled) {
  std::unique_ptr<WebContents> test_web_contents(
      content::WebContentsTester::CreateTestWebContents(
          web_contents()->GetBrowserContext(), nullptr));
  std::unique_ptr<MockChromePasswordManagerClient> client(
      new MockChromePasswordManagerClient(test_web_contents.get()));
  EXPECT_CALL(*client->password_protection_service(),
              MaybeStartPasswordFieldOnFocusRequest(_, _, _, _))
      .Times(1);
  client->CheckSafeBrowsingReputation(GURL("http://foo.com/submit"),
                                      GURL("http://foo.com/iframe.html"));
}

TEST_F(ChromePasswordManagerClientTest,
       VerifyMaybeProtectedPasswordEntryRequestCalled) {
  std::unique_ptr<WebContents> test_web_contents(
      content::WebContentsTester::CreateTestWebContents(
          web_contents()->GetBrowserContext(), nullptr));
  std::unique_ptr<MockChromePasswordManagerClient> client(
      new MockChromePasswordManagerClient(test_web_contents.get()));
  EXPECT_CALL(*client->password_protection_service(),
              MaybeStartProtectedPasswordEntryRequest(_, _, _, true))
      .Times(1);
  client->CheckProtectedPasswordEntry(std::string("saved_domain.com"), true);
}

#endif
