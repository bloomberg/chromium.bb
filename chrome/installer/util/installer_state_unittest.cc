// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include <fstream>

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/version.h"
#include "base/win/registry.h"
#include "base/win/scoped_handle.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/helper.h"
#include "chrome/installer/util/installation_state.h"
#include "chrome/installer/util/installer_state.h"
#include "chrome/installer/util/master_preferences.h"
#include "chrome/installer/util/product_unittest.h"
#include "chrome/installer/util/work_item.h"
#include "testing/gtest/include/gtest/gtest.h"

class InstallerStateTest : public TestWithTempDirAndDeleteTempOverrideKeys {
 protected:
};

// An installer state on which we can tweak the target path.
class MockInstallerState : public installer::InstallerState {
 public:
  MockInstallerState() : InstallerState() { }
  void set_target_path(const FilePath& target_path) {
    target_path_ = target_path;
  }
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

void BuildSingleChromeState(const FilePath& target_dir,
                            MockInstallerState* installer_state) {
  CommandLine cmd_line = CommandLine::FromString(L"setup.exe");
  installer::MasterPreferences prefs(cmd_line);
  installer::InstallationState machine_state;
  machine_state.Initialize();
  installer_state->Initialize(cmd_line, prefs, machine_state);
  installer_state->set_target_path(target_dir);
  EXPECT_TRUE(installer_state->FindProduct(BrowserDistribution::CHROME_BROWSER)
      != NULL);
  EXPECT_TRUE(installer_state->FindProduct(BrowserDistribution::CHROME_FRAME)
      == NULL);
}

wchar_t text_content_1[] = L"delete me";
wchar_t text_content_2[] = L"delete me as well";

// Delete version directories. Everything lower than the given version
// should be deleted.
TEST_F(InstallerStateTest, Delete) {
  // TODO(grt): move common stuff into the test fixture.
  // Create a Chrome dir
  FilePath chrome_dir(test_dir_.path());
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

  MockInstallerState installer_state;
  BuildSingleChromeState(chrome_dir, &installer_state);
  scoped_ptr<Version> latest_version(Version::GetVersionFromString("1.0.4.0"));
  installer_state.RemoveOldVersionDirectories(*latest_version.get());

  // old versions should be gone
  EXPECT_FALSE(file_util::PathExists(chrome_dir_1));
  EXPECT_FALSE(file_util::PathExists(chrome_dir_2));
  EXPECT_FALSE(file_util::PathExists(chrome_dir_3));
  // the latest version should stay
  EXPECT_TRUE(file_util::PathExists(chrome_dll_4));
}

// Delete older version directories, keeping the one in used intact.
TEST_F(InstallerStateTest, DeleteInUsed) {
  // Create a Chrome dir
  FilePath chrome_dir(test_dir_.path());
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

  MockInstallerState installer_state;
  BuildSingleChromeState(chrome_dir, &installer_state);
  scoped_ptr<Version> latest_version(Version::GetVersionFromString("1.0.4.0"));
  installer_state.RemoveOldVersionDirectories(*latest_version.get());

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

// Tests a few basic things of the Package class.  Makes sure that the path
// operations are correct
TEST_F(InstallerStateTest, Basic) {
  const bool multi_install = false;
  const bool system_level = true;
  CommandLine cmd_line = CommandLine::FromString(
      std::wstring(L"setup.exe") +
      (multi_install ? L" --multi-install --chrome" : L"") +
      (system_level ? L" --system-level" : L""));
  installer::MasterPreferences prefs(cmd_line);
  installer::InstallationState machine_state;
  machine_state.Initialize();
  MockInstallerState installer_state;
  installer_state.Initialize(cmd_line, prefs, machine_state);
  installer_state.set_target_path(test_dir_.path());
  EXPECT_EQ(test_dir_.path().value(), installer_state.target_path().value());
  EXPECT_EQ(1U, installer_state.products().size());

  const char kOldVersion[] = "1.2.3.4";
  const char kNewVersion[] = "2.3.4.5";

  scoped_ptr<Version> new_version(Version::GetVersionFromString(kNewVersion));
  scoped_ptr<Version> old_version(Version::GetVersionFromString(kOldVersion));
  ASSERT_TRUE(new_version.get() != NULL);
  ASSERT_TRUE(old_version.get() != NULL);

  FilePath installer_dir(
      installer_state.GetInstallerDirectory(*new_version.get()));
  EXPECT_FALSE(installer_dir.empty());

  FilePath new_version_dir(installer_state.target_path().Append(
      UTF8ToWide(new_version->GetString())));
  FilePath old_version_dir(installer_state.target_path().Append(
      UTF8ToWide(old_version->GetString())));

  EXPECT_FALSE(file_util::PathExists(new_version_dir));
  EXPECT_FALSE(file_util::PathExists(old_version_dir));

  EXPECT_FALSE(file_util::PathExists(installer_dir));
  file_util::CreateDirectory(installer_dir);
  EXPECT_TRUE(file_util::PathExists(new_version_dir));

  file_util::CreateDirectory(old_version_dir);
  EXPECT_TRUE(file_util::PathExists(old_version_dir));

  // Create a fake chrome.dll key file in the old version directory.  This
  // should prevent the old version directory from getting deleted.
  FilePath old_chrome_dll(old_version_dir.Append(installer::kChromeDll));
  EXPECT_FALSE(file_util::PathExists(old_chrome_dll));

  // Hold on to the file exclusively to prevent the directory from
  // being deleted.
  base::win::ScopedHandle file(
    ::CreateFile(old_chrome_dll.value().c_str(), GENERIC_READ,
                 0, NULL, OPEN_ALWAYS, 0, NULL));
  EXPECT_TRUE(file.IsValid());
  EXPECT_TRUE(file_util::PathExists(old_chrome_dll));

  installer_state.RemoveOldVersionDirectories(*new_version.get());
  // The old directory should still exist.
  EXPECT_TRUE(file_util::PathExists(old_version_dir));
  EXPECT_TRUE(file_util::PathExists(new_version_dir));

  // Now close the file handle to make it possible to delete our key file.
  file.Close();

  installer_state.RemoveOldVersionDirectories(*new_version.get());
  // The new directory should still exist.
  EXPECT_TRUE(file_util::PathExists(new_version_dir));

  // Now, the old directory and key file should be gone.
  EXPECT_FALSE(file_util::PathExists(old_chrome_dll));
  EXPECT_FALSE(file_util::PathExists(old_version_dir));
}

TEST_F(InstallerStateTest, WithProduct) {
  const bool multi_install = false;
  const bool system_level = true;
  CommandLine cmd_line = CommandLine::FromString(
      std::wstring(L"setup.exe") +
      (multi_install ? L" --multi-install --chrome" : L"") +
      (system_level ? L" --system-level" : L""));
  installer::MasterPreferences prefs(cmd_line);
  installer::InstallationState machine_state;
  machine_state.Initialize();
  MockInstallerState installer_state;
  installer_state.Initialize(cmd_line, prefs, machine_state);
  installer_state.set_target_path(test_dir_.path());
  EXPECT_EQ(1U, installer_state.products().size());
  EXPECT_EQ(system_level, installer_state.system_install());

  const char kCurrentVersion[] = "1.2.3.4";
  scoped_ptr<Version> current_version(
      Version::GetVersionFromString(kCurrentVersion));

  HKEY root = system_level ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
  EXPECT_EQ(root, installer_state.root_key());
  {
    TempRegKeyOverride override(root, L"root_pit");
    BrowserDistribution* dist = BrowserDistribution::GetSpecificDistribution(
        BrowserDistribution::CHROME_BROWSER);
    base::win::RegKey chrome_key(root, dist->GetVersionKey().c_str(),
                                 KEY_ALL_ACCESS);
    EXPECT_TRUE(chrome_key.Valid());
    if (chrome_key.Valid()) {
      chrome_key.WriteValue(google_update::kRegVersionField,
                            UTF8ToWide(current_version->GetString()).c_str());
      machine_state.Initialize();
      // TODO(tommi): Also test for when there exists a new_chrome.exe.
      scoped_ptr<Version> found_version(installer_state.GetCurrentVersion(
          machine_state));
      EXPECT_TRUE(found_version.get() != NULL);
      if (found_version.get()) {
        EXPECT_TRUE(current_version->Equals(*found_version));
      }
    }
  }
}
