// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>

#include <fstream>

#include "base/file_util.h"
#include "base/path_service.h"
#include "base/scoped_temp_dir.h"
#include "base/win/scoped_comptr.h"
#include "base/win/windows_version.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/master_preferences.h"
#include "chrome/installer/util/shell_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
bool VerifyChromeShortcut(const std::wstring& exe_path,
                          const std::wstring& shortcut,
                          const std::wstring& description,
                          int icon_index) {
  base::win::ScopedComPtr<IShellLink> i_shell_link;
  base::win::ScopedComPtr<IPersistFile> i_persist_file;

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
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  }

  ScopedTempDir temp_dir_;
};
};

// Test that we can open archives successfully.
TEST_F(ShellUtilTest, UpdateChromeShortcutTest) {
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  ASSERT_TRUE(dist != NULL);
  // Create an executable in test path by copying ourself to it.
  wchar_t exe_full_path_str[MAX_PATH];
  EXPECT_FALSE(::GetModuleFileName(NULL, exe_full_path_str, MAX_PATH) == 0);
  FilePath exe_full_path(exe_full_path_str);

  FilePath exe_path = temp_dir_.path().AppendASCII("setup.exe");
  EXPECT_TRUE(file_util::CopyFile(exe_full_path, exe_path));

  FilePath shortcut_path = temp_dir_.path().AppendASCII("shortcut.lnk");
  const std::wstring description(L"dummy description");
  EXPECT_TRUE(ShellUtil::UpdateChromeShortcut(dist,
                                              exe_path.value(),
                                              shortcut_path.value(),
                                              L"",
                                              description,
                                              true));
  EXPECT_TRUE(VerifyChromeShortcut(exe_path.value(),
                                   shortcut_path.value(),
                                   description, 0));

  // Now specify an icon index in master prefs and make sure it works.
  FilePath prefs_path = temp_dir_.path().AppendASCII(
      installer::kDefaultMasterPrefs);
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
  EXPECT_TRUE(ShellUtil::UpdateChromeShortcut(dist,
                                              exe_path.value(),
                                              shortcut_path.value(),
                                              L"",
                                              description,
                                              true));
  EXPECT_TRUE(VerifyChromeShortcut(exe_path.value(),
                                   shortcut_path.value(),
                                   description, 1));

  // Now change only description to update shortcut and make sure icon index
  // doesn't change.
  const std::wstring description2(L"dummy description 2");
  EXPECT_TRUE(ShellUtil::UpdateChromeShortcut(dist,
                                              exe_path.value(),
                                              shortcut_path.value(),
                                              L"",
                                              description2,
                                              false));
  EXPECT_TRUE(VerifyChromeShortcut(exe_path.value(),
                                   shortcut_path.value(),
                                   description2, 1));
}

