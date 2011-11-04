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
#include "chrome/installer/util/browser_distribution.h"

// This class has methods to install and uninstall Chrome mini installer.
class ChromeMiniInstaller {
 public:
  ChromeMiniInstaller(bool system_install,
      bool is_chrome_frame, const std::string& build);
  ~ChromeMiniInstaller() {}

  enum RepairChrome {
    REGISTRY,
    VERSION_FOLDER
  };

  // Get the location at which Chrome or Chrome Frame is installed.
  bool GetInstallDirectory(FilePath* path);

  // Get the base multi-install command.
  CommandLine GetBaseMultiInstallCommand();

  // Installs the latest full installer.
  void InstallFullInstaller(bool over_install);

  void InstallChromeUsingMultiInstall();
  void InstallChromeFrameUsingMultiInstall();
  void InstallChromeAndChromeFrame(bool ready_mode);

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

  // Launch Chrome, assert process started.
  // If |kill|, kill process after launch.
  void LaunchChrome(bool kill);

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

  // Will delete user data profile.
  void DeleteUserDataFolder();

  // This will delete pv key from client registry.
  void DeletePvRegistryKey();

  // This method verifies Chrome shortcut.
  void FindChromeShortcut();

  // Get HKEY based on install type.
  HKEY GetRootRegistryKey();

  // Returns Chrome pv registry key value.
  bool GetChromeVersionFromRegistry(std::string* value);

  // This method gets the shortcut path from start menu based on install type.
  FilePath GetStartMenuShortcutPath();

  // Get user data directory path.
  FilePath GetUserDataDirPath();

  // This method verifies if Chrome/Chrome Frame installed correctly.
  void VerifyInstall(bool over_install);

  // This method will verify if ChromeFrame got successfully installed on the
  // machine.
  void VerifyChromeFrameInstall();

  // Launch IE with |navigate_url|.
  void LaunchIE(const std::wstring& navigate_url);

  // Run installer with given |command|. If installer is
  // system level append "--system-level" flag.
  void RunInstaller(const CommandLine& command);

  // Compares the registry key values after overinstall.
  bool VerifyOverInstall(const std::string& reg_key_value_before_overinstall,
                         const std::string& reg_key_value_after_overinstall);

  // This method will verify if the installed build is correct.
  bool VerifyStandaloneInstall();

  // This method will create a command line to run apply tag.
  CommandLine GetCommandForTagging();

  bool GetFullInstaller(FilePath* path);
  bool GetDiffInstaller(FilePath* path);
  bool GetMiniInstaller(FilePath* path);
  bool GetPreviousInstaller(FilePath* path);
  bool GetStandaloneInstaller(FilePath* path);
  bool GetInstaller(const std::string& pattern, FilePath* path);

  // Get current browser distribution.
  BrowserDistribution* GetCurrentBrowserDistribution();

  // If true install system level. Otherwise install user level.
  bool system_install_;

  bool is_chrome_frame_;

  // Build under test.
  std::string build_;
  // Build numbers.
  std::string current_build_, current_diff_build_, previous_build_;

  DISALLOW_COPY_AND_ASSIGN(ChromeMiniInstaller);
};

#endif  // CHROME_TEST_MINI_INSTALLER_TEST_CHROME_MINI_INSTALLER_H_
