// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/helper.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/installation_validation_helper.h"
#include "chrome/installer/util/util_constants.h"
#include "chrome/test/base/chrome_process_util.h"
#include "chrome/test/mini_installer_test/mini_installer_test_constants.h"
#include "chrome/test/mini_installer_test/mini_installer_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

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
    const FilePath::StringType& pattern,
    file_util::FileEnumerator::FileType type,
    std::vector<FilePath>* paths) {
  file_util::FileEnumerator files(root, false, type, pattern);
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
    const FilePath::StringType& pattern,
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
                                         bool is_chrome_frame)
    : is_chrome_frame_(is_chrome_frame),
      system_install_(system_install) {}

void ChromeMiniInstaller::SetBuildUnderTest(
    const std::wstring& build) {
  if (!LocateInstallers(build)) {
    LOG(WARNING) << "Could not find one or more installers.";
  }
}

// Installs Chrome.
void ChromeMiniInstaller::Install() {
  InstallMiniInstaller(false, mini_installer_);
}

// This method will get the previous latest full installer from
// nightly location, install it and over install with specified install_type.
void ChromeMiniInstaller::OverInstallOnFullInstaller(
    const std::wstring& install_type, bool should_start_ie) {
  ASSERT_TRUE(!full_installer_.empty() && !diff_installer_.empty() &&
              !previous_installer_.empty());

  if (should_start_ie)
    LaunchIE(L"http://www.google.com");

  InstallMiniInstaller(false, previous_installer_);

  std::wstring got_prev_version;
  GetChromeVersionFromRegistry(&got_prev_version);
  printf("\n\nPreparing to overinstall...\n");

  if (install_type == mini_installer_constants::kDiffInstall) {
    printf("\nOver installing with latest differential installer: %ls\n",
           diff_installer_.value());
    InstallMiniInstaller(true, diff_installer_);

  } else if (install_type == mini_installer_constants::kFullInstall) {
    printf("\nOver installing with latest full insatller: %ls\n",
           full_installer_.value());
    InstallMiniInstaller(true, full_installer_);
  }

  std::wstring got_curr_version;
  GetChromeVersionFromRegistry(&got_curr_version);

  if (got_prev_version == previous_build_ &&
      got_curr_version == current_build_) {
    LOG(INFO) << "The over install was successful.\n"
              << "Full installer: " << previous_build_;
    LOG(INFO) << "Diff installer: " << current_build_;
  } else {
    LOG(INFO) << "The over install was not successful.\n"
              << "Expected full installer value: " << previous_build_;
    LOG(INFO) << "Actual value is: " << got_prev_version;
    LOG(INFO) << "Expected diff: " << current_build_
              << "Actual value is: " << got_curr_version;
    FAIL();
  }
}

// This method will get the latest full installer from nightly location
// and installs it.
void ChromeMiniInstaller::InstallFullInstaller(bool over_install) {
  if (!full_installer_.empty())
    InstallMiniInstaller(over_install, full_installer_);
}

// Installs the Chrome mini-installer, checks the registry and shortcuts.
void ChromeMiniInstaller::InstallMiniInstaller(bool over_install,
                                               const FilePath& path) {
  LOG(INFO) << "Install level is: "
      << (system_install_ ? "system" : "user");
  RunInstaller(CommandLine(path));

  std::wstring version;
  ASSERT_TRUE(GetChromeVersionFromRegistry(&version))
      << "Install failed: unable to get version.";
  VerifyInstall(over_install);
}

