// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/mini_installer_test/chrome_mini_installer.h"

#include <algorithm>

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/process.h"
#include "base/process_util.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/threading/platform_thread.h"
#include "base/win/registry.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/helper.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/installation_validation_helper.h"
#include "chrome/installer/util/util_constants.h"
#include "chrome/test/base/chrome_process_util.h"
#include "chrome/test/mini_installer_test/mini_installer_test_constants.h"
#include "chrome/test/mini_installer_test/mini_installer_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::TimeDelta;
using base::win::RegKey;
using installer::InstallationValidator;

namespace {

struct FilePathInfo {
  file_util::FileEnumerator::FindInfo info;
  FilePath path;
};

bool CompareDate(const FilePathInfo& a,
                 const FilePathInfo& b) {
#if defined(OS_POSIX)
  return a.info.stat.st_mtime > b.info.stat.st_mtime;
#elif defined(OS_WIN)
  if (a.info.ftLastWriteTime.dwHighDateTime ==
      b.info.ftLastWriteTime.dwHighDateTime) {
    return a.info.ftLastWriteTime.dwLowDateTime >
           b.info.ftLastWriteTime.dwLowDateTime;
  } else {
    return a.info.ftLastWriteTime.dwHighDateTime >
           b.info.ftLastWriteTime.dwHighDateTime;
  }
#endif
}

// Get list of file |type| matching |pattern| in |root|.
// The list is sorted in last modified date order.
// Return true if files/directories are found.
bool FindMatchingFiles(const FilePath& root,
                       const std::string& pattern,
                       file_util::FileEnumerator::FileType type,
                       std::vector<FilePath>* paths) {
  file_util::FileEnumerator files(root, false, type,
      FilePath().AppendASCII(pattern).value());
  std::vector<FilePathInfo> matches;
  for (FilePath current = files.Next(); !current.empty();
      current = files.Next()) {
    FilePathInfo entry;
    files.GetFindInfo(&entry.info);
    entry.path = current;
    matches.push_back(entry);
  }

  if (matches.empty())
    return false;

  std::sort(matches.begin(), matches.end(), CompareDate);
  std::vector<FilePathInfo>::iterator current;
  for (current = matches.begin(); current != matches.end(); ++current) {
    paths->push_back(current->path);
  }
  return true;
}

bool FindNewestMatchingFile(const FilePath& root,
                            const std::string& pattern,
                            file_util::FileEnumerator::FileType type,
                            FilePath* path) {
  std::vector<FilePath> paths;
  if (FindMatchingFiles(root, pattern, type, &paths)) {
    *path = paths[0];
    return true;
  }
  return false;
}
}  // namespace

ChromeMiniInstaller::ChromeMiniInstaller(bool system_install,
                                         bool is_chrome_frame,
                                         const std::string& build)
    : is_chrome_frame_(is_chrome_frame),
      system_install_(system_install),
      build_(build) {
  FilePath full_installer;
  FilePath previous_installer;
  if (!GetFullInstaller(&full_installer) ||
      !GetPreviousInstaller(&previous_installer))
    return;
  current_build_ =
      full_installer.DirName().DirName().BaseName().MaybeAsASCII();
  previous_build_ =
      previous_installer.DirName().DirName().BaseName().MaybeAsASCII();
}

// Installs Chrome.
void ChromeMiniInstaller::Install() {
  FilePath mini_installer;
  ASSERT_TRUE(GetMiniInstaller(&mini_installer));
  InstallMiniInstaller(false, mini_installer);
}

// This method will get the previous latest full installer from
// nightly location, install it and over install with specified install_type.
void ChromeMiniInstaller::OverInstallOnFullInstaller(
    const std::wstring& install_type, bool should_start_ie) {
  FilePath full_installer;
  FilePath diff_installer;
  FilePath previous_installer;
  ASSERT_TRUE(GetFullInstaller(&full_installer) &&
      GetDiffInstaller(&diff_installer) &&
      GetPreviousInstaller(&previous_installer));

  if (should_start_ie)
    LaunchIE(L"http://www.google.com");

  InstallMiniInstaller(false, previous_installer);

  std::string got_prev_version;
  ASSERT_TRUE(GetChromeVersionFromRegistry(&got_prev_version));

  std::string expected_version;
  if (install_type == mini_installer_constants::kDiffInstall) {
    LOG(INFO) << "Over installing with latest differential installer: "
              << diff_installer.value();
    expected_version = current_diff_build_;
    InstallMiniInstaller(true, diff_installer);

  } else if (install_type == mini_installer_constants::kFullInstall) {
    LOG(INFO) << "Over installing with latest full installer: "
              << full_installer.value();
    expected_version = current_build_;
    InstallMiniInstaller(true, full_installer);
  }

  std::string got_curr_version;
  ASSERT_TRUE(GetChromeVersionFromRegistry(&got_curr_version));

  if (got_prev_version == previous_build_ &&
      got_curr_version == expected_version) {
    LOG(INFO) << "The over install was successful.\n"
              << "Previously installed version: " << got_prev_version;
    LOG(INFO) << "Currently installed: " << expected_version;
  } else {
    LOG(INFO) << "The over install was not successful.\n"
              << "Expected previous version: " << previous_build_;
    LOG(INFO) << "Actual previous version: " << got_prev_version;
    LOG(INFO) << "Expected new version: " << expected_version
              << "\nActual new version: " << got_curr_version;
    FAIL();
  }
}

