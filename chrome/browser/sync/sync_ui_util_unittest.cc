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
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/signin/signin_error_controller_factory.h"
#include "chrome/browser/sync/profile_sync_test_util.h"
#include "chrome/browser/sync/sync_ui_util.h"
#include "chrome/test/base/testing_profile.h"
#include "components/sync/driver/test_sync_service.h"
#include "content/public/test/test_browser_thread.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "services/identity/public/cpp/identity_manager.h"
#include "services/identity/public/cpp/identity_test_environment.h"
#include "services/identity/public/cpp/identity_test_utils.h"
#include "services/identity/public/cpp/primary_account_mutator.h"
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
const char kRefreshToken[] = "refresh_token";
const char kPassword[] = "password";

}  // namespace

class SyncUIUtilTest : public testing::Test {
 private:
  content::TestBrowserThreadBundle thread_bundle_;
};

// Sets up a TestSyncService to emulate one of a number of distinct cases in
// order to perform tests on the generated messages.
void GetDistinctCase(TestSyncService* service,
                     identity::IdentityManager* identity_manager,
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

      // This case will be run in platforms supporting mutation of the primary
      // account (i.e., not ChromeOS) only, so we can assume mutator != nullptr.
      auto* primary_account_mutator =
          identity_manager->GetPrimaryAccountMutator();
      DCHECK(primary_account_mutator);

      // Starting the auth process and not completing it will make the mutator
      // report "auth in progress" when checked from the test later on.
      primary_account_mutator
          ->LegacyStartSigninWithRefreshTokenForPrimaryAccount(
              kRefreshToken, kTestGaiaId, kTestUser, kPassword,
              base::BindOnce([](const std::string& refresh_token) {
                // Check that the token is properly passed along.
                EXPECT_EQ(kRefreshToken, refresh_token);
              }));
      return;
    }
    case STATUS_CASE_AUTH_ERROR: {
      service->SetFirstSetupComplete(true);
      service->SetTransportState(syncer::SyncService::TransportState::ACTIVE);
      service->SetPassphraseRequired(false);
      service->SetDetailedSyncStatus(false, syncer::SyncEngine::Status());

      // Make sure to fail authentication with an error in this case.
      std::string account_id = identity_manager->GetPrimaryAccountId();
      identity::SetRefreshTokenForPrimaryAccount(identity_manager);
      identity::UpdatePersistentErrorOfRefreshTokenForAccount(
          identity_manager, account_id,
          GoogleServiceAuthError(GoogleServiceAuthError::State::SERVICE_ERROR));
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
    std::unique_ptr<Profile> profile = IdentityTestEnvironmentProfileAdaptor::
        CreateProfileForIdentityTestEnvironment();

    IdentityTestEnvironmentProfileAdaptor env_adaptor(profile.get());
    identity::IdentityTestEnvironment* environment =
        env_adaptor.identity_test_env();
    identity::IdentityManager* identity_manager =
        environment->identity_manager();

    // We can't check the "Authenticating" case in platforms that don't support
    // mutation of the primary account (e.g. ChromeOS), so skip those cases.
    if (idx == STATUS_CASE_AUTHENTICATING &&
        !identity_manager->GetPrimaryAccountMutator()) {
      continue;
    }

    // Need a primary account signed in before calling GetDistinctCase().
    environment->MakePrimaryAccountAvailable(kTestUser);

    TestSyncService service;
    GetDistinctCase(&service, identity_manager, idx);
    base::string16 status_label;
    base::string16 link_label;
    sync_ui_util::ActionType action_type = sync_ui_util::NO_ACTION;
    sync_ui_util::GetStatusLabels(profile.get(), &service, identity_manager,
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
  }
}

TEST_F(SyncUIUtilTest, UnrecoverableErrorWithActionableError) {
  std::unique_ptr<Profile> profile(MakeSignedInTestingProfile());
  identity::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile.get());

  TestSyncService service;
  service.SetFirstSetupComplete(true);
  service.SetDisableReasons(
      syncer::SyncService::DISABLE_REASON_UNRECOVERABLE_ERROR);

  // First time action is not set. We should get unrecoverable error.
  service.SetDetailedSyncStatus(true, syncer::SyncStatus());

  base::string16 link_label;
  base::string16 unrecoverable_error_status_label;
  sync_ui_util::ActionType action_type = sync_ui_util::NO_ACTION;
  sync_ui_util::GetStatusLabels(profile.get(), &service, identity_manager,
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
  sync_ui_util::GetStatusLabels(profile.get(), &service, identity_manager,
                                &upgrade_client_status_label, &link_label,
                                &action_type);
  // Expect an explicit 'client upgrade' action.
  EXPECT_EQ(sync_ui_util::UPGRADE_CLIENT, action_type);

  EXPECT_NE(unrecoverable_error_status_label, upgrade_client_status_label);
}

TEST_F(SyncUIUtilTest, ActionableErrorWithPassiveMessage) {
  std::unique_ptr<Profile> profile(MakeSignedInTestingProfile());
  identity::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile.get());

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
  sync_ui_util::GetStatusLabels(profile.get(), &service, identity_manager,
                                &first_actionable_error_status_label,
                                &link_label, &action_type);
  // Expect a 'client upgrade' call to action.
  EXPECT_EQ(sync_ui_util::UPGRADE_CLIENT, action_type);

  // This time set action to ENABLE_SYNC_ON_ACCOUNT.
  status.sync_protocol_error.action = syncer::ENABLE_SYNC_ON_ACCOUNT;
  service.SetDetailedSyncStatus(true, status);

  base::string16 second_actionable_error_status_label;
  action_type = sync_ui_util::NO_ACTION;
  sync_ui_util::GetStatusLabels(profile.get(), &service, identity_manager,
                                &second_actionable_error_status_label,
                                &link_label, &action_type);
  // Expect a passive message instead of a call to action.
  EXPECT_EQ(sync_ui_util::NO_ACTION, action_type);

  EXPECT_NE(first_actionable_error_status_label,
            second_actionable_error_status_label);
}

TEST_F(SyncUIUtilTest, SyncSettingsConfirmationNeededTest) {
  std::unique_ptr<Profile> profile(MakeSignedInTestingProfile());
  identity::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile.get());

  TestSyncService service;
  service.SetFirstSetupComplete(false);
  ASSERT_TRUE(sync_ui_util::ShouldRequestSyncConfirmation(&service));

  base::string16 actionable_error_status_label;
  base::string16 link_label;
  sync_ui_util::ActionType action_type = sync_ui_util::NO_ACTION;

  sync_ui_util::GetStatusLabels(profile.get(), &service, identity_manager,
                                &actionable_error_status_label, &link_label,
                                &action_type);

  EXPECT_EQ(action_type, sync_ui_util::CONFIRM_SYNC_SETTINGS);
}
