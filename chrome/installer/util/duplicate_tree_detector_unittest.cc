// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include <fstream>

#include "base/file_util.h"
#include "base/memory/scoped_temp_dir.h"
#include "base/string_util.h"
#include "chrome/installer/util/duplicate_tree_detector.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class DuplicateTreeDetectorTest : public testing::Test {
 protected:
  virtual void SetUp() {
    ASSERT_TRUE(temp_source_dir_.CreateUniqueTempDir());
    ASSERT_TRUE(temp_dest_dir_.CreateUniqueTempDir());
  }

  // Simple function to dump some text into a new file.
  void CreateTextFile(const std::string& filename,
                      const std::wstring& contents) {
    std::wofstream file;
    file.open(filename.c_str());
    ASSERT_TRUE(file.is_open());
    file << contents;
    file.close();
  }

  // Simple function to read text from a file.
  std::wstring ReadTextFile(const FilePath& path) {
    WCHAR contents[64];
    std::wifstream file;
    file.open(WideToASCII(path.value()).c_str());
    EXPECT_TRUE(file.is_open());
    file.getline(contents, arraysize(contents));
    file.close();
    return std::wstring(contents);
  }

  // Creates a two level deep source dir with a file in each.
  void CreateFileHierarchy(const FilePath& root) {
    FilePath d1(root);
    d1 = d1.AppendASCII("D1");
    file_util::CreateDirectory(d1);
    ASSERT_TRUE(file_util::PathExists(d1));

    FilePath f1(d1);
    f1 = f1.AppendASCII("F1");
    CreateTextFile(f1.MaybeAsASCII(), text_content_1_);
    ASSERT_TRUE(file_util::PathExists(f1));

    FilePath d2(d1);
    d2 = d2.AppendASCII("D2");
    file_util::CreateDirectory(d2);
    ASSERT_TRUE(file_util::PathExists(d2));

    FilePath f2(d2);
    f2 = f2.AppendASCII("F2");
    CreateTextFile(f2.MaybeAsASCII(), text_content_2_);
    ASSERT_TRUE(file_util::PathExists(f2));
  }

  ScopedTempDir temp_source_dir_;
  ScopedTempDir temp_dest_dir_;

  static const wchar_t text_content_1_[];
  static const wchar_t text_content_2_[];
  static const wchar_t text_content_3_[];
};

const wchar_t DuplicateTreeDetectorTest::text_content_1_[] =
    L"Gooooooooooooooooooooogle";
const wchar_t DuplicateTreeDetectorTest::text_content_2_[] =
    L"Overwrite Me";
const wchar_t DuplicateTreeDetectorTest::text_content_3_[] =
    L"I'd rather see your watermelon and raise you ham and a half.";

};  // namespace

// Test the DuplicateTreeChecker's definition of identity on two identical
// directory structures.
TEST_F(DuplicateTreeDetectorTest, TestIdenticalDirs) {
  // Create two sets of identical file hierarchys:
  CreateFileHierarchy(temp_source_dir_.path());
  CreateFileHierarchy(temp_dest_dir_.path());

  EXPECT_TRUE(installer::IsIdenticalFileHierarchy(temp_source_dir_.path(),
                                                  temp_dest_dir_.path()));
}

// Test when source entirely contains dest but contains other files as well.
// IsIdenticalTo should return false in this case.
TEST_F(DuplicateTreeDetectorTest, TestSourceContainsDest) {
  // Create two sets of identical file hierarchys:
  CreateFileHierarchy(temp_source_dir_.path());
  CreateFileHierarchy(temp_dest_dir_.path());

  FilePath new_file(temp_source_dir_.path());
  new_file = new_file.AppendASCII("FNew");
  CreateTextFile(new_file.MaybeAsASCII(), text_content_1_);
  ASSERT_TRUE(file_util::PathExists(new_file));

  EXPECT_FALSE(installer::IsIdenticalFileHierarchy(temp_source_dir_.path(),
                                                  temp_dest_dir_.path()));
}

// Test when dest entirely contains source but contains other files as well.
// IsIdenticalTo should return true in this case.
TEST_F(DuplicateTreeDetectorTest, TestDestContainsSource) {
  // Create two sets of identical file hierarchys:
  CreateFileHierarchy(temp_source_dir_.path());
  CreateFileHierarchy(temp_dest_dir_.path());

  FilePath new_file(temp_dest_dir_.path());
  new_file = new_file.AppendASCII("FNew");
  CreateTextFile(new_file.MaybeAsASCII(), text_content_1_);
  ASSERT_TRUE(file_util::PathExists(new_file));

  EXPECT_TRUE(installer::IsIdenticalFileHierarchy(temp_source_dir_.path(),
                                                  temp_dest_dir_.path()));
}

// Test when the file hierarchies are the same but one of the files is changed.
TEST_F(DuplicateTreeDetectorTest, TestIdenticalDirsDifferentFiles) {
  // Create two sets of identical file hierarchys:
  CreateFileHierarchy(temp_source_dir_.path());
  CreateFileHierarchy(temp_dest_dir_.path());

  FilePath existing_file(temp_dest_dir_.path());
  existing_file = existing_file.AppendASCII("D1")
                               .AppendASCII("D2")
                               .AppendASCII("F2");
  CreateTextFile(existing_file.MaybeAsASCII(), text_content_3_);

  EXPECT_FALSE(installer::IsIdenticalFileHierarchy(temp_source_dir_.path(),
                                                   temp_dest_dir_.path()));
}

// Test when both file hierarchies are empty.
TEST_F(DuplicateTreeDetectorTest, TestEmptyDirs) {
  EXPECT_TRUE(installer::IsIdenticalFileHierarchy(temp_source_dir_.path(),
                                                  temp_dest_dir_.path()));
}

// Test on single files.
TEST_F(DuplicateTreeDetectorTest, TestSingleFiles) {
  // Create a source file.
  FilePath source_file(temp_source_dir_.path());
  source_file = source_file.AppendASCII("F1");
  CreateTextFile(source_file.MaybeAsASCII(), text_content_1_);

  // This file should be the same.
  FilePath dest_file(temp_dest_dir_.path());
  dest_file = dest_file.AppendASCII("F1");
  CreateTextFile(dest_file.MaybeAsASCII(), text_content_1_);

  // This file should be different.
  FilePath other_file(temp_dest_dir_.path());
  other_file = other_file.AppendASCII("F2");
  CreateTextFile(other_file.MaybeAsASCII(), text_content_2_);

  EXPECT_TRUE(installer::IsIdenticalFileHierarchy(source_file, dest_file));
  EXPECT_FALSE(installer::IsIdenticalFileHierarchy(source_file, other_file));
}