TEST_F(ShellUtilTest, CreateChromeDesktopShortcutTest) {
  // Run this test on Vista+ only if we are running elevated.
  if (base::win::GetVersion() > base::win::VERSION_XP && !IsUserAnAdmin()) {
    LOG(ERROR) << "Must be admin to run this test on Vista+";
    return;
  }

  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  ASSERT_TRUE(dist != NULL);
  // Create an executable in test path by copying ourself to it.
  wchar_t exe_full_path_str[MAX_PATH];
  EXPECT_FALSE(::GetModuleFileName(NULL, exe_full_path_str, MAX_PATH) == 0);
  FilePath exe_full_path(exe_full_path_str);

  FilePath exe_path = temp_dir_.path().AppendASCII("setup.exe");
  EXPECT_TRUE(file_util::CopyFile(exe_full_path, exe_path));

  const std::wstring description(L"dummy description");

  FilePath user_desktop_path;
  EXPECT_TRUE(ShellUtil::GetDesktopPath(false, &user_desktop_path));
  FilePath system_desktop_path;
  EXPECT_TRUE(ShellUtil::GetDesktopPath(true, &system_desktop_path));

  std::wstring shortcut_name;
  EXPECT_TRUE(ShellUtil::GetChromeShortcutName(dist, false, L"",
                                               &shortcut_name));

  std::wstring default_profile_shortcut_name;
  const std::wstring default_profile_user_name = L"Minsk";
  EXPECT_TRUE(ShellUtil::GetChromeShortcutName(dist, false,
                                               default_profile_user_name,
                                               &default_profile_shortcut_name));

  std::wstring second_profile_shortcut_name;
  const std::wstring second_profile_user_name = L"Pinsk";
  EXPECT_TRUE(ShellUtil::GetChromeShortcutName(dist, false,
                                               second_profile_user_name,
                                               &second_profile_shortcut_name));

  FilePath user_shortcut_path = user_desktop_path.Append(shortcut_name);
  FilePath system_shortcut_path = system_desktop_path.Append(shortcut_name);
  FilePath default_profile_shortcut_path = user_desktop_path.Append(
      default_profile_shortcut_name);
  FilePath second_profile_shortcut_path = user_desktop_path.Append(
      second_profile_shortcut_name);

  // Test simple creation of a user-level shortcut.
  EXPECT_TRUE(ShellUtil::CreateChromeDesktopShortcut(dist,
                                                     exe_path.value(),
                                                     description,
                                                     L"",
                                                     L"",
                                                     ShellUtil::CURRENT_USER,
                                                     false,
                                                     true));
  EXPECT_TRUE(VerifyChromeShortcut(exe_path.value(),
                                   user_shortcut_path.value(),
                                   description,
                                   0));
  EXPECT_TRUE(ShellUtil::RemoveChromeDesktopShortcut(dist,
                                                     ShellUtil::CURRENT_USER,
                                                     false));

  // Test simple creation of a system-level shortcut.
  EXPECT_TRUE(ShellUtil::CreateChromeDesktopShortcut(dist,
                                                     exe_path.value(),
                                                     description,
                                                     L"",
                                                     L"",
                                                     ShellUtil::SYSTEM_LEVEL,
                                                     false,
                                                     true));
  EXPECT_TRUE(VerifyChromeShortcut(exe_path.value(),
                                   system_shortcut_path.value(),
                                   description,
                                   0));
  EXPECT_TRUE(ShellUtil::RemoveChromeDesktopShortcut(dist,
                                                     ShellUtil::SYSTEM_LEVEL,
                                                     false));

  // Test creation of a user-level shortcut when a system-level shortcut
  // is already present (should fail).
  EXPECT_TRUE(ShellUtil::CreateChromeDesktopShortcut(dist,
                                                     exe_path.value(),
                                                     description,
                                                     L"",
                                                     L"",
                                                     ShellUtil::SYSTEM_LEVEL,
                                                     false,
                                                     true));
  EXPECT_FALSE(ShellUtil::CreateChromeDesktopShortcut(dist,
                                                      exe_path.value(),
                                                      description,
                                                      L"",
                                                      L"",
                                                      ShellUtil::CURRENT_USER,
                                                      false,
                                                      true));
  EXPECT_TRUE(VerifyChromeShortcut(exe_path.value(),
                                   system_shortcut_path.value(),
                                   description,
                                   0));
  EXPECT_FALSE(file_util::PathExists(user_shortcut_path));
  EXPECT_TRUE(ShellUtil::RemoveChromeDesktopShortcut(dist,
                                                     ShellUtil::SYSTEM_LEVEL,
                                                     false));

  // Test creation of a system-level shortcut when a user-level shortcut
  // is already present (should succeed).
  EXPECT_TRUE(ShellUtil::CreateChromeDesktopShortcut(dist,
                                                     exe_path.value(),
                                                     description,
                                                     L"",
                                                     L"",
                                                     ShellUtil::CURRENT_USER,
                                                     false,
                                                     true));
  EXPECT_TRUE(ShellUtil::CreateChromeDesktopShortcut(dist,
                                                     exe_path.value(),
                                                     description,
                                                     L"",
                                                     L"",
                                                     ShellUtil::SYSTEM_LEVEL,
                                                     false,
                                                     true));
  EXPECT_TRUE(VerifyChromeShortcut(exe_path.value(),
                                   user_shortcut_path.value(),
                                   description,
                                   0));
  EXPECT_TRUE(VerifyChromeShortcut(exe_path.value(),
                                   system_shortcut_path.value(),
                                   description,
                                   0));
  EXPECT_TRUE(ShellUtil::RemoveChromeDesktopShortcut(dist,
                                                     ShellUtil::CURRENT_USER,
                                                     false));
  EXPECT_TRUE(ShellUtil::RemoveChromeDesktopShortcut(dist,
                                                     ShellUtil::SYSTEM_LEVEL,
                                                     false));

  // Test creation of two profile-specific shortcuts (these are always
  // user-level).
  EXPECT_TRUE(ShellUtil::CreateChromeDesktopShortcut(
      dist,
      exe_path.value(),
      description,
      default_profile_user_name,
      L"--profile-directory=\"Default\"",
      ShellUtil::CURRENT_USER,
      false,
      true));
  EXPECT_TRUE(VerifyChromeShortcut(exe_path.value(),
                                   default_profile_shortcut_path.value(),
                                   description,
                                   0));
  EXPECT_TRUE(ShellUtil::CreateChromeDesktopShortcut(
      dist,
      exe_path.value(),
      description,
      second_profile_user_name,
      L"--profile-directory=\"Profile 1\"",
      ShellUtil::CURRENT_USER,
      false,
      true));
  EXPECT_TRUE(VerifyChromeShortcut(exe_path.value(),
                                   second_profile_shortcut_path.value(),
                                   description,
                                   0));
  std::vector<string16> profile_names;
  profile_names.push_back(default_profile_shortcut_name);
  profile_names.push_back(second_profile_shortcut_name);
  EXPECT_TRUE(ShellUtil::RemoveChromeDesktopShortcutsWithAppendedNames(
      profile_names));
}
