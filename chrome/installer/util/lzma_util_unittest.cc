// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include "base/file_util.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/installer/util/lzma_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
class LzmaUtilTest : public testing::Test {
 protected:
  virtual void SetUp() {
    ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &data_dir_));
    data_dir_ = data_dir_.AppendASCII("installer");
    ASSERT_TRUE(file_util::PathExists(data_dir_));

    // Name a subdirectory of the user temp directory.
    ASSERT_TRUE(PathService::Get(base::DIR_TEMP, &test_dir_));
    test_dir_ = test_dir_.AppendASCII("LzmaUtilTest");

    // Create a fresh, empty copy of this test directory.
    file_util::Delete(test_dir_, true);
    file_util::CreateDirectoryW(test_dir_);
    ASSERT_TRUE(file_util::PathExists(test_dir_));
  }

  virtual void TearDown() {
    // Clean up test directory
    ASSERT_TRUE(file_util::Delete(test_dir_, false));
    ASSERT_FALSE(file_util::PathExists(test_dir_));
  }

  // The path to temporary directory used to contain the test operations.
  FilePath test_dir_;

  // The path to input data used in tests.
  FilePath data_dir_;
};
};

// Test that we can open archives successfully.
TEST_F(LzmaUtilTest, OpenArchiveTest) {
  FilePath archive = data_dir_.AppendASCII("archive1.7z");
  LzmaUtil lzma_util;
  EXPECT_EQ(lzma_util.OpenArchive(archive.value()), NO_ERROR);

  // We allow opening another archive (which will automatically close the first
  // archive).
  archive = data_dir_.AppendASCII("archive2.7z");
  EXPECT_EQ(lzma_util.OpenArchive(archive.value()), NO_ERROR);

  // Explicitly close and open the first archive again.
  lzma_util.CloseArchive();
  archive = data_dir_.AppendASCII("archive1.7z");
  EXPECT_EQ(lzma_util.OpenArchive(archive.value()), NO_ERROR);

  // Make sure non-existent archive returns error.
  archive = data_dir_.AppendASCII("archive.non_existent.7z");
  EXPECT_EQ(lzma_util.OpenArchive(archive.value()), ERROR_FILE_NOT_FOUND);
}

// Test that we can extract archives successfully.
TEST_F(LzmaUtilTest, UnPackTest) {
  FilePath extract_dir(test_dir_);
  extract_dir = extract_dir.AppendASCII("UnPackTest");
  ASSERT_FALSE(file_util::PathExists(extract_dir));
  EXPECT_TRUE(file_util::CreateDirectory(extract_dir));
  ASSERT_TRUE(file_util::PathExists(extract_dir));

  FilePath archive = data_dir_.AppendASCII("archive1.7z");
  LzmaUtil lzma_util;
  EXPECT_EQ(lzma_util.OpenArchive(archive.value()), NO_ERROR);
  std::wstring unpacked_file;
  EXPECT_EQ(lzma_util.UnPack(extract_dir.value(), &unpacked_file),
            NO_ERROR);
  EXPECT_TRUE(file_util::PathExists(extract_dir.AppendASCII("a.exe")));
  EXPECT_TRUE(unpacked_file == extract_dir.AppendASCII("a.exe").value());

  archive = data_dir_.AppendASCII("archive2.7z");
  EXPECT_EQ(lzma_util.OpenArchive(archive.value()), NO_ERROR);
  EXPECT_EQ(lzma_util.UnPack(extract_dir.value(), &unpacked_file),
            NO_ERROR);
  EXPECT_TRUE(file_util::PathExists(extract_dir.AppendASCII("b.exe")));
  EXPECT_TRUE(unpacked_file == extract_dir.AppendASCII("b.exe").value());

  lzma_util.CloseArchive();
  archive = data_dir_.AppendASCII("invalid_archive.7z");
  EXPECT_EQ(lzma_util.UnPack(extract_dir.value(), &unpacked_file),
            ERROR_INVALID_HANDLE);
  EXPECT_EQ(lzma_util.OpenArchive(archive.value()), NO_ERROR);
  EXPECT_EQ(lzma_util.UnPack(extract_dir.value(), &unpacked_file),
            ERROR_INVALID_HANDLE);

  archive = data_dir_.AppendASCII("archive3.7z");
  EXPECT_EQ(lzma_util.OpenArchive(archive.value()), NO_ERROR);
  EXPECT_EQ(lzma_util.UnPack(extract_dir.value(), &unpacked_file),
            NO_ERROR);
  EXPECT_TRUE(file_util::PathExists(extract_dir.AppendASCII("archive\\a.exe")));
  EXPECT_TRUE(file_util::PathExists(
      extract_dir.AppendASCII("archive\\sub_dir\\text.txt")));
}

// Test the static method that can be used to unpack archives.
TEST_F(LzmaUtilTest, UnPackArchiveTest) {
  FilePath extract_dir(test_dir_);
  extract_dir = extract_dir.AppendASCII("UnPackArchiveTest");
  ASSERT_FALSE(file_util::PathExists(extract_dir));
  EXPECT_TRUE(file_util::CreateDirectory(extract_dir));
  ASSERT_TRUE(file_util::PathExists(extract_dir));

  FilePath archive = data_dir_.AppendASCII("archive1.7z");
  std::wstring unpacked_file;
  EXPECT_EQ(LzmaUtil::UnPackArchive(archive.value(), extract_dir.value(),
                                    &unpacked_file), NO_ERROR);
  EXPECT_TRUE(file_util::PathExists(extract_dir.AppendASCII("a.exe")));
  EXPECT_TRUE(unpacked_file == extract_dir.AppendASCII("a.exe").value());

  archive = data_dir_.AppendASCII("archive2.7z");
  EXPECT_EQ(LzmaUtil::UnPackArchive(archive.value(), extract_dir.value(),
                                    &unpacked_file), NO_ERROR);
  EXPECT_TRUE(file_util::PathExists(extract_dir.AppendASCII("b.exe")));
  EXPECT_TRUE(unpacked_file == extract_dir.AppendASCII("b.exe").value());

  archive = data_dir_.AppendASCII("invalid_archive.7z");
  EXPECT_NE(LzmaUtil::UnPackArchive(archive.value(), extract_dir.value(),
                                    &unpacked_file), NO_ERROR);
}
