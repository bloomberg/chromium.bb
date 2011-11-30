// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>
#include "base/basictypes.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/sync/profile_sync_service_mock.h"
#include "chrome/browser/sync/sync_ui_util.h"
#include "content/test/test_browser_thread.h"
#include "testing/gmock/include/gmock/gmock-actions.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::AtMost;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::NiceMock;
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

// Utility function to test that GetStatusLabelsForSyncGlobalError returns
// the correct results for the given states.
void VerifySyncGlobalErrorResult(NiceMock<ProfileSyncServiceMock>* service,
                                 GoogleServiceAuthError::State error_state,
                                 bool is_signed_in,
                                 bool is_error) {
  EXPECT_CALL(*service, HasSyncSetupCompleted())
              .WillRepeatedly(Return(is_signed_in));

  if (error_state == GoogleServiceAuthError::SERVICE_UNAVAILABLE) {
    EXPECT_CALL(*service, GetAuthenticatedUsername())
                .WillRepeatedly(Return(UTF8ToUTF16("")));
  } else {
    EXPECT_CALL(*service, GetAuthenticatedUsername())
                .WillRepeatedly(Return(UTF8ToUTF16("foo")));
  }

  GoogleServiceAuthError auth_error(error_state);
  EXPECT_CALL(*service, GetAuthError()).WillRepeatedly(ReturnRef(auth_error));

  string16 label1, label2, label3;
  sync_ui_util::GetStatusLabelsForSyncGlobalError(
      service, &label1, &label2, &label3);
  EXPECT_EQ(label1.empty(), !is_error);
  EXPECT_EQ(label2.empty(), !is_error);
  EXPECT_EQ(label3.empty(), !is_error);
}

} // namespace

TEST(SyncUIUtilTest, ConstructAboutInformationWithUnrecoverableErrorTest) {
  MessageLoopForUI message_loop;
  content::TestBrowserThread ui_thread(BrowserThread::UI, &message_loop);
  NiceMock<ProfileSyncServiceMock> service;
  DictionaryValue strings;

  // Will be released when the dictionary is destroyed
  string16 str(ASCIIToUTF16("none"));

  browser_sync::SyncBackendHost::Status status;
  status.summary = browser_sync::SyncBackendHost::Status::OFFLINE_UNUSABLE;

  EXPECT_CALL(service, HasSyncSetupCompleted())
              .WillOnce(Return(true));
  EXPECT_CALL(service, QueryDetailedSyncStatus())
              .WillOnce(Return(status));

  GoogleServiceAuthError auth_error(GoogleServiceAuthError::NONE);
  EXPECT_CALL(service, GetAuthError()).WillOnce(ReturnRef(auth_error));

  EXPECT_CALL(service, unrecoverable_error_detected())
             .WillOnce(Return(true));

  EXPECT_CALL(service, GetLastSyncedTimeString())
             .WillOnce(Return(str));

  sync_ui_util::ConstructAboutInformation(&service, &strings);

  EXPECT_TRUE(strings.HasKey("unrecoverable_error_detected"));
}

// Test that GetStatusLabelsForSyncGlobalError returns an error if a
// passphrase is required.
TEST(SyncUIUtilTest, PassphraseGlobalError) {
  MessageLoopForUI message_loop;
  content::TestBrowserThread ui_thread(BrowserThread::UI, &message_loop);
  NiceMock<ProfileSyncServiceMock> service;

  EXPECT_CALL(service, IsPassphraseRequired())
              .WillOnce(Return(true));
  EXPECT_CALL(service, IsPassphraseRequiredForDecryption())
              .WillOnce(Return(true));
  VerifySyncGlobalErrorResult(
      &service, GoogleServiceAuthError::NONE, true, true);
}

