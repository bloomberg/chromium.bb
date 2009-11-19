// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>

#include "base/file_util.h"
#include "base/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/zip.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace {

// Make the test a PlatformTest to setup autorelease pools properly on Mac.
class ZipTest : public PlatformTest {
 protected:
  virtual void SetUp() {
    PlatformTest::SetUp();

    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    test_dir_ = temp_dir_.path();

    FilePath zip_path(test_dir_);
    zip_contents_.insert(zip_path.AppendASCII("foo.txt"));
    zip_path = zip_path.AppendASCII("foo");
    zip_contents_.insert(zip_path);
    zip_contents_.insert(zip_path.AppendASCII("bar.txt"));
    zip_path = zip_path.AppendASCII("bar");
    zip_contents_.insert(zip_path);
    zip_contents_.insert(zip_path.AppendASCII("baz.txt"));
    zip_contents_.insert(zip_path.AppendASCII("quux.txt"));
    zip_contents_.insert(zip_path.AppendASCII(".hidden"));
  }

  virtual void TearDown() {
    PlatformTest::TearDown();
  }

  void TestUnzipFile(const FilePath::StringType& filename,
                     bool expect_hidden_files, bool need_success) {
    FilePath test_dir;
    ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &test_dir));
    test_dir = test_dir.AppendASCII("zip");
    TestUnzipFile(test_dir.Append(filename), expect_hidden_files,
                  need_success);
  }

  void TestUnzipFile(const FilePath& path, bool expect_hidden_files,
                     bool need_success) {
    ASSERT_TRUE(file_util::PathExists(path)) << "no file " << path.value();
    if (need_success) {
      ASSERT_TRUE(Unzip(path, test_dir_));
    } else {
      ASSERT_FALSE(Unzip(path, test_dir_));
      return;
    }

    file_util::FileEnumerator files(test_dir_, true,
        static_cast<file_util::FileEnumerator::FILE_TYPE>(
            file_util::FileEnumerator::FILES |
            file_util::FileEnumerator::DIRECTORIES));
    FilePath next_path = files.Next();
    size_t count = 0;
    while (!next_path.value().empty()) {
      if (next_path.value().find(FILE_PATH_LITERAL(".svn")) ==
          FilePath::StringType::npos) {
        EXPECT_EQ(zip_contents_.count(next_path), 1U) <<
            "Couldn't find " << next_path.value();
        count++;
      }
      next_path = files.Next();
    }

    size_t expected_count = 0;
    for (std::set<FilePath>::iterator iter = zip_contents_.begin();
         iter != zip_contents_.end(); ++iter) {
      if (expect_hidden_files || iter->BaseName().ToWStringHack()[0] != L'.')
        ++expected_count;
    }

    EXPECT_EQ(expected_count, count);
  }

  // the path to temporary directory used to contain the test operations
  FilePath test_dir_;

  ScopedTempDir temp_dir_;

  // hard-coded contents of a known zip file
  std::set<FilePath> zip_contents_;
};

TEST_F(ZipTest, Unzip) {
  TestUnzipFile(FILE_PATH_LITERAL("test.zip"), true, true);
}

TEST_F(ZipTest, UnzipUncompressed) {
  TestUnzipFile(FILE_PATH_LITERAL("test_nocompress.zip"), true, true);
}

TEST_F(ZipTest, UnzipEvil) {
  TestUnzipFile(FILE_PATH_LITERAL("evil.zip"), true, false);
  FilePath evil_file = test_dir_;
  evil_file = evil_file.AppendASCII(
      "../levilevilevilevilevilevilevilevilevilevilevilevil");
  ASSERT_FALSE(file_util::PathExists(evil_file));
}

TEST_F(ZipTest, UnzipEvil2) {
  ScopedTempDir dest_dir;
  ASSERT_TRUE(dest_dir.CreateUniqueTempDir());

  FilePath test_dir;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &test_dir));
  test_dir = test_dir.AppendASCII("zip");
  TestUnzipFile(FILE_PATH_LITERAL("evil_via_invalid_utf8.zip"), true, false);

  FilePath evil_file = dest_dir.path();
  evil_file = evil_file.AppendASCII("../evil.txt");
  ASSERT_FALSE(file_util::PathExists(evil_file));
}

TEST_F(ZipTest, Zip) {
  FilePath src_dir;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &src_dir));
  src_dir = src_dir.AppendASCII("zip").AppendASCII("test");

  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  FilePath zip_file = temp_dir.path().AppendASCII("out.zip");

  EXPECT_TRUE(Zip(src_dir, zip_file, true));
  TestUnzipFile(zip_file, true, true);
}

TEST_F(ZipTest, ZipIgnoreHidden) {
  FilePath src_dir;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &src_dir));
  src_dir = src_dir.AppendASCII("zip").AppendASCII("test");

  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  FilePath zip_file = temp_dir.path().AppendASCII("out.zip");

  EXPECT_TRUE(Zip(src_dir, zip_file, false));
  TestUnzipFile(zip_file, false, true);
}

}  // namespace
