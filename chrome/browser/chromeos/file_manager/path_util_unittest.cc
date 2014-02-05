// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_manager/path_util.h"

#include "base/files/file_path.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/test/base/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace file_manager {
namespace util {
namespace {

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

TEST(FileManagerPathUtilTest, MultiProfileDriveFolderMigration) {
  TestingProfile profile;

  // This looks like "/special/drive-xxx" in the production
  // environment.
  const base::FilePath kDrive = drive::util::GetDriveMountPointPath(&profile);

  base::FilePath path;

  EXPECT_TRUE(MigratePathFromOldFormat(
      &profile,
      base::FilePath::FromUTF8Unsafe("/special/drive"),
      &path));
  EXPECT_EQ(kDrive, path);

  EXPECT_TRUE(MigratePathFromOldFormat(
      &profile,
      base::FilePath::FromUTF8Unsafe("/special/drive/a/b"),
      &path));
  EXPECT_EQ(kDrive.AppendASCII("a/b"), path);

  // Path already in the new format is not converted.
  EXPECT_FALSE(MigratePathFromOldFormat(
      &profile,
      kDrive.AppendASCII("a/b"),
      &path));

  // Only the "/special/drive" path is converted.
  EXPECT_FALSE(MigratePathFromOldFormat(
      &profile,
      base::FilePath::FromUTF8Unsafe("/special/notdrive"),
      &path));
}

}  // namespace
}  // namespace util
}  // namespace file_manager
