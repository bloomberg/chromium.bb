// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/installer_state.h"

#include <windows.h>
#include <stddef.h>

#include <fstream>

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_path_override.h"
#include "base/test/test_reg_util_win.h"
#include "base/version.h"
#include "base/win/registry.h"
#include "base/win/scoped_handle.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/installer/test/alternate_version_generator.h"
#include "chrome/installer/util/fake_installation_state.h"
#include "chrome/installer/util/fake_product_state.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/helper.h"
#include "chrome/installer/util/installation_state.h"
#include "chrome/installer/util/installer_util_strings.h"
#include "chrome/installer/util/master_preferences.h"
#include "chrome/installer/util/util_constants.h"
#include "chrome/installer/util/work_item.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::win::RegKey;
using installer::InstallationState;
using installer::InstallerState;
using installer::MasterPreferences;
using registry_util::RegistryOverrideManager;

class InstallerStateTest : public testing::Test {
 protected:
  InstallerStateTest() {}

  void SetUp() override {
    ASSERT_TRUE(test_dir_.CreateUniqueTempDir());
  }

  base::ScopedTempDir test_dir_;

 private:
  DISALLOW_COPY_AND_ASSIGN(InstallerStateTest);
};

// An installer state on which we can access otherwise protected members.
class MockInstallerState : public InstallerState {
 public:
  MockInstallerState() : InstallerState() { }
  void set_target_path(const base::FilePath& target_path) {
    target_path_ = target_path;
  }
  static bool IsFileInUse(const base::FilePath& file) {
    return InstallerState::IsFileInUse(file);
  }
  const Version& critical_update_version() const {
    return critical_update_version_;
  }
  void GetExistingExeVersions(std::set<std::string>* existing_version_strings) {
    return InstallerState::GetExistingExeVersions(existing_version_strings);
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

void BuildSingleChromeState(const base::FilePath& target_dir,
                            MockInstallerState* installer_state) {
  base::CommandLine cmd_line = base::CommandLine::FromString(L"setup.exe");
  MasterPreferences prefs(cmd_line);
  InstallationState machine_state;
  machine_state.Initialize();
  installer_state->Initialize(cmd_line, prefs, machine_state);
  installer_state->set_target_path(target_dir);
  EXPECT_TRUE(installer_state->FindProduct(BrowserDistribution::CHROME_BROWSER)
      != NULL);
}

wchar_t text_content_1[] = L"delete me";
wchar_t text_content_2[] = L"delete me as well";

// Delete version directories. Everything lower than the given version
// should be deleted.
TEST_F(InstallerStateTest, Delete) {
  // TODO(grt): move common stuff into the test fixture.
  // Create a Chrome dir
  base::FilePath chrome_dir(test_dir_.path());
  chrome_dir = chrome_dir.AppendASCII("chrome");
  base::CreateDirectory(chrome_dir);
  ASSERT_TRUE(base::PathExists(chrome_dir));

  base::FilePath chrome_dir_1(chrome_dir);
  chrome_dir_1 = chrome_dir_1.AppendASCII("1.0.1.0");
  base::CreateDirectory(chrome_dir_1);
  ASSERT_TRUE(base::PathExists(chrome_dir_1));

  base::FilePath chrome_dir_2(chrome_dir);
  chrome_dir_2 = chrome_dir_2.AppendASCII("1.0.2.0");
  base::CreateDirectory(chrome_dir_2);
  ASSERT_TRUE(base::PathExists(chrome_dir_2));

  base::FilePath chrome_dir_3(chrome_dir);
  chrome_dir_3 = chrome_dir_3.AppendASCII("1.0.3.0");
  base::CreateDirectory(chrome_dir_3);
  ASSERT_TRUE(base::PathExists(chrome_dir_3));

  base::FilePath chrome_dir_4(chrome_dir);
  chrome_dir_4 = chrome_dir_4.AppendASCII("1.0.4.0");
  base::CreateDirectory(chrome_dir_4);
  ASSERT_TRUE(base::PathExists(chrome_dir_4));

  base::FilePath chrome_dll_1(chrome_dir_1);
  chrome_dll_1 = chrome_dll_1.AppendASCII("chrome.dll");
  CreateTextFile(chrome_dll_1.value(), text_content_1);
  ASSERT_TRUE(base::PathExists(chrome_dll_1));

  base::FilePath chrome_dll_2(chrome_dir_2);
  chrome_dll_2 = chrome_dll_2.AppendASCII("chrome.dll");
  CreateTextFile(chrome_dll_2.value(), text_content_1);
  ASSERT_TRUE(base::PathExists(chrome_dll_2));

  base::FilePath chrome_dll_3(chrome_dir_3);
  chrome_dll_3 = chrome_dll_3.AppendASCII("chrome.dll");
  CreateTextFile(chrome_dll_3.value(), text_content_1);
  ASSERT_TRUE(base::PathExists(chrome_dll_3));

  base::FilePath chrome_dll_4(chrome_dir_4);
  chrome_dll_4 = chrome_dll_4.AppendASCII("chrome.dll");
  CreateTextFile(chrome_dll_4.value(), text_content_1);
  ASSERT_TRUE(base::PathExists(chrome_dll_4));

  MockInstallerState installer_state;
  BuildSingleChromeState(chrome_dir, &installer_state);
  Version latest_version("1.0.4.0");
  {
    base::ScopedTempDir temp_dir;
    ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
    installer_state.RemoveOldVersionDirectories(latest_version, NULL,
                                                temp_dir.path());
  }

  // old versions should be gone
  EXPECT_FALSE(base::PathExists(chrome_dir_1));
  EXPECT_FALSE(base::PathExists(chrome_dir_2));
  EXPECT_FALSE(base::PathExists(chrome_dir_3));
  // the latest version should stay
  EXPECT_TRUE(base::PathExists(chrome_dll_4));
}

// Delete older version directories, keeping the one in used intact.
TEST_F(InstallerStateTest, DeleteInUsed) {
  // Create a Chrome dir
  base::FilePath chrome_dir(test_dir_.path());
  chrome_dir = chrome_dir.AppendASCII("chrome");
  base::CreateDirectory(chrome_dir);
  ASSERT_TRUE(base::PathExists(chrome_dir));

  base::FilePath chrome_dir_1(chrome_dir);
  chrome_dir_1 = chrome_dir_1.AppendASCII("1.0.1.0");
  base::CreateDirectory(chrome_dir_1);
  ASSERT_TRUE(base::PathExists(chrome_dir_1));

  base::FilePath chrome_dir_2(chrome_dir);
  chrome_dir_2 = chrome_dir_2.AppendASCII("1.0.2.0");
  base::CreateDirectory(chrome_dir_2);
  ASSERT_TRUE(base::PathExists(chrome_dir_2));

  base::FilePath chrome_dir_3(chrome_dir);
  chrome_dir_3 = chrome_dir_3.AppendASCII("1.0.3.0");
  base::CreateDirectory(chrome_dir_3);
  ASSERT_TRUE(base::PathExists(chrome_dir_3));

  base::FilePath chrome_dir_4(chrome_dir);
  chrome_dir_4 = chrome_dir_4.AppendASCII("1.0.4.0");
  base::CreateDirectory(chrome_dir_4);
  ASSERT_TRUE(base::PathExists(chrome_dir_4));

  base::FilePath chrome_dll_1(chrome_dir_1);
  chrome_dll_1 = chrome_dll_1.AppendASCII("chrome.dll");
  CreateTextFile(chrome_dll_1.value(), text_content_1);
  ASSERT_TRUE(base::PathExists(chrome_dll_1));

  base::FilePath chrome_dll_2(chrome_dir_2);
  chrome_dll_2 = chrome_dll_2.AppendASCII("chrome.dll");
  CreateTextFile(chrome_dll_2.value(), text_content_1);
  ASSERT_TRUE(base::PathExists(chrome_dll_2));

  // Open the file to make it in use.
  std::ofstream file;
  file.open(chrome_dll_2.value().c_str());

  base::FilePath chrome_othera_2(chrome_dir_2);
  chrome_othera_2 = chrome_othera_2.AppendASCII("othera.dll");
  CreateTextFile(chrome_othera_2.value(), text_content_2);
  ASSERT_TRUE(base::PathExists(chrome_othera_2));

  base::FilePath chrome_otherb_2(chrome_dir_2);
  chrome_otherb_2 = chrome_otherb_2.AppendASCII("otherb.dll");
  CreateTextFile(chrome_otherb_2.value(), text_content_2);
  ASSERT_TRUE(base::PathExists(chrome_otherb_2));

  base::FilePath chrome_dll_3(chrome_dir_3);
  chrome_dll_3 = chrome_dll_3.AppendASCII("chrome.dll");
  CreateTextFile(chrome_dll_3.value(), text_content_1);
  ASSERT_TRUE(base::PathExists(chrome_dll_3));

  base::FilePath chrome_dll_4(chrome_dir_4);
  chrome_dll_4 = chrome_dll_4.AppendASCII("chrome.dll");
  CreateTextFile(chrome_dll_4.value(), text_content_1);
  ASSERT_TRUE(base::PathExists(chrome_dll_4));

  MockInstallerState installer_state;
  BuildSingleChromeState(chrome_dir, &installer_state);
  Version latest_version("1.0.4.0");
  Version existing_version("1.0.1.0");
  {
    base::ScopedTempDir temp_dir;
    ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
    installer_state.RemoveOldVersionDirectories(latest_version,
                                                &existing_version,
                                                temp_dir.path());
  }

  // the version defined as the existing version should stay
  EXPECT_TRUE(base::PathExists(chrome_dir_1));
  // old versions not in used should be gone
  EXPECT_FALSE(base::PathExists(chrome_dir_3));
  // every thing under in used version should stay
  EXPECT_TRUE(base::PathExists(chrome_dir_2));
  EXPECT_TRUE(base::PathExists(chrome_dll_2));
  EXPECT_TRUE(base::PathExists(chrome_othera_2));
  EXPECT_TRUE(base::PathExists(chrome_otherb_2));
  // the latest version should stay
  EXPECT_TRUE(base::PathExists(chrome_dll_4));
}

// Tests a few basic things of the Package class.  Makes sure that the path
// operations are correct
TEST_F(InstallerStateTest, Basic) {
  const bool multi_install = false;
  const bool system_level = true;
  base::CommandLine cmd_line = base::CommandLine::FromString(
      std::wstring(L"setup.exe") +
      (multi_install ? L" --multi-install --chrome" : L"") +
      (system_level ? L" --system-level" : L""));
  MasterPreferences prefs(cmd_line);
  InstallationState machine_state;
  machine_state.Initialize();
  MockInstallerState installer_state;
  installer_state.Initialize(cmd_line, prefs, machine_state);
  installer_state.set_target_path(test_dir_.path());
  EXPECT_EQ(test_dir_.path().value(), installer_state.target_path().value());
  EXPECT_EQ(1U, installer_state.products().size());

  const char kOldVersion[] = "1.2.3.4";
  const char kNewVersion[] = "2.3.4.5";

  Version new_version(kNewVersion);
  Version old_version(kOldVersion);
  ASSERT_TRUE(new_version.IsValid());
  ASSERT_TRUE(old_version.IsValid());

  base::FilePath installer_dir(
      installer_state.GetInstallerDirectory(new_version));
  EXPECT_FALSE(installer_dir.empty());

  base::FilePath new_version_dir(installer_state.target_path().Append(
      base::UTF8ToWide(new_version.GetString())));
  base::FilePath old_version_dir(installer_state.target_path().Append(
      base::UTF8ToWide(old_version.GetString())));

  EXPECT_FALSE(base::PathExists(new_version_dir));
  EXPECT_FALSE(base::PathExists(old_version_dir));

  EXPECT_FALSE(base::PathExists(installer_dir));
  base::CreateDirectory(installer_dir);
  EXPECT_TRUE(base::PathExists(new_version_dir));

  base::CreateDirectory(old_version_dir);
  EXPECT_TRUE(base::PathExists(old_version_dir));

  // Create a fake chrome.dll key file in the old version directory.  This
  // should prevent the old version directory from getting deleted.
  base::FilePath old_chrome_dll(old_version_dir.Append(installer::kChromeDll));
  EXPECT_FALSE(base::PathExists(old_chrome_dll));

  // Hold on to the file exclusively to prevent the directory from
  // being deleted.
  base::win::ScopedHandle file(
    ::CreateFile(old_chrome_dll.value().c_str(), GENERIC_READ,
                 0, NULL, OPEN_ALWAYS, 0, NULL));
  EXPECT_TRUE(file.IsValid());
  EXPECT_TRUE(base::PathExists(old_chrome_dll));

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  // Don't explicitly tell the directory cleanup logic not to delete the
  // old version, rely on the key files to keep it around.
  installer_state.RemoveOldVersionDirectories(new_version,
                                              NULL,
                                              temp_dir.path());

  // The old directory should still exist.
  EXPECT_TRUE(base::PathExists(old_version_dir));
  EXPECT_TRUE(base::PathExists(new_version_dir));

  // Now close the file handle to make it possible to delete our key file.
  file.Close();

  installer_state.RemoveOldVersionDirectories(new_version,
                                              NULL,
                                              temp_dir.path());
  // The new directory should still exist.
  EXPECT_TRUE(base::PathExists(new_version_dir));

  // Now, the old directory and key file should be gone.
  EXPECT_FALSE(base::PathExists(old_chrome_dll));
  EXPECT_FALSE(base::PathExists(old_version_dir));
}

TEST_F(InstallerStateTest, WithProduct) {
  const bool multi_install = false;
  const bool system_level = true;
  base::CommandLine cmd_line = base::CommandLine::FromString(
      std::wstring(L"setup.exe") +
      (multi_install ? L" --multi-install --chrome" : L"") +
      (system_level ? L" --system-level" : L""));
  MasterPreferences prefs(cmd_line);
  InstallationState machine_state;
  machine_state.Initialize();
  MockInstallerState installer_state;
  installer_state.Initialize(cmd_line, prefs, machine_state);
  installer_state.set_target_path(test_dir_.path());
  EXPECT_EQ(1U, installer_state.products().size());
  EXPECT_EQ(system_level, installer_state.system_install());

  const char kCurrentVersion[] = "1.2.3.4";
  Version current_version(kCurrentVersion);

  HKEY root = system_level ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
  EXPECT_EQ(root, installer_state.root_key());

  {
    RegistryOverrideManager override_manager;
    override_manager.OverrideRegistry(root);
    BrowserDistribution* dist = BrowserDistribution::GetSpecificDistribution(
        BrowserDistribution::CHROME_BROWSER);
    RegKey chrome_key(root, dist->GetVersionKey().c_str(), KEY_ALL_ACCESS);
    EXPECT_TRUE(chrome_key.Valid());
    if (chrome_key.Valid()) {
      chrome_key.WriteValue(google_update::kRegVersionField,
                            base::UTF8ToWide(
                                current_version.GetString()).c_str());
      machine_state.Initialize();
      // TODO(tommi): Also test for when there exists a new_chrome.exe.
      Version found_version(*installer_state.GetCurrentVersion(machine_state));
      EXPECT_TRUE(found_version.IsValid());
      if (found_version.IsValid())
        EXPECT_EQ(current_version, found_version);
    }
  }
}

TEST_F(InstallerStateTest, InstallerResult) {
  const bool system_level = true;
  HKEY root = system_level ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;

  RegKey key;
  std::wstring launch_cmd = L"hey diddle diddle";
  std::wstring value;
  DWORD dw_value;

  // check results for a fresh install of single Chrome
  {
    RegistryOverrideManager override_manager;
    override_manager.OverrideRegistry(root);
    base::CommandLine cmd_line =
        base::CommandLine::FromString(L"setup.exe --system-level");
    const MasterPreferences prefs(cmd_line);
    InstallationState machine_state;
    machine_state.Initialize();
    InstallerState state;
    state.Initialize(cmd_line, prefs, machine_state);
    state.WriteInstallerResult(installer::FIRST_INSTALL_SUCCESS,
                               IDS_INSTALL_OS_ERROR_BASE, &launch_cmd);
    BrowserDistribution* distribution =
        BrowserDistribution::GetSpecificDistribution(
            BrowserDistribution::CHROME_BROWSER);
    EXPECT_EQ(ERROR_SUCCESS,
        key.Open(root, distribution->GetStateKey().c_str(), KEY_READ));
    EXPECT_EQ(ERROR_SUCCESS,
        key.ReadValueDW(installer::kInstallerResult, &dw_value));
    EXPECT_EQ(static_cast<DWORD>(0), dw_value);
    EXPECT_EQ(ERROR_SUCCESS,
        key.ReadValueDW(installer::kInstallerError, &dw_value));
    EXPECT_EQ(static_cast<DWORD>(installer::FIRST_INSTALL_SUCCESS), dw_value);
    EXPECT_EQ(ERROR_SUCCESS,
        key.ReadValue(installer::kInstallerResultUIString, &value));
    EXPECT_FALSE(value.empty());
    EXPECT_EQ(ERROR_SUCCESS,
        key.ReadValue(installer::kInstallerSuccessLaunchCmdLine, &value));
    EXPECT_EQ(launch_cmd, value);
  }

  // check results for a fresh install of multi Chrome
  {
    RegistryOverrideManager override_manager;
    override_manager.OverrideRegistry(root);
    base::CommandLine cmd_line = base::CommandLine::FromString(
        L"setup.exe --system-level --multi-install --chrome");
    const MasterPreferences prefs(cmd_line);
    InstallationState machine_state;
    machine_state.Initialize();
    InstallerState state;
    state.Initialize(cmd_line, prefs, machine_state);
    state.WriteInstallerResult(installer::FIRST_INSTALL_SUCCESS, 0,
                               &launch_cmd);
    BrowserDistribution* distribution =
        BrowserDistribution::GetSpecificDistribution(
            BrowserDistribution::CHROME_BROWSER);
    BrowserDistribution* binaries =
        BrowserDistribution::GetSpecificDistribution(
            BrowserDistribution::CHROME_BINARIES);
    EXPECT_EQ(ERROR_SUCCESS,
        key.Open(root, distribution->GetStateKey().c_str(), KEY_READ));
    EXPECT_EQ(ERROR_SUCCESS,
        key.ReadValue(installer::kInstallerSuccessLaunchCmdLine, &value));
    EXPECT_EQ(launch_cmd, value);
    EXPECT_EQ(ERROR_SUCCESS,
        key.Open(root, binaries->GetStateKey().c_str(), KEY_READ));
    EXPECT_EQ(ERROR_SUCCESS,
        key.ReadValue(installer::kInstallerSuccessLaunchCmdLine, &value));
    EXPECT_EQ(launch_cmd, value);
    key.Close();
  }
}

// Test GetCurrentVersion when migrating single Chrome to multi
TEST_F(InstallerStateTest, GetCurrentVersionMigrateChrome) {
  using installer::FakeInstallationState;

  const bool system_install = false;
  FakeInstallationState machine_state;

  // Pretend that this version of single-install Chrome is already installed.
  machine_state.AddChrome(system_install, false,
                          new Version(chrome::kChromeVersion));

  // Now we're invoked to install multi Chrome.
  base::CommandLine cmd_line(
      base::CommandLine::FromString(L"setup.exe --multi-install --chrome"));
  MasterPreferences prefs(cmd_line);
  InstallerState installer_state;
  installer_state.Initialize(cmd_line, prefs, machine_state);

  // Is the Chrome version picked up?
  scoped_ptr<Version> version(installer_state.GetCurrentVersion(machine_state));
  EXPECT_TRUE(version.get() != NULL);
}

TEST_F(InstallerStateTest, IsFileInUse) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  base::FilePath temp_file;
  ASSERT_TRUE(base::CreateTemporaryFileInDir(temp_dir.path(), &temp_file));

  EXPECT_FALSE(MockInstallerState::IsFileInUse(temp_file));

  {
    // Open a handle to the file with the same access mode and sharing options
    // as the loader.
    base::win::ScopedHandle temp_handle(
        CreateFile(temp_file.value().c_str(),
                   SYNCHRONIZE | FILE_EXECUTE,
                   FILE_SHARE_DELETE | FILE_SHARE_READ,
                   NULL, OPEN_EXISTING, 0, 0));
    ASSERT_TRUE(temp_handle.IsValid());

    // The file should now be in use.
    EXPECT_TRUE(MockInstallerState::IsFileInUse(temp_file));
  }

  // And once the handle is gone, it should no longer be in use.
  EXPECT_FALSE(MockInstallerState::IsFileInUse(temp_file));
}

TEST_F(InstallerStateTest, RemoveOldVersionDirs) {
  MockInstallerState installer_state;
  installer_state.set_target_path(test_dir_.path());
  EXPECT_EQ(test_dir_.path().value(), installer_state.target_path().value());

  const char kOldVersion[] = "2.0.0.0";
  const char kNewVersion[] = "3.0.0.0";
  const char kOldChromeExeVersion[] = "2.1.0.0";
  const char kChromeExeVersion[] = "2.1.1.1";
  const char kNewChromeExeVersion[] = "3.0.0.0";

  Version new_version(kNewVersion);
  Version old_version(kOldVersion);
  Version old_chrome_exe_version(kOldChromeExeVersion);
  Version chrome_exe_version(kChromeExeVersion);
  Version new_chrome_exe_version(kNewChromeExeVersion);

  ASSERT_TRUE(new_version.IsValid());
  ASSERT_TRUE(old_version.IsValid());
  ASSERT_TRUE(old_chrome_exe_version.IsValid());
  ASSERT_TRUE(chrome_exe_version.IsValid());
  ASSERT_TRUE(new_chrome_exe_version.IsValid());

  // Set up a bunch of version dir paths.
  base::FilePath version_dirs[] = {
    installer_state.target_path().Append(L"1.2.3.4"),
    installer_state.target_path().Append(L"1.2.3.5"),
    installer_state.target_path().Append(L"1.2.3.6"),
    installer_state.target_path().AppendASCII(kOldVersion),
    installer_state.target_path().AppendASCII(kOldChromeExeVersion),
    installer_state.target_path().Append(L"2.1.1.0"),
    installer_state.target_path().AppendASCII(kChromeExeVersion),
    installer_state.target_path().AppendASCII(kNewVersion),
    installer_state.target_path().Append(L"3.9.1.1"),
  };

  // Create the version directories.
  for (size_t i = 0; i < arraysize(version_dirs); i++) {
    base::CreateDirectory(version_dirs[i]);
    EXPECT_TRUE(base::PathExists(version_dirs[i]));
  }

  // Create exes with the appropriate version resource.
  // Use the current test exe as a baseline.
  base::FilePath exe_path;
  ASSERT_TRUE(PathService::Get(base::FILE_EXE, &exe_path));

  struct target_info {
    base::FilePath target_file;
    const Version& target_version;
  } targets[] = {
    { installer_state.target_path().Append(installer::kChromeOldExe),
      old_chrome_exe_version },
    { installer_state.target_path().Append(installer::kChromeExe),
      chrome_exe_version },
    { installer_state.target_path().Append(installer::kChromeNewExe),
      new_chrome_exe_version },
  };
  for (size_t i = 0; i < arraysize(targets); ++i) {
    ASSERT_TRUE(upgrade_test::GenerateSpecificPEFileVersion(
        exe_path, targets[i].target_file, targets[i].target_version));
  }

  // Call GetExistingExeVersions, validate that picks up the
  // exe resources.
  std::set<std::string> expected_exe_versions;
  expected_exe_versions.insert(kOldChromeExeVersion);
  expected_exe_versions.insert(kChromeExeVersion);
  expected_exe_versions.insert(kNewChromeExeVersion);

  std::set<std::string> actual_exe_versions;
  installer_state.GetExistingExeVersions(&actual_exe_versions);
  EXPECT_EQ(expected_exe_versions, actual_exe_versions);

  // Call RemoveOldVersionDirectories
  installer_state.RemoveOldVersionDirectories(new_version,
                                              &old_version,
                                              installer_state.target_path());

  // What we expect to have left.
  std::set<std::string> expected_remaining_dirs;
  expected_remaining_dirs.insert(kOldVersion);
  expected_remaining_dirs.insert(kNewVersion);
  expected_remaining_dirs.insert(kOldChromeExeVersion);
  expected_remaining_dirs.insert(kChromeExeVersion);
  expected_remaining_dirs.insert(kNewChromeExeVersion);

  // Enumerate dirs in target_path(), ensure only desired remain.
  base::FileEnumerator version_enum(installer_state.target_path(), false,
                                    base::FileEnumerator::DIRECTORIES);
  for (base::FilePath next_version = version_enum.Next(); !next_version.empty();
       next_version = version_enum.Next()) {
    base::FilePath dir_name(next_version.BaseName());
    Version version(base::UTF16ToASCII(dir_name.value()));
    if (version.IsValid()) {
      EXPECT_TRUE(expected_remaining_dirs.erase(version.GetString()))
          << "Unexpected version dir found: " << version.GetString();
    }
  }

  std::set<std::string>::const_iterator iter(
      expected_remaining_dirs.begin());
  for (; iter != expected_remaining_dirs.end(); ++iter)
    ADD_FAILURE() << "Expected to find version dir for " << *iter;
}

TEST_F(InstallerStateTest, InitializeTwice) {
  // Override these paths so that they can be found after the registry override
  // manager is in place.
  base::FilePath temp;
  PathService::Get(base::DIR_PROGRAM_FILES, &temp);
  base::ScopedPathOverride program_files_override(base::DIR_PROGRAM_FILES,
                                                  temp);
  PathService::Get(base::DIR_PROGRAM_FILESX86, &temp);
  base::ScopedPathOverride program_filesx86_override(base::DIR_PROGRAM_FILESX86,
                                                     temp);
  PathService::Get(base::DIR_LOCAL_APP_DATA, &temp);
  base::ScopedPathOverride local_app_data_override(base::DIR_LOCAL_APP_DATA,
                                                   temp);
  registry_util::RegistryOverrideManager override_manager;
  override_manager.OverrideRegistry(HKEY_CURRENT_USER);
  override_manager.OverrideRegistry(HKEY_LOCAL_MACHINE);

  InstallationState machine_state;
  machine_state.Initialize();

  InstallerState installer_state;

  // Initialize the instance to install multi Chrome.
  {
    base::CommandLine cmd_line(
        base::CommandLine::FromString(L"setup.exe --multi-install --chrome"));
    MasterPreferences prefs(cmd_line);
    installer_state.Initialize(cmd_line, prefs, machine_state);
  }
  // Confirm the expected state.
  EXPECT_EQ(InstallerState::USER_LEVEL, installer_state.level());
  EXPECT_EQ(InstallerState::MULTI_PACKAGE, installer_state.package_type());
  EXPECT_EQ(InstallerState::MULTI_INSTALL, installer_state.operation());
  EXPECT_TRUE(wcsstr(installer_state.target_path().value().c_str(),
                     BrowserDistribution::GetSpecificDistribution(
                         BrowserDistribution::CHROME_BINARIES)->
                         GetInstallSubDir().c_str()));
  EXPECT_FALSE(installer_state.verbose_logging());
  EXPECT_EQ(installer_state.state_key(),
            BrowserDistribution::GetSpecificDistribution(
                BrowserDistribution::CHROME_BROWSER)->GetStateKey());
  EXPECT_EQ(installer_state.state_type(), BrowserDistribution::CHROME_BROWSER);
  EXPECT_TRUE(installer_state.multi_package_binaries_distribution());
  EXPECT_TRUE(installer_state.FindProduct(BrowserDistribution::CHROME_BROWSER));

  // Now initialize it to install system-level single Chrome.
  {
    base::CommandLine cmd_line(base::CommandLine::FromString(
        L"setup.exe --system-level --verbose-logging"));
    MasterPreferences prefs(cmd_line);
    installer_state.Initialize(cmd_line, prefs, machine_state);
  }

  // Confirm that the old state is gone.
  EXPECT_EQ(InstallerState::SYSTEM_LEVEL, installer_state.level());
  EXPECT_EQ(InstallerState::SINGLE_PACKAGE, installer_state.package_type());
  EXPECT_EQ(InstallerState::SINGLE_INSTALL_OR_UPDATE,
            installer_state.operation());
  EXPECT_TRUE(wcsstr(installer_state.target_path().value().c_str(),
                     BrowserDistribution::GetSpecificDistribution(
                         BrowserDistribution::CHROME_BROWSER)->
                         GetInstallSubDir().c_str()));
  EXPECT_TRUE(installer_state.verbose_logging());
  EXPECT_EQ(installer_state.state_key(),
            BrowserDistribution::GetSpecificDistribution(
                BrowserDistribution::CHROME_BROWSER)->GetStateKey());
  EXPECT_EQ(installer_state.state_type(), BrowserDistribution::CHROME_BROWSER);
  EXPECT_TRUE(installer_state.FindProduct(BrowserDistribution::CHROME_BROWSER));
}

// A fixture for testing InstallerState::DetermineCriticalVersion.  Individual
// tests must invoke Initialize() with a critical version.
class InstallerStateCriticalVersionTest : public ::testing::Test {
 protected:
  InstallerStateCriticalVersionTest()
      : cmd_line_(base::CommandLine::NO_PROGRAM) {}

