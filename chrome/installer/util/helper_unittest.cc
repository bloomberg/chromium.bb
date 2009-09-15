// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include <fstream>
#include <iostream>

#include "base/base_paths.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/string_util.h"
#include "chrome/installer/util/helper.h"
#include "chrome/installer/util/work_item.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
  class SetupHelperTest : public testing::Test {
   protected:
    virtual void SetUp() {
      // Name a subdirectory of the user temp directory.
      ASSERT_TRUE(PathService::Get(base::DIR_TEMP, &test_dir_));
      test_dir_ = test_dir_.AppendASCII("SetupHelperTest");

      // Create a fresh, empty copy of this test directory.
      file_util::Delete(test_dir_, true);
      file_util::CreateDirectoryW(test_dir_);
      ASSERT_TRUE(file_util::PathExists(test_dir_));
    }

    virtual void TearDown() {
      logging::CloseLogFile();
      // Clean up test directory
      ASSERT_TRUE(file_util::Delete(test_dir_, false));
      ASSERT_FALSE(file_util::PathExists(test_dir_));
    }

    // the path to temporary directory used to contain the test operations
    FilePath test_dir_;
  };

  // Simple function to dump some text into a new file.
  void CreateTextFile(const std::wstring& filename,
                      const std::wstring& contents) {
    std::ofstream file;
    file.open(filename.c_str());
    ASSERT_TRUE(file.is_open());
    file << contents;
    file.close();
  }

  wchar_t text_content_1[] = L"delete me";
  wchar_t text_content_2[] = L"delete me as well";
};

// Delete version directories. Everything lower than the given version
// should be deleted.
TEST_F(SetupHelperTest, Delete) {
  // Create a Chrome dir
  FilePath chrome_dir(test_dir_);
  chrome_dir = chrome_dir.AppendASCII("chrome");
  file_util::CreateDirectory(chrome_dir);
  ASSERT_TRUE(file_util::PathExists(chrome_dir));

  FilePath chrome_dir_1(chrome_dir);
  chrome_dir_1 = chrome_dir_1.AppendASCII("1.0.1.0");
  file_util::CreateDirectory(chrome_dir_1);
  ASSERT_TRUE(file_util::PathExists(chrome_dir_1));

  FilePath chrome_dir_2(chrome_dir);
  chrome_dir_2 = chrome_dir_2.AppendASCII("1.0.2.0");
  file_util::CreateDirectory(chrome_dir_2);
  ASSERT_TRUE(file_util::PathExists(chrome_dir_2));

  FilePath chrome_dir_3(chrome_dir);
  chrome_dir_3 = chrome_dir_3.AppendASCII("1.0.3.0");
  file_util::CreateDirectory(chrome_dir_3);
  ASSERT_TRUE(file_util::PathExists(chrome_dir_3));

  FilePath chrome_dir_4(chrome_dir);
  chrome_dir_4 = chrome_dir_4.AppendASCII("1.0.4.0");
  file_util::CreateDirectory(chrome_dir_4);
  ASSERT_TRUE(file_util::PathExists(chrome_dir_4));

  FilePath chrome_dll_1(chrome_dir_1);
  chrome_dll_1 = chrome_dll_1.AppendASCII("chrome.dll");
  CreateTextFile(chrome_dll_1.value(), text_content_1);
  ASSERT_TRUE(file_util::PathExists(chrome_dll_1));

  FilePath chrome_dll_2(chrome_dir_2);
  chrome_dll_2 = chrome_dll_2.AppendASCII("chrome.dll");
  CreateTextFile(chrome_dll_2.value(), text_content_1);
  ASSERT_TRUE(file_util::PathExists(chrome_dll_2));

  FilePath chrome_dll_3(chrome_dir_3);
  chrome_dll_3 = chrome_dll_3.AppendASCII("chrome.dll");
  CreateTextFile(chrome_dll_3.value(), text_content_1);
  ASSERT_TRUE(file_util::PathExists(chrome_dll_3));

  FilePath chrome_dll_4(chrome_dir_4);
  chrome_dll_4 = chrome_dll_4.AppendASCII("chrome.dll");
  CreateTextFile(chrome_dll_4.value(), text_content_1);
  ASSERT_TRUE(file_util::PathExists(chrome_dll_4));

  std::wstring latest_version(L"1.0.4.0");
  installer::RemoveOldVersionDirs(chrome_dir.value(), latest_version);

  // old versions should be gone
  EXPECT_FALSE(file_util::PathExists(chrome_dir_1));
  EXPECT_FALSE(file_util::PathExists(chrome_dir_2));
  EXPECT_FALSE(file_util::PathExists(chrome_dir_3));
  // the latest version should stay
  EXPECT_TRUE(file_util::PathExists(chrome_dll_4));
}

