// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <memory>
#include <set>

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/account_tracker_service_factory.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_error_controller_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/sync/profile_sync_test_util.h"
#include "chrome/browser/sync/sync_ui_util.h"
#include "chrome/test/base/testing_profile.h"
#include "components/signin/core/browser/fake_signin_manager.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/sync/driver/test_sync_service.h"
#include "content/public/test/test_browser_thread.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "google_apis/gaia/oauth2_token_service_delegate.h"
#include "testing/gmock/include/gmock/gmock-actions.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;
using syncer::TestSyncService;
using ::testing::_;
using ::testing::AtMost;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::SetArgPointee;

// A number of distinct states of the SyncService can be generated for tests.
enum DistinctState {
  STATUS_CASE_SETUP_IN_PROGRESS,
  STATUS_CASE_SETUP_ERROR,
  STATUS_CASE_AUTHENTICATING,
  STATUS_CASE_AUTH_ERROR,
  STATUS_CASE_PROTOCOL_ERROR,
  STATUS_CASE_PASSPHRASE_ERROR,
  STATUS_CASE_CONFIRM_SYNC_SETTINGS,
  STATUS_CASE_SYNCED,
  STATUS_CASE_SYNC_DISABLED_BY_POLICY,
  NUMBER_OF_STATUS_CASES
};

namespace {

const char kTestGaiaId[] = "gaia-id-test_user@test.com";
const char kTestUser[] = "test_user@test.com";

}  // namespace

class SyncUIUtilTest : public testing::Test {
 private:
  content::TestBrowserThreadBundle thread_bundle_;
};

// TODO(tim): This shouldn't be required. r194857 removed the
// AuthInProgress override from FakeSigninManager, which meant this test started
// using the "real" SigninManager AuthInProgress logic. Without that override,
// it's no longer possible to test both chrome os + desktop flows as part of the
// same test, because AuthInProgress is always false on chrome os. Most of the
// tests are unaffected, but STATUS_CASE_AUTHENTICATING can't exist in both
// versions, so it we will require two separate tests, one using SigninManager
// and one using SigninManagerBase (which require different setup procedures.
class FakeSigninManagerForSyncUIUtilTest : public FakeSigninManagerBase {
 public:
  explicit FakeSigninManagerForSyncUIUtilTest(Profile* profile)
      : FakeSigninManagerBase(
            ChromeSigninClientFactory::GetForProfile(profile),
            AccountTrackerServiceFactory::GetForProfile(profile),
            SigninErrorControllerFactory::GetForProfile(profile)),
        auth_in_progress_(false) {
    Initialize(nullptr);
  }

  ~FakeSigninManagerForSyncUIUtilTest() override {}

  bool AuthInProgress() const override { return auth_in_progress_; }

  void set_auth_in_progress() {
    auth_in_progress_ = true;
  }

 private:
  bool auth_in_progress_;
};

