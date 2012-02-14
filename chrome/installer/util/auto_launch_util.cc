// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/auto_launch_util.h"

#include "base/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "base/win/win_util.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/installer/util/util_constants.h"

namespace auto_launch_util {

// The name of the Chrome Auto-launch key under the Run key.
const wchar_t kAutolaunchKeyValue[] = L"GoogleChromeAutoLaunch";

bool WillLaunchAtLogin(const FilePath& application_path) {
  string16 autolaunch;
  if (!base::win::ReadCommandFromAutoRun(
      HKEY_CURRENT_USER, kAutolaunchKeyValue, &autolaunch)) {
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

void SetWillLaunchAtLogin(bool auto_launch, const FilePath& application_path) {
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

    base::win::AddCommandToAutoRun(
        HKEY_CURRENT_USER, kAutolaunchKeyValue, cmd_line);
  } else {
    base::win::RemoveCommandFromAutoRun(HKEY_CURRENT_USER, kAutolaunchKeyValue);
  }
}

}  // namespace auto_launch_util