// Delete older version directories, keeping the one in used intact.
TEST_F(SetupHelperTest, DeleteInUsed) {
  // Create a Chrome dir
  FilePath chrome_dir(test_dir_);
  chrome_dir = chrome_dir.AppendASCII("chrome");
  file_util::CreateDirectory(chrome_dir);
  ASSERT_TRUE(file_util::PathExists(chrome_dir));

  FilePath chrome_dir_1(chrome_dir);
  chrome_dir_1 = chrome_dir_1.AppendASCII("1.0.1.0");
  file_util::CreateDirectory(chrome_dir_1);
  ASSERT_TRUE(file_util::PathExists(chrome_dir_1));

  FilePath chrome_dir_2(chrome_dir);
  chrome_dir_2 = chrome_dir_2.AppendASCII("1.0.2.0");
  file_util::CreateDirectory(chrome_dir_2);
  ASSERT_TRUE(file_util::PathExists(chrome_dir_2));

  FilePath chrome_dir_3(chrome_dir);
  chrome_dir_3 = chrome_dir_3.AppendASCII("1.0.3.0");
  file_util::CreateDirectory(chrome_dir_3);
  ASSERT_TRUE(file_util::PathExists(chrome_dir_3));

  FilePath chrome_dir_4(chrome_dir);
  chrome_dir_4 = chrome_dir_4.AppendASCII("1.0.4.0");
  file_util::CreateDirectory(chrome_dir_4);
  ASSERT_TRUE(file_util::PathExists(chrome_dir_4));

  FilePath chrome_dll_1(chrome_dir_1);
  chrome_dll_1 = chrome_dll_1.AppendASCII("chrome.dll");
  CreateTextFile(chrome_dll_1.value(), text_content_1);
  ASSERT_TRUE(file_util::PathExists(chrome_dll_1));

  FilePath chrome_dll_2(chrome_dir_2);
  chrome_dll_2 = chrome_dll_2.AppendASCII("chrome.dll");
  CreateTextFile(chrome_dll_2.value(), text_content_1);
  ASSERT_TRUE(file_util::PathExists(chrome_dll_2));

  // Open the file to make it in use.
  std::ofstream file;
  file.open(chrome_dll_2.value().c_str());

  FilePath chrome_othera_2(chrome_dir_2);
  chrome_othera_2 = chrome_othera_2.AppendASCII("othera.dll");
  CreateTextFile(chrome_othera_2.value(), text_content_2);
  ASSERT_TRUE(file_util::PathExists(chrome_othera_2));

  FilePath chrome_otherb_2(chrome_dir_2);
  chrome_otherb_2 = chrome_otherb_2.AppendASCII("otherb.dll");
  CreateTextFile(chrome_otherb_2.value(), text_content_2);
  ASSERT_TRUE(file_util::PathExists(chrome_otherb_2));

  FilePath chrome_dll_3(chrome_dir_3);
  chrome_dll_3 = chrome_dll_3.AppendASCII("chrome.dll");
  CreateTextFile(chrome_dll_3.value(), text_content_1);
  ASSERT_TRUE(file_util::PathExists(chrome_dll_3));

  FilePath chrome_dll_4(chrome_dir_4);
  chrome_dll_4 = chrome_dll_4.AppendASCII("chrome.dll");
  CreateTextFile(chrome_dll_4.value(), text_content_1);
  ASSERT_TRUE(file_util::PathExists(chrome_dll_4));

  std::wstring latest_version(L"1.0.4.0");
  installer::RemoveOldVersionDirs(chrome_dir.value(), latest_version);

  // old versions not in used should be gone
  EXPECT_FALSE(file_util::PathExists(chrome_dir_1));
  EXPECT_FALSE(file_util::PathExists(chrome_dir_3));
  // every thing under in used version should stay
  EXPECT_TRUE(file_util::PathExists(chrome_dir_2));
  EXPECT_TRUE(file_util::PathExists(chrome_dll_2));
  EXPECT_TRUE(file_util::PathExists(chrome_othera_2));
  EXPECT_TRUE(file_util::PathExists(chrome_otherb_2));
  // the latest version should stay
  EXPECT_TRUE(file_util::PathExists(chrome_dll_4));
}
