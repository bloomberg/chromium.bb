// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/chrome_launcher.h"

#include <windows.h>
#include <shellapi.h>
#include <shlwapi.h>

#include "policy/policy_constants.h"

// Herein lies stuff selectively stolen from Chrome. We don't pull it in
// directly because all of it results in many things we don't want being
// included as well.
namespace {

// These are the switches we will allow (along with their values) in the
// safe-for-Low-Integrity version of the Chrome command line.
// Including the chrome switch files pulls in a bunch of dependencies sadly, so
// we redefine things here:
const wchar_t* kAllowedSwitches[] = {
  L"automation-channel",
  L"chrome-frame",
  L"chrome-version",
  L"disable-background-mode",
  L"disable-popup-blocking",
  L"disable-print-preview",
  L"disable-renderer-accessibility",
  L"enable-experimental-extension-apis",
  L"force-renderer-accessibility",
  L"full-memory-crash-report",
  L"lang",
  L"no-default-browser-check",
  L"no-first-run",
  L"noerrdialogs",
  L"user-data-dir",
};

const wchar_t kWhitespaceChars[] = {
  0x0009, /* <control-0009> to <control-000D> */
  0x000A,
  0x000B,
  0x000C,
  0x000D,
  0x0020, /* Space */
  0x0085, /* <control-0085> */
  0x00A0, /* No-Break Space */
  0x1680, /* Ogham Space Mark */
  0x180E, /* Mongolian Vowel Separator */
  0x2000, /* En Quad to Hair Space */
  0x2001,
  0x2002,
  0x2003,
  0x2004,
  0x2005,
  0x2006,
  0x2007,
  0x2008,
  0x2009,
  0x200A,
  0x200C, /* Zero Width Non-Joiner */
  0x2028, /* Line Separator */
  0x2029, /* Paragraph Separator */
  0x202F, /* Narrow No-Break Space */
  0x205F, /* Medium Mathematical Space */
  0x3000, /* Ideographic Space */
  0
};

const wchar_t kLauncherExeBaseName[] = L"chrome_launcher.exe";
const wchar_t kBrowserProcessExecutableName[] = L"chrome.exe";

}  // end namespace


