// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/auto_launch_util.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/win_util.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/installer/util/util_constants.h"
#include "crypto/sha2.h"

using base::ASCIIToUTF16;
using base::ASCIIToWide;

namespace auto_launch_util {

// The prefix of the Chrome Auto-launch key under the Run key.
const wchar_t kAutolaunchKeyValue[] = L"GoogleChromeAutoLaunch";

// We use one Run key with flags specifying which feature we want to start up.
// When we change our Run key we need to specify what we want to do with each
// flag. This lists the possible actions we can take with the flags.
enum FlagSetting {
  FLAG_DISABLE,   // Disable the flag.
  FLAG_ENABLE,    // Enable the flag.
  FLAG_PRESERVE,  // Preserve the value that the flag has currently.
};

// A helper function that takes a |profile_directory| and builds a registry key
// name to use when deciding where to read/write the auto-launch value
// to/from. It takes into account the name of the profile (so that different
// installations of Chrome don't conflict, and so the in the future different
// profiles can be auto-launched (or not) separately).
base::string16 ProfileToKeyName(const base::string16& profile_directory) {
  base::FilePath path;
  const bool success = PathService::Get(chrome::DIR_USER_DATA, &path);
  DCHECK(success);
  path = path.Append(profile_directory);

  std::string input(path.AsUTF8Unsafe());
  uint8 hash[16];
  crypto::SHA256HashString(input, hash, sizeof(hash));
  std::string hash_string = base::HexEncode(hash, sizeof(hash));
  return base::string16(kAutolaunchKeyValue) + ASCIIToWide("_") +
         ASCIIToWide(hash_string);
}

// Returns whether the Chrome executable specified in |application_path| is set
// to auto-launch at computer startup with a given |command_line_switch|.
// NOTE: |application_path| is optional and should be blank in most cases (as
// it will default to the application path of the current executable).
// |profile_directory| is the name of the directory (leaf, not the full path)
// that contains the profile that should be opened at computer startup.
// |command_line_switch| is the switch we are optionally interested in and, if
// not blank, must be present for the function to return true. If blank, it acts
// like a wildcard.
bool WillLaunchAtLoginWithSwitch(const base::FilePath& application_path,
                                 const base::string16& profile_directory,
                                 const std::string& command_line_switch) {
  base::string16 key_name(ProfileToKeyName(profile_directory));
  base::string16 autolaunch;
  if (!base::win::ReadCommandFromAutoRun(
      HKEY_CURRENT_USER, key_name, &autolaunch)) {
    return false;
  }

  base::FilePath chrome_exe(application_path);
  if (chrome_exe.empty()) {
    if (!PathService::Get(base::DIR_EXE, &chrome_exe)) {
      NOTREACHED();
      return false;
    }
  }
  chrome_exe = chrome_exe.Append(installer::kChromeExe);

  if (autolaunch.find(chrome_exe.value()) == base::string16::npos)
    return false;

  return command_line_switch.empty() ||
         autolaunch.find(ASCIIToUTF16(command_line_switch)) !=
             base::string16::npos;
}

bool AutoStartRequested(const base::string16& profile_directory,
                        bool window_requested,
                        const base::FilePath& application_path) {
  if (window_requested) {
    return WillLaunchAtLoginWithSwitch(application_path,
                                       profile_directory,
                                       switches::kAutoLaunchAtStartup);
  } else {
    // Background mode isn't profile specific, but is attached to the Run key
    // for the Default profile.
    return WillLaunchAtLoginWithSwitch(application_path,
                                       ASCIIToUTF16(chrome::kInitialProfile),
                                       switches::kNoStartupWindow);
  }
}

bool CheckAndRemoveDeprecatedBackgroundModeSwitch() {
  // For backwards compatibility we need to provide a migration path from the
  // previously used key "chromium" that the BackgroundMode used to set, as it
  // is incompatible with the new key (can't have two Run keys with
  // conflicting switches).
  base::string16 chromium = ASCIIToUTF16("chromium");
  base::string16 value;
  if (base::win::ReadCommandFromAutoRun(HKEY_CURRENT_USER, chromium, &value)) {
    if (value.find(ASCIIToUTF16(switches::kNoStartupWindow)) !=
        base::string16::npos) {
      base::win::RemoveCommandFromAutoRun(HKEY_CURRENT_USER, chromium);
      return true;
    }
  }

  return false;
}

void SetWillLaunchAtLogin(const base::FilePath& application_path,
                          const base::string16& profile_directory,
                          FlagSetting foreground_mode,
                          FlagSetting background_mode) {
  if (CheckAndRemoveDeprecatedBackgroundModeSwitch()) {
    // We've found the deprecated switch, we must migrate it (unless background
    // mode is being turned off).
    if (profile_directory == ASCIIToUTF16(chrome::kInitialProfile) &&
        background_mode == FLAG_PRESERVE) {
      // Preserve in this case also covers the deprecated value, so we must
      // explicitly turn the flag on and the rest will be taken care of below.
      background_mode = FLAG_ENABLE;
    } else {
      // When we add support for multiple profiles for foreground mode we need
      // to think about where to store the background mode switch. I think we
      // need to store it with the Default profile (call SetWillLaunchAtLogin
      // again specifying the Default profile), but concerns were raised in
      // review.
      NOTREACHED();
    }
  }
  base::string16 key_name(ProfileToKeyName(profile_directory));

  // Check which feature should be enabled.
  bool in_foreground =
      foreground_mode == FLAG_ENABLE ||
      (foreground_mode == FLAG_PRESERVE &&
          WillLaunchAtLoginWithSwitch(application_path,
                                      profile_directory,
                                      switches::kAutoLaunchAtStartup));
  bool in_background =
      background_mode == FLAG_ENABLE ||
      (background_mode == FLAG_PRESERVE &&
          WillLaunchAtLoginWithSwitch(application_path,
                                      profile_directory,
                                      switches::kNoStartupWindow));

  // TODO(finnur): Convert this into a shortcut, instead of using the Run key.
  if (in_foreground || in_background) {
    base::FilePath path(application_path);
    if (path.empty()) {
      if (!PathService::Get(base::DIR_EXE, &path)) {
        NOTREACHED();
        return;
      }
    }
    base::string16 cmd_line = ASCIIToUTF16("\"");
    cmd_line += path.value();
    cmd_line += ASCIIToUTF16("\\");
    cmd_line += installer::kChromeExe;
    cmd_line += ASCIIToUTF16("\"");

    if (in_background) {
      cmd_line += ASCIIToUTF16(" --");
      cmd_line += ASCIIToUTF16(switches::kNoStartupWindow);
    }
    if (in_foreground) {
      cmd_line += ASCIIToUTF16(" --");
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
    }

    base::win::AddCommandToAutoRun(HKEY_CURRENT_USER, key_name, cmd_line);
  } else {
    base::win::RemoveCommandFromAutoRun(HKEY_CURRENT_USER, key_name);
  }
}

void DisableAllAutoStartFeatures(const base::string16& profile_directory) {
  DisableForegroundStartAtLogin(profile_directory);
  DisableBackgroundStartAtLogin();
}

void EnableForegroundStartAtLogin(const base::string16& profile_directory,
                                  const base::FilePath& application_path) {
  SetWillLaunchAtLogin(
      application_path, profile_directory, FLAG_ENABLE, FLAG_PRESERVE);
}

void DisableForegroundStartAtLogin(const base::string16& profile_directory) {
  SetWillLaunchAtLogin(
      base::FilePath(), profile_directory, FLAG_DISABLE, FLAG_PRESERVE);
}

void EnableBackgroundStartAtLogin() {
  // Background mode isn't profile specific, but we specify the Default profile
  // just to have a unique Run key to attach it to. FilePath is blank because
  // this function is not called from the installer (see comments for
  // EnableAutoStartAtLogin).
  SetWillLaunchAtLogin(base::FilePath(),
                       ASCIIToUTF16(chrome::kInitialProfile),
                       FLAG_PRESERVE,
                       FLAG_ENABLE);
}

void DisableBackgroundStartAtLogin() {
  SetWillLaunchAtLogin(base::FilePath(),
                       ASCIIToUTF16(chrome::kInitialProfile),
                       FLAG_PRESERVE,
                       FLAG_DISABLE);
}

}  // namespace auto_launch_util
