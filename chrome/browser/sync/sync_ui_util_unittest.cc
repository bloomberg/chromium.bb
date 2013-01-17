// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>
#include "base/basictypes.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/signin_manager.h"
#include "chrome/browser/signin/signin_manager_fake.h"
#include "chrome/browser/sync/profile_sync_service_mock.h"
#include "chrome/browser/sync/sync_ui_util.h"
#include "content/public/test/test_browser_thread.h"
#include "grit/generated_resources.h"
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
  NUMBER_OF_STATUS_CASES
};

namespace {

// Mock that allows us to mock a SigninManager that is authenticating.
class SigninManagerMock : public SigninManager {
 public:
  MOCK_CONST_METHOD0(AuthInProgress, bool());
};

// Utility function to test that GetStatusLabelsForSyncGlobalError returns
// the correct results for the given states.
void VerifySyncGlobalErrorResult(NiceMock<ProfileSyncServiceMock>* service,
                                 const SigninManager& signin,
                                 GoogleServiceAuthError::State error_state,
                                 bool is_signed_in,
                                 bool is_error) {
  EXPECT_CALL(*service, HasSyncSetupCompleted())
              .WillRepeatedly(Return(is_signed_in));

  GoogleServiceAuthError auth_error(error_state);
  EXPECT_CALL(*service, GetAuthError()).WillRepeatedly(ReturnRef(auth_error));

  string16 label1, label2, label3;
  sync_ui_util::GetStatusLabelsForSyncGlobalError(
      service, signin, &label1, &label2, &label3);
  EXPECT_EQ(label1.empty(), !is_error);
  EXPECT_EQ(label2.empty(), !is_error);
  EXPECT_EQ(label3.empty(), !is_error);
}

} // namespace


// Test that GetStatusLabelsForSyncGlobalError returns an error if a
// passphrase is required.
TEST(SyncUIUtilTest, PassphraseGlobalError) {
  MessageLoopForUI message_loop;
  content::TestBrowserThread ui_thread(BrowserThread::UI, &message_loop);
  scoped_ptr<Profile> profile(
      ProfileSyncServiceMock::MakeSignedInTestingProfile());
  NiceMock<ProfileSyncServiceMock> service(profile.get());
  FakeSigninManager signin(profile.get());
  browser_sync::SyncBackendHost::Status status;
  EXPECT_CALL(service, QueryDetailedSyncStatus(_))
              .WillRepeatedly(Return(false));

  EXPECT_CALL(service, IsPassphraseRequired())
              .WillRepeatedly(Return(true));
  EXPECT_CALL(service, IsPassphraseRequiredForDecryption())
              .WillRepeatedly(Return(true));
  VerifySyncGlobalErrorResult(
      &service, signin, GoogleServiceAuthError::NONE, true, true);
}

// Test that GetStatusLabelsForSyncGlobalError returns an error if a
// passphrase is required and not for auth errors.
TEST(SyncUIUtilTest, AuthAndPassphraseGlobalError) {
  MessageLoopForUI message_loop;
  content::TestBrowserThread ui_thread(BrowserThread::UI, &message_loop);
  scoped_ptr<Profile> profile(
      ProfileSyncServiceMock::MakeSignedInTestingProfile());
  NiceMock<ProfileSyncServiceMock> service(profile.get());
  FakeSigninManager signin(profile.get());
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
  string16 menu_label, label2, label3;
  sync_ui_util::GetStatusLabelsForSyncGlobalError(
      &service, signin, &menu_label, &label2, &label3);
  // Make sure we are still displaying the passphrase error badge (don't show
  // auth errors through SyncUIUtil).
  EXPECT_EQ(menu_label, l10n_util::GetStringUTF16(
      IDS_SYNC_PASSPHRASE_ERROR_WRENCH_MENU_ITEM));
}

