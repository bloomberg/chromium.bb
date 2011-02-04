// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/mini_installer_test/chrome_mini_installer.h"

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/process.h"
#include "base/process_util.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/threading/platform_thread.h"
#include "base/win/registry.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/test/mini_installer_test/mini_installer_test_constants.h"
#include "chrome/test/mini_installer_test/mini_installer_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::win::RegKey;

ChromeMiniInstaller::ChromeMiniInstaller(const std::wstring& install_type,
                                         bool is_chrome_frame)
    : is_chrome_frame_(is_chrome_frame),
      install_type_(install_type),
      has_diff_installer_(false),
      has_full_installer_(false),
      has_prev_installer_(false) {
  installer_name_ = StringPrintf(L"%ls (%ls)",
      mini_installer_constants::kChromeBuildType, install_type_.c_str());
}

void ChromeMiniInstaller::SetBuildUnderTest(const std::wstring& build) {
  // Locate the full, diff, and previous installers.
  const wchar_t * build_prefix;
  if (LowerCaseEqualsASCII(build, "dev"))
    build_prefix = mini_installer_constants::kDevChannelBuild;
  else if (LowerCaseEqualsASCII(build, "stable"))
    build_prefix = mini_installer_constants::kStableChannelBuild;
  else if (LowerCaseEqualsASCII(build, "latest"))
    build_prefix = L"";
  else
    build_prefix = build.c_str();

  // Do not fail here if cannot find the installer. Set the bool and allow
  // to fail in the particular test.
  has_full_installer_ = MiniInstallerTestUtil::GetInstaller(
      mini_installer_constants::kFullInstallerPattern,
      &full_installer_, build_prefix, is_chrome_frame_);
  has_diff_installer_ = MiniInstallerTestUtil::GetInstaller(
      mini_installer_constants::kDiffInstallerPattern,
      &diff_installer_, build_prefix, is_chrome_frame_);

  if (has_diff_installer_) {
    has_prev_installer_ = MiniInstallerTestUtil::GetPreviousFullInstaller(
        diff_installer_, &prev_installer_, is_chrome_frame_);
  }

  // Find the version names. The folder two-levels up from the installer
  // is named this.
  if (has_full_installer_) {
    FilePath folder = FilePath(full_installer_).DirName().DirName();
    curr_version_ = folder.BaseName().value();
  }
  if (has_prev_installer_) {
    FilePath folder = FilePath(prev_installer_).DirName().DirName();
    prev_version_ = folder.BaseName().value();
  }
}

// Installs Chrome.
void ChromeMiniInstaller::Install() {
  std::wstring installer_path = MiniInstallerTestUtil::GetFilePath(
      mini_installer_constants::kChromeMiniInstallerExecutable);
  InstallMiniInstaller(false, installer_path);
}

// This method will get the previous latest full installer from
// nightly location, install it and over install with specified install_type.
void ChromeMiniInstaller::OverInstallOnFullInstaller(
    const std::wstring& install_type) {
  ASSERT_TRUE(has_full_installer_ && has_diff_installer_ &&
    has_prev_installer_);

  InstallMiniInstaller(false, prev_installer_);

  std::wstring got_prev_version;
  GetChromeVersionFromRegistry(&got_prev_version);
  printf("\n\nPreparing to overinstall...\n");

  if (install_type == mini_installer_constants::kDiffInstall) {
    printf("\nOver installing with latest differential installer: %ls\n",
           diff_installer_.c_str());
    InstallMiniInstaller(true, diff_installer_);

  } else if (install_type == mini_installer_constants::kFullInstall) {
    printf("\nOver installing with latest full insatller: %ls\n",
           full_installer_.c_str());
    InstallMiniInstaller(true, full_installer_);
  }

  std::wstring got_curr_version;
  GetChromeVersionFromRegistry(&got_curr_version);

  if (got_prev_version == prev_version_ &&
      got_curr_version == curr_version_) {
    printf("\n The over install was successful. Here are the values:\n");
    printf("\n full installer value: %ls and diff installer value is %ls\n",
           prev_version_.c_str(), curr_version_.c_str());
  } else {
    printf("\n The over install was not successful. Here are the values:\n");
    printf("\n Expected full installer value: %ls and actual value is %ls\n",
           prev_version_.c_str(), got_prev_version.c_str());
    printf("\n Expected diff installer value: %ls and actual value is %ls\n",
           curr_version_.c_str(), got_curr_version.c_str());
    FAIL();
  }
}


