// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>
#include "base/basictypes.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/fake_signin_manager.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/sync/profile_sync_service_mock.h"
#include "chrome/browser/sync/sync_ui_util.h"
#include "chrome/grit/generated_resources.h"
#include "components/signin/core/browser/fake_auth_status_provider.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
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

const char kTestUser[] = "test_user@test.com";

#if !defined(OS_CHROMEOS)
// Utility function to test that GetStatusLabelsForSyncGlobalError returns
// the correct results for the given states.
void VerifySyncGlobalErrorResult(NiceMock<ProfileSyncServiceMock>* service,
                                 GoogleServiceAuthError::State error_state,
                                 bool is_signed_in,
                                 bool is_error) {
  EXPECT_CALL(*service, HasSyncSetupCompleted())
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

} // namespace


class SyncUIUtilTest : public testing::Test {
 private:
  content::TestBrowserThreadBundle thread_bundle_;
};

#if !defined(OS_CHROMEOS)
// Test that GetStatusLabelsForSyncGlobalError returns an error if a
// passphrase is required.
TEST_F(SyncUIUtilTest, PassphraseGlobalError) {
  scoped_ptr<Profile> profile(
      ProfileSyncServiceMock::MakeSignedInTestingProfile());
  NiceMock<ProfileSyncServiceMock> service(profile.get());
  browser_sync::SyncBackendHost::Status status;
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
  scoped_ptr<Profile> profile(
      ProfileSyncServiceMock::MakeSignedInTestingProfile());
  NiceMock<ProfileSyncServiceMock> service(profile.get());
  browser_sync::SyncBackendHost::Status status;
  EXPECT_CALL(service, QueryDetailedSyncStatus(_))
              .WillRepeatedly(Return(false));

  EXPECT_CALL(service, IsPassphraseRequired())
              .WillRepeatedly(Return(true));
  EXPECT_CALL(service, IsPassphraseRequiredForDecryption())
              .WillRepeatedly(Return(true));
  EXPECT_CALL(service, HasSyncSetupCompleted())
              .WillRepeatedly(Return(true));

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
  scoped_ptr<Profile> profile(
      ProfileSyncServiceMock::MakeSignedInTestingProfile());
  NiceMock<ProfileSyncServiceMock> service(profile.get());

  browser_sync::SyncBackendHost::Status status;
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
    GoogleServiceAuthError::REQUEST_CANCELED,
    GoogleServiceAuthError::HOSTED_NOT_ALLOWED
  };

  FakeSigninManagerBase signin(profile.get());
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
      : FakeSigninManagerBase(profile), auth_in_progress_(false) {
    Initialize(NULL);
  }

  virtual ~FakeSigninManagerForSyncUIUtilTest() {
  }

  virtual bool AuthInProgress() const OVERRIDE {
    return auth_in_progress_;
  }

  void set_auth_in_progress() {
    auth_in_progress_ = true;
  }

 private:
  bool auth_in_progress_;
};

