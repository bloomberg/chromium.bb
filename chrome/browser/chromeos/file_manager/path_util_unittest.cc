// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_manager/path_util.h"

#include "base/files/file_path.h"
#include "chrome/test/base/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace file_manager {
namespace util {
namespace {

TEST(FileManagerPathUtilTest, MyDriveFolderMigration) {
  TestingProfile profile;
  base::FilePath path;

  EXPECT_TRUE(MigratePathFromOldFormat(
      &profile,
      base::FilePath::FromUTF8Unsafe("/special/drive"),
      &path));
  EXPECT_EQ(base::FilePath::FromUTF8Unsafe("/special/drive/root"), path);

  EXPECT_TRUE(MigratePathFromOldFormat(
      &profile,
      base::FilePath::FromUTF8Unsafe("/special/drive/a/b"),
      &path));
  EXPECT_EQ(base::FilePath::FromUTF8Unsafe("/special/drive/root/a/b"), path);

  // Path already in the drive/root format should not be converted.
  EXPECT_FALSE(MigratePathFromOldFormat(
      &profile,
      base::FilePath::FromUTF8Unsafe("/special/drive/root/a/b"),
      &path));
}

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

}  // namespace
}  // namespace util
}  // namespace file_manager