// Test that GetStatusLabelsForSyncGlobalError does not indicate errors for
// auth errors (these are reported through SigninGlobalError).
TEST(SyncUIUtilTest, AuthStateGlobalError) {
  MessageLoopForUI message_loop;
  content::TestBrowserThread ui_thread(BrowserThread::UI, &message_loop);
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

  FakeSigninManager signin(profile.get());
  for (size_t i = 0; i < arraysize(table); ++i) {
    VerifySyncGlobalErrorResult(&service, signin, table[i], true, false);
    VerifySyncGlobalErrorResult(&service, signin, table[i], false, false);
  }
}
// Loads a ProfileSyncServiceMock to emulate one of a number of distinct cases
// in order to perform tests on the generated messages.
void GetDistinctCase(ProfileSyncServiceMock& service,
                     SigninManagerMock& signin,
                     GoogleServiceAuthError** auth_error,
                     int caseNumber) {
  // Auth Error object is returned by reference in mock and needs to stay in
  // scope throughout test, so it is owned by calling method. However it is
  // immutable so can only be allocated in this method.
  switch (caseNumber) {
    case STATUS_CASE_SETUP_IN_PROGRESS: {
      EXPECT_CALL(service, HasSyncSetupCompleted())
                  .WillOnce(Return(false));
      EXPECT_CALL(service, FirstSetupInProgress())
                  .WillOnce(Return(true));
      browser_sync::SyncBackendHost::Status status;
      EXPECT_CALL(service, QueryDetailedSyncStatus(_))
                  .WillOnce(DoAll(SetArgPointee<0>(status),
                                  Return(false)));
      *auth_error = new GoogleServiceAuthError(GoogleServiceAuthError::NONE);
      EXPECT_CALL(service, GetAuthError())
                  .WillOnce(ReturnRef(**auth_error));
      EXPECT_CALL(signin, AuthInProgress()).WillRepeatedly(Return(false));
      return;
    }
   case STATUS_CASE_SETUP_ERROR: {
      EXPECT_CALL(service, HasSyncSetupCompleted())
                  .WillOnce(Return(false));
      EXPECT_CALL(service, FirstSetupInProgress())
                  .WillOnce(Return(false));
      EXPECT_CALL(service, HasUnrecoverableError())
                  .WillOnce(Return(true));
      EXPECT_CALL(signin, AuthInProgress()).WillRepeatedly(Return(false));
      browser_sync::SyncBackendHost::Status status;
      EXPECT_CALL(service, QueryDetailedSyncStatus(_))
                  .WillOnce(DoAll(SetArgPointee<0>(status),
                                  Return(false)));
      return;
    }
    case STATUS_CASE_AUTHENTICATING: {
      EXPECT_CALL(service, HasSyncSetupCompleted())
                  .WillOnce(Return(true));
      browser_sync::SyncBackendHost::Status status;
      EXPECT_CALL(service, QueryDetailedSyncStatus(_))
                  .WillOnce(DoAll(SetArgPointee<0>(status),
                                  Return(false)));
      EXPECT_CALL(service, HasUnrecoverableError())
                  .WillOnce(Return(false));
      EXPECT_CALL(signin, AuthInProgress()).WillRepeatedly(Return(true));
      *auth_error = new GoogleServiceAuthError(GoogleServiceAuthError::NONE);
      EXPECT_CALL(service, GetAuthError())
                  .WillOnce(ReturnRef(**auth_error));
      return;
    }
    case STATUS_CASE_AUTH_ERROR: {
      EXPECT_CALL(service, HasSyncSetupCompleted())
                  .WillOnce(Return(true));
      browser_sync::SyncBackendHost::Status status;
      EXPECT_CALL(service, QueryDetailedSyncStatus(_))
                  .WillOnce(DoAll(SetArgPointee<0>(status),
                                  Return(false)));
      *auth_error = new GoogleServiceAuthError(
         GoogleServiceAuthError::SERVICE_UNAVAILABLE);
      EXPECT_CALL(service, HasUnrecoverableError())
                  .WillOnce(Return(false));
      EXPECT_CALL(signin, AuthInProgress()).WillRepeatedly(Return(false));
      EXPECT_CALL(service, GetAuthError())
                  .WillOnce(ReturnRef(**auth_error));
      return;
    }
    case STATUS_CASE_PROTOCOL_ERROR: {
      EXPECT_CALL(service, HasSyncSetupCompleted())
                  .WillOnce(Return(true));
      syncer::SyncProtocolError protocolError;
      protocolError.action = syncer::STOP_AND_RESTART_SYNC;
      browser_sync::SyncBackendHost::Status status;
      status.sync_protocol_error = protocolError;
      EXPECT_CALL(service, QueryDetailedSyncStatus(_))
                  .WillOnce(DoAll(SetArgPointee<0>(status),
                                  Return(false)));
      *auth_error = new GoogleServiceAuthError(GoogleServiceAuthError::NONE);
      EXPECT_CALL(service, GetAuthError())
                  .WillOnce(ReturnRef(**auth_error));
      EXPECT_CALL(signin, AuthInProgress()).WillRepeatedly(Return(false));
      EXPECT_CALL(service, HasUnrecoverableError())
                  .WillOnce(Return(false));
      return;
    }
    case STATUS_CASE_PASSPHRASE_ERROR: {
      EXPECT_CALL(service, HasSyncSetupCompleted())
                  .WillOnce(Return(true));
      browser_sync::SyncBackendHost::Status status;
      EXPECT_CALL(service, QueryDetailedSyncStatus(_))
                  .WillOnce(DoAll(SetArgPointee<0>(status),
                                  Return(false)));
      *auth_error = new GoogleServiceAuthError(GoogleServiceAuthError::NONE);
      EXPECT_CALL(service, GetAuthError())
                  .WillOnce(ReturnRef(**auth_error));
      EXPECT_CALL(service, HasUnrecoverableError())
                  .WillOnce(Return(false));
      EXPECT_CALL(signin, AuthInProgress()).WillRepeatedly(Return(false));
      EXPECT_CALL(service, IsPassphraseRequired())
                  .WillOnce(Return(true));
      EXPECT_CALL(service, IsPassphraseRequiredForDecryption())
                  .WillOnce(Return(true));
      return;
    }
    case STATUS_CASE_SYNCED: {
      EXPECT_CALL(service, HasSyncSetupCompleted())
              .WillOnce(Return(true));
      browser_sync::SyncBackendHost::Status status;
      EXPECT_CALL(service, QueryDetailedSyncStatus(_))
                  .WillOnce(DoAll(SetArgPointee<0>(status),
                                  Return(false)));
      *auth_error = new GoogleServiceAuthError(GoogleServiceAuthError::NONE);
      EXPECT_CALL(service, GetAuthError())
                  .WillOnce(ReturnRef(**auth_error));
      EXPECT_CALL(signin, AuthInProgress()).WillRepeatedly(Return(false));
      EXPECT_CALL(service, HasUnrecoverableError())
                  .WillOnce(Return(false));
      EXPECT_CALL(service, IsPassphraseRequired())
                  .WillOnce(Return(false));
      return;
    }
    default:
      NOTREACHED();
  }
}