// This method will get the latest full installer from nightly location
// and installs it.
void ChromeMiniInstaller::InstallFullInstaller(bool over_install) {
  FilePath full_installer;
  ASSERT_TRUE(GetFullInstaller(&full_installer));
  InstallMiniInstaller(over_install, full_installer);
}

// Installs the Chrome mini-installer, checks the registry and shortcuts.
void ChromeMiniInstaller::InstallMiniInstaller(bool over_install,
                                               const FilePath& path) {
  LOG(INFO) << "Install level is: "
      << (system_install_ ? "system" : "user");
  ASSERT_TRUE(file_util::PathExists(path));
  CommandLine installer(path);
  if (is_chrome_frame_) {
    installer.AppendSwitch(installer::switches::kDoNotCreateShortcuts);
    installer.AppendSwitch(installer::switches::kDoNotLaunchChrome);
    installer.AppendSwitch(installer::switches::kDoNotRegisterForUpdateLaunch);
    installer.AppendSwitch(installer::switches::kChromeFrame);
  }
  RunInstaller(installer);
  std::string version;
  ASSERT_TRUE(GetChromeVersionFromRegistry(&version))
      << "Install failed: unable to get version.";
  VerifyInstall(over_install);
}

CommandLine ChromeMiniInstaller::GetBaseMultiInstallCommand() {
  FilePath mini_installer;
  if (!GetMiniInstaller(&mini_installer))
    return CommandLine(CommandLine::NO_PROGRAM);
  CommandLine cmd(mini_installer);
  cmd.AppendSwitch(installer::switches::kMultiInstall);
  return cmd;
}

void ChromeMiniInstaller::InstallChromeUsingMultiInstall() {
  CommandLine cmd = GetBaseMultiInstallCommand();
  cmd.AppendSwitch(installer::switches::kChrome);
  RunInstaller(cmd);

  // Verify installation.
  InstallationValidator::InstallationType type;
  InstallationValidator::ValidateInstallationType(system_install_, &type);
  BrowserDistribution* dist = GetCurrentBrowserDistribution();
  ASSERT_TRUE(InstallUtil::IsMultiInstall(dist, system_install_));
  EXPECT_TRUE(type & InstallationValidator::ProductBits::CHROME_MULTI);
  FindChromeShortcut();
  LaunchChrome(true);
}

void ChromeMiniInstaller::InstallChromeFrameUsingMultiInstall() {
  CommandLine cmd = GetBaseMultiInstallCommand();
  cmd.AppendSwitch(installer::switches::kDoNotCreateShortcuts);
  cmd.AppendSwitch(installer::switches::kDoNotLaunchChrome);
  cmd.AppendSwitch(installer::switches::kDoNotRegisterForUpdateLaunch);
  cmd.AppendSwitch(installer::switches::kChromeFrame);
  RunInstaller(cmd);

  // Verify installation.
  InstallationValidator::InstallationType type =
      installer::ExpectValidInstallation(system_install_);
  BrowserDistribution* dist = GetCurrentBrowserDistribution();
  ASSERT_TRUE(InstallUtil::IsMultiInstall(dist, system_install_));
  EXPECT_TRUE(type & InstallationValidator::ProductBits::CHROME_FRAME_MULTI);
  // Launch IE
  LaunchIE(L"gcf:about:version");
  if (system_install_) {
    MiniInstallerTestUtil::VerifyProcessLaunch(installer::kChromeExe, true);
  } else {
    MiniInstallerTestUtil::VerifyProcessLaunch(
        installer::kChromeFrameHelperExe, true);
  }
  FindChromeShortcut();
}

