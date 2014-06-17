// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/supervised_user/supervised_user_shared_settings_service.h"
#include "chrome/browser/supervised_user/supervised_user_shared_settings_update.h"
#include "chrome/test/base/testing_profile.h"
#include "sync/api/sync_change.h"
#include "testing/gtest/include/gtest/gtest.h"

class SupervisedUserSharedSettingsUpdateTest : public testing::Test {
 public:
  SupervisedUserSharedSettingsUpdateTest() : service_(profile_.GetPrefs()) {}
  virtual ~SupervisedUserSharedSettingsUpdateTest() {}

  void OnSettingUpdated(bool success) {
    result_.reset(new bool(success));
  }

 protected:
  TestingProfile profile_;
  SupervisedUserSharedSettingsService service_;
  scoped_ptr<bool> result_;
};

TEST_F(SupervisedUserSharedSettingsUpdateTest, Success) {
  SupervisedUserSharedSettingsUpdate update(
      &service_,
      "abcdef",
      "name",
      scoped_ptr<base::Value>(new base::StringValue("Hans Moleman")),
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
      &service_,
      "abcdef",
      "name",
      scoped_ptr<base::Value>(new base::StringValue("Hans Moleman")),
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
        &service_,
        "abcdef",
        "name",
        scoped_ptr<base::Value>(new base::StringValue("Hans Moleman")),
        base::Bind(&SupervisedUserSharedSettingsUpdateTest::OnSettingUpdated,
                   base::Unretained(this)));
    ASSERT_FALSE(result_);
  }
  ASSERT_FALSE(result_);
}