// This method will get the latest full installer from nightly location
// and installs it.
void ChromeMiniInstaller::InstallFullInstaller(bool over_install) {
  ASSERT_TRUE(has_full_installer_);
  InstallMiniInstaller(over_install, full_installer_);
}

// Installs the Chrome mini-installer, checks the registry and shortcuts.
void ChromeMiniInstaller::InstallMiniInstaller(bool over_install,
                                               const std::wstring& path) {
  printf("\nChrome will be installed at %ls level\n", install_type_.c_str());
  printf("\nWill proceed with the test only if this path exists: %ls\n\n",
         path.c_str());

  FilePath exe_path(path);
  ASSERT_TRUE(file_util::PathExists(exe_path)) << path << " does not exist.";
  LaunchInstaller(path, exe_path.BaseName().value().c_str());
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  ASSERT_TRUE(CheckRegistryKey(dist->GetVersionKey())) << dist->GetVersionKey()
                                                       << " does not exist.";
  VerifyInstall(over_install);
}

// This method tests the standalone installer by verifying the steps listed at:
// https://sites.google.com/a/google.com/chrome-pmo/
// standalone-installers/testing-standalone-installers
// This method applies appropriate tags to standalone installer and deletes
// old installer before running the new tagged installer. It also verifies
// that the installed version is correct.
void ChromeMiniInstaller::InstallStandaloneInstaller() {
  file_util::Delete(mini_installer_constants::kStandaloneInstaller, true);
  std::wstring tag_installer_command;
  ASSERT_TRUE(MiniInstallerTestUtil::GetCommandForTagging(
      &tag_installer_command));
  base::LaunchApp(tag_installer_command, true, false, NULL);
  std::wstring installer_path = MiniInstallerTestUtil::GetFilePath(
      mini_installer_constants::kStandaloneInstaller);
  InstallMiniInstaller(false, installer_path);
  ASSERT_TRUE(VerifyStandaloneInstall());
  file_util::Delete(mini_installer_constants::kStandaloneInstaller, true);
}

// Installs chromesetup.exe, waits for the install to finish and then
// checks the registry and shortcuts.
void ChromeMiniInstaller::InstallMetaInstaller() {
  // Install Google Chrome through meta installer.
  LaunchInstaller(mini_installer_constants::kChromeMetaInstallerExe,
                  mini_installer_constants::kChromeSetupExecutable);
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
  LaunchAndCloseChrome(false);
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
  VerifyChromeLaunch(false);
  printf("\nInstalling Chrome again to see if it can be repaired\n\n");
  InstallFullInstaller(true);
  printf("Chrome repair successful.\n");
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
  std::wstring product_name;
  if (is_chrome_frame_)
    product_name = mini_installer_constants::kChromeFrameProductName;
  else
    product_name = mini_installer_constants::kChromeProductName;
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  if (!CheckRegistryKey(dist->GetVersionKey())) {
    printf("%ls is not installed.\n", product_name.c_str());
    return;
  }
  if (is_chrome_frame_) {
    MiniInstallerTestUtil::CloseProcesses(
        mini_installer_constants::kIEProcessName);
  } else {
    MiniInstallerTestUtil::CloseProcesses(installer::kNaClExe);
  }
  MiniInstallerTestUtil::CloseProcesses(installer::kChromeExe);
  std::wstring uninstall_path = GetUninstallPath();
  if (uninstall_path == L"") {
    printf("\n %ls install is in a weird state. Cleaning the machine...\n",
            product_name.c_str());
    CleanChromeInstall();
    return;
  }
  ASSERT_TRUE(file_util::PathExists(FilePath(uninstall_path)));
  std::wstring uninstall_args(L"\"");
  uninstall_args.append(uninstall_path);
  uninstall_args.append(L"\" --uninstall --force-uninstall");
  if (is_chrome_frame_)
    uninstall_args.append(L" --chrome-frame");
  if (install_type_ == mini_installer_constants::kSystemInstall)
    uninstall_args = uninstall_args + L" --system-level";

  base::ProcessHandle setup_handle;
  base::LaunchApp(uninstall_args, false, false, &setup_handle);

  if (is_chrome_frame_)
    ASSERT_TRUE(CloseUninstallWindow());
  ASSERT_TRUE(MiniInstallerTestUtil::VerifyProcessHandleClosed(setup_handle));
  ASSERT_FALSE(CheckRegistryKeyOnUninstall(dist->GetVersionKey()));

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
  FilePath install_path(GetChromeInstallDirectoryLocation());
  std::wstring temp_chrome_dir;
  if (is_chrome_frame_) {
    temp_chrome_dir = mini_installer_constants::kChromeFrameAppDir;
  } else {
    temp_chrome_dir = mini_installer_constants::kChromeAppDir;
  }

  if (wcscmp(folder_name, L"version_folder") == 0) {
    std::wstring build_number;
    GetChromeVersionFromRegistry(&build_number);
    temp_chrome_dir = temp_chrome_dir + build_number;
    install_path = install_path.Append(temp_chrome_dir);
  } else if (wcscmp(folder_name, temp_chrome_dir.c_str()) == 0) {
    install_path = install_path.Append(folder_name).StripTrailingSeparators();
  }
  printf("This path will be deleted: %ls\n", install_path.value().c_str());
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
    printf("Chrome shortcuts found are:\n%ls\n%ls\n\n",
           path.value().c_str(), uninstall_lnk.value().c_str());
  } else {
    printf("Chrome shortcuts not found\n\n");
  }
}