void ChromeMiniInstaller::InstallChromeAndChromeFrame(bool ready_mode) {
  CommandLine cmd = GetBaseMultiInstallCommand();
  cmd.AppendSwitch(installer::switches::kChrome);
  cmd.AppendSwitch(installer::switches::kChromeFrame);
  if (ready_mode)
    cmd.AppendSwitch(installer::switches::kChromeFrameReadyMode);
  RunInstaller(cmd);
  // Verify installation.
  InstallationValidator::InstallationType type =
      installer::ExpectValidInstallation(system_install_);
  BrowserDistribution* dist = GetCurrentBrowserDistribution();
  ASSERT_TRUE(InstallUtil::IsMultiInstall(dist, system_install_));
  EXPECT_TRUE(type & InstallationValidator::ProductBits::CHROME_MULTI);
  if (ready_mode) {
    EXPECT_TRUE(type &
        InstallationValidator::ProductBits::CHROME_FRAME_READY_MODE);
  }
  FindChromeShortcut();
  LaunchChrome(true);
  LaunchIE(L"gcf:about:version");
  // Check if Chrome process got spawned.
  MiniInstallerTestUtil::VerifyProcessLaunch(L"chrome_frame.exe", false);
}

// This method tests the standalone installer by verifying the steps listed at:
// https://sites.google.com/a/google.com/chrome-pmo/
// standalone-installers/testing-standalone-installers
// This method applies appropriate tags to standalone installer and deletes
// old installer before running the new tagged installer. It also verifies
// that the installed version is correct.
void ChromeMiniInstaller::InstallStandaloneInstaller() {
  file_util::Delete(mini_installer_constants::kStandaloneInstaller, true);
  CommandLine tag_installer_command =
      ChromeMiniInstaller::GetCommandForTagging();
  ASSERT_FALSE(tag_installer_command.GetCommandLineString().empty());
  base::LaunchOptions options;
  options.wait = true;
  base::LaunchProcess(tag_installer_command, options, NULL);
  FilePath installer_path = MiniInstallerTestUtil::GetFilePath(
      mini_installer_constants::kStandaloneInstaller);
  InstallMiniInstaller(false, installer_path);
  ASSERT_TRUE(VerifyStandaloneInstall());
  file_util::Delete(mini_installer_constants::kStandaloneInstaller, true);
}

CommandLine ChromeMiniInstaller::GetCommandForTagging() {
  FilePath tagged_installer = MiniInstallerTestUtil::GetFilePath(
      mini_installer_constants::kStandaloneInstaller);
  FilePath standalone_installer;
  if (!GetStandaloneInstaller(&standalone_installer))
    return CommandLine::FromString(L"");
  CommandLine command = CommandLine::FromString(
      base::StringPrintf(L"%ls %ls %ls %ls",
      mini_installer_constants::kChromeApplyTagExe,
      standalone_installer.value().c_str(),
      tagged_installer.value().c_str(),
      mini_installer_constants::kChromeApplyTagParameters));
  LOG(INFO) << "Tagging command: " << command.GetCommandLineString();
  return command;
}

// Installs chromesetup.exe, waits for the install to finish and then
// checks the registry and shortcuts.
void ChromeMiniInstaller::InstallMetaInstaller() {
  // Install Google Chrome through meta installer.
  CommandLine installer(FilePath::FromWStringHack(
      mini_installer_constants::kChromeMetaInstallerExe));
  ASSERT_TRUE(file_util::PathExists(installer.GetProgram()));
  if (is_chrome_frame_) {
    installer.AppendSwitch(installer::switches::kDoNotCreateShortcuts);
    installer.AppendSwitch(installer::switches::kDoNotLaunchChrome);
    installer.AppendSwitch(installer::switches::kDoNotRegisterForUpdateLaunch);
    installer.AppendSwitch(installer::switches::kChromeFrame);
  }
  RunInstaller(installer);

  ASSERT_TRUE(MiniInstallerTestUtil::VerifyProcessClose(
      mini_installer_constants::kChromeMetaInstallerExecutable));

  std::wstring chrome_google_update_state_key(
      google_update::kRegPathClients);
  chrome_google_update_state_key.append(L"\\");

  BrowserDistribution* dist = GetCurrentBrowserDistribution();
  chrome_google_update_state_key.append(dist->GetAppGuid());

  ASSERT_TRUE(CheckRegistryKey(chrome_google_update_state_key));
  ASSERT_TRUE(CheckRegistryKey(dist->GetVersionKey()));
  FindChromeShortcut();
  LaunchChrome(true);
}

