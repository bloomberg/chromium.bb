// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/pup_data/pup_disk_util.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "chrome/chrome_cleaner/os/disk_util.h"
#include "chrome/chrome_cleaner/os/file_path_set.h"
#include "chrome/chrome_cleaner/pup_data/pup_cleaner_util.h"
#include "chrome/chrome_cleaner/test/test_file_util.h"
#include "chrome/chrome_cleaner/test/test_pup_data.h"
#include "chrome/chrome_cleaner/test/test_registry_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_cleaner {

namespace {

const wchar_t kFileName1[] = L"Filename one";
const wchar_t kFileName2[] = L"Filename two";
const wchar_t kFileName3[] = L"Third filename";
const wchar_t kSubFolder[] = L"folder";
const char kFileContent1[] = "This is the file content.";
const char kFileContent2[] = "Hi!";
const char kFileContent3[] = "Hello World!";

}  // namespace

class PUPDiskUtilTests : public testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    ASSERT_TRUE(base::CreateTemporaryDirInDir(
        temp_dir_.GetPath(), kSubFolder, &subfolder_path_));
  }

  base::FilePath CreateFileInTopDir(const base::FilePath& basename,
                                    const char* content,
                                    size_t content_length) {
    base::FilePath file_path = temp_dir_.GetPath().Append(basename);
    CreateFileWithContent(file_path, content, content_length);
    return file_path;
  }

  base::FilePath CreateFileInSubfolder(const base::FilePath& basename,
                                       const char* content,
                                       size_t content_length) {
    base::FilePath file_path = subfolder_path_.Append(basename);
    CreateFileWithContent(file_path, content, content_length);
    return file_path;
  }

 protected:
  base::ScopedTempDir temp_dir_;
  base::FilePath subfolder_path_;
};

TEST_F(PUPDiskUtilTests, CollectPathsRecursively) {
  base::FilePath file_path1 = CreateFileInTopDir(
      base::FilePath(kFileName1), kFileContent1, sizeof(kFileContent1));
  base::FilePath file_path2 = CreateFileInTopDir(
      base::FilePath(kFileName2), kFileContent2, sizeof(kFileContent2));
  base::FilePath file_path3 = CreateFileInSubfolder(
      base::FilePath(kFileName3), kFileContent3, sizeof(kFileContent3));

  SimpleTestPUP pup;
  CollectPathsRecursively(temp_dir_.GetPath(), &pup);

  // Adding the same path twice, should not add it again.
  CollectPathsRecursively(temp_dir_.GetPath(), &pup);
  FilePathSet expected_file_paths;
  expected_file_paths.Insert(temp_dir_.GetPath());
  expected_file_paths.Insert(file_path1);
  expected_file_paths.Insert(file_path2);
  expected_file_paths.Insert(subfolder_path_);
  expected_file_paths.Insert(file_path3);

  EXPECT_EQ(expected_file_paths, pup.expanded_disk_footprints);
}

TEST_F(PUPDiskUtilTests, CollectPathsRecursivelyWithLimits) {
  base::FilePath file_path1 = CreateFileInTopDir(
      base::FilePath(kFileName1), kFileContent1, sizeof(kFileContent1));
  base::FilePath file_path2 = CreateFileInTopDir(
      base::FilePath(kFileName2), kFileContent2, sizeof(kFileContent2));
  base::FilePath file_path3 = CreateFileInSubfolder(
      base::FilePath(kFileName3), kFileContent3, sizeof(kFileContent3));

  static const bool kAllowFolders = true;
  static const bool kForbidFolders = false;
  static const size_t kSmallFileSizes = 5;
  static const size_t kLargeFileSizes = 50000;
  // Collect only 1 file: must fail because there is more than 1 file.
  SimpleTestPUP pup;
  EXPECT_FALSE(CollectPathsRecursivelyWithLimits(
      temp_dir_.GetPath(), 1, kLargeFileSizes, kAllowFolders, &pup));
  EXPECT_TRUE(pup.expanded_disk_footprints.empty());

  // Collect files with a small file limit: must fail.
  pup.ClearDiskFootprints();
  EXPECT_FALSE(CollectPathsRecursivelyWithLimits(
      temp_dir_.GetPath(), 5, kSmallFileSizes, kAllowFolders, &pup));
  EXPECT_TRUE(pup.expanded_disk_footprints.empty());

  // Collect a folder: must fail with |kForbidFolders|.
  pup.ClearDiskFootprints();
  EXPECT_FALSE(CollectPathsRecursivelyWithLimits(
      temp_dir_.GetPath(), 5, kLargeFileSizes, kForbidFolders, &pup));
  EXPECT_TRUE(pup.expanded_disk_footprints.empty());

  // Collect 5 files: must pass.
  pup.ClearDiskFootprints();
  EXPECT_TRUE(CollectPathsRecursivelyWithLimits(
      temp_dir_.GetPath(), 5, kLargeFileSizes, kAllowFolders, &pup));
  FilePathSet expected_file_paths;
  expected_file_paths.Insert(temp_dir_.GetPath());
  expected_file_paths.Insert(file_path1);
  expected_file_paths.Insert(file_path2);
  expected_file_paths.Insert(file_path3);
  expected_file_paths.Insert(subfolder_path_);
  EXPECT_EQ(expected_file_paths, pup.expanded_disk_footprints);
}

}  // namespace chrome_cleaner