// This method returns path to either program files
// or documents and setting based on the install type.
std::wstring ChromeMiniInstaller::GetChromeInstallDirectoryLocation() {
  FilePath path;
  if (install_type_ == mini_installer_constants::kSystemInstall)
    PathService::Get(base::DIR_PROGRAM_FILES, &path);
  else
    PathService::Get(base::DIR_LOCAL_APP_DATA, &path);
  return path.value();
}

FilePath ChromeMiniInstaller::GetStartMenuShortcutPath() {
  FilePath path_name;
  if (install_type_ == mini_installer_constants::kSystemInstall)
    PathService::Get(base::DIR_COMMON_START_MENU, &path_name);
  else
    PathService::Get(base::DIR_START_MENU, &path_name);
  return path_name;
}

// Gets the path for uninstall.
std::wstring ChromeMiniInstaller::GetUninstallPath() {
  std::wstring path, reg_key_value;
  if (!GetChromeVersionFromRegistry(&reg_key_value))
    return std::wstring();
  path = GetChromeInstallDirectoryLocation();
  if (is_chrome_frame_) {
    file_util::AppendToPath(&path,
        mini_installer_constants::kChromeFrameAppDir);
  } else {
    file_util::AppendToPath(&path, mini_installer_constants::kChromeAppDir);
  }
  file_util::AppendToPath(&path, reg_key_value);
  file_util::AppendToPath(&path, installer::kInstallerDir);
  file_util::AppendToPath(&path,
      mini_installer_constants::kChromeSetupExecutable);
  if (!file_util::PathExists(FilePath(path))) {
    printf("This uninstall path is not correct %ls. Will not proceed further",
           path.c_str());
    return L"";
  }
  printf("uninstall path is %ls\n", path.c_str());
  return path;
}

// Returns Chrome pv registry key value
bool ChromeMiniInstaller::GetChromeVersionFromRegistry(
    std::wstring* build_key_value) {
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  RegKey key(GetRootRegistryKey(), dist->GetVersionKey().c_str(), KEY_READ);
  LONG result = key.ReadValue(L"pv", build_key_value);
  if (result != ERROR_SUCCESS) {
    printf("registry read for chrome version failed. error: %d\n", result);
    return false;
  }
  printf("Build key value is %ls\n\n", build_key_value->c_str());
  return true;
}

// Get HKEY based on install type.
HKEY ChromeMiniInstaller::GetRootRegistryKey() {
  HKEY type = HKEY_CURRENT_USER;
  if (install_type_ == mini_installer_constants::kSystemInstall)
    type = HKEY_LOCAL_MACHINE;
  return type;
}

// Launches the chrome installer and waits for it to end.
void ChromeMiniInstaller::LaunchInstaller(const std::wstring& path,
                                          const wchar_t* process_name) {
  ASSERT_TRUE(file_util::PathExists(FilePath(path)));
  std::wstring launch_args(L" --verbose-logging");
  if (is_chrome_frame_) {
    launch_args.append(L" --do-not-create-shortcuts");
    launch_args.append(L" --do-not-launch-chrome");
    launch_args.append(L" --do-not-register-for-update-launch");
    launch_args.append(L" --chrome-frame");
  }
  if (install_type_ == mini_installer_constants::kSystemInstall) {
    launch_args.append(L" --system-level");
  }

  base::ProcessHandle app_handle;
  base::LaunchApp(L"\"" + path + L"\"" + launch_args, false, false,
                  &app_handle);

  printf("Waiting while this process is running  %ls ....\n", process_name);
  MiniInstallerTestUtil::VerifyProcessLaunch(process_name, true);
  ASSERT_TRUE(MiniInstallerTestUtil::VerifyProcessHandleClosed(app_handle));
}