// Loads a ProfileSyncServiceMock to emulate one of a number of distinct cases
// in order to perform tests on the generated messages.
void GetDistinctCase(ProfileSyncServiceMock& service,
                     FakeSigninManagerForSyncUIUtilTest* signin,
                     FakeAuthStatusProvider* provider,
                     int caseNumber) {
  // Auth Error object is returned by reference in mock and needs to stay in
  // scope throughout test, so it is owned by calling method. However it is
  // immutable so can only be allocated in this method.
  switch (caseNumber) {
    case STATUS_CASE_SETUP_IN_PROGRESS: {
      EXPECT_CALL(service, HasSyncSetupCompleted())
                  .WillRepeatedly(Return(false));
      EXPECT_CALL(service, FirstSetupInProgress())
                  .WillRepeatedly(Return(true));
      browser_sync::SyncBackendHost::Status status;
      EXPECT_CALL(service, QueryDetailedSyncStatus(_))
                  .WillRepeatedly(DoAll(SetArgPointee<0>(status),
                                  Return(false)));
      return;
    }
   case STATUS_CASE_SETUP_ERROR: {
      EXPECT_CALL(service, HasSyncSetupCompleted())
                  .WillRepeatedly(Return(false));
      EXPECT_CALL(service, FirstSetupInProgress())
                  .WillRepeatedly(Return(false));
      EXPECT_CALL(service, HasUnrecoverableError())
                  .WillRepeatedly(Return(true));
      browser_sync::SyncBackendHost::Status status;
      EXPECT_CALL(service, QueryDetailedSyncStatus(_))
                  .WillRepeatedly(DoAll(SetArgPointee<0>(status),
                                  Return(false)));
      return;
    }
    case STATUS_CASE_AUTHENTICATING: {
      EXPECT_CALL(service, HasSyncSetupCompleted())
                  .WillRepeatedly(Return(true));
      EXPECT_CALL(service, sync_initialized()).WillRepeatedly(Return(true));
      EXPECT_CALL(service, IsPassphraseRequired())
                  .WillRepeatedly(Return(false));
      browser_sync::SyncBackendHost::Status status;
      EXPECT_CALL(service, QueryDetailedSyncStatus(_))
                  .WillRepeatedly(DoAll(SetArgPointee<0>(status),
                                  Return(false)));
      EXPECT_CALL(service, HasUnrecoverableError())
                  .WillRepeatedly(Return(false));
      signin->set_auth_in_progress();
      return;
    }
    case STATUS_CASE_AUTH_ERROR: {
      EXPECT_CALL(service, HasSyncSetupCompleted())
                  .WillRepeatedly(Return(true));
      EXPECT_CALL(service, sync_initialized()).WillRepeatedly(Return(true));
      EXPECT_CALL(service, IsPassphraseRequired())
                  .WillRepeatedly(Return(false));
      browser_sync::SyncBackendHost::Status status;
      EXPECT_CALL(service, QueryDetailedSyncStatus(_))
                  .WillRepeatedly(DoAll(SetArgPointee<0>(status),
                                  Return(false)));
      provider->SetAuthError(
          kTestUser,
          kTestUser,
          GoogleServiceAuthError(GoogleServiceAuthError::SERVICE_UNAVAILABLE));
      EXPECT_CALL(service, HasUnrecoverableError())
                  .WillRepeatedly(Return(false));
      return;
    }
    case STATUS_CASE_PROTOCOL_ERROR: {
      EXPECT_CALL(service, HasSyncSetupCompleted())
                  .WillRepeatedly(Return(true));
      EXPECT_CALL(service, sync_initialized()).WillRepeatedly(Return(true));
      EXPECT_CALL(service, IsPassphraseRequired())
                  .WillRepeatedly(Return(false));
      syncer::SyncProtocolError protocolError;
      protocolError.action = syncer::STOP_AND_RESTART_SYNC;
      browser_sync::SyncBackendHost::Status status;
      status.sync_protocol_error = protocolError;
      EXPECT_CALL(service, QueryDetailedSyncStatus(_))
                  .WillRepeatedly(DoAll(SetArgPointee<0>(status),
                                  Return(false)));
      EXPECT_CALL(service, HasUnrecoverableError())
                  .WillRepeatedly(Return(false));
      return;
    }
    case STATUS_CASE_PASSPHRASE_ERROR: {
      EXPECT_CALL(service, HasSyncSetupCompleted())
                  .WillRepeatedly(Return(true));
      EXPECT_CALL(service, sync_initialized()).WillRepeatedly(Return(true));
      browser_sync::SyncBackendHost::Status status;
      EXPECT_CALL(service, QueryDetailedSyncStatus(_))
                  .WillRepeatedly(DoAll(SetArgPointee<0>(status),
                                  Return(false)));
      EXPECT_CALL(service, HasUnrecoverableError())
                  .WillRepeatedly(Return(false));
      EXPECT_CALL(service, IsPassphraseRequired())
                  .WillRepeatedly(Return(true));
      EXPECT_CALL(service, IsPassphraseRequiredForDecryption())
                  .WillRepeatedly(Return(true));
      return;
    }
    case STATUS_CASE_SYNCED: {
      EXPECT_CALL(service, HasSyncSetupCompleted())
              .WillRepeatedly(Return(true));
      EXPECT_CALL(service, sync_initialized()).WillRepeatedly(Return(true));
      EXPECT_CALL(service, IsPassphraseRequired())
                  .WillRepeatedly(Return(false));
      browser_sync::SyncBackendHost::Status status;
      EXPECT_CALL(service, QueryDetailedSyncStatus(_))
                  .WillRepeatedly(DoAll(SetArgPointee<0>(status),
                                  Return(false)));
      EXPECT_CALL(service, HasUnrecoverableError())
                  .WillRepeatedly(Return(false));
      EXPECT_CALL(service, IsPassphraseRequired())
                  .WillRepeatedly(Return(false));
      return;
    }
    case STATUS_CASE_SYNC_DISABLED_BY_POLICY: {
      EXPECT_CALL(service, IsManaged()).WillRepeatedly(Return(true));
      EXPECT_CALL(service, HasSyncSetupCompleted())
          .WillRepeatedly(Return(false));
      EXPECT_CALL(service, sync_initialized()).WillRepeatedly(Return(false));
      EXPECT_CALL(service, IsPassphraseRequired())
                  .WillRepeatedly(Return(false));
      browser_sync::SyncBackendHost::Status status;
      EXPECT_CALL(service, QueryDetailedSyncStatus(_))
                  .WillRepeatedly(DoAll(SetArgPointee<0>(status),
                                  Return(false)));
      EXPECT_CALL(service, HasUnrecoverableError())
                  .WillRepeatedly(Return(false));
      return;
    }
    default:
      NOTREACHED();
  }
}

