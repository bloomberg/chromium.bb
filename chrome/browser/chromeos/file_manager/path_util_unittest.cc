// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_manager/path_util.h"

#include "base/files/file_path.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace file_manager {
namespace util {
namespace {

class ProfileRelatedTest : public testing::Test {
 protected:
  ProfileRelatedTest()
      : testing_profile_manager_(TestingBrowserProcess::GetGlobal()) {
  }

  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(testing_profile_manager_.SetUp());
  }

  Profile* CreateProfileWithName(const std::string& name) {
    return testing_profile_manager_.CreateTestingProfile(
        chrome::kProfileDirPrefix + name);
  }

 private:
  TestingProfileManager testing_profile_manager_;
};

TEST(FileManagerPathUtilTest, MultiProfileDownloadsFolderMigration) {
  TestingProfile profile;

  // This looks like "/home/chronos/u-hash/Downloads" in the production
  // environment.
  const base::FilePath kDownloads = GetDownloadsFolderForProfile(&profile);

  base::FilePath path;

  EXPECT_TRUE(MigratePathFromOldFormat(
      &profile,
      base::FilePath::FromUTF8Unsafe("/home/chronos/user/Downloads"),
      &path));
  EXPECT_EQ(kDownloads, path);

  EXPECT_TRUE(MigratePathFromOldFormat(
      &profile,
      base::FilePath::FromUTF8Unsafe("/home/chronos/user/Downloads/a/b"),
      &path));
  EXPECT_EQ(kDownloads.AppendASCII("a/b"), path);

  // Path already in the new format is not converted.
  EXPECT_FALSE(MigratePathFromOldFormat(
      &profile,
      kDownloads.AppendASCII("a/b"),
      &path));

  // Only the "Downloads" path is converted.
  EXPECT_FALSE(MigratePathFromOldFormat(
      &profile,
      base::FilePath::FromUTF8Unsafe("/home/chronos/user/dl"),
      &path));
}

TEST_F(ProfileRelatedTest, MultiProfileDriveFolderMigration) {
  Profile* profile = CreateProfileWithName("hash");

  const base::FilePath kDrive = drive::util::GetDriveMountPointPath(profile);
  ASSERT_EQ(base::FilePath::FromUTF8Unsafe("/special/drive-hash"), kDrive);

  base::FilePath path;

  EXPECT_TRUE(MigratePathFromOldFormat(
      profile,
      base::FilePath::FromUTF8Unsafe("/special/drive"),
      &path));
  EXPECT_EQ(kDrive, path);

  EXPECT_TRUE(MigratePathFromOldFormat(
      profile,
      base::FilePath::FromUTF8Unsafe("/special/drive/a/b"),
      &path));
  EXPECT_EQ(kDrive.AppendASCII("a/b"), path);

  // Path already in the new format is not converted.
  EXPECT_FALSE(MigratePathFromOldFormat(
      profile,
      kDrive.AppendASCII("a/b"),
      &path));

  // Only the "/special/drive" path is converted.
  EXPECT_FALSE(MigratePathFromOldFormat(
      profile,
      base::FilePath::FromUTF8Unsafe("/special/notdrive"),
      &path));
}

}  // namespace
}  // namespace util
}  // namespace file_manager