// Gets the path to launch Chrome.
bool ChromeMiniInstaller::GetChromeLaunchPath(std::wstring* launch_path) {
  std::wstring path;
  path = GetChromeInstallDirectoryLocation();
  file_util::AppendToPath(&path, mini_installer_constants::kChromeAppDir);
  file_util::AppendToPath(&path, installer::kChromeExe);
  launch_path->assign(path);
  return file_util::PathExists(FilePath(path));
}

// Launch Chrome to see if it works after overinstall. Then close it.
void ChromeMiniInstaller::LaunchAndCloseChrome(bool over_install) {
  VerifyChromeLaunch(true);
  if ((install_type_ == mini_installer_constants::kSystemInstall) &&
      (!over_install)) {
    MiniInstallerTestUtil::VerifyProcessLaunch(
        installer::kChromeExe, true);
  }
  MiniInstallerTestUtil::CloseProcesses(installer::kChromeExe);
}

// This method will get Chrome exe path and launch it.
void ChromeMiniInstaller::VerifyChromeLaunch(bool expected_status) {
  std::wstring launch_path;
  GetChromeLaunchPath(&launch_path);
  LaunchBrowser(launch_path, L"", installer::kChromeExe, expected_status);
}

// Verifies Chrome/Chrome Frame install.
void ChromeMiniInstaller::VerifyInstall(bool over_install) {
  if (is_chrome_frame_) {
    VerifyChromeFrameInstall();
  } else {
    if ((install_type_ == mini_installer_constants::kUserInstall) &&
        (!over_install)) {
      MiniInstallerTestUtil::VerifyProcessLaunch(
          installer::kChromeExe, true);
    }
    base::PlatformThread::Sleep(800);
    FindChromeShortcut();
    LaunchAndCloseChrome(over_install);
  }
}

// This method will verify if ChromeFrame installed successfully. It will
// launch IE with cf:about:version, then check if
// chrome.exe process got spawned.
void ChromeMiniInstaller::VerifyChromeFrameInstall() {
  FilePath browser_path;
  PathService::Get(base::DIR_PROGRAM_FILES, &browser_path);
  browser_path = browser_path.Append(mini_installer_constants::kIELocation);
  browser_path = browser_path.Append(mini_installer_constants::kIEProcessName);

  CommandLine cmd_line(browser_path);
  cmd_line.AppendArgNative(L"gcf:about:version");

  // Launch IE
  base::LaunchApp(cmd_line, false, false, NULL);

  // Check if Chrome process got spawned.
  MiniInstallerTestUtil::VerifyProcessLaunch(installer::kChromeExe, true);

  // Verify if IExplore folder got created
  FilePath path = GetUserDataDirPath();
  path = path.AppendASCII("IEXPLORE");
  if (!file_util::PathExists(path)) {
    // The profile folder for IE 6 and 7 lives in the user's Temporary Internet
    // Files folder. Check there if the previous one was not found.
    PathService::Get(base::DIR_IE_INTERNET_CACHE, &path);
    path = path.AppendASCII("Google Chrome Frame");
  }

  ASSERT_TRUE(file_util::PathExists(path));
}

// This method will launch any requested browser.
void ChromeMiniInstaller::LaunchBrowser(const std::wstring& launch_path,
                                        const std::wstring& launch_args,
                                        const std::wstring& process_name,
                                        bool expected_status) {
  base::LaunchApp(L"\"" + launch_path + L"\"" + L" " + launch_args,
      false, false, NULL);
  base::PlatformThread::Sleep(1000);
  MiniInstallerTestUtil::VerifyProcessLaunch(process_name.c_str(),
                                             expected_status);
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
  std::wstring reg_key_value_returned, standalone_installer_version;
  MiniInstallerTestUtil::GetStandaloneVersion(&standalone_installer_version);
  GetChromeVersionFromRegistry(&reg_key_value_returned);
  if (standalone_installer_version.compare(reg_key_value_returned) == 0)
    return true;
  else
    return false;
}