  // Creates a set of versions for use by all test runs.
  static void SetUpTestCase() {
    low_version_    = new Version("15.0.874.106");
    opv_version_    = new Version("15.0.874.255");
    middle_version_ = new Version("16.0.912.32");
    pv_version_     = new Version("16.0.912.255");
    high_version_   = new Version("17.0.932.0");
  }

  // Cleans up versions used by all test runs.
  static void TearDownTestCase() {
    delete low_version_;
    delete opv_version_;
    delete middle_version_;
    delete pv_version_;
    delete high_version_;
  }

  // Initializes the InstallerState to use for a test run.  The returned
  // instance's critical update version is set to |version|.  |version| may be
  // NULL, in which case the critical update version is unset.
  MockInstallerState& Initialize(const Version* version) {
    cmd_line_ = version == NULL ? base::CommandLine::FromString(L"setup.exe")
                                : base::CommandLine::FromString(
                                      L"setup.exe --critical-update-version=" +
                                      base::ASCIIToUTF16(version->GetString()));
    prefs_.reset(new MasterPreferences(cmd_line_));
    machine_state_.Initialize();
    installer_state_.Initialize(cmd_line_, *prefs_, machine_state_);
    return installer_state_;
  }

  static Version* low_version_;
  static Version* opv_version_;
  static Version* middle_version_;
  static Version* pv_version_;
  static Version* high_version_;

