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
#include "chrome/browser/signin/signin_error_controller_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/sync/profile_sync_test_util.h"
#include "chrome/browser/sync/sync_ui_util.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_profile.h"
#include "components/browser_sync/profile_sync_service_mock.h"
#include "components/signin/core/browser/fake_auth_status_provider.h"
#include "components/signin/core/browser/fake_signin_manager.h"
#include "components/signin/core/browser/signin_manager.h"
#include "content/public/test/test_browser_thread.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gmock/include/gmock/gmock-actions.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

using ::testing::AtMost;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::SetArgPointee;
using ::testing::_;
using browser_sync::ProfileSyncService;
using browser_sync::ProfileSyncServiceMock;
using content::BrowserThread;

// A number of distinct states of the ProfileSyncService can be generated for
// tests.
enum DistinctState {
  STATUS_CASE_SETUP_IN_PROGRESS,
  STATUS_CASE_SETUP_ERROR,
  STATUS_CASE_AUTHENTICATING,
  STATUS_CASE_AUTH_ERROR,
  STATUS_CASE_PROTOCOL_ERROR,
  STATUS_CASE_PASSPHRASE_ERROR,
  STATUS_CASE_SYNCED,
  STATUS_CASE_SYNC_DISABLED_BY_POLICY,
  NUMBER_OF_STATUS_CASES
};

namespace {

const char kTestGaiaId[] = "gaia-id-test_user@test.com";
const char kTestUser[] = "test_user@test.com";

#if !defined(OS_CHROMEOS)
// Utility function to test that GetStatusLabelsForSyncGlobalError returns
// the correct results for the given states.
void VerifySyncGlobalErrorResult(ProfileSyncServiceMock* service,
                                 GoogleServiceAuthError::State error_state,
                                 bool is_signed_in,
                                 bool is_error) {
  EXPECT_CALL(*service, IsFirstSetupComplete())
      .WillRepeatedly(Return(is_signed_in));

  GoogleServiceAuthError auth_error(error_state);
  EXPECT_CALL(*service, GetAuthError()).WillRepeatedly(ReturnRef(auth_error));

  base::string16 label1, label2, label3;
  sync_ui_util::GetStatusLabelsForSyncGlobalError(
      service, &label1, &label2, &label3);
  EXPECT_EQ(label1.empty(), !is_error);
  EXPECT_EQ(label2.empty(), !is_error);
  EXPECT_EQ(label3.empty(), !is_error);
}
#endif

}  // namespace

class SyncUIUtilTest : public testing::Test {
 private:
  content::TestBrowserThreadBundle thread_bundle_;
};

#if !defined(OS_CHROMEOS)
// Test that GetStatusLabelsForSyncGlobalError returns an error if a
// passphrase is required.
TEST_F(SyncUIUtilTest, PassphraseGlobalError) {
  std::unique_ptr<Profile> profile = MakeSignedInTestingProfile();
  ProfileSyncServiceMock service(
      CreateProfileSyncServiceParamsForTest(profile.get()));
  syncer::SyncEngine::Status status;
  EXPECT_CALL(service, QueryDetailedSyncStatus(_))
              .WillRepeatedly(Return(false));
  EXPECT_CALL(service, IsPassphraseRequired())
              .WillRepeatedly(Return(true));
  EXPECT_CALL(service, IsPassphraseRequiredForDecryption())
              .WillRepeatedly(Return(true));

  VerifySyncGlobalErrorResult(&service,
                              GoogleServiceAuthError::NONE,
                              true /* signed in */,
                              true /* error */);
}

// Test that GetStatusLabelsForSyncGlobalError returns an error if a
// passphrase is required and not for auth errors.
TEST_F(SyncUIUtilTest, AuthAndPassphraseGlobalError) {
  std::unique_ptr<Profile> profile(MakeSignedInTestingProfile());
  ProfileSyncServiceMock service(
      CreateProfileSyncServiceParamsForTest(profile.get()));
  syncer::SyncEngine::Status status;
  EXPECT_CALL(service, QueryDetailedSyncStatus(_))
              .WillRepeatedly(Return(false));

  EXPECT_CALL(service, IsPassphraseRequired())
              .WillRepeatedly(Return(true));
  EXPECT_CALL(service, IsPassphraseRequiredForDecryption())
              .WillRepeatedly(Return(true));
  EXPECT_CALL(service, IsFirstSetupComplete()).WillRepeatedly(Return(true));

  GoogleServiceAuthError auth_error(
      GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS);
  EXPECT_CALL(service, GetAuthError()).WillRepeatedly(ReturnRef(auth_error));
  base::string16 menu_label, label2, label3;
  sync_ui_util::GetStatusLabelsForSyncGlobalError(
      &service, &menu_label, &label2, &label3);
  // Make sure we are still displaying the passphrase error badge (don't show
  // auth errors through SyncUIUtil).
  EXPECT_EQ(menu_label, l10n_util::GetStringUTF16(
      IDS_SYNC_PASSPHRASE_ERROR_WRENCH_MENU_ITEM));
}