// This test ensures that a each distinctive ProfileSyncService statuses
// will return a unique combination of status and link messages from
// GetStatusLabels().
TEST_F(SyncUIUtilTest, DistinctCasesReportUniqueMessageSets) {
  std::set<base::string16> messages;
  for (int idx = 0; idx != NUMBER_OF_STATUS_CASES; idx++) {
    scoped_ptr<Profile> profile(new TestingProfile());
    ProfileSyncServiceMock service(profile.get());
    GoogleServiceAuthError error = GoogleServiceAuthError::AuthErrorNone();
    EXPECT_CALL(service, GetAuthError()).WillRepeatedly(ReturnRef(error));
    FakeSigninManagerForSyncUIUtilTest signin(profile.get());
    signin.SetAuthenticatedUsername(kTestUser);
    scoped_ptr<FakeAuthStatusProvider> provider(new FakeAuthStatusProvider(
        ProfileOAuth2TokenServiceFactory::GetForProfile(profile.get())->
            signin_error_controller()));
    GetDistinctCase(service, &signin, provider.get(), idx);
    base::string16 status_label;
    base::string16 link_label;
    sync_ui_util::GetStatusLabels(&service,
                                  signin,
                                  sync_ui_util::WITH_HTML,
                                  &status_label,
                                  &link_label);
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
    scoped_ptr<Profile> profile(
        ProfileSyncServiceMock::MakeSignedInTestingProfile());
    ProfileSyncServiceMock service(profile.get());
    GoogleServiceAuthError error = GoogleServiceAuthError::AuthErrorNone();
    EXPECT_CALL(service, GetAuthError()).WillRepeatedly(ReturnRef(error));
    FakeSigninManagerForSyncUIUtilTest signin(profile.get());
    signin.SetAuthenticatedUsername(kTestUser);
    scoped_ptr<FakeAuthStatusProvider> provider(new FakeAuthStatusProvider(
        ProfileOAuth2TokenServiceFactory::GetForProfile(profile.get())->
            signin_error_controller()));
    GetDistinctCase(service, &signin, provider.get(), idx);
    base::string16 status_label;
    base::string16 link_label;
    sync_ui_util::GetStatusLabels(&service,
                                  signin,
                                  sync_ui_util::PLAIN_TEXT,
                                  &status_label,
                                  &link_label);

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