  base::CommandLine cmd_line_;
  scoped_ptr<MasterPreferences> prefs_;
  InstallationState machine_state_;
  MockInstallerState installer_state_;
};

Version* InstallerStateCriticalVersionTest::low_version_ = NULL;
Version* InstallerStateCriticalVersionTest::opv_version_ = NULL;
Version* InstallerStateCriticalVersionTest::middle_version_ = NULL;
Version* InstallerStateCriticalVersionTest::pv_version_ = NULL;
Version* InstallerStateCriticalVersionTest::high_version_ = NULL;

// Test the case where the critical version is less than the currently-running
// Chrome.  The critical version is ignored since it doesn't apply.
TEST_F(InstallerStateCriticalVersionTest, CriticalBeforeOpv) {
  MockInstallerState& installer_state(Initialize(low_version_));

  EXPECT_EQ(installer_state.critical_update_version(), *low_version_);
  // Unable to determine the installed version, so assume critical update.
  EXPECT_TRUE(
      installer_state.DetermineCriticalVersion(NULL, *pv_version_).IsValid());
  // Installed version is past the critical update.
  EXPECT_FALSE(
      installer_state.DetermineCriticalVersion(opv_version_, *pv_version_)
          .IsValid());
  // Installed version is past the critical update.
  EXPECT_FALSE(
      installer_state.DetermineCriticalVersion(pv_version_, *pv_version_)
          .IsValid());
}

// Test the case where the critical version is equal to the currently-running
// Chrome.  The critical version is ignored since it doesn't apply.
TEST_F(InstallerStateCriticalVersionTest, CriticalEqualsOpv) {
  MockInstallerState& installer_state(Initialize(opv_version_));

  EXPECT_EQ(installer_state.critical_update_version(), *opv_version_);
  // Unable to determine the installed version, so assume critical update.
  EXPECT_TRUE(
      installer_state.DetermineCriticalVersion(NULL, *pv_version_).IsValid());
  // Installed version equals the critical update.
  EXPECT_FALSE(
      installer_state.DetermineCriticalVersion(opv_version_, *pv_version_)
          .IsValid());
  // Installed version equals the critical update.
  EXPECT_FALSE(
      installer_state.DetermineCriticalVersion(pv_version_, *pv_version_)
          .IsValid());
}

// Test the case where the critical version is between the currently-running
// Chrome and the to-be-installed Chrome.
TEST_F(InstallerStateCriticalVersionTest, CriticalBetweenOpvAndPv) {
  MockInstallerState& installer_state(Initialize(middle_version_));

  EXPECT_EQ(installer_state.critical_update_version(), *middle_version_);
  // Unable to determine the installed version, so assume critical update.
  EXPECT_TRUE(
      installer_state.DetermineCriticalVersion(NULL, *pv_version_).IsValid());
  // Installed version before the critical update.
  EXPECT_TRUE(
      installer_state.DetermineCriticalVersion(opv_version_, *pv_version_)
          .IsValid());
  // Installed version is past the critical update.
  EXPECT_FALSE(
      installer_state.DetermineCriticalVersion(pv_version_, *pv_version_)
          .IsValid());
}

// Test the case where the critical version is the same as the to-be-installed
// Chrome.
TEST_F(InstallerStateCriticalVersionTest, CriticalEqualsPv) {
  MockInstallerState& installer_state(Initialize(pv_version_));

  EXPECT_EQ(installer_state.critical_update_version(), *pv_version_);
  // Unable to determine the installed version, so assume critical update.
  EXPECT_TRUE(
      installer_state.DetermineCriticalVersion(NULL, *pv_version_).IsValid());
  // Installed version before the critical update.
  EXPECT_TRUE(
      installer_state.DetermineCriticalVersion(opv_version_, *pv_version_)
          .IsValid());
  // Installed version equals the critical update.
  EXPECT_FALSE(
      installer_state.DetermineCriticalVersion(pv_version_, *pv_version_)
          .IsValid());
}

// Test the case where the critical version is greater than the to-be-installed
// Chrome.
TEST_F(InstallerStateCriticalVersionTest, CriticalAfterPv) {
  MockInstallerState& installer_state(Initialize(high_version_));

  EXPECT_EQ(installer_state.critical_update_version(), *high_version_);
  // Critical update newer than the new version.
  EXPECT_FALSE(
      installer_state.DetermineCriticalVersion(NULL, *pv_version_).IsValid());
  EXPECT_FALSE(
      installer_state.DetermineCriticalVersion(opv_version_, *pv_version_)
          .IsValid());
  EXPECT_FALSE(
      installer_state.DetermineCriticalVersion(pv_version_, *pv_version_)
          .IsValid());
}
