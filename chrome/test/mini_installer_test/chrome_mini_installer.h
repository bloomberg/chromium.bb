// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_MINI_INSTALLER_TEST_CHROME_MINI_INSTALLER_H_
#define CHROME_TEST_MINI_INSTALLER_TEST_CHROME_MINI_INSTALLER_H_
#pragma once

#include <windows.h>
#include <string>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/command_line.h"

// This class has methods to install and uninstall Chrome mini installer.
class ChromeMiniInstaller {
 public:
  ChromeMiniInstaller(bool system_install, bool is_chrome_frame);
  ~ChromeMiniInstaller() {}

  enum RepairChrome {
    REGISTRY,
    VERSION_FOLDER
  };

  // This method returns path to either program files
  // or documents and setting based on the install type.
  bool GetChromeInstallDirectoryLocation(FilePath* path);

  // Installs the latest full installer.
  void InstallFullInstaller(bool over_install);

  void InstallUsingMultiInstall();

  // Installs chrome.
  void Install();

  // This method will first install the full installer and
  // then over installs with diff installer. If |should_start_ie| is true,
  // start IE browser before launch installer, and leave the process running
  // through the installtions.
  void OverInstallOnFullInstaller(const std::wstring& install_type,
                                  bool should_start_ie);

  // Installs Google Chrome through meta installer.
  void InstallMetaInstaller();

  // Installs Chrome Mini Installer.
  void InstallMiniInstaller(bool over_install, const FilePath& path);

  // This will test the standalone installer.
  void InstallStandaloneInstaller();

  // Repairs Chrome based on the passed argument.
  void Repair(RepairChrome repair_type);

  // Uninstalls Chrome.
  void UnInstall();

  // This method uninstalls Chrome Frame without closing IE browser first.
  // IE browser should be running before this method is called.
  void UnInstallChromeFrameWithIERunning();

  // This method will perform a over install
  void OverInstall();

  void SetBuildUnderTest(const std::wstring& build);

 private:
  // Will clean up the machine if Chrome install is messed up.
  void CleanChromeInstall();

  // Closes Chrome uninstall confirm dialog window.
  bool CloseUninstallWindow();

  // Closes Chrome browser.
  bool CloseChromeBrowser();

  // Checks for registry key.
  bool CheckRegistryKey(const std::wstring& key_path);

  // Checks for registry key on uninstall.
  bool CheckRegistryKeyOnUninstall(const std::wstring& key_path);

  // Deletes specified folder after getting the install path.
  void DeleteFolder(const wchar_t* folder_name);

  // Will delete user data profile.
  void DeleteUserDataFolder();

  // This will delete pv key from client registry.
  void DeletePvRegistryKey();

  // This method verifies Chrome shortcut.
  void FindChromeShortcut();

  // Get HKEY based on install type.
  HKEY GetRootRegistryKey();

  // Returns Chrome pv registry key value.
  bool GetChromeVersionFromRegistry(std::wstring* reg_key_value);

  // This method gets the shortcut path from start menu based on install type.
  FilePath GetStartMenuShortcutPath();

  // Get user data directory path.
  FilePath GetUserDataDirPath();

  // Launch Chrome. Kill process if |kill| is true.
  void LaunchChrome(bool kill);

  // This method verifies if Chrome/Chrome Frame installed correctly.
  void VerifyInstall(bool over_install);

  // This method will verify if ChromeFrame got successfully installed on the
  // machine.
  void VerifyChromeFrameInstall();

  // Launch IE with |navigate_url|.
  void LaunchIE(const std::wstring& navigate_url);

  // Run installer using provided |command|.
  void RunInstaller(const CommandLine& command);

  // Compares the registry key values after overinstall.
  bool VerifyOverInstall(const std::wstring& reg_key_value_before_overinstall,
                         const std::wstring& reg_key_value_after_overinstall);

  // This method will verify if the installed build is correct.
  bool VerifyStandaloneInstall();

  // Get all the latest installers base on last modified date.
  bool LocateInstallers(const std::wstring& build);

  // This method will create a command line to run apply tag.
  CommandLine GetCommandForTagging();

  // If true install system level. Otherwise install user level.
  bool system_install_;

  bool is_chrome_frame_;

  FilePath full_installer_;
  FilePath diff_installer_;
  FilePath previous_installer_;
  FilePath standalone_installer_;
  FilePath mini_installer_;

  // Build numbers.
  std::wstring current_build_, previous_build_;

  DISALLOW_COPY_AND_ASSIGN(ChromeMiniInstaller);
};

#endif  // CHROME_TEST_MINI_INSTALLER_TEST_CHROME_MINI_INSTALLER_H_
