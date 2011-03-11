// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/win/registry.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/installation_state.h"
#include "chrome/installer/util/installer_state.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/master_preferences.h"
#include "chrome/installer/util/product_unittest.h"

using base::win::RegKey;
using installer::InstallationState;
using installer::InstallerState;
using installer::MasterPreferences;

class InstallUtilTest : public TestWithTempDirAndDeleteTempOverrideKeys {
 protected:
};

TEST_F(InstallUtilTest, InstallerResult) {
  const bool system_level = true;
  bool multi_install = false;
  HKEY root = system_level ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;

  RegKey key;
  std::wstring launch_cmd = L"hey diddle diddle";
  std::wstring value;

  // check results for a fresh install of single Chrome
  {
    TempRegKeyOverride override(root, L"root_inst_res");
    CommandLine cmd_line = CommandLine::FromString(L"setup.exe --system-level");
    const MasterPreferences prefs(cmd_line);
    InstallationState machine_state;
    machine_state.Initialize();
    InstallerState state;
    state.Initialize(cmd_line, prefs, machine_state);
    InstallUtil::WriteInstallerResult(system_level, state.state_key(),
        installer::FIRST_INSTALL_SUCCESS, 0, &launch_cmd);
    BrowserDistribution* distribution =
        BrowserDistribution::GetSpecificDistribution(
            BrowserDistribution::CHROME_BROWSER);
    EXPECT_EQ(ERROR_SUCCESS,
        key.Open(root, distribution->GetStateKey().c_str(), KEY_READ));
    EXPECT_EQ(ERROR_SUCCESS,
        key.ReadValue(installer::kInstallerSuccessLaunchCmdLine, &value));
    EXPECT_EQ(launch_cmd, value);
  }
  TempRegKeyOverride::DeleteAllTempKeys();

  // check results for a fresh install of multi Chrome
  {
    TempRegKeyOverride override(root, L"root_inst_res");
    CommandLine cmd_line = CommandLine::FromString(
        L"setup.exe --system-level --multi-install --chrome");
    const MasterPreferences prefs(cmd_line);
    InstallationState machine_state;
    machine_state.Initialize();
    InstallerState state;
    state.Initialize(cmd_line, prefs, machine_state);
    InstallUtil::WriteInstallerResult(system_level, state.state_key(),
        installer::FIRST_INSTALL_SUCCESS, 0, &launch_cmd);
    BrowserDistribution* distribution =
        BrowserDistribution::GetSpecificDistribution(
            BrowserDistribution::CHROME_BROWSER);
    EXPECT_EQ(ERROR_SUCCESS,
        key.Open(root, distribution->GetStateKey().c_str(), KEY_READ));
    EXPECT_EQ(ERROR_SUCCESS,
        key.ReadValue(installer::kInstallerSuccessLaunchCmdLine, &value));
    EXPECT_EQ(launch_cmd, value);
    key.Close();
  }
  TempRegKeyOverride::DeleteAllTempKeys();
}

TEST_F(InstallUtilTest, MakeUninstallCommand) {
  CommandLine command_line(CommandLine::NO_PROGRAM);

  std::pair<std::wstring, std::wstring> params[] = {
    std::make_pair(std::wstring(L""), std::wstring(L"")),
    std::make_pair(std::wstring(L""), std::wstring(L"--do-something --silly")),
    std::make_pair(std::wstring(L"spam.exe"), std::wstring(L"")),
    std::make_pair(std::wstring(L"spam.exe"),
                   std::wstring(L"--do-something --silly")),
  };
  for (int i = 0; i < arraysize(params); ++i) {
    std::pair<std::wstring, std::wstring>& param = params[i];
    InstallUtil::MakeUninstallCommand(param.first, param.second, &command_line);
    EXPECT_EQ(param.first, command_line.GetProgram().value());
    if (param.second.empty()) {
      EXPECT_EQ(0U, command_line.GetSwitchCount());
    } else {
      EXPECT_EQ(2U, command_line.GetSwitchCount());
      EXPECT_TRUE(command_line.HasSwitch("do-something"));
      EXPECT_TRUE(command_line.HasSwitch("silly"));
    }
  }
}

TEST_F(InstallUtilTest, GetCurrentDate) {
  std::wstring date(InstallUtil::GetCurrentDate());
  EXPECT_EQ(8, date.length());
  if (date.length() == 8) {
    // For an invalid date value, SystemTimeToFileTime will fail.
    // We use this to validate that we have a correct date string.
    SYSTEMTIME systime = {0};
    FILETIME ft = {0};
    // Just to make sure our assumption holds.
    EXPECT_FALSE(SystemTimeToFileTime(&systime, &ft));
    // Now fill in the values from our string.
    systime.wYear = _wtoi(date.substr(0, 4).c_str());
    systime.wMonth = _wtoi(date.substr(4, 2).c_str());
    systime.wDay = _wtoi(date.substr(6, 2).c_str());
    // Check if they make sense.
    EXPECT_TRUE(SystemTimeToFileTime(&systime, &ft));
  }
}

TEST_F(InstallUtilTest, UpdateInstallerStage) {
  const bool system_level = false;
  const HKEY root = system_level ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
  std::wstring state_key_path(L"PhonyClientState");

  // Update the stage when there's no "ap" value.
  {
    TempRegKeyOverride override(root, L"root_inst_res");
    RegKey(root, state_key_path.c_str(), KEY_SET_VALUE);
    InstallUtil::UpdateInstallerStage(system_level, state_key_path,
                                      installer::BUILDING);
    std::wstring value;
    EXPECT_EQ(ERROR_SUCCESS,
              RegKey(root, state_key_path.c_str(), KEY_QUERY_VALUE)
                  .ReadValue(google_update::kRegApField, &value));
    EXPECT_EQ(L"-stage:building", value);
  }
  TempRegKeyOverride::DeleteAllTempKeys();

  // Update the stage when there is an "ap" value.
  {
    TempRegKeyOverride override(root, L"root_inst_res");
    RegKey(root, state_key_path.c_str(), KEY_SET_VALUE)
        .WriteValue(google_update::kRegApField, L"2.0-dev");
    InstallUtil::UpdateInstallerStage(system_level, state_key_path,
                                      installer::BUILDING);
    std::wstring value;
    EXPECT_EQ(ERROR_SUCCESS,
              RegKey(root, state_key_path.c_str(), KEY_QUERY_VALUE)
                  .ReadValue(google_update::kRegApField, &value));
    EXPECT_EQ(L"2.0-dev-stage:building", value);
  }
  TempRegKeyOverride::DeleteAllTempKeys();

  // Clear the stage.
  {
    TempRegKeyOverride override(root, L"root_inst_res");
    RegKey(root, state_key_path.c_str(), KEY_SET_VALUE)
      .WriteValue(google_update::kRegApField, L"2.0-dev-stage:building");
    InstallUtil::UpdateInstallerStage(system_level, state_key_path,
                                      installer::NO_STAGE);
    std::wstring value;
    EXPECT_EQ(ERROR_SUCCESS,
              RegKey(root, state_key_path.c_str(), KEY_QUERY_VALUE)
                  .ReadValue(google_update::kRegApField, &value));
    EXPECT_EQ(L"2.0-dev", value);
  }
  TempRegKeyOverride::DeleteAllTempKeys();
}