// Test that GetStatusLabelsForSyncGlobalError does not indicate errors for
// auth errors (these are reported through SigninGlobalError).
TEST_F(SyncUIUtilTest, AuthStateGlobalError) {
  std::unique_ptr<Profile> profile(MakeSignedInTestingProfile());
  ProfileSyncService::InitParams init_params =
      CreateProfileSyncServiceParamsForTest(profile.get());
  NiceMock<ProfileSyncServiceMock> service(&init_params);

  syncer::SyncEngine::Status status;
  EXPECT_CALL(service, QueryDetailedSyncStatus(_))
              .WillRepeatedly(Return(false));

  GoogleServiceAuthError::State table[] = {
    GoogleServiceAuthError::NONE,
    GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS,
    GoogleServiceAuthError::USER_NOT_SIGNED_UP,
    GoogleServiceAuthError::CONNECTION_FAILED,
    GoogleServiceAuthError::CAPTCHA_REQUIRED,
    GoogleServiceAuthError::ACCOUNT_DELETED,
    GoogleServiceAuthError::ACCOUNT_DISABLED,
    GoogleServiceAuthError::SERVICE_UNAVAILABLE,
    GoogleServiceAuthError::TWO_FACTOR,
    GoogleServiceAuthError::REQUEST_CANCELED
  };

  for (size_t i = 0; i < arraysize(table); ++i) {
    VerifySyncGlobalErrorResult(&service,
                                table[i],
                                true /* signed in */,
                                false /* no error */);
    VerifySyncGlobalErrorResult(&service,
                                table[i],
                                false /* not signed in */,
                                false /* no error */);
  }
}
#endif

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
            AccountTrackerServiceFactory::GetForProfile(profile)),
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

