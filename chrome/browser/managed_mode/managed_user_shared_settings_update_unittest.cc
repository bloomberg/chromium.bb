// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/managed_mode/managed_user_shared_settings_service.h"
#include "chrome/browser/managed_mode/managed_user_shared_settings_update.h"
#include "chrome/test/base/testing_profile.h"
#include "sync/api/sync_change.h"
#include "testing/gtest/include/gtest/gtest.h"

class ManagedUserSharedSettingsUpdateTest : public testing::Test {
 public:
  ManagedUserSharedSettingsUpdateTest() : service_(profile_.GetPrefs()) {}
  virtual ~ManagedUserSharedSettingsUpdateTest() {}

  void OnSettingUpdated(bool success) {
    result_.reset(new bool(success));
  }

 protected:
  TestingProfile profile_;
  ManagedUserSharedSettingsService service_;
  scoped_ptr<bool> result_;
};

TEST_F(ManagedUserSharedSettingsUpdateTest, Success) {
  ManagedUserSharedSettingsUpdate update(
      &service_,
      "abcdef",
      "name",
      scoped_ptr<base::Value>(new base::StringValue("Hans Moleman")),
      base::Bind(&ManagedUserSharedSettingsUpdateTest::OnSettingUpdated,
                 base::Unretained(this)));
  syncer::SyncChangeList changes;
  changes.push_back(syncer::SyncChange(
      FROM_HERE,
      syncer::SyncChange::ACTION_UPDATE,
      ManagedUserSharedSettingsService::CreateSyncDataForSetting(
          "abcdef", "name", base::StringValue("Hans Moleman"), true)));
  syncer::SyncError error = service_.ProcessSyncChanges(FROM_HERE, changes);
  EXPECT_FALSE(error.IsSet()) << error.ToString();
  ASSERT_TRUE(result_);
  EXPECT_TRUE(*result_);
}

TEST_F(ManagedUserSharedSettingsUpdateTest, Failure) {
  ManagedUserSharedSettingsUpdate update(
      &service_,
      "abcdef",
      "name",
      scoped_ptr<base::Value>(new base::StringValue("Hans Moleman")),
      base::Bind(&ManagedUserSharedSettingsUpdateTest::OnSettingUpdated,
                 base::Unretained(this)));

  // Syncing down a different change will cause the update to fail.
  syncer::SyncChangeList changes;
  changes.push_back(syncer::SyncChange(
      FROM_HERE,
      syncer::SyncChange::ACTION_UPDATE,
      ManagedUserSharedSettingsService::CreateSyncDataForSetting(
          "abcdef",
          "name",
          base::StringValue("Barney Gumble"),
          true)));
  syncer::SyncError error = service_.ProcessSyncChanges(FROM_HERE, changes);
  EXPECT_FALSE(error.IsSet()) << error.ToString();
  ASSERT_TRUE(result_);
  EXPECT_FALSE(*result_);
}

TEST_F(ManagedUserSharedSettingsUpdateTest, Cancel) {
  {
    ManagedUserSharedSettingsUpdate update(
        &service_,
        "abcdef",
        "name",
        scoped_ptr<base::Value>(new base::StringValue("Hans Moleman")),
        base::Bind(&ManagedUserSharedSettingsUpdateTest::OnSettingUpdated,
                   base::Unretained(this)));
    ASSERT_FALSE(result_);
  }
  ASSERT_FALSE(result_);
}
