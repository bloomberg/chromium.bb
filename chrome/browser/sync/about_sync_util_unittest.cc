// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/about_sync_util.h"
#include "chrome/browser/sync/profile_sync_service_mock.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::NiceMock;
using ::testing::Return;
using ::testing::_;

namespace sync_ui_util {
namespace {

TEST(SyncUIUtilTestAbout, ConstructAboutInformationWithUnrecoverableErrorTest) {
  content::TestBrowserThreadBundle thread_bundle(
      content::TestBrowserThreadBundle::DEFAULT);
  scoped_ptr<Profile> profile(
      ProfileSyncServiceMock::MakeSignedInTestingProfile());
  NiceMock<ProfileSyncServiceMock> service(profile.get());

  // Will be released when the dictionary is destroyed
  base::string16 str(base::ASCIIToUTF16("none"));

  browser_sync::SyncBackendHost::Status status;

  EXPECT_CALL(service, HasSyncSetupCompleted())
              .WillOnce(Return(true));
  EXPECT_CALL(service, QueryDetailedSyncStatus(_))
              .WillOnce(Return(false));

  EXPECT_CALL(service, HasUnrecoverableError())
              .WillRepeatedly(Return(true));

  EXPECT_CALL(service, GetLastSyncedTimeString())
              .WillOnce(Return(str));

  scoped_ptr<base::DictionaryValue> strings(
      ConstructAboutInformation(&service));

  EXPECT_TRUE(strings->HasKey("unrecoverable_error_detected"));
}

} // namespace
} // namespace sync_ui_util