void ChromeMiniInstaller::InstallUsingMultiInstall() {
  ASSERT_TRUE(file_util::PathExists(mini_installer_));
  CommandLine cmd(mini_installer_);
  cmd.AppendSwitch(installer::switches::kMultiInstall);
  cmd.AppendSwitch(installer::switches::kChrome);
  RunInstaller(cmd);

  // Verify installation.
  InstallationValidator::InstallationType type =
      installer::ExpectValidInstallation(system_install_);
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  ASSERT_TRUE(InstallUtil::IsMultiInstall(dist, system_install_));
  if (is_chrome_frame_) {
    EXPECT_TRUE(type & InstallationValidator::ProductBits::CHROME_FRAME_MULTI);
    VerifyChromeFrameInstall();
  } else {
    EXPECT_TRUE(type & InstallationValidator::ProductBits::CHROME_MULTI);
  }
  FindChromeShortcut();
  LaunchChrome(false);
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
  if (standalone_installer_.empty())
    return CommandLine::FromString(L"");
  CommandLine command = CommandLine::FromString(
      base::StringPrintf(L"%ls %ls %ls %ls",
      mini_installer_constants::kChromeApplyTagExe,
      standalone_installer_.value().c_str(),
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
  RunInstaller(installer);
  ASSERT_TRUE(MiniInstallerTestUtil::VerifyProcessClose(
      mini_installer_constants::kChromeMetaInstallerExecutable));

  std::wstring chrome_google_update_state_key(
      google_update::kRegPathClients);
  chrome_google_update_state_key.append(L"\\");

  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
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
  std::wstring reg_key_value_returned;
  // gets the registry key value before overinstall.
  GetChromeVersionFromRegistry(&reg_key_value_returned);
  printf("\n\nPreparing to overinstall...\n");
  InstallFullInstaller(true);
  std::wstring reg_key_value_after_overinstall;
  // Get the registry key value after over install
  GetChromeVersionFromRegistry(&reg_key_value_after_overinstall);
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
    DeleteFolder(L"version_folder");
    printf("Deleted folder. Now trying to launch chrome\n");
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
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  std::wstring version;
  std::string result;

  if (!GetChromeVersionFromRegistry(&version))
    return;

  // Close running products.
  if (is_chrome_frame_) {
    MiniInstallerTestUtil::CloseProcesses(
        mini_installer_constants::kIEProcessName);
  } else {
    MiniInstallerTestUtil::CloseProcesses(installer::kNaClExe);
  }
  MiniInstallerTestUtil::CloseProcesses(installer::kChromeExe);
  // Get uninstall command.
  CommandLine cmd = InstallUtil::GetChromeUninstallCmd(
      system_install_, dist->GetType());

  LOG(INFO) << "Uninstall command: " << cmd.GetCommandLineString();
  ASSERT_TRUE(file_util::PathExists(cmd.GetProgram()))
      << "Uninstall executable does not exist.";

  cmd.AppendSwitch(installer::switches::kUninstall);
  cmd.AppendSwitch(installer::switches::kForceUninstall);
  if (is_chrome_frame_)
    cmd.AppendSwitch(installer::switches::kChromeFrame);
  if (system_install_)
    cmd.AppendSwitch(installer::switches::kSystemLevel);

  base::ProcessHandle setup_handle;
  ASSERT_TRUE(base::LaunchProcess(cmd, base::LaunchOptions(), &setup_handle))
      << "Failed to launch uninstall command.";

  if (is_chrome_frame_)
    ASSERT_TRUE(CloseUninstallWindow());
  ASSERT_TRUE(MiniInstallerTestUtil::VerifyProcessHandleClosed(setup_handle));
  ASSERT_FALSE(CheckRegistryKeyOnUninstall(dist->GetVersionKey()))
      << "It appears Chrome was not completely uninstalled.";

  DeleteUserDataFolder();
  // Close IE survey window that gets launched on uninstall.
  if (!is_chrome_frame_) {
    FindChromeShortcut();
    MiniInstallerTestUtil::CloseProcesses(
        mini_installer_constants::kIEExecutable);
    ASSERT_EQ(0,
        base::GetProcessCount(mini_installer_constants::kIEExecutable, NULL));
  }
}

void ChromeMiniInstaller::UnInstallChromeFrameWithIERunning() {
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
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
  DeleteFolder(mini_installer_constants::kChromeAppDir);
}

bool ChromeMiniInstaller::CloseUninstallWindow() {
  HWND hndl = NULL;
  int timer = 0;
  std::wstring window_name;
  if (is_chrome_frame_)
    window_name = mini_installer_constants::kChromeFrameAppName;
  else
    window_name = mini_installer_constants::kChromeUninstallDialogName;
  while (hndl == NULL && timer < 5000) {
    hndl = FindWindow(NULL, window_name.c_str());
    base::PlatformThread::Sleep(200);
    timer = timer + 200;
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
  int timer = 0;
  HWND handle = NULL;
  // This loop iterates through all of the top-level Windows
  // named Chrome_WidgetWin_0 and closes them
  while ((base::GetProcessCount(installer::kChromeExe, NULL) > 0) &&
         (timer < 40000)) {
    // Chrome may have been launched, but the window may not have appeared
    // yet. Wait for it to appear for 10 seconds, but exit if it takes longer
    // than that.
    while (!handle && timer < 10000) {
      handle = FindWindowEx(NULL, handle, L"Chrome_WidgetWin_0", NULL);
      if (!handle) {
        base::PlatformThread::Sleep(100);
        timer = timer + 100;
      }
    }
    if (!handle)
      return false;
    SetForegroundWindow(handle);
    LRESULT _result = SendMessage(handle, WM_CLOSE, 1, 0);
    if (_result != 0)
      return false;
    base::PlatformThread::Sleep(1000);
    timer = timer + 1000;
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
  std::wstring reg_key_value_returned;
  if (!GetChromeVersionFromRegistry(&reg_key_value_returned))
    return false;
  return true;
}

// Checks for Chrome registry keys on uninstall.
bool ChromeMiniInstaller::CheckRegistryKeyOnUninstall(
    const std::wstring& key_path) {
  RegKey key;
  int timer = 0;
  while ((key.Open(GetRootRegistryKey(), key_path.c_str(),
                   KEY_ALL_ACCESS) == ERROR_SUCCESS) &&
         (timer < 20000)) {
    base::PlatformThread::Sleep(200);
    timer = timer + 200;
  }
  return CheckRegistryKey(key_path);
}

// Deletes Installer folder from Applications directory.
void ChromeMiniInstaller::DeleteFolder(const wchar_t* folder_name) {
  FilePath install_path;
  ASSERT_TRUE(GetChromeInstallDirectoryLocation(&install_path));
  std::wstring temp_chrome_dir;
  if (is_chrome_frame_) {
    temp_chrome_dir = mini_installer_constants::kChromeFrameAppDir;
  }

  if (wcscmp(folder_name, L"version_folder") == 0) {
    std::wstring build_number;
    GetChromeVersionFromRegistry(&build_number);
    temp_chrome_dir = temp_chrome_dir + build_number;
    install_path = install_path.Append(temp_chrome_dir);
  } else if (wcscmp(folder_name, temp_chrome_dir.c_str()) == 0) {
    install_path = install_path.Append(folder_name).StripTrailingSeparators();
  }
  ASSERT_TRUE(file_util::PathExists(install_path))
      << "Could not find path to delete:" << install_path.value();
  LOG(INFO) << "This path will be deleted: " << install_path.value();
  ASSERT_TRUE(file_util::Delete(install_path, true));
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

  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
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

bool ChromeMiniInstaller::GetChromeInstallDirectoryLocation(FilePath* path) {
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
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

// Returns Chrome pv registry key value
bool ChromeMiniInstaller::GetChromeVersionFromRegistry(
    std::wstring* build_key_value) {
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  RegKey key(GetRootRegistryKey(), dist->GetVersionKey().c_str(), KEY_READ);
  LONG result = key.ReadValue(L"pv", build_key_value);
  if (result != ERROR_SUCCESS) {
    LOG(WARNING) << "Registry read for Chrome version error: " << result;
    return false;
  }
  LOG(INFO) << "Build key value is " << build_key_value;
  return true;
}

// Get HKEY based on install type.
HKEY ChromeMiniInstaller::GetRootRegistryKey() {
  HKEY type = HKEY_CURRENT_USER;
  if (system_install_)
    type = HKEY_LOCAL_MACHINE;
  return type;
}

// Launches the chrome installer and waits for it to end.
void ChromeMiniInstaller::RunInstaller(const CommandLine& command) {
  ASSERT_TRUE(file_util::PathExists(command.GetProgram()));
  CommandLine installer(command);
  if (is_chrome_frame_) {
    installer.AppendSwitch(installer::switches::kDoNotCreateShortcuts);
    installer.AppendSwitch(installer::switches::kDoNotLaunchChrome);
    installer.AppendSwitch(installer::switches::kDoNotRegisterForUpdateLaunch);
    installer.AppendSwitch(installer::switches::kChromeFrame);
  }
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
  ASSERT_TRUE(GetChromeInstallDirectoryLocation(&install_path));
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

  base::PlatformThread::Sleep(800);
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

  base::LaunchProcess(cmd_line, base::LaunchOptions(), NULL);
}

// This method compares the registry keys after overinstall.
bool ChromeMiniInstaller::VerifyOverInstall(
    const std::wstring& value_before_overinstall,
    const std::wstring& value_after_overinstall) {
  int64 reg_key_value_before_overinstall;
  base::StringToInt64(value_before_overinstall,
                      &reg_key_value_before_overinstall);
  int64 reg_key_value_after_overinstall;
  base::StringToInt64(value_after_overinstall,
                      &reg_key_value_after_overinstall);

  // Compare to see if the version is less.
  printf("Reg Key value before overinstall is%ls\n",
          value_before_overinstall.c_str());
  printf("Reg Key value after overinstall is%ls\n",
         value_after_overinstall.c_str());
  if (reg_key_value_before_overinstall > reg_key_value_after_overinstall) {
    printf("FAIL: Overinstalled a lower version of Chrome\n");
    return false;
  }
  return true;
}

// This method will verify if the installed build is correct.
bool ChromeMiniInstaller::VerifyStandaloneInstall() {
  std::wstring reg_key_value_returned;
  GetChromeVersionFromRegistry(&reg_key_value_returned);
  if (current_build_.compare(reg_key_value_returned) == 0)
    return true;
  else
    return false;
}

// Search all the specified |build| directory to find the latest
// diff and full installers. |build| can be empty.
bool ChromeMiniInstaller::LocateInstallers(
    const std::wstring& build) {
  FilePath::StringType full_installer_pattern =
      FILE_PATH_LITERAL("*_chrome_installer*");
  FilePath::StringType diff_installer_pattern =
      FILE_PATH_LITERAL("*_from_*");
  FilePath::StringType mini_installer_pattern =
      FILE_PATH_LITERAL("mini_installer.exe");
  FilePath root(mini_installer_constants::kChromeInstallersLocation);
  std::vector<FilePath> paths;
  if (!FindMatchingFiles(root, build,
      file_util::FileEnumerator::DIRECTORIES, &paths)) {
    return false;
  }

  // Find full and diff installers;
  std::vector<FilePath>::const_iterator dir;
  for (dir = paths.begin(); dir != paths.end(); ++dir) {
    FilePath windir = dir->Append(
        mini_installer_constants::kWinFolder);
    if (FindNewestMatchingFile(windir, full_installer_pattern,
            file_util::FileEnumerator::FILES, &full_installer_) &&
        FindNewestMatchingFile(windir, diff_installer_pattern,
            file_util::FileEnumerator::FILES, &diff_installer_) &&
        FindNewestMatchingFile(windir, mini_installer_pattern,
            file_util::FileEnumerator::FILES, &mini_installer_)) {
      break;
    }
  }

  if (full_installer_.empty() || diff_installer_.empty())
    return false;

  current_build_ =
      full_installer_.DirName().DirName().BaseName().value();

  // Find previous full installer.
  std::vector<std::wstring> tokenized_name;
  Tokenize(diff_installer_.BaseName().value(),
      L"_", &tokenized_name);
  std::wstring build_pattern = base::StringPrintf(
      L"*%ls", tokenized_name[2].c_str());
  std::vector<FilePath> previous_build;
  if (FindMatchingFiles(diff_installer_.DirName().DirName().DirName(),
      build_pattern, file_util::FileEnumerator::DIRECTORIES,
      &previous_build)) {
    FilePath windir = previous_build.at(0).Append(
        mini_installer_constants::kWinFolder);
    FindNewestMatchingFile(windir, full_installer_pattern,
        file_util::FileEnumerator::FILES, &previous_installer_);
  }

  if (previous_installer_.empty())
    return false;
  previous_build_ =
        previous_installer_.DirName().DirName().BaseName().value();

  // Get standalone installer.
  FilePath standalone_installer(
      mini_installer_constants::kChromeStandAloneInstallerLocation);

  // Get the file name.
  std::vector<std::wstring> tokenizedBuildNumber;
  Tokenize(current_build_, L".", &tokenizedBuildNumber);
  std::wstring standalone_installer_filename = base::StringPrintf(
      L"%ls%ls_%ls.exe", mini_installer_constants::kUntaggedInstallerPattern,
          tokenizedBuildNumber[2].c_str(), tokenizedBuildNumber[3].c_str());
  standalone_installer = standalone_installer.Append(current_build_)
      .Append(mini_installer_constants::kWinFolder)
      .Append(standalone_installer_filename);

  standalone_installer_ = standalone_installer;

  return !standalone_installer_.empty();
}
