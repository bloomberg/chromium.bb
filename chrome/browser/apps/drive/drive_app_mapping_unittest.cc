// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/drive/drive_app_mapping.h"

#include <memory>

#include "base/macros.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"

class DriveAppMappingTest : public testing::Test {
 public:
  DriveAppMappingTest() {}
  ~DriveAppMappingTest() override {}

  // testing::Test:
  void SetUp() override {
    pref_service_.reset(new sync_preferences::TestingPrefServiceSyncable);
    DriveAppMapping::RegisterProfilePrefs(pref_service_->registry());

    mapping_.reset(new DriveAppMapping(pref_service_.get()));
  }

  DriveAppMapping* mapping() { return mapping_.get(); }

 private:
  std::unique_ptr<sync_preferences::TestingPrefServiceSyncable> pref_service_;
  std::unique_ptr<DriveAppMapping> mapping_;

  DISALLOW_COPY_AND_ASSIGN(DriveAppMappingTest);
};

TEST_F(DriveAppMappingTest, Empty) {
  EXPECT_EQ("", mapping()->GetChromeApp(""));
  EXPECT_EQ("", mapping()->GetDriveApp(""));
  EXPECT_EQ("", mapping()->GetChromeApp("non-existent-drive-app"));
  EXPECT_EQ("", mapping()->GetDriveApp("non-existent-chrome-app"));
  EXPECT_EQ(0u, mapping()->GetDriveAppIds().size());
}

TEST_F(DriveAppMappingTest, Add) {
  std::set<std::string> drive_app_ids;

  // Add one.
  mapping()->Add("drive", "chrome", false);
  EXPECT_EQ("chrome", mapping()->GetChromeApp("drive"));
  EXPECT_EQ("drive", mapping()->GetDriveApp("chrome"));
  EXPECT_FALSE(mapping()->IsChromeAppGenerated("chrome"));
  drive_app_ids = mapping()->GetDriveAppIds();
  EXPECT_EQ(1u, drive_app_ids.size());
  EXPECT_EQ(1u, drive_app_ids.count("drive"));

  // Overwrite previous mapping if added under the same key.
  mapping()->Add("drive", "another-chrome-app", true);
  EXPECT_EQ("", mapping()->GetDriveApp("chrome"));
  EXPECT_FALSE(mapping()->IsChromeAppGenerated("chrome"));
  EXPECT_EQ("another-chrome-app", mapping()->GetChromeApp("drive"));
  EXPECT_EQ("drive", mapping()->GetDriveApp("another-chrome-app"));
  EXPECT_TRUE(mapping()->IsChromeAppGenerated("another-chrome-app"));
  drive_app_ids = mapping()->GetDriveAppIds();
  EXPECT_EQ(1u, drive_app_ids.size());
  EXPECT_EQ(1u, drive_app_ids.count("drive"));

  // Add another one.
  mapping()->Add("drive-1", "chrome-1", false);
  EXPECT_EQ("chrome-1", mapping()->GetChromeApp("drive-1"));
  EXPECT_EQ("drive-1", mapping()->GetDriveApp("chrome-1"));
  drive_app_ids = mapping()->GetDriveAppIds();
  EXPECT_EQ(2u, drive_app_ids.size());
  EXPECT_EQ(1u, drive_app_ids.count("drive"));
  EXPECT_EQ(1u, drive_app_ids.count("drive-1"));

  // Previous mapping should not be affected by new mapping.
  EXPECT_EQ("another-chrome-app", mapping()->GetChromeApp("drive"));
  EXPECT_EQ("drive", mapping()->GetDriveApp("another-chrome-app"));
  EXPECT_TRUE(mapping()->IsChromeAppGenerated("another-chrome-app"));

  // Non-existent value returns empty string.
  EXPECT_EQ("", mapping()->GetChromeApp("non-existent-drive-app"));
  EXPECT_EQ("", mapping()->GetDriveApp("non-existent-chrome-app"));
}

TEST_F(DriveAppMappingTest, Remove) {
  std::set<std::string> drive_app_ids;

  // Prepare data.
  mapping()->Add("drive-1", "chrome-1", false);
  mapping()->Add("drive-2", "chrome-2", false);
  drive_app_ids = mapping()->GetDriveAppIds();
  EXPECT_EQ(2u, drive_app_ids.size());

  // Remove non-existent.
  mapping()->Remove("non-existent-drive-app");
  drive_app_ids = mapping()->GetDriveAppIds();
  EXPECT_EQ(2u, drive_app_ids.size());

  // Remove one.
  EXPECT_EQ("chrome-1", mapping()->GetChromeApp("drive-1"));
  mapping()->Remove("drive-1");
  EXPECT_EQ("", mapping()->GetChromeApp("drive-1"));
  drive_app_ids = mapping()->GetDriveAppIds();
  EXPECT_EQ(1u, drive_app_ids.size());

  // Remove it again has no effect.
  mapping()->Remove("drive-1");
  drive_app_ids = mapping()->GetDriveAppIds();
  EXPECT_EQ(1u, drive_app_ids.size());

  // Remove another one.
  EXPECT_EQ("chrome-2", mapping()->GetChromeApp("drive-2"));
  mapping()->Remove("drive-2");
  EXPECT_EQ("", mapping()->GetChromeApp("drive-2"));
  drive_app_ids = mapping()->GetDriveAppIds();
  EXPECT_EQ(0u, drive_app_ids.size());
}

TEST_F(DriveAppMappingTest, TrackUninstall) {
  const std::string drive_app_id = "drive-1";

  mapping()->AddUninstalledDriveApp(drive_app_id);
  EXPECT_TRUE(mapping()->IsUninstalledDriveApp(drive_app_id));

  mapping()->RemoveUninstalledDriveApp(drive_app_id);
  EXPECT_FALSE(mapping()->IsUninstalledDriveApp(drive_app_id));
}