namespace chrome_launcher {

std::wstring TrimWhiteSpace(const wchar_t* input_str) {
  std::wstring output;
  if (input_str != NULL) {
    std::wstring str(input_str);

    const std::wstring::size_type first_good_char =
        str.find_first_not_of(kWhitespaceChars);
    const std::wstring::size_type last_good_char =
        str.find_last_not_of(kWhitespaceChars);

    if (first_good_char != std::wstring::npos &&
        last_good_char != std::wstring::npos &&
        last_good_char >= first_good_char) {
      // + 1 because find_last_not_of returns the index, and we want the count
      output = str.substr(first_good_char,
                          last_good_char - first_good_char + 1);
    }
  }

  return output;
}

bool IsValidArgument(const std::wstring& arg) {
  if (arg.length() < 2) {
    return false;
  }

  for (int i = 0; i < arraysize(kAllowedSwitches); ++i) {
    size_t arg_length = lstrlenW(kAllowedSwitches[i]);
    if (arg.find(kAllowedSwitches[i], 2) == 2) {
      // The argument starts off right, now it must either end here, or be
      // followed by an equals sign.
      if (arg.length() == (arg_length + 2) ||
          (arg.length() > (arg_length + 2) && arg[arg_length+2] == L'=')) {
        return true;
      }
    }
  }

  return false;
}

bool IsValidCommandLine(const wchar_t* command_line) {
  if (command_line == NULL) {
    return false;
  }

  int num_args = 0;
  wchar_t** args = NULL;
  args = CommandLineToArgvW(command_line, &num_args);

  bool success = true;
  // Note that we skip args[0] since that is just our executable name and
  // doesn't get passed through to Chrome.
  for (int i = 1; i < num_args; ++i) {
    std::wstring trimmed_arg = TrimWhiteSpace(args[i]);
    if (!IsValidArgument(trimmed_arg.c_str())) {
      success = false;
      break;
    }
  }

  return success;
}

// Looks up optionally configured launch parameters for Chrome that may have
// been set via group policy.
void AppendAdditionalLaunchParameters(std::wstring* command_line) {
  static const HKEY kRootKeys[] = {
    HKEY_LOCAL_MACHINE,
    HKEY_CURRENT_USER
  };

  std::wstring launch_params_value_name(
      &policy::key::kAdditionalLaunchParameters[0],
      &policy::key::kAdditionalLaunchParameters[
          lstrlenA(policy::key::kAdditionalLaunchParameters)]);

  // Used for basic checks since CreateProcess doesn't support command lines
  // longer than 0x8000 characters. If we surpass that length, we do not add the
  // additional parameters.  Because we need to add a space before the
  // extra parameters, we use 0x7fff and not 0x8000.
  const size_t kMaxChars = 0x7FFF - command_line->size();
  HKEY key;
  LONG result;
  bool found = false;
  for (int i = 0; !found && i < arraysize(kRootKeys); ++i) {
    result = ::RegOpenKeyExW(kRootKeys[i], policy::kRegistryChromePolicyKey, 0,
                             KEY_QUERY_VALUE, &key);
    if (result == ERROR_SUCCESS) {
      DWORD size = 0;
      DWORD type = 0;
      result = RegQueryValueExW(key, launch_params_value_name.c_str(),
                                0, &type, NULL, &size);
      if (result == ERROR_SUCCESS && type == REG_SZ && size > 0 &&
          (size / sizeof(wchar_t)) < kMaxChars) {
        // This size includes any terminating null character or characters
        // unless the data was stored without them, so for safety we allocate
        // one extra char and zero out the buffer.
        wchar_t* value = new wchar_t[(size / sizeof(wchar_t)) + 1];
        memset(value, 0, size + sizeof(wchar_t));
        result = RegQueryValueExW(key, launch_params_value_name.c_str(), 0,
                                  &type, reinterpret_cast<BYTE*>(&value[0]),
                                  &size);
        if (result == ERROR_SUCCESS) {
          *command_line += L' ';
          *command_line += value;
          found = true;
        }
        delete [] value;
      }
      ::RegCloseKey(key);
    }
  }
}

bool SanitizeAndLaunchChrome(const wchar_t* command_line) {
  bool success = false;
  if (IsValidCommandLine(command_line)) {
    std::wstring chrome_path;
    if (GetChromeExecutablePath(&chrome_path)) {
      const wchar_t* args = PathGetArgs(command_line);

      // Build the command line string with the quoted path to chrome.exe.
      std::wstring command_line;
      command_line.reserve(chrome_path.size() + 2);
      command_line.append(1, L'\"').append(chrome_path).append(1, L'\"');

      if (args != NULL) {
        command_line += L' ';
        command_line += args;
      }

      // Append parameters that might be set by group policy.
      AppendAdditionalLaunchParameters(&command_line);

      STARTUPINFO startup_info = {0};
      startup_info.cb = sizeof(startup_info);
      startup_info.dwFlags = STARTF_USESHOWWINDOW;
      startup_info.wShowWindow = SW_SHOW;
      PROCESS_INFORMATION process_info = {0};
      if (CreateProcess(&chrome_path[0], &command_line[0],
                        NULL, NULL, FALSE, 0, NULL, NULL,
                        &startup_info, &process_info)) {
        // Close handles.
        CloseHandle(process_info.hThread);
        CloseHandle(process_info.hProcess);
        success = true;
      } else {
        _ASSERT(FALSE);
      }
    }
  }

  return success;
}

bool GetChromeExecutablePath(std::wstring* chrome_path) {
  _ASSERT(chrome_path);

  wchar_t cur_path[MAX_PATH * 4] = {0};
  // Assume that we are always built into an exe.
  GetModuleFileName(NULL, cur_path, arraysize(cur_path) / 2);

  PathRemoveFileSpec(cur_path);

  bool success = false;
  if (PathAppend(cur_path, kBrowserProcessExecutableName)) {
    if (!PathFileExists(cur_path)) {
      // The installation model for Chrome places the DLLs in a versioned
      // sub-folder one down from the Chrome executable. If we fail to find
      // chrome.exe in the current path, try looking one up and launching that
      // instead. In practice, that means we back up two and append the
      // executable name again.
      PathRemoveFileSpec(cur_path);
      PathRemoveFileSpec(cur_path);
      PathAppend(cur_path, kBrowserProcessExecutableName);
    }

    if (PathFileExists(cur_path)) {
      *chrome_path = cur_path;
      success = true;
    }
  }

  return success;
}

}  // namespace chrome_launcher