// If the build type is Google Chrome, then it first installs meta installer
// and then over installs with mini_installer. It also verifies if Chrome can
// be launched successfully after overinstall.
void ChromeMiniInstaller::OverInstall() {
  InstallMetaInstaller();
  std::string reg_key_value_returned;
  // gets the registry key value before overinstall.
  ASSERT_TRUE(GetChromeVersionFromRegistry(&reg_key_value_returned));
  printf("\n\nPreparing to overinstall...\n");
  InstallFullInstaller(true);
  std::string reg_key_value_after_overinstall;
  // Get the registry key value after over install
  ASSERT_TRUE(GetChromeVersionFromRegistry(&reg_key_value_after_overinstall));
  ASSERT_TRUE(VerifyOverInstall(reg_key_value_returned,
                                reg_key_value_after_overinstall));
}

// This method will first install Chrome. Deletes either registry or
// folder based on the passed argument, then tries to launch Chrome.
// Then installs Chrome again to repair.
void ChromeMiniInstaller::Repair(
    ChromeMiniInstaller::RepairChrome repair_type) {
  InstallFullInstaller(false);
  if (is_chrome_frame_) {
    MiniInstallerTestUtil::CloseProcesses(
        mini_installer_constants::kIEProcessName);
  } else {
    MiniInstallerTestUtil::CloseProcesses(installer::kNaClExe);
  }
  MiniInstallerTestUtil::CloseProcesses(installer::kChromeExe);
  if (repair_type == ChromeMiniInstaller::VERSION_FOLDER) {
    std::string build_number;
    ASSERT_TRUE(GetChromeVersionFromRegistry(&build_number));
    FilePath install_path;
    ASSERT_TRUE(GetInstallDirectory(&install_path));
    install_path = install_path.AppendASCII(build_number);
    ASSERT_TRUE(file_util::Delete(install_path, true));
  } else if (repair_type == ChromeMiniInstaller::REGISTRY) {
    DeletePvRegistryKey();
    printf("Deleted registry. Now trying to launch chrome\n");
  }
  FilePath current_path;
  ASSERT_TRUE(MiniInstallerTestUtil::ChangeCurrentDirectory(&current_path));
  LaunchChrome(false);
  LOG(INFO) << "Installing Chrome again to see if it can be repaired.";
  InstallFullInstaller(true);
  LOG(INFO) << "Chrome repair successful.";
  // Set the current directory back to original path.
  file_util::SetCurrentDirectory(current_path);
}

// This method first checks if Chrome is running.
// If yes, will close all the processes.
// Then will find and spawn uninstall path.
// Handles uninstall confirm dialog.
// Waits until setup.exe ends.
// Checks if registry key exist even after uninstall.
// Deletes App dir.
// Closes feedback form.
void ChromeMiniInstaller::UnInstall() {
  BrowserDistribution* dist = GetCurrentBrowserDistribution();
  std::string version;
  std::string result;

  if (!GetChromeVersionFromRegistry(&version))
    return;

  // Attempt to kill all Chrome/Chrome Frame related processes.
  MiniInstallerTestUtil::CloseProcesses(
      mini_installer_constants::kIEProcessName);
  MiniInstallerTestUtil::CloseProcesses(installer::kNaClExe);
  MiniInstallerTestUtil::CloseProcesses(installer::kChromeExe);
  // Get uninstall command.
  CommandLine cmd = InstallUtil::GetChromeUninstallCmd(
      system_install_, dist->GetType());
  ASSERT_TRUE(file_util::PathExists(cmd.GetProgram()))
      << "Uninstall executable does not exist.";

  cmd.AppendSwitch(installer::switches::kForceUninstall);
  LOG(INFO) << "Uninstall command: " << cmd.GetCommandLineString();

  base::ProcessHandle setup_handle;
  ASSERT_TRUE(base::LaunchProcess(cmd, base::LaunchOptions(), &setup_handle))
      << "Failed to launch uninstall command.";

  LOG(INFO) << (CloseUninstallWindow() ? "Succeeded " : "Failed ")
      << "in attempt to closed uninstall window.";
  ASSERT_TRUE(MiniInstallerTestUtil::VerifyProcessHandleClosed(setup_handle));
  ASSERT_FALSE(CheckRegistryKeyOnUninstall(dist->GetVersionKey()))
      << "It appears Chrome was not completely uninstalled.";

  DeleteUserDataFolder();
  // Close IE survey window that gets launched on uninstall.
  MiniInstallerTestUtil::CloseProcesses(
      mini_installer_constants::kIEExecutable);
}

