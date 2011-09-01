// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/process_util.h"
#include "base/test/test_suite.h"
#include "base/win/windows_version.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/installer/util/util_constants.h"
#include "chrome/test/mini_installer_test/mini_installer_test_constants.h"
#include "chrome_mini_installer.h"

void BackUpProfile(bool chrome_frame) {
  if (base::GetProcessCount(L"chrome.exe", NULL) > 0) {
    printf("Chrome is currently running and cannot backup the profile."
           "Please close Chrome and run the tests again.\n");
    exit(1);
  }
  ChromeMiniInstaller installer(mini_installer_constants::kUserInstall,
                                chrome_frame);
  FilePath path =
      FilePath::FromWStringHack(installer.GetChromeInstallDirectoryLocation());
  path = path.Append(mini_installer_constants::kChromeAppDir).DirName();
  FilePath backup_path = path;
  // Will hold User Data path that needs to be backed-up.
  path = path.Append(mini_installer_constants::kChromeUserDataDir);
  // Will hold new backup path to save the profile.
  backup_path = backup_path.Append(
      mini_installer_constants::kChromeUserDataBackupDir);
  // Will check if User Data profile is available.
  if (file_util::PathExists(path)) {
    // Will check if User Data is already backed up.
    // If yes, will delete and create new one.
    if (file_util::PathExists(backup_path))
      file_util::Delete(backup_path, true);
    file_util::CopyDirectory(path, backup_path, true);
  } else {
    printf("Chrome is not installed. Will not take any backup\n");
  }
}

int main(int argc, char** argv) {
  // Check command line to decide if the tests should continue
  // with cleaning the system or make a backup before continuing.
  CommandLine::Init(argc, argv);
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  base::TestSuite test_suite(argc, argv);

  if (command_line.HasSwitch(switches::kInstallerHelp)) {
    printf("This test needs command line arguments.\n");
    printf("Usage: %ls [-backup] [-build <version>] [-force] \n",
           command_line.GetProgram().value().c_str());
    printf("-backup arg will make a copy of User Data before uninstalling"
           " your chrome at all levels. The copy will be named as"
           " User Data Copy.\n"
           "-build specifies the build to be tested, e.g., 3.0.195.24."
           " Specifying 'dev' or 'stable' will use the latest build from that"
           " channel. 'latest', the default, will use the latest build.\n"
           "-force allows these tests to be run on the current platform,"
           " regardless of whether it is supported.\n");
    return 1;
  }

  if (command_line.HasSwitch(switches::kInstallerTestBackup)) {
    BackUpProfile(command_line.HasSwitch(
        installer::switches::kChromeFrame));
  }

  if (base::win::GetVersion() < base::win::VERSION_VISTA ||
      command_line.HasSwitch(switches::kInstallerTestForce)) {
    return test_suite.Run();
  }
  printf("These tests don't run on this platform.\n");
  return 0;
}