// Sets up a TestSyncService to emulate one of a number of distinct cases in
// order to perform tests on the generated messages.
void GetDistinctCase(TestSyncService* service,
                     FakeSigninManagerForSyncUIUtilTest* signin,
                     ProfileOAuth2TokenService* token_service,
                     int case_number) {
  // Auth Error object is returned by reference in mock and needs to stay in
  // scope throughout test, so it is owned by calling method. However it is
  // immutable so can only be allocated in this method.
  switch (case_number) {
    case STATUS_CASE_SETUP_IN_PROGRESS: {
      service->SetFirstSetupComplete(false);
      service->SetSetupInProgress(true);
      service->SetDetailedSyncStatus(false, syncer::SyncEngine::Status());
      return;
    }
    case STATUS_CASE_SETUP_ERROR: {
      service->SetFirstSetupComplete(false);
      service->SetSetupInProgress(false);
      service->SetDisableReasons(
          syncer::SyncService::DISABLE_REASON_UNRECOVERABLE_ERROR);
      service->SetDetailedSyncStatus(false, syncer::SyncEngine::Status());
      return;
    }
    case STATUS_CASE_AUTHENTICATING: {
      service->SetFirstSetupComplete(true);
      service->SetTransportState(syncer::SyncService::TransportState::ACTIVE);
      service->SetPassphraseRequired(false);
      service->SetDisableReasons(syncer::SyncService::DISABLE_REASON_NONE);
      service->SetDetailedSyncStatus(false, syncer::SyncEngine::Status());
      signin->set_auth_in_progress();
      return;
    }
    case STATUS_CASE_AUTH_ERROR: {
      service->SetFirstSetupComplete(true);
      service->SetTransportState(syncer::SyncService::TransportState::ACTIVE);
      service->SetPassphraseRequired(false);
      service->SetDetailedSyncStatus(false, syncer::SyncEngine::Status());
      std::string account_id = signin->GetAuthenticatedAccountId();
      token_service->UpdateCredentials(account_id, "refresh_token");
      // TODO(https://crbug.com/836212): Do not use the delegate directly,
      // because it is internal API.
      token_service->GetDelegate()->UpdateAuthError(
          account_id,
          GoogleServiceAuthError(GoogleServiceAuthError::SERVICE_ERROR));
      service->SetDisableReasons(syncer::SyncService::DISABLE_REASON_NONE);
      return;
    }
    case STATUS_CASE_PROTOCOL_ERROR: {
      service->SetFirstSetupComplete(true);
      service->SetTransportState(syncer::SyncService::TransportState::ACTIVE);
      service->SetPassphraseRequired(false);
      syncer::SyncProtocolError protocol_error;
      protocol_error.action = syncer::UPGRADE_CLIENT;
      syncer::SyncEngine::Status status;
      status.sync_protocol_error = protocol_error;
      service->SetDetailedSyncStatus(false, status);
      service->SetDisableReasons(syncer::SyncService::DISABLE_REASON_NONE);
      return;
    }
    case STATUS_CASE_CONFIRM_SYNC_SETTINGS: {
      service->SetFirstSetupComplete(false);
      service->SetPassphraseRequired(false);
      service->SetDetailedSyncStatus(false, syncer::SyncEngine::Status());
      return;
    }
    case STATUS_CASE_PASSPHRASE_ERROR: {
      service->SetFirstSetupComplete(true);
      service->SetTransportState(syncer::SyncService::TransportState::ACTIVE);
      service->SetDetailedSyncStatus(false, syncer::SyncEngine::Status());
      service->SetDisableReasons(syncer::SyncService::DISABLE_REASON_NONE);
      service->SetPassphraseRequired(true);
      service->SetPassphraseRequiredForDecryption(true);
      return;
    }
    case STATUS_CASE_SYNCED: {
      service->SetFirstSetupComplete(true);
      service->SetTransportState(syncer::SyncService::TransportState::ACTIVE);
      service->SetDetailedSyncStatus(false, syncer::SyncEngine::Status());
      service->SetDisableReasons(syncer::SyncService::DISABLE_REASON_NONE);
      service->SetPassphraseRequired(false);
      return;
    }
    case STATUS_CASE_SYNC_DISABLED_BY_POLICY: {
      service->SetDisableReasons(
          syncer::SyncService::DISABLE_REASON_ENTERPRISE_POLICY);
      service->SetFirstSetupComplete(false);
      service->SetTransportState(syncer::SyncService::TransportState::DISABLED);
      service->SetPassphraseRequired(false);
      service->SetDetailedSyncStatus(false, syncer::SyncEngine::Status());
      return;
    }
    default:
      NOTREACHED();
  }
}