void ChromeMiniInstaller::UnInstallChromeFrameWithIERunning() {
  BrowserDistribution* dist = GetCurrentBrowserDistribution();
  std::wstring product_name =
      mini_installer_constants::kChromeFrameProductName;
  if (!CheckRegistryKey(dist->GetVersionKey())) {
    printf("%ls is not installed.\n", product_name.c_str());
    return;
  }

  MiniInstallerTestUtil::CloseProcesses(installer::kChromeExe);

  CommandLine cmd = InstallUtil::GetChromeUninstallCmd(
      system_install_, dist->GetType());

  if (cmd.GetProgram().empty()) {
    LOG(ERROR) << "Unable to get uninstall command.";
    CleanChromeInstall();
  }

  ASSERT_TRUE(file_util::PathExists(cmd.GetProgram()));

  cmd.AppendSwitch(installer::switches::kUninstall);
  cmd.AppendSwitch(installer::switches::kForceUninstall);
  cmd.AppendSwitch(installer::switches::kChromeFrame);

  if (system_install_)
    cmd.AppendSwitch(installer::switches::kSystemLevel);

  base::ProcessHandle setup_handle;
  base::LaunchProcess(cmd, base::LaunchOptions(), &setup_handle);

  ASSERT_TRUE(CloseUninstallWindow());
  ASSERT_TRUE(MiniInstallerTestUtil::VerifyProcessHandleClosed(setup_handle));
  ASSERT_FALSE(CheckRegistryKeyOnUninstall(dist->GetVersionKey()));

  DeleteUserDataFolder();
  MiniInstallerTestUtil::CloseProcesses(
      mini_installer_constants::kIEProcessName);
}

// Will clean up the machine if Chrome install is messed up.
void ChromeMiniInstaller::CleanChromeInstall() {
  DeletePvRegistryKey();
  FilePath install_path;
  ASSERT_TRUE(GetInstallDirectory(&install_path));
  ASSERT_TRUE(file_util::Delete(install_path, true));
}

bool ChromeMiniInstaller::CloseUninstallWindow() {
  HWND hndl = NULL;
  TimeDelta timer;
  const TimeDelta kDelay = TimeDelta::FromMilliseconds(200);
  std::wstring window_name;
  if (is_chrome_frame_)
    window_name = mini_installer_constants::kChromeFrameAppName;
  else
    window_name = mini_installer_constants::kChromeUninstallDialogName;
  while (hndl == NULL && timer < TimeDelta::FromSeconds(5)) {
    hndl = FindWindow(NULL, window_name.c_str());
    base::PlatformThread::Sleep(kDelay);
    timer += kDelay;
  }

  if (!is_chrome_frame_) {
    if (hndl == NULL)
      hndl = FindWindow(NULL, mini_installer_constants::kChromeBuildType);

    if (hndl == NULL)
      return false;

    SetForegroundWindow(hndl);
    MiniInstallerTestUtil::SendEnterKeyToWindow();
  }
  return true;
}

// Closes Chrome browser.
bool ChromeMiniInstaller::CloseChromeBrowser() {
  TimeDelta timer;
  const TimeDelta kShortDelay = TimeDelta::FromMilliseconds(100);
  const TimeDelta kLongDelay = TimeDelta::FromSeconds(1);
  HWND handle = NULL;
  // This loop iterates through all of the top-level Windows
  // named Chrome_WidgetWin_0 and closes them
  while ((base::GetProcessCount(installer::kChromeExe, NULL) > 0) &&
         (timer < TimeDelta::FromSeconds(40))) {
    // Chrome may have been launched, but the window may not have appeared
    // yet. Wait for it to appear for 10 seconds, but exit if it takes longer
    // than that.
    while (!handle && timer < TimeDelta::FromSeconds(10)) {
      handle = FindWindowEx(NULL, handle, L"Chrome_WidgetWin_0", NULL);
      if (!handle) {
        base::PlatformThread::Sleep(kShortDelay);
        timer += kShortDelay;
      }
    }
    if (!handle)
      return false;
    SetForegroundWindow(handle);
    LRESULT _result = SendMessage(handle, WM_CLOSE, 1, 0);
    if (_result != 0)
      return false;
    base::PlatformThread::Sleep(kLongDelay);
    timer += kLongDelay;
  }
  if (base::GetProcessCount(installer::kChromeExe, NULL) > 0) {
    printf("Chrome.exe is still running even after closing all windows\n");
    return false;
  }
  if (base::GetProcessCount(installer::kNaClExe, NULL) > 0) {
    printf("NaCl.exe is still running even after closing all windows\n");
    return false;
  }
  return true;
}

// Checks for Chrome registry keys.
bool ChromeMiniInstaller::CheckRegistryKey(const std::wstring& key_path) {
  RegKey key;
  LONG ret = key.Open(GetRootRegistryKey(), key_path.c_str(), KEY_ALL_ACCESS);
  if (ret != ERROR_SUCCESS) {
    printf("Cannot open reg key. error: %d\n", ret);
    return false;
  }
  std::string reg_key_value_returned;
  if (!GetChromeVersionFromRegistry(&reg_key_value_returned))
    return false;
  return true;
}

