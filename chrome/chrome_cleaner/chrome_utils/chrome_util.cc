// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/chrome_utils/chrome_util.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/strings/pattern.h"
#include "base/strings/string_util.h"
#include "base/win/registry.h"
#include "base/win/windows_version.h"
#include "chrome/chrome_cleaner/os/disk_util.h"
#include "chrome/chrome_cleaner/os/file_path_sanitization.h"
#include "chrome/chrome_cleaner/os/registry.h"
#include "chrome/chrome_cleaner/os/system_util.h"
#include "components/chrome_cleaner/public/constants/constants.h"

namespace chrome_cleaner {

// Chrome shortcut filename.
const wchar_t kChromeShortcutFilename[] = L"Google Chrome.lnk";

// The KO language version doesn't have the term Google in the filename.
const wchar_t kKOChromeShortcutFilename[] = L"Chrome.lnk";

bool RetrieveChromeVersionAndInstalledDomain(base::string16* chrome_version,
                                             bool* system_install) {
  DCHECK(chrome_version);

  const base::CommandLine* const command_line =
      base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(kChromeVersionSwitch)) {
    LOG(WARNING) << "Can't get Chrome version information from flag: "
                 << "The " << kChromeVersionSwitch << " switch was not set.";
    return false;
  }

  *chrome_version = command_line->GetSwitchValueNative(kChromeVersionSwitch);
  // The system install flag should be set only by Chrome, in which case the
  // Chrome version flag will also be set. Therefore, the presence or absence
  // of the system install flag at this point fully determines whether or not
  // we have a system-level install of Chrome.
  if (system_install)
    *system_install = command_line->HasSwitch(kChromeSystemInstallSwitch);
  return true;
}

bool RetrieveChromeExePathFromCommandLine(base::FilePath* chrome_exe_path) {
  DCHECK(chrome_exe_path);

  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(kChromeExePathSwitch)) {
    LOG(WARNING) << "Failed to locate Chrome executable from flag: "
                 << "The " << kChromeExePathSwitch << " switch was not set.";
    return false;
  }

  base::FilePath chrome_exe_from_flag =
      command_line->GetSwitchValuePath(kChromeExePathSwitch);
  if (!base::PathExists(chrome_exe_from_flag)) {
    LOG(WARNING) << "Failed to locate Chrome executable from flag: "
                 << kChromeExePathSwitch << " = '"
                 << SanitizePath(chrome_exe_from_flag) << "'";
    return false;
  }

  *chrome_exe_path = chrome_exe_from_flag;
  return true;
}

}  // namespace chrome_cleaner