// Loads a ProfileSyncServiceMock to emulate one of a number of distinct cases
// in order to perform tests on the generated messages.
void GetDistinctCase(ProfileSyncServiceMock* service,
                     FakeSigninManagerForSyncUIUtilTest* signin,
                     FakeAuthStatusProvider* provider,
                     int caseNumber) {
  // Auth Error object is returned by reference in mock and needs to stay in
  // scope throughout test, so it is owned by calling method. However it is
  // immutable so can only be allocated in this method.
  switch (caseNumber) {
    case STATUS_CASE_SETUP_IN_PROGRESS: {
      EXPECT_CALL(*service, IsFirstSetupComplete())
          .WillRepeatedly(Return(false));
      EXPECT_CALL(*service, IsFirstSetupInProgress())
          .WillRepeatedly(Return(true));
      syncer::SyncEngine::Status status;
      EXPECT_CALL(*service, QueryDetailedSyncStatus(_))
          .WillRepeatedly(DoAll(SetArgPointee<0>(status), Return(false)));
      return;
    }
    case STATUS_CASE_SETUP_ERROR: {
      EXPECT_CALL(*service, IsFirstSetupComplete())
          .WillRepeatedly(Return(false));
      EXPECT_CALL(*service, IsFirstSetupInProgress())
          .WillRepeatedly(Return(false));
      EXPECT_CALL(*service, HasUnrecoverableError())
          .WillRepeatedly(Return(true));
      syncer::SyncEngine::Status status;
      EXPECT_CALL(*service, QueryDetailedSyncStatus(_))
          .WillRepeatedly(DoAll(SetArgPointee<0>(status), Return(false)));
      return;
    }
    case STATUS_CASE_AUTHENTICATING: {
      EXPECT_CALL(*service, IsFirstSetupComplete())
          .WillRepeatedly(Return(true));
      EXPECT_CALL(*service, IsSyncActive()).WillRepeatedly(Return(true));
      EXPECT_CALL(*service, IsPassphraseRequired())
          .WillRepeatedly(Return(false));
      syncer::SyncEngine::Status status;
      EXPECT_CALL(*service, QueryDetailedSyncStatus(_))
          .WillRepeatedly(DoAll(SetArgPointee<0>(status), Return(false)));
      EXPECT_CALL(*service, HasUnrecoverableError())
          .WillRepeatedly(Return(false));
      signin->set_auth_in_progress();
      return;
    }
    case STATUS_CASE_AUTH_ERROR: {
      EXPECT_CALL(*service, IsFirstSetupComplete())
          .WillRepeatedly(Return(true));
      EXPECT_CALL(*service, IsSyncActive()).WillRepeatedly(Return(true));
      EXPECT_CALL(*service, IsPassphraseRequired())
          .WillRepeatedly(Return(false));
      syncer::SyncEngine::Status status;
      EXPECT_CALL(*service, QueryDetailedSyncStatus(_))
          .WillRepeatedly(DoAll(SetArgPointee<0>(status), Return(false)));
      provider->SetAuthError(
          signin->GetAuthenticatedAccountId(),
          GoogleServiceAuthError(GoogleServiceAuthError::SERVICE_UNAVAILABLE));
      EXPECT_CALL(*service, HasUnrecoverableError())
          .WillRepeatedly(Return(false));
      return;
    }
    case STATUS_CASE_PROTOCOL_ERROR: {
      EXPECT_CALL(*service, IsFirstSetupComplete())
          .WillRepeatedly(Return(true));
      EXPECT_CALL(*service, IsSyncActive()).WillRepeatedly(Return(true));
      EXPECT_CALL(*service, IsPassphraseRequired())
          .WillRepeatedly(Return(false));
      syncer::SyncProtocolError protocolError;
      protocolError.action = syncer::UPGRADE_CLIENT;
      syncer::SyncEngine::Status status;
      status.sync_protocol_error = protocolError;
      EXPECT_CALL(*service, QueryDetailedSyncStatus(_))
          .WillRepeatedly(DoAll(SetArgPointee<0>(status), Return(false)));
      EXPECT_CALL(*service, HasUnrecoverableError())
          .WillRepeatedly(Return(false));
      return;
    }
    case STATUS_CASE_PASSPHRASE_ERROR: {
      EXPECT_CALL(*service, IsFirstSetupComplete())
          .WillRepeatedly(Return(true));
      EXPECT_CALL(*service, IsSyncActive()).WillRepeatedly(Return(true));
      syncer::SyncEngine::Status status;
      EXPECT_CALL(*service, QueryDetailedSyncStatus(_))
          .WillRepeatedly(DoAll(SetArgPointee<0>(status), Return(false)));
      EXPECT_CALL(*service, HasUnrecoverableError())
          .WillRepeatedly(Return(false));
      EXPECT_CALL(*service, IsPassphraseRequired())
          .WillRepeatedly(Return(true));
      EXPECT_CALL(*service, IsPassphraseRequiredForDecryption())
          .WillRepeatedly(Return(true));
      return;
    }
    case STATUS_CASE_SYNCED: {
      EXPECT_CALL(*service, IsFirstSetupComplete())
          .WillRepeatedly(Return(true));
      EXPECT_CALL(*service, IsSyncActive()).WillRepeatedly(Return(true));
      EXPECT_CALL(*service, IsPassphraseRequired())
          .WillRepeatedly(Return(false));
      syncer::SyncEngine::Status status;
      EXPECT_CALL(*service, QueryDetailedSyncStatus(_))
          .WillRepeatedly(DoAll(SetArgPointee<0>(status), Return(false)));
      EXPECT_CALL(*service, HasUnrecoverableError())
          .WillRepeatedly(Return(false));
      EXPECT_CALL(*service, IsPassphraseRequired())
          .WillRepeatedly(Return(false));
      return;
    }
    case STATUS_CASE_SYNC_DISABLED_BY_POLICY: {
      EXPECT_CALL(*service, IsManaged()).WillRepeatedly(Return(true));
      EXPECT_CALL(*service, IsFirstSetupComplete())
          .WillRepeatedly(Return(false));
      EXPECT_CALL(*service, IsSyncActive()).WillRepeatedly(Return(false));
      EXPECT_CALL(*service, IsPassphraseRequired())
          .WillRepeatedly(Return(false));
      syncer::SyncEngine::Status status;
      EXPECT_CALL(*service, QueryDetailedSyncStatus(_))
          .WillRepeatedly(DoAll(SetArgPointee<0>(status), Return(false)));
      EXPECT_CALL(*service, HasUnrecoverableError())
          .WillRepeatedly(Return(false));
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
      return sync_ui_util::NO_ACTION;
    case STATUS_CASE_PROTOCOL_ERROR:
      return sync_ui_util::UPGRADE_CLIENT;
    case STATUS_CASE_PASSPHRASE_ERROR:
      return sync_ui_util::ENTER_PASSPHRASE;
    case STATUS_CASE_SYNCED:
      return sync_ui_util::NO_ACTION;
    case STATUS_CASE_SYNC_DISABLED_BY_POLICY:
      return sync_ui_util::NO_ACTION;
    default:
      NOTREACHED();
      return sync_ui_util::NO_ACTION;
  }
}

// This test ensures that a each distinctive ProfileSyncService statuses
// will return a unique combination of status and link messages from
// GetStatusLabels().
TEST_F(SyncUIUtilTest, DistinctCasesReportUniqueMessageSets) {
  std::set<base::string16> messages;
  for (int idx = 0; idx != NUMBER_OF_STATUS_CASES; idx++) {
    std::unique_ptr<Profile> profile(new TestingProfile());
    ProfileSyncService::InitParams init_params =
        CreateProfileSyncServiceParamsForTest(profile.get());
    NiceMock<ProfileSyncServiceMock> service(&init_params);
    GoogleServiceAuthError error = GoogleServiceAuthError::AuthErrorNone();
    EXPECT_CALL(service, GetAuthError()).WillRepeatedly(ReturnRef(error));
    FakeSigninManagerForSyncUIUtilTest signin(profile.get());
    signin.SetAuthenticatedAccountInfo(kTestGaiaId, kTestUser);
    std::unique_ptr<FakeAuthStatusProvider> provider(new FakeAuthStatusProvider(
        SigninErrorControllerFactory::GetForProfile(profile.get())));
    GetDistinctCase(&service, &signin, provider.get(), idx);
    base::string16 status_label;
    base::string16 link_label;
    sync_ui_util::ActionType action_type = sync_ui_util::NO_ACTION;
    sync_ui_util::GetStatusLabels(profile.get(), &service, signin,
                                  sync_ui_util::WITH_HTML, &status_label,
                                  &link_label, &action_type);

    EXPECT_EQ(GetActionTypeforDistinctCase(idx), action_type);
    // If the status and link message combination is already present in the set
    // of messages already seen, this is a duplicate rather than a unique
    // message, and the test has failed.
    EXPECT_FALSE(status_label.empty()) <<
        "Empty status label returned for case #" << idx;
    base::string16 combined_label =
        status_label + base::ASCIIToUTF16("#") + link_label;
    EXPECT_TRUE(messages.find(combined_label) == messages.end()) <<
        "Duplicate message for case #" << idx << ": " << combined_label;
    messages.insert(combined_label);
    testing::Mock::VerifyAndClearExpectations(&service);
    testing::Mock::VerifyAndClearExpectations(&signin);
    EXPECT_CALL(service, GetAuthError()).WillRepeatedly(ReturnRef(error));
    provider.reset();
    signin.Shutdown();
  }
}

// This test ensures that the html_links parameter on GetStatusLabels() is
// honored.
TEST_F(SyncUIUtilTest, HtmlNotIncludedInStatusIfNotRequested) {
  for (int idx = 0; idx != NUMBER_OF_STATUS_CASES; idx++) {
    std::unique_ptr<Profile> profile(MakeSignedInTestingProfile());
    ProfileSyncService::InitParams init_params =
        CreateProfileSyncServiceParamsForTest(profile.get());
    NiceMock<ProfileSyncServiceMock> service(&init_params);
    GoogleServiceAuthError error = GoogleServiceAuthError::AuthErrorNone();
    EXPECT_CALL(service, GetAuthError()).WillRepeatedly(ReturnRef(error));
    FakeSigninManagerForSyncUIUtilTest signin(profile.get());
    signin.SetAuthenticatedAccountInfo(kTestGaiaId, kTestUser);
    std::unique_ptr<FakeAuthStatusProvider> provider(new FakeAuthStatusProvider(
        SigninErrorControllerFactory::GetForProfile(profile.get())));
    GetDistinctCase(&service, &signin, provider.get(), idx);
    base::string16 status_label;
    base::string16 link_label;
    sync_ui_util::ActionType action_type = sync_ui_util::NO_ACTION;
    sync_ui_util::GetStatusLabels(profile.get(), &service, signin,
                                  sync_ui_util::PLAIN_TEXT, &status_label,
                                  &link_label, &action_type);

    EXPECT_EQ(GetActionTypeforDistinctCase(idx), action_type);
    // Ensures a search for string 'href' (found in links, not a string to be
    // found in an English language message) fails when links are excluded from
    // the status label.
    EXPECT_FALSE(status_label.empty());
    EXPECT_EQ(status_label.find(base::ASCIIToUTF16("href")),
              base::string16::npos);
    testing::Mock::VerifyAndClearExpectations(&service);
    testing::Mock::VerifyAndClearExpectations(&signin);
    EXPECT_CALL(service, GetAuthError()).WillRepeatedly(ReturnRef(error));
    provider.reset();
    signin.Shutdown();
  }
}

TEST_F(SyncUIUtilTest, UnrecoverableErrorWithActionableError) {
  std::unique_ptr<Profile> profile(MakeSignedInTestingProfile());
  SigninManagerBase* signin =
      SigninManagerFactory::GetForProfile(profile.get());

  ProfileSyncServiceMock service(
      CreateProfileSyncServiceParamsForTest(profile.get()));
  EXPECT_CALL(service, IsFirstSetupComplete()).WillRepeatedly(Return(true));
  EXPECT_CALL(service, HasUnrecoverableError()).WillRepeatedly(Return(true));

  // First time action is not set. We should get unrecoverable error.
  syncer::SyncStatus status;
  EXPECT_CALL(service, QueryDetailedSyncStatus(_))
      .WillOnce(DoAll(SetArgPointee<0>(status), Return(true)));

  base::string16 link_label;
  base::string16 unrecoverable_error_status_label;
  sync_ui_util::ActionType action_type = sync_ui_util::NO_ACTION;
  sync_ui_util::GetStatusLabels(profile.get(), &service, *signin,
                                sync_ui_util::PLAIN_TEXT,
                                &unrecoverable_error_status_label, &link_label,
                                &action_type);

  // Expect the generic unrecoverable error action which is to reauthenticate.
  EXPECT_EQ(sync_ui_util::REAUTHENTICATE, action_type);

  // This time set action to UPGRADE_CLIENT. Ensure that status label differs
  // from previous one.
  status.sync_protocol_error.action = syncer::UPGRADE_CLIENT;
  EXPECT_CALL(service, QueryDetailedSyncStatus(_))
      .WillOnce(DoAll(SetArgPointee<0>(status), Return(true)));
  base::string16 upgrade_client_status_label;
  sync_ui_util::GetStatusLabels(profile.get(), &service, *signin,
                                sync_ui_util::PLAIN_TEXT,
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

  ProfileSyncServiceMock service(
      CreateProfileSyncServiceParamsForTest(profile.get()));
  EXPECT_CALL(service, IsFirstSetupComplete()).WillRepeatedly(Return(true));
  EXPECT_CALL(service, HasUnrecoverableError()).WillRepeatedly(Return(true));

  // Set action to UPGRADE_CLIENT.
  syncer::SyncStatus status;
  status.sync_protocol_error.action = syncer::UPGRADE_CLIENT;
  EXPECT_CALL(service, QueryDetailedSyncStatus(_))
      .WillOnce(DoAll(SetArgPointee<0>(status), Return(true)));

  base::string16 first_actionable_error_status_label;
  base::string16 link_label;
  sync_ui_util::ActionType action_type = sync_ui_util::NO_ACTION;
  sync_ui_util::GetStatusLabels(profile.get(), &service, *signin,
                                sync_ui_util::PLAIN_TEXT,
                                &first_actionable_error_status_label,
                                &link_label, &action_type);
  // Expect a 'client upgrade' call to action.
  EXPECT_EQ(sync_ui_util::UPGRADE_CLIENT, action_type);

  // This time set action to ENABLE_SYNC_ON_ACCOUNT.
  status.sync_protocol_error.action = syncer::ENABLE_SYNC_ON_ACCOUNT;
  EXPECT_CALL(service, QueryDetailedSyncStatus(_))
      .WillOnce(DoAll(SetArgPointee<0>(status), Return(true)));

  base::string16 second_actionable_error_status_label;
  action_type = sync_ui_util::NO_ACTION;
  sync_ui_util::GetStatusLabels(profile.get(), &service, *signin,
                                sync_ui_util::PLAIN_TEXT,
                                &second_actionable_error_status_label,
                                &link_label, &action_type);
  // Expect a passive message instead of a call to action.
  EXPECT_EQ(sync_ui_util::NO_ACTION, action_type);

  EXPECT_NE(first_actionable_error_status_label,
            second_actionable_error_status_label);
}