// Returns the expected value for the output argument |action_type| for each
// of the distinct cases.
sync_ui_util::ActionType GetActionTypeforDistinctCase(int case_number) {
  switch (case_number) {
    case STATUS_CASE_SETUP_IN_PROGRESS:
      return sync_ui_util::NO_ACTION;
    case STATUS_CASE_SETUP_ERROR:
      return sync_ui_util::REAUTHENTICATE;
    case STATUS_CASE_AUTHENTICATING:
      return sync_ui_util::NO_ACTION;
    case STATUS_CASE_AUTH_ERROR:
      return sync_ui_util::REAUTHENTICATE;
    case STATUS_CASE_PROTOCOL_ERROR:
      return sync_ui_util::UPGRADE_CLIENT;
    case STATUS_CASE_PASSPHRASE_ERROR:
      return sync_ui_util::ENTER_PASSPHRASE;
    case STATUS_CASE_CONFIRM_SYNC_SETTINGS:
      return sync_ui_util::CONFIRM_SYNC_SETTINGS;
    case STATUS_CASE_SYNCED:
      return sync_ui_util::NO_ACTION;
    case STATUS_CASE_SYNC_DISABLED_BY_POLICY:
      return sync_ui_util::NO_ACTION;
    default:
      NOTREACHED();
      return sync_ui_util::NO_ACTION;
  }
}

// This test ensures that each distinctive SyncService status will return a
// unique combination of status and link messages from GetStatusLabels().
TEST_F(SyncUIUtilTest, DistinctCasesReportUniqueMessageSets) {
  std::set<base::string16> messages;
  for (int idx = 0; idx != NUMBER_OF_STATUS_CASES; idx++) {
    std::unique_ptr<Profile> profile = std::make_unique<TestingProfile>();
    TestSyncService service;
    FakeSigninManagerForSyncUIUtilTest signin(profile.get());
    signin.SetAuthenticatedAccountInfo(kTestGaiaId, kTestUser);
    ProfileOAuth2TokenService* token_service =
        ProfileOAuth2TokenServiceFactory::GetForProfile(profile.get());
    GetDistinctCase(&service, &signin, token_service, idx);
    base::string16 status_label;
    base::string16 link_label;
    sync_ui_util::ActionType action_type = sync_ui_util::NO_ACTION;
    sync_ui_util::GetStatusLabels(profile.get(), &service, signin,
                                  &status_label, &link_label, &action_type);

    EXPECT_EQ(GetActionTypeforDistinctCase(idx), action_type)
        << "Wrong action returned for case #" << idx;
    // If the status and link message combination is already present in the set
    // of messages already seen, this is a duplicate rather than a unique
    // message, and the test has failed.
    EXPECT_FALSE(status_label.empty())
        << "Empty status label returned for case #" << idx;
    // Ensures a search for string 'href' (found in links, not a string to be
    // found in an English language message) fails, since links are excluded
    // from the status label.
    EXPECT_EQ(status_label.find(base::ASCIIToUTF16("href")),
              base::string16::npos);
    base::string16 combined_label =
        status_label + base::ASCIIToUTF16("#") + link_label;
    EXPECT_TRUE(messages.find(combined_label) == messages.end()) <<
        "Duplicate message for case #" << idx << ": " << combined_label;
    messages.insert(combined_label);
    testing::Mock::VerifyAndClearExpectations(&service);
    testing::Mock::VerifyAndClearExpectations(&signin);
    signin.Shutdown();
  }
}

