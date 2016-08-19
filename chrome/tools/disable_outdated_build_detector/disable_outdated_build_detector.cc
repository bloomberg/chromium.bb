// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/tools/disable_outdated_build_detector/disable_outdated_build_detector.h"

#include <stdlib.h>

#include <algorithm>
#include <string>

#include "chrome/tools/disable_outdated_build_detector/google_update_integration.h"

namespace {

// A rudimentary switch finder that ignores prefixes (e.g., --, -, /).
bool HasSwitch(const wchar_t* command_line, const wchar_t* a_switch) {
  if (!command_line || !*command_line)
    return false;
  const wchar_t* line_end =
      command_line + std::char_traits<wchar_t>::length(command_line);
  const wchar_t* switch_end =
      a_switch + std::char_traits<wchar_t>::length(a_switch);
  return std::search(command_line, line_end, a_switch, switch_end) != line_end;
}

// Reads the value named |value_name| from a registry key into |value|. The
// type in the registry is ignored. |value| is not modified in case of error.
// presubmit: allow wstring
uint32_t ReadString(HKEY key, const wchar_t* value_name, std::wstring* value) {
  // Read the size of the value (in bytes).
  uint32_t byte_length = 0;
  uint32_t result = ::RegQueryValueEx(
      key, value_name, nullptr /* lpReserved */, nullptr /* lpType */,
      nullptr /* lpData */, reinterpret_cast<DWORD*>(&byte_length));
  if (result != ERROR_SUCCESS)
    return result;
  if (!byte_length) {
    value->resize(0);
    return result;
  }

  // Handle odd sizes and missing string terminators.
  size_t char_length = (byte_length + 3) / sizeof(wchar_t);
  // presubmit: allow wstring
  std::wstring buffer(char_length, L'\0');

  byte_length = static_cast<uint32_t>(char_length * sizeof(wchar_t));
  result = ::RegQueryValueEx(key, value_name, nullptr /* lpReserved */,
                             nullptr /* lpType */,
                             reinterpret_cast<uint8_t*>(&buffer[0]),
                             reinterpret_cast<DWORD*>(&byte_length));
  if (result != ERROR_SUCCESS)
    return result;
  if (!byte_length) {
    value->resize(0);
    return result;
  }

  // Shrink the string to chop off the terminator.
  if (!buffer[(byte_length - 1) / sizeof(wchar_t)])
    buffer.resize((byte_length - 1) / sizeof(wchar_t));
  else
    buffer.resize((byte_length + 1) / sizeof(wchar_t));

  value->swap(buffer);
  return result;
}

// Returns true if the tool is to operate on a system-level Chrome on the basis
// of either the --system-level command line switch or the GoogleUpdateIsMachine
// environment variable.
bool IsSystemLevel(const wchar_t* command_line) {
  if (HasSwitch(command_line, switches::kSystemLevel))
    return true;

  wchar_t buffer[2] = {};
  return ::GetEnvironmentVariableW(env::kGoogleUpdateIsMachine, &buffer[0],
                                   static_cast<DWORD>(_countof(buffer))) == 1 &&
         buffer[0] == L'1';
}

// Returns true if Chrome is multi-install.
bool IsChromeMultiInstall(bool system_level) {
  HKEY key = nullptr;

  LONG result = OpenClientStateKey(system_level, App::CHROME_BROWSER, &key);
  if (result != ERROR_SUCCESS)
    return false;
  // presubmit: allow wstring
  std::wstring uninstall_arguments;
  ReadString(key, kUninstallArguments, &uninstall_arguments);
  ::RegCloseKey(key);
  return HasSwitch(uninstall_arguments.c_str(), switches::kMultiInstall);
}

// Disables the outdated build detector for |app|. On failures, |detail| will be
// populated with a Windows error code corresponding to the failure mode.
// Returns the exit code for the operation.
ExitCode DisableForApp(bool system_level, App app, uint32_t* detail) {
  HKEY key = nullptr;

  *detail = OpenClientStateKey(system_level, app, &key);
  if (*detail == ERROR_FILE_NOT_FOUND)
    return ExitCode::NO_CHROME;
  if (*detail != ERROR_SUCCESS)
    return ExitCode::FAILED_OPENING_KEY;

  ExitCode exit_code = ExitCode::UNKNOWN_FAILURE;
  // presubmit: allow wstring
  std::wstring brand;
  *detail = ReadString(key, kBrand, &brand);
  if (*detail != ERROR_SUCCESS && *detail != ERROR_FILE_NOT_FOUND) {
    exit_code = ExitCode::FAILED_READING_BRAND;
  } else if (!IsOrganic(brand)) {
    exit_code = ExitCode::NON_ORGANIC_BRAND;
  } else {
    *detail = ::RegSetValueEx(
        key, kBrand, 0 /* Reserved */, REG_SZ,
        reinterpret_cast<const uint8_t*>(&kAOHY[0]),
        (std::char_traits<wchar_t>::length(kAOHY) + 1) * sizeof(wchar_t));
    exit_code = *detail == ERROR_SUCCESS ? ExitCode::CHROME_BRAND_UPDATED
                                         : ExitCode::FAILED_WRITING_BRAND;
  }
  ::RegCloseKey(key);

  return exit_code;
}

// Disables the outdated build detector for organic installs of Chrome by
// switching the install to the AOHY non-organic brand code. If Chrome's brand
// is modified and it is a multi-install Chrome, the binaries' brand code will
// likewise be modified. On failures, |detail| will be populated with a Windows
// error code corresponding to the failure mode. Returns the process exit code
// for the operation.
ExitCode DisableOutdatedBuildDetectorImpl(bool system_level, uint32_t* detail) {
  // Update Chrome's brand code.
  ExitCode exit_code = DisableForApp(system_level, App::CHROME_BROWSER, detail);

  // If that succeeded and Chrome is multi-install, make a best-effort attempt
  // to update the binaries' brand code.
  if (exit_code == ExitCode::CHROME_BRAND_UPDATED &&
      IsChromeMultiInstall(system_level)) {
    ExitCode secondary_code =
        DisableForApp(system_level, App::CHROME_BINARIES, detail);
    if (secondary_code == ExitCode::CHROME_BRAND_UPDATED)
      exit_code = ExitCode::BOTH_BRANDS_UPDATED;
  }
  return exit_code;
}

}  // namespace

ExitCode DisableOutdatedBuildDetector(const wchar_t* command_line) {
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
