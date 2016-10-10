// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/legacy/supervised_user_shared_settings_update.h"

#include <memory>

#include "base/bind.h"
#include "chrome/browser/supervised_user/legacy/supervised_user_shared_settings_service.h"
#include "chrome/test/base/testing_profile.h"
#include "components/sync/model/sync_change.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

class SupervisedUserSharedSettingsUpdateTest : public testing::Test {
 public:
  SupervisedUserSharedSettingsUpdateTest() : service_(profile_.GetPrefs()) {}
  ~SupervisedUserSharedSettingsUpdateTest() override {}

  void OnSettingUpdated(bool success) {
    result_.reset(new bool(success));
  }

 protected:
  content::TestBrowserThreadBundle thread_bundle_;
  TestingProfile profile_;
  SupervisedUserSharedSettingsService service_;
  std::unique_ptr<bool> result_;
};

TEST_F(SupervisedUserSharedSettingsUpdateTest, Success) {
  SupervisedUserSharedSettingsUpdate update(
      &service_, "abcdef", "name",
      std::unique_ptr<base::Value>(new base::StringValue("Hans Moleman")),
      base::Bind(&SupervisedUserSharedSettingsUpdateTest::OnSettingUpdated,
                 base::Unretained(this)));
  syncer::SyncChangeList changes;
  changes.push_back(syncer::SyncChange(
      FROM_HERE,
      syncer::SyncChange::ACTION_UPDATE,
      SupervisedUserSharedSettingsService::CreateSyncDataForSetting(
          "abcdef", "name", base::StringValue("Hans Moleman"), true)));
  syncer::SyncError error = service_.ProcessSyncChanges(FROM_HERE, changes);
  EXPECT_FALSE(error.IsSet()) << error.ToString();
  ASSERT_TRUE(result_);
  EXPECT_TRUE(*result_);
}

TEST_F(SupervisedUserSharedSettingsUpdateTest, Failure) {
  SupervisedUserSharedSettingsUpdate update(
      &service_, "abcdef", "name",
      std::unique_ptr<base::Value>(new base::StringValue("Hans Moleman")),
      base::Bind(&SupervisedUserSharedSettingsUpdateTest::OnSettingUpdated,
                 base::Unretained(this)));

  // Syncing down a different change will cause the update to fail.
  syncer::SyncChangeList changes;
  changes.push_back(syncer::SyncChange(
      FROM_HERE,
      syncer::SyncChange::ACTION_UPDATE,
      SupervisedUserSharedSettingsService::CreateSyncDataForSetting(
          "abcdef",
          "name",
          base::StringValue("Barney Gumble"),
          true)));
  syncer::SyncError error = service_.ProcessSyncChanges(FROM_HERE, changes);
  EXPECT_FALSE(error.IsSet()) << error.ToString();
  ASSERT_TRUE(result_);
  EXPECT_FALSE(*result_);
}

TEST_F(SupervisedUserSharedSettingsUpdateTest, Cancel) {
  {
    SupervisedUserSharedSettingsUpdate update(
        &service_, "abcdef", "name",
        std::unique_ptr<base::Value>(new base::StringValue("Hans Moleman")),
        base::Bind(&SupervisedUserSharedSettingsUpdateTest::OnSettingUpdated,
                   base::Unretained(this)));
    ASSERT_FALSE(result_);
  }
  ASSERT_FALSE(result_);
}