TEST_F(SyncUIUtilTest, UnrecoverableErrorWithActionableError) {
  std::unique_ptr<Profile> profile(MakeSignedInTestingProfile());
  SigninManagerBase* signin =
      SigninManagerFactory::GetForProfile(profile.get());

  TestSyncService service;
  service.SetFirstSetupComplete(true);
  service.SetDisableReasons(
      syncer::SyncService::DISABLE_REASON_UNRECOVERABLE_ERROR);

  // First time action is not set. We should get unrecoverable error.
  service.SetDetailedSyncStatus(true, syncer::SyncStatus());

  base::string16 link_label;
  base::string16 unrecoverable_error_status_label;
  sync_ui_util::ActionType action_type = sync_ui_util::NO_ACTION;
  sync_ui_util::GetStatusLabels(profile.get(), &service, *signin,
                                &unrecoverable_error_status_label, &link_label,
                                &action_type);

  // Expect the generic unrecoverable error action which is to reauthenticate.
  EXPECT_EQ(sync_ui_util::REAUTHENTICATE, action_type);

  // This time set action to UPGRADE_CLIENT. Ensure that status label differs
  // from previous one.
  syncer::SyncStatus status;
  status.sync_protocol_error.action = syncer::UPGRADE_CLIENT;
  service.SetDetailedSyncStatus(true, status);
  base::string16 upgrade_client_status_label;
  sync_ui_util::GetStatusLabels(profile.get(), &service, *signin,
                                &upgrade_client_status_label, &link_label,
                                &action_type);
  // Expect an explicit 'client upgrade' action.
  EXPECT_EQ(sync_ui_util::UPGRADE_CLIENT, action_type);

  EXPECT_NE(unrecoverable_error_status_label, upgrade_client_status_label);
}

TEST_F(SyncUIUtilTest, ActionableErrorWithPassiveMessage) {
  std::unique_ptr<Profile> profile(MakeSignedInTestingProfile());
  SigninManagerBase* signin =
      SigninManagerFactory::GetForProfile(profile.get());

  TestSyncService service;
  service.SetFirstSetupComplete(true);
  service.SetDisableReasons(
      syncer::SyncService::DISABLE_REASON_UNRECOVERABLE_ERROR);

  // Set action to UPGRADE_CLIENT.
  syncer::SyncStatus status;
  status.sync_protocol_error.action = syncer::UPGRADE_CLIENT;
  service.SetDetailedSyncStatus(true, status);

  base::string16 first_actionable_error_status_label;
  base::string16 link_label;
  sync_ui_util::ActionType action_type = sync_ui_util::NO_ACTION;
  sync_ui_util::GetStatusLabels(profile.get(), &service, *signin,
                                &first_actionable_error_status_label,
                                &link_label, &action_type);
  // Expect a 'client upgrade' call to action.
  EXPECT_EQ(sync_ui_util::UPGRADE_CLIENT, action_type);

  // This time set action to ENABLE_SYNC_ON_ACCOUNT.
  status.sync_protocol_error.action = syncer::ENABLE_SYNC_ON_ACCOUNT;
  service.SetDetailedSyncStatus(true, status);

  base::string16 second_actionable_error_status_label;
  action_type = sync_ui_util::NO_ACTION;
  sync_ui_util::GetStatusLabels(profile.get(), &service, *signin,
                                &second_actionable_error_status_label,
                                &link_label, &action_type);
  // Expect a passive message instead of a call to action.
  EXPECT_EQ(sync_ui_util::NO_ACTION, action_type);

  EXPECT_NE(first_actionable_error_status_label,
            second_actionable_error_status_label);
}

TEST_F(SyncUIUtilTest, SyncSettingsConfirmationNeededTest) {
  std::unique_ptr<Profile> profile(MakeSignedInTestingProfile());
  SigninManagerBase* signin =
      SigninManagerFactory::GetForProfile(profile.get());

  TestSyncService service;
  service.SetFirstSetupComplete(false);
  ASSERT_TRUE(sync_ui_util::ShouldRequestSyncConfirmation(&service));

  base::string16 actionable_error_status_label;
  base::string16 link_label;
  sync_ui_util::ActionType action_type = sync_ui_util::NO_ACTION;

  sync_ui_util::GetStatusLabels(profile.get(), &service, *signin,
                                &actionable_error_status_label, &link_label,
                                &action_type);

  EXPECT_EQ(action_type, sync_ui_util::CONFIRM_SYNC_SETTINGS);
}