// Checks for Chrome registry keys on uninstall.
bool ChromeMiniInstaller::CheckRegistryKeyOnUninstall(
    const std::wstring& key_path) {
  RegKey key;
  TimeDelta timer;
  const TimeDelta kDelay = TimeDelta::FromMilliseconds(200);
  while ((key.Open(GetRootRegistryKey(), key_path.c_str(),
                   KEY_ALL_ACCESS) == ERROR_SUCCESS) &&
         (timer < TimeDelta::FromSeconds(20))) {
    base::PlatformThread::Sleep(kDelay);
    timer += kDelay;
  }
  return CheckRegistryKey(key_path);
}

// Will delete user data profile.
void ChromeMiniInstaller::DeleteUserDataFolder() {
  FilePath path = GetUserDataDirPath();
  if (file_util::PathExists(path))
    ASSERT_TRUE(file_util::Delete(path, true));
}

// Gets user data directory path
FilePath ChromeMiniInstaller::GetUserDataDirPath() {
  FilePath path;
  PathService::Get(base::DIR_LOCAL_APP_DATA, &path);
  FilePath profile_path = path;
  if (is_chrome_frame_) {
    profile_path = profile_path.Append(
        mini_installer_constants::kChromeFrameAppDir);
  } else {
    profile_path = profile_path.Append(
        mini_installer_constants::kChromeAppDir);
  }
  profile_path = profile_path.DirName();
  profile_path = profile_path.Append(
      mini_installer_constants::kChromeUserDataDir);
  return profile_path;
}

// Deletes pv key from Clients.
void ChromeMiniInstaller::DeletePvRegistryKey() {
  std::wstring pv_key(google_update::kRegPathClients);
  pv_key.append(L"\\");

  BrowserDistribution* dist = GetCurrentBrowserDistribution();
  pv_key.append(dist->GetAppGuid());

  RegKey key;
  if (key.Open(GetRootRegistryKey(), pv_key.c_str(), KEY_ALL_ACCESS) ==
      ERROR_SUCCESS)
    ASSERT_EQ(ERROR_SUCCESS, key.DeleteValue(L"pv"));
  printf("Deleted %ls key\n", pv_key.c_str());
}

// Verifies if Chrome shortcut exists.
void ChromeMiniInstaller::FindChromeShortcut() {
  bool return_val = false;
  FilePath uninstall_lnk;
  FilePath path = GetStartMenuShortcutPath();
  path = path.Append(mini_installer_constants::kChromeBuildType);
  // Verify if path exists.
  if (file_util::PathExists(path)) {
    return_val = true;
    uninstall_lnk = path;
    path = path.Append(mini_installer_constants::kChromeLaunchShortcut);
    uninstall_lnk = uninstall_lnk.Append(
        mini_installer_constants::kChromeUninstallShortcut);
    ASSERT_TRUE(file_util::PathExists(path));
    ASSERT_TRUE(file_util::PathExists(uninstall_lnk));
  }
  if (return_val) {
    LOG(INFO) << "Found Chrome shortcuts:\n"
              << path.value() << "\n"
              << uninstall_lnk.value();
  } else {
    LOG(INFO) << "No Chrome shortcuts found.";
  }
}

bool ChromeMiniInstaller::GetInstallDirectory(FilePath* path) {
  BrowserDistribution* dist = GetCurrentBrowserDistribution();
    *path = installer::GetChromeInstallPath(system_install_, dist);
  FilePath parent;
  if (system_install_) {
    PathService::Get(base::DIR_PROGRAM_FILES, &parent);
    return file_util::ContainsPath(parent, *path);
  } else {
    PathService::Get(base::DIR_LOCAL_APP_DATA, &parent);
    return file_util::ContainsPath(parent, *path);
  }
}

FilePath ChromeMiniInstaller::GetStartMenuShortcutPath() {
  FilePath path_name;
  if (system_install_)
    PathService::Get(base::DIR_COMMON_START_MENU, &path_name);
  else
    PathService::Get(base::DIR_START_MENU, &path_name);
  return path_name;
}

bool ChromeMiniInstaller::GetChromeVersionFromRegistry(std::string* value) {
  BrowserDistribution* dist = GetCurrentBrowserDistribution();
  scoped_ptr<Version> version(
      InstallUtil::GetChromeVersion(dist, system_install_));
  if (!version.get())
    return false;
  *value = version->GetString();
  return !value->empty();
}

// Get HKEY based on install type.
HKEY ChromeMiniInstaller::GetRootRegistryKey() {
  HKEY type = HKEY_CURRENT_USER;
  if (system_install_)
    type = HKEY_LOCAL_MACHINE;
  return type;
}

