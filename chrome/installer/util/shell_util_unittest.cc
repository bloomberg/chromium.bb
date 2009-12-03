// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>

#include <fstream>
#include <iostream>

#include "base/file_util.h"
#include "base/path_service.h"
#include "base/scoped_comptr_win.h"
#include "chrome/installer/util/master_preferences.h"
#include "chrome/installer/util/shell_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
bool VerifyChromeShortcut(const std::wstring& exe_path,
                          const std::wstring& shortcut,
                          const std::wstring& description,
                          int icon_index) {
  ScopedComPtr<IShellLink> i_shell_link;
  ScopedComPtr<IPersistFile> i_persist_file;

  // Get pointer to the IShellLink interface
  bool failed = FAILED(i_shell_link.CreateInstance(CLSID_ShellLink, NULL,
                                                   CLSCTX_INPROC_SERVER));
  EXPECT_FALSE(failed) << "Failed to get IShellLink";
  if (failed)
    return false;

  // Query IShellLink for the IPersistFile interface
  failed = FAILED(i_persist_file.QueryFrom(i_shell_link));
  EXPECT_FALSE(failed) << "Failed to get IPersistFile";
  if (failed)
    return false;

  failed = FAILED(i_persist_file->Load(shortcut.c_str(), 0));
  EXPECT_FALSE(failed) << "Failed to load shortcut " << shortcut.c_str();
  if (failed)
    return false;

  wchar_t long_path[MAX_PATH] = {0};
  wchar_t short_path[MAX_PATH] = {0};
  failed = ((::GetLongPathName(exe_path.c_str(), long_path, MAX_PATH) == 0) ||
            (::GetShortPathName(exe_path.c_str(), short_path, MAX_PATH) == 0));
  EXPECT_FALSE(failed) << "Failed to get long and short path names for "
                       << exe_path;
  if (failed)
    return false;

  wchar_t file_path[MAX_PATH] = {0};
  failed = ((FAILED(i_shell_link->GetPath(file_path, MAX_PATH, NULL,
                                          SLGP_UNCPRIORITY))) ||
            ((FilePath(file_path) != FilePath(long_path)) &&
             (FilePath(file_path) != FilePath(short_path))));
  EXPECT_FALSE(failed) << "File path " << file_path << " did not match with "
                       << exe_path;
  if (failed)
    return false;

  wchar_t desc[MAX_PATH] = {0};
  failed = ((FAILED(i_shell_link->GetDescription(desc, MAX_PATH))) ||
            (std::wstring(desc) != std::wstring(description)));
  EXPECT_FALSE(failed) << "Description " << desc << " did not match with "
                       << description;
  if (failed)
    return false;

  wchar_t icon_path[MAX_PATH] = {0};
  int index = 0;
  failed = ((FAILED(i_shell_link->GetIconLocation(icon_path, MAX_PATH,
                                                  &index))) ||
            ((FilePath(file_path) != FilePath(long_path)) &&
             (FilePath(file_path) != FilePath(short_path))) ||
            (index != icon_index));
  EXPECT_FALSE(failed);
  if (failed)
    return false;

  return true;
}

class ShellUtilTest : public testing::Test {
 protected:
  virtual void SetUp() {
    // Name a subdirectory of the user temp directory.
    ASSERT_TRUE(PathService::Get(base::DIR_TEMP, &test_dir_));
    test_dir_ = test_dir_.AppendASCII("ShellUtilTest");

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
};
};

// Test that we can open archives successfully.
TEST_F(ShellUtilTest, UpdateChromeShortcutTest) {
  // Create an executable in test path by copying ourself to it.
  wchar_t exe_full_path_str[MAX_PATH];
  EXPECT_FALSE(::GetModuleFileName(NULL, exe_full_path_str, MAX_PATH) == 0);
  FilePath exe_full_path(exe_full_path_str);

  FilePath exe_path = test_dir_.AppendASCII("setup.exe");
  EXPECT_TRUE(file_util::CopyFile(exe_full_path, exe_path));

  FilePath shortcut_path = test_dir_.AppendASCII("shortcut.lnk");
  const std::wstring description(L"dummy description");
  EXPECT_TRUE(ShellUtil::UpdateChromeShortcut(exe_path.value(),
                                              shortcut_path.value(),
                                              description, true));
  EXPECT_TRUE(VerifyChromeShortcut(exe_path.value(),
                                   shortcut_path.value(),
                                   description, 0));

  // Now specify an icon index in master prefs and make sure it works.
  FilePath prefs_path = test_dir_.Append(installer_util::kDefaultMasterPrefs);
  std::ofstream file;
  file.open(prefs_path.value().c_str());
  ASSERT_TRUE(file.is_open());
  file <<
"{"
" \"distribution\":{"
"   \"chrome_shortcut_icon_index\" : 1"
" }"
"}";
  file.close();
  ASSERT_TRUE(file_util::Delete(shortcut_path, false));
  EXPECT_TRUE(ShellUtil::UpdateChromeShortcut(exe_path.value(),
                                              shortcut_path.value(),
                                              description, true));
  EXPECT_TRUE(VerifyChromeShortcut(exe_path.value(),
                                   shortcut_path.value(),
                                   description, 1));

  // Now change only description to update shortcut and make sure icon index
  // doesn't change.
  const std::wstring description2(L"dummy description 2");
  EXPECT_TRUE(ShellUtil::UpdateChromeShortcut(exe_path.value(),
                                              shortcut_path.value(),
                                              description2, false));
  EXPECT_TRUE(VerifyChromeShortcut(exe_path.value(),
                                   shortcut_path.value(),
                                   description2, 1));
}
