// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "chrome/browser/sync/sync_ui_util.h"
#include "chrome/browser/sync/profile_sync_service_mock.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gmock/include/gmock/gmock-actions.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Return;
using ::testing::NiceMock;
TEST(SyncUIUtilTest, ConstructAboutInformationWithUnrecoverableErrorTest) {
  NiceMock<ProfileSyncServiceMock> service;
  DictionaryValue strings;

  // Will be released when the dictionary is destroyed
  string16 str(ASCIIToUTF16("none"));

  browser_sync::SyncBackendHost::Status status =
      { browser_sync::SyncBackendHost::Status::OFFLINE_UNUSABLE };

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