void ChromeMiniInstaller::RunInstaller(const CommandLine& command) {
  ASSERT_TRUE(file_util::PathExists(command.GetProgram()));
  CommandLine installer(command);
  if (system_install_) {
    installer.AppendSwitch(installer::switches::kSystemLevel);
  }

  LOG(INFO) << "Running installer command: "
      << installer.GetCommandLineString();
  base::ProcessHandle app_handle;
  ASSERT_TRUE(
      base::LaunchProcess(installer, base::LaunchOptions(), &app_handle))
      << "Installer failed.";
  ASSERT_TRUE(base::WaitForSingleProcess(app_handle, 60 * 1000))
      << "Installer did not complete.";
}

void ChromeMiniInstaller::LaunchChrome(bool kill) {
  MiniInstallerTestUtil::CloseProcesses(installer::kChromeExe);

  FilePath install_path;
  ASSERT_TRUE(GetInstallDirectory(&install_path));
  install_path = install_path.Append(installer::kChromeExe);
  CommandLine browser(install_path);

  FilePath exe = browser.GetProgram();
  LOG(INFO) << "Browser launch command: " << browser.GetCommandLineString();
  base::ProcessHandle chrome;
  ASSERT_TRUE(base::LaunchProcess(browser, base::LaunchOptions(), &chrome))
      << "Could not launch process: " << exe.value();

  if (kill) {
    ASSERT_TRUE(base::KillProcess(chrome, 0, true))
        << "Failed to kill chrome.exe";
  }
}

// Verifies Chrome/Chrome Frame install.
void ChromeMiniInstaller::VerifyInstall(bool over_install) {
  InstallationValidator::InstallationType type =
      installer::ExpectValidInstallation(system_install_);
  if (is_chrome_frame_) {
    EXPECT_NE(0,
              type & InstallationValidator::ProductBits::CHROME_FRAME_SINGLE);
  } else {
    EXPECT_NE(0, type & InstallationValidator::ProductBits::CHROME_SINGLE);
  }

  if (is_chrome_frame_)
    VerifyChromeFrameInstall();
  else if (!system_install_ && !over_install)
    MiniInstallerTestUtil::VerifyProcessLaunch(installer::kChromeExe, true);

  base::PlatformThread::Sleep(TimeDelta::FromMilliseconds(800));
  FindChromeShortcut();
  LaunchChrome(true);
}

// This method will verify if ChromeFrame installed successfully. It will
// launch IE with cf:about:version, then check if
// chrome.exe process got spawned.
void ChromeMiniInstaller::VerifyChromeFrameInstall() {
  // Launch IE
  LaunchIE(L"gcf:about:version");

  // Check if Chrome process got spawned.
  MiniInstallerTestUtil::VerifyProcessLaunch(installer::kChromeExe, true);
}

void ChromeMiniInstaller::LaunchIE(const std::wstring& navigate_url) {
  FilePath browser_path;
  PathService::Get(base::DIR_PROGRAM_FILES, &browser_path);
  browser_path = browser_path.Append(mini_installer_constants::kIELocation);
  browser_path = browser_path.Append(mini_installer_constants::kIEProcessName);

  CommandLine cmd_line(browser_path);
  cmd_line.AppendArgNative(navigate_url);
  ASSERT_TRUE(base::LaunchProcess(cmd_line, base::LaunchOptions(), NULL));
}

// This method compares the registry keys after overinstall.
bool ChromeMiniInstaller::VerifyOverInstall(
    const std::string& value_before_overinstall,
    const std::string& value_after_overinstall) {
  int64 reg_key_value_before_overinstall;
  base::StringToInt64(value_before_overinstall,
                      &reg_key_value_before_overinstall);
  int64 reg_key_value_after_overinstall;
  base::StringToInt64(value_after_overinstall,
                      &reg_key_value_after_overinstall);

  // Compare to see if the version is less.
  LOG(INFO) << "Reg Key value before overinstall is: "
            << value_before_overinstall;
  LOG(INFO) << "Reg Key value after overinstall is: "
            << value_after_overinstall;
  if (reg_key_value_before_overinstall > reg_key_value_after_overinstall) {
    LOG(ERROR) << "FAIL: Overinstalled a lower version of Chrome.";
    return false;
  }
  return true;
}

// This method will verify if the installed build is correct.
bool ChromeMiniInstaller::VerifyStandaloneInstall() {
  std::string reg_key_value_returned;
  if (!GetChromeVersionFromRegistry(&reg_key_value_returned))
    return false;
  if (current_build_.compare(reg_key_value_returned) == 0)
    return true;
  else
    return false;
}

