// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/tools/disable_outdated_build_detector/disable_outdated_build_detector.h"

#include "base/command_line.h"
#include "base/environment.h"
#include "base/strings/string16.h"
#include "base/win/registry.h"
#include "chrome/tools/disable_outdated_build_detector/google_update_integration.h"

namespace {

// Returns true if the tool is to operate on a system-level Chrome on the basis
// of either the --system-level command line switch or the GoogleUpdateIsMachine
// environment variable.
bool IsSystemLevel(const base::CommandLine& command_line) {
  if (command_line.HasSwitch(switches::kSystemLevel))
    return true;

  std::string is_machine;
  return base::Environment::Create()->GetVar(env::kGoogleUpdateIsMachine,
                                             &is_machine) &&
         is_machine.size() == 1 && is_machine[0] == '1';
}

// Returns true if Chrome is multi-install.
bool IsChromeMultiInstall(bool system_level) {
  base::win::RegKey key;

  LONG result = OpenClientStateKey(system_level, kChromeAppGuid, &key);
  if (result != ERROR_SUCCESS)
    return false;
  base::string16 uninstall_arguments;
  result = key.ReadValue(kUninstallArguments, &uninstall_arguments);
  if (result != ERROR_SUCCESS)
    return false;
  base::CommandLine command_line =
      base::CommandLine::FromString(L"\"foo\" " + uninstall_arguments);
  return command_line.HasSwitch(switches::kMultiInstall);
}

// Disables the outdated build detector for |app_guid|. On failures, |detail|
// will be populated with a Windows error code corresponding to the failure
// mode. Returns the exit code for the operation.
ExitCode DisableForApp(bool system_level,
                       const wchar_t* app_guid,
                       uint32_t* detail) {
  base::win::RegKey key;

  *detail = OpenClientStateKey(system_level, app_guid, &key);
  if (*detail == ERROR_FILE_NOT_FOUND)
    return ExitCode::NO_CHROME;
  if (*detail != ERROR_SUCCESS)
    return ExitCode::FAILED_OPENING_KEY;

  base::string16 brand;
  *detail = key.ReadValue(kBrand, &brand);
  if (*detail != ERROR_SUCCESS && *detail != ERROR_FILE_NOT_FOUND)
    return ExitCode::FAILED_READING_BRAND;

  if (!IsOrganic(brand))
    return ExitCode::NON_ORGANIC_BRAND;

  *detail = key.WriteValue(kBrand, kAOHY);
  if (*detail != ERROR_SUCCESS)
    return ExitCode::FAILED_WRITING_BRAND;

  return ExitCode::CHROME_BRAND_UPDATED;
}

// Disables the outdated build detector for organic installs of Chrome by
// switching the install to the AOHY non-organic brand code. If Chrome's brand
// is modified and it is a multi-install Chrome, the binaries' brand code will
// likewise be modified. On failures, |detail| will be populated with a Windows
// error code corresponding to the failure mode. Returns the process exit code
// for the operation.
ExitCode DisableOutdatedBuildDetectorImpl(bool system_level, uint32_t* detail) {
  // Update Chrome's brand code.
  ExitCode exit_code = DisableForApp(system_level, kChromeAppGuid, detail);

  // If that succeeded and Chrome is multi-install, make a best-effort attempt
  // to update the binaries' brand code.
  if (exit_code == ExitCode::CHROME_BRAND_UPDATED &&
      IsChromeMultiInstall(system_level)) {
    ExitCode secondary_code =
        DisableForApp(system_level, kBinariesAppGuid, detail);
    if (secondary_code == ExitCode::CHROME_BRAND_UPDATED)
      exit_code = ExitCode::BOTH_BRANDS_UPDATED;
  }
  return exit_code;
}

}  // namespace

ExitCode DisableOutdatedBuildDetector(const base::CommandLine& command_line) {
  const bool system_level = IsSystemLevel(command_line);
  ResultInfo result_info = {
      InstallerResult::FAILED_CUSTOM_ERROR,  // installer_result
      ExitCode::UNKNOWN_FAILURE,             // installer_error
      0                                      // installer_extra_code1
  };

  result_info.installer_error = DisableOutdatedBuildDetectorImpl(
      system_level, &result_info.installer_extra_code1);

  WriteResultInfo(system_level, result_info);
  return result_info.installer_error;
}
