// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/auto_launch_util.h"

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "base/win/win_util.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/product.h"
#include "chrome/installer/util/util_constants.h"
#include "crypto/sha2.h"

namespace auto_launch_util {

// The prefix of the Chrome Auto-launch key under the Run key.
const wchar_t kAutolaunchKeyValue[] = L"GoogleChromeAutoLaunch";

// A helper function that takes a |profile_path| and builds a registry key
// name to use when deciding where to read/write the auto-launch value
// to/from. It takes into account the name of the profile (so that different
// installations of Chrome don't conflict, and so the in the future different
// profiles can be auto-launched (or not) separately).
string16 ProfileToKeyName(const string16& profile_directory) {
  FilePath path;
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kUserDataDir)) {
    path = command_line.GetSwitchValuePath(switches::kUserDataDir);
  } else {
    // Get the path from the same source as the installer, to make sure there
    // are no differences.
    BrowserDistribution* distribution =
        BrowserDistribution::GetSpecificDistribution(
            BrowserDistribution::CHROME_BROWSER);
    installer::Product product(distribution);
    path = product.GetUserDataPath();
  }
  path = path.Append(profile_directory);

  std::string input(path.AsUTF8Unsafe());
  uint8 hash[16];
  crypto::SHA256HashString(input, hash, sizeof(hash));
  std::string hash_string = base::HexEncode(hash, sizeof(hash));
  return string16(kAutolaunchKeyValue) +
      ASCIIToWide("_") + ASCIIToWide(hash_string);
}

bool WillLaunchAtLogin(const FilePath& application_path,
                       const string16& profile_directory) {
  string16 key_name(ProfileToKeyName(profile_directory));
  string16 autolaunch;
  if (!base::win::ReadCommandFromAutoRun(
      HKEY_CURRENT_USER, key_name, &autolaunch)) {
    return false;
  }

  FilePath chrome_exe(application_path);
  if (chrome_exe.empty()) {
    if (!PathService::Get(base::DIR_EXE, &chrome_exe)) {
      NOTREACHED();
      return false;
    }
  }
  chrome_exe = chrome_exe.Append(installer::kChromeExe);

  return autolaunch.find(chrome_exe.value()) != string16::npos;
}

void SetWillLaunchAtLogin(bool auto_launch,
                          const FilePath& application_path,
                          const string16& profile_directory) {
  string16 key_name(ProfileToKeyName(profile_directory));

  // TODO(finnur): Convert this into a shortcut, instead of using the Run key.
  if (auto_launch) {
    FilePath path(application_path);
    if (path.empty()) {
      if (!PathService::Get(base::DIR_EXE, &path)) {
        NOTREACHED();
        return;
      }
    }
    string16 cmd_line = ASCIIToUTF16("\"");
    cmd_line += path.value();
    cmd_line += ASCIIToUTF16("\\");
    cmd_line += installer::kChromeExe;
    cmd_line += ASCIIToUTF16("\" --");
    cmd_line += ASCIIToUTF16(switches::kAutoLaunchAtStartup);

    const CommandLine& command_line = *CommandLine::ForCurrentProcess();
    if (command_line.HasSwitch(switches::kUserDataDir)) {
      cmd_line += ASCIIToUTF16(" --");
      cmd_line += ASCIIToUTF16(switches::kUserDataDir);
      cmd_line += ASCIIToUTF16("=\"");
      cmd_line +=
          command_line.GetSwitchValuePath(switches::kUserDataDir).value();
      cmd_line += ASCIIToUTF16("\"");
    }

    cmd_line += ASCIIToUTF16(" --");
    cmd_line += ASCIIToUTF16(switches::kProfileDirectory);
    cmd_line += ASCIIToUTF16("=\"");
    cmd_line += profile_directory;
    cmd_line += ASCIIToUTF16("\"");

    base::win::AddCommandToAutoRun(
        HKEY_CURRENT_USER, key_name, cmd_line);
  } else {
    base::win::RemoveCommandFromAutoRun(HKEY_CURRENT_USER, key_name);
  }
}

}  // namespace auto_launch_util