// Test that GetStatusLabelsForSyncGlobalError indicates errors for conditions
// that can be resolved by the user and suppresses errors for conditions that
// cannot be resolved by the user.
TEST(SyncUIUtilTest, AuthStateGlobalError) {
  MessageLoopForUI message_loop;
  content::TestBrowserThread ui_thread(BrowserThread::UI, &message_loop);
  NiceMock<ProfileSyncServiceMock> service;

  browser_sync::SyncBackendHost::Status status;
  EXPECT_CALL(service, QueryDetailedSyncStatus())
              .WillRepeatedly(Return(status));

  struct {
    GoogleServiceAuthError::State error_state;
    bool is_error;
  } table[] = {
    { GoogleServiceAuthError::NONE, false },
    { GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS, true },
    { GoogleServiceAuthError::USER_NOT_SIGNED_UP, true },
    { GoogleServiceAuthError::CONNECTION_FAILED, false },
    { GoogleServiceAuthError::CAPTCHA_REQUIRED, true },
    { GoogleServiceAuthError::ACCOUNT_DELETED, true },
    { GoogleServiceAuthError::ACCOUNT_DISABLED, true },
    { GoogleServiceAuthError::SERVICE_UNAVAILABLE, true },
    { GoogleServiceAuthError::TWO_FACTOR, true },
    { GoogleServiceAuthError::REQUEST_CANCELED, true },
    { GoogleServiceAuthError::HOSTED_NOT_ALLOWED, true },
  };

  for (size_t i = 0; i < sizeof(table)/sizeof(*table); ++i) {
    VerifySyncGlobalErrorResult(
        &service, table[i].error_state, true, table[i].is_error);
    VerifySyncGlobalErrorResult(
        &service, table[i].error_state, false, false);
  }
}
// Loads a ProfileSyncServiceMock to emulate one of a number of distinct cases
// in order to perform tests on the generated messages.
void GetDistinctCase(ProfileSyncServiceMock& service,
                     GoogleServiceAuthError** auth_error,
                     int caseNumber) {
  // Auth Error object is returned by reference in mock and needs to stay in
  // scope throughout test, so it is owned by calling method. However it is
  // immutable so can only be allocated in this method.
  switch (caseNumber) {
    case STATUS_CASE_SETUP_IN_PROGRESS: {
      EXPECT_CALL(service, HasSyncSetupCompleted())
                  .WillOnce(Return(false));
      EXPECT_CALL(service, SetupInProgress())
                  .WillOnce(Return(true));
      browser_sync::SyncBackendHost::Status status;
      status.summary = browser_sync::SyncBackendHost::Status::READY;
      EXPECT_CALL(service, QueryDetailedSyncStatus())
                  .WillOnce(Return(status));
      *auth_error = new GoogleServiceAuthError(GoogleServiceAuthError::NONE);
      EXPECT_CALL(service, GetAuthError())
                  .WillOnce(ReturnRef(**auth_error));
      EXPECT_CALL(service, UIShouldDepictAuthInProgress())
                  .WillOnce(Return(false));
      return;
    }
   case STATUS_CASE_SETUP_ERROR: {
      EXPECT_CALL(service, HasSyncSetupCompleted())
                  .WillOnce(Return(false));
      EXPECT_CALL(service, SetupInProgress())
                  .WillOnce(Return(false));
      EXPECT_CALL(service, unrecoverable_error_detected())
                  .WillOnce(Return(true));
      browser_sync::SyncBackendHost::Status status;
      status.summary = browser_sync::SyncBackendHost::Status::READY;
      EXPECT_CALL(service, QueryDetailedSyncStatus())
                  .WillOnce(Return(status));
      return;
    }
    case STATUS_CASE_AUTHENTICATING: {
      EXPECT_CALL(service, HasSyncSetupCompleted())
                  .WillOnce(Return(true));
      browser_sync::SyncBackendHost::Status status;
      status.summary = browser_sync::SyncBackendHost::Status::READY;
      EXPECT_CALL(service, QueryDetailedSyncStatus())
                  .WillOnce(Return(status));
      EXPECT_CALL(service, unrecoverable_error_detected())
                  .WillOnce(Return(false));
      EXPECT_CALL(service, UIShouldDepictAuthInProgress())
                  .WillOnce(Return(true));
      *auth_error = new GoogleServiceAuthError(GoogleServiceAuthError::NONE);
      EXPECT_CALL(service, GetAuthError())
                  .WillOnce(ReturnRef(**auth_error));
      return;
    }
    case STATUS_CASE_AUTH_ERROR: {
      EXPECT_CALL(service, HasSyncSetupCompleted())
                  .WillOnce(Return(true));
      browser_sync::SyncBackendHost::Status status;
      status.summary = browser_sync::SyncBackendHost::Status::READY;
      EXPECT_CALL(service, QueryDetailedSyncStatus())
                  .WillOnce(Return(status));
      *auth_error = new GoogleServiceAuthError(
         GoogleServiceAuthError::SERVICE_UNAVAILABLE);
      EXPECT_CALL(service, unrecoverable_error_detected())
                  .WillOnce(Return(false));
      EXPECT_CALL(service, GetAuthenticatedUsername())
                  .Times(AtMost(1)).WillRepeatedly(Return(ASCIIToUTF16("")));
      EXPECT_CALL(service, GetAuthError())
                  .WillOnce(ReturnRef(**auth_error));
      EXPECT_CALL(service, UIShouldDepictAuthInProgress())
                  .WillOnce(Return(false));
      return;
    }
    case STATUS_CASE_PROTOCOL_ERROR: {
      EXPECT_CALL(service, HasSyncSetupCompleted())
                  .WillOnce(Return(true));
      browser_sync::SyncProtocolError protocolError;
      protocolError.action = browser_sync::STOP_AND_RESTART_SYNC;
      browser_sync::SyncBackendHost::Status status;
      status.summary = browser_sync::SyncBackendHost::Status::READY;
      status.sync_protocol_error = protocolError;
      EXPECT_CALL(service, QueryDetailedSyncStatus())
                  .WillOnce(Return(status));
      *auth_error = new GoogleServiceAuthError(GoogleServiceAuthError::NONE);
      EXPECT_CALL(service, GetAuthError())
                  .WillOnce(ReturnRef(**auth_error));
      EXPECT_CALL(service, unrecoverable_error_detected())
                  .WillOnce(Return(false));
      EXPECT_CALL(service, UIShouldDepictAuthInProgress())
                  .WillOnce(Return(false));
      return;
    }
    case STATUS_CASE_PASSPHRASE_ERROR: {
      EXPECT_CALL(service, HasSyncSetupCompleted())
                  .WillOnce(Return(true));
      browser_sync::SyncBackendHost::Status status;
      status.summary = browser_sync::SyncBackendHost::Status::READY;
      EXPECT_CALL(service, QueryDetailedSyncStatus())
                  .WillOnce(Return(status));
      *auth_error = new GoogleServiceAuthError(GoogleServiceAuthError::NONE);
      EXPECT_CALL(service, GetAuthError())
                  .WillOnce(ReturnRef(**auth_error));
      EXPECT_CALL(service, unrecoverable_error_detected())
                  .WillOnce(Return(false));
      EXPECT_CALL(service, GetAuthenticatedUsername())
                  .WillOnce(Return(ASCIIToUTF16("example@example.com")));
      EXPECT_CALL(service, UIShouldDepictAuthInProgress())
                  .WillOnce(Return(false));
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
      status.summary = browser_sync::SyncBackendHost::Status::READY;
      EXPECT_CALL(service, QueryDetailedSyncStatus())
                  .WillOnce(Return(status));
      *auth_error = new GoogleServiceAuthError(GoogleServiceAuthError::NONE);
      EXPECT_CALL(service, GetAuthError())
                  .WillOnce(ReturnRef(**auth_error));
      EXPECT_CALL(service, unrecoverable_error_detected())
                  .WillOnce(Return(false));
      EXPECT_CALL(service, GetAuthenticatedUsername())
                  .WillOnce(Return(ASCIIToUTF16("example@example.com")));
      EXPECT_CALL(service, UIShouldDepictAuthInProgress())
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
  ProfileSyncServiceMock service;

  std::set<string16> messages;
  for (int idx = 0; idx != NUMBER_OF_STATUS_CASES; idx++) {
    ProfileSyncServiceMock service;
    GoogleServiceAuthError* auth_error = NULL;
    GetDistinctCase(service, &auth_error, idx);
    string16 status_label;
    string16 link_label;
    sync_ui_util::GetStatusLabels(&service,
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
  ProfileSyncServiceMock service;
  for (int idx = 0; idx != NUMBER_OF_STATUS_CASES; idx++) {
    ProfileSyncServiceMock service;
    GoogleServiceAuthError* auth_error = NULL;
    GetDistinctCase(service, &auth_error, idx);
    string16 status_label;
    string16 link_label;
    sync_ui_util::GetStatusLabels(&service,
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
