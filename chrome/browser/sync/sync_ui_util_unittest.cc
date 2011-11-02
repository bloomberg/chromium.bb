// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/sync/profile_sync_service_mock.h"
#include "chrome/browser/sync/sync_ui_util.h"
#include "content/test/test_browser_thread.h"
#include "testing/gmock/include/gmock/gmock-actions.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Return;
using ::testing::NiceMock;
using content::BrowserThread;

namespace {

// Utility function to test that GetStatusLabelsForSyncGlobalError returns
// the correct results for the given states.
void VerifySyncGlobalErrorResult(NiceMock<ProfileSyncServiceMock>* service,
                                 GoogleServiceAuthError::State error_state,
                                 bool is_signed_in,
                                 bool is_error) {
  GoogleServiceAuthError auth_error(error_state);
  service->UpdateAuthErrorState(auth_error);

  EXPECT_CALL(*service, HasSyncSetupCompleted())
              .WillRepeatedly(Return(is_signed_in));
  if (error_state == GoogleServiceAuthError::SERVICE_UNAVAILABLE) {
    EXPECT_CALL(*service, GetAuthenticatedUsername())
                .WillRepeatedly(Return(UTF8ToUTF16("")));
  } else {
    EXPECT_CALL(*service, GetAuthenticatedUsername())
                .WillRepeatedly(Return(UTF8ToUTF16("foo")));
  }

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