// This test ensures that a each distinctive ProfileSyncService statuses
// will return a unique combination of status and link messages from
// GetStatusLabels().
TEST(SyncUIUtilTest, DistinctCasesReportUniqueMessageSets) {
  std::set<string16> messages;
  for (int idx = 0; idx != NUMBER_OF_STATUS_CASES; idx++) {
    scoped_ptr<Profile> profile(
        ProfileSyncServiceMock::MakeSignedInTestingProfile());
    ProfileSyncServiceMock service(profile.get());
    NiceMock<SigninManagerMock> signin;
    GoogleServiceAuthError* auth_error = NULL;
    GetDistinctCase(service, signin, &auth_error, idx);
    string16 status_label;
    string16 link_label;
    sync_ui_util::GetStatusLabels(&service,
                                  signin,
                                  sync_ui_util::WITH_HTML,
                                  &status_label,
                                  &link_label);
    // If the status and link message combination is already present in the set
    // of messages already seen, this is a duplicate rather than a unique
    // message, and the test has failed.
    string16 combined_label =
        status_label + string16(ASCIIToUTF16("#")) + link_label;
    EXPECT_TRUE(messages.find(combined_label) == messages.end());
    messages.insert(combined_label);
    if (auth_error)
      delete auth_error;
  }
}

// This test ensures that the html_links parameter on GetStatusLabels() is
// honored.
TEST(SyncUIUtilTest, HtmlNotIncludedInStatusIfNotRequested) {
  for (int idx = 0; idx != NUMBER_OF_STATUS_CASES; idx++) {
    scoped_ptr<Profile> profile(
        ProfileSyncServiceMock::MakeSignedInTestingProfile());
    ProfileSyncServiceMock service(profile.get());
    NiceMock<SigninManagerMock> signin;
    GoogleServiceAuthError* auth_error = NULL;
    GetDistinctCase(service, signin, &auth_error, idx);
    string16 status_label;
    string16 link_label;
    sync_ui_util::GetStatusLabels(&service,
                                  signin,
                                  sync_ui_util::PLAIN_TEXT,
                                  &status_label,
                                  &link_label);
    // Ensures a search for string 'href' (found in links, not a string to be
    // found in an English language message) fails when links are excluded from
    // the status label.
    EXPECT_EQ(status_label.find(string16(ASCIIToUTF16("href"))),
              string16::npos);
    if (auth_error) {
      delete auth_error;
    }
  }
}