BrowserDistribution* ChromeMiniInstaller::GetCurrentBrowserDistribution() {
    return BrowserDistribution::GetSpecificDistribution(
        is_chrome_frame_ ?
            BrowserDistribution::CHROME_FRAME :
            BrowserDistribution::CHROME_BROWSER);
}

bool ChromeMiniInstaller::GetFullInstaller(FilePath* path) {
  std::string full_installer_pattern("*_chrome_installer*");
  return GetInstaller(full_installer_pattern, path);
}

bool ChromeMiniInstaller::GetDiffInstaller(FilePath* path) {
  std::string diff_installer_pattern("*_from_*");
  bool retval = GetInstaller(diff_installer_pattern, path);
  if (retval)
    current_diff_build_ = path->DirName().DirName().BaseName().MaybeAsASCII();
  return retval;
}

bool ChromeMiniInstaller::GetMiniInstaller(FilePath* path) {
  // Use local copy of installer, else fall back to filer.
  FilePath mini_installer = MiniInstallerTestUtil::GetFilePath(
      mini_installer_constants::kChromeMiniInstallerExecutable);
  if (file_util::PathExists(mini_installer)) {
    *path = mini_installer;
    return true;
  }
  std::string mini_installer_pattern("mini_installer.exe");
  return GetInstaller(mini_installer_pattern, path);
}

bool ChromeMiniInstaller::GetPreviousInstaller(FilePath* path) {
  std::string diff_installer_pattern("*_from_*");
  std::string full_installer_pattern("*_chrome_installer*");
  FilePath diff_installer;
  if (!GetInstaller(diff_installer_pattern, &diff_installer))
    return false;

  FilePath previous_installer;
  std::vector<std::string> tokenized_name;
  Tokenize(diff_installer.BaseName().MaybeAsASCII(),
      "_", &tokenized_name);
  std::string build_pattern = base::StringPrintf(
      "*%s", tokenized_name[2].c_str());
  std::vector<FilePath> previous_build;
  if (FindMatchingFiles(diff_installer.DirName().DirName().DirName(),
      build_pattern, file_util::FileEnumerator::DIRECTORIES,
      &previous_build)) {
    FilePath windir = previous_build.at(0).Append(
        mini_installer_constants::kWinFolder);
    FindNewestMatchingFile(windir, full_installer_pattern,
        file_util::FileEnumerator::FILES, &previous_installer);
  }

  if (previous_installer.empty())
    return false;
  *path = previous_installer;
  return true;
}

bool ChromeMiniInstaller::GetStandaloneInstaller(FilePath* path) {
  // Get standalone installer.
  FilePath standalone_installer(
      mini_installer_constants::kChromeStandAloneInstallerLocation);

  // Get the file name.
  std::vector<std::string> tokenizedBuildNumber;
  Tokenize(current_build_, ".", &tokenizedBuildNumber);
  std::string standalone_installer_filename = base::StringPrintf(
      "%s%s_%s.exe",
      FilePath(mini_installer_constants::kUntaggedInstallerPattern)
          .MaybeAsASCII().c_str(),
      tokenizedBuildNumber[2].c_str(),
      tokenizedBuildNumber[3].c_str());
  standalone_installer = standalone_installer.AppendASCII(current_build_)
      .Append(mini_installer_constants::kWinFolder)
      .AppendASCII(standalone_installer_filename);
  *path = standalone_installer;
  return file_util::PathExists(standalone_installer);
}

bool ChromeMiniInstaller::GetInstaller(const std::string& pattern,
    FilePath* path) {
  FilePath installer;

  // Search directory where current exe is located.
  FilePath dir_exe;
  if (PathService::Get(base::DIR_EXE, &dir_exe) &&
      FindNewestMatchingFile(dir_exe, pattern,
          file_util::FileEnumerator::FILES, &installer)) {
    *path = installer;
    return true;
  }
  // Fall back to filer.
  FilePath root(mini_installer_constants::kChromeInstallersLocation);
  std::vector<FilePath> paths;
  if (!FindMatchingFiles(root, build_,
      file_util::FileEnumerator::DIRECTORIES, &paths)) {
    return false;
  }

  std::vector<FilePath>::const_iterator dir;
  for (dir = paths.begin(); dir != paths.end(); ++dir) {
    FilePath windir = dir->Append(
        mini_installer_constants::kWinFolder);
    if (FindNewestMatchingFile(windir, pattern,
            file_util::FileEnumerator::FILES, &installer)) {
      break;
    }
  }

  if (installer.empty()) {
    LOG(WARNING) << "Failed to find installer with pattern: " << pattern;
    return false;
  }

  *path = installer;
  return true;
}
