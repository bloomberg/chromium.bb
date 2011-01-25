// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/product.h"

#include <algorithm>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/process_util.h"
#include "base/win/registry.h"
#include "chrome/installer/util/chrome_browser_operations.h"
#include "chrome/installer/util/chrome_browser_sxs_operations.h"
#include "chrome/installer/util/chrome_frame_operations.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/helper.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/master_preferences.h"
#include "chrome/installer/util/master_preferences_constants.h"
#include "chrome/installer/util/product_operations.h"

using base::win::RegKey;
using installer::MasterPreferences;

namespace installer {

Product::Product(BrowserDistribution* distribution)
    : distribution_(distribution) {
  switch (distribution->GetType()) {
    case BrowserDistribution::CHROME_BROWSER:
      operations_.reset(InstallUtil::IsChromeSxSProcess() ?
          new ChromeBrowserSxSOperations() :
          new ChromeBrowserOperations());
      break;
    case BrowserDistribution::CHROME_FRAME:
      operations_.reset(new ChromeFrameOperations());
      break;
    default:
      NOTREACHED() << "Unsupported BrowserDistribution::Type: "
                   << distribution->GetType();
  }
}

Product::~Product() {
}

void Product::InitializeFromPreferences(const MasterPreferences& prefs) {
  operations_->ReadOptions(prefs, &options_);
}

void Product::InitializeFromUninstallCommand(
    const CommandLine& uninstall_command) {
  operations_->ReadOptions(uninstall_command, &options_);
}

FilePath Product::GetUserDataPath() const {
  return GetChromeUserDataPath(distribution_);
}

bool Product::LaunchChrome(const FilePath& application_path) const {
  bool success = !application_path.empty();
  if (success) {
    CommandLine cmd(application_path.Append(installer::kChromeExe));
    success = base::LaunchApp(cmd, false, false, NULL);
  }
  return success;
}

bool Product::LaunchChromeAndWait(const FilePath& application_path,
                                  const CommandLine& options,
                                  int32* exit_code) const {
  if (application_path.empty())
    return false;

  CommandLine cmd(application_path.Append(installer::kChromeExe));
  cmd.AppendArguments(options, false);

  bool success = false;
  STARTUPINFOW si = { sizeof(si) };
  PROCESS_INFORMATION pi = {0};
  // Cast away constness of the command_line_string() since CreateProcess
  // might modify the string (insert \0 to separate the program from the
  // arguments).  Since we're not using the cmd variable beyond this point
  // we don't care.
  if (!::CreateProcess(cmd.GetProgram().value().c_str(),
                       const_cast<wchar_t*>(cmd.command_line_string().c_str()),
                       NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL,
                       &si, &pi)) {
    PLOG(ERROR) << "Failed to launch: " << cmd.command_line_string();
  } else {
    ::CloseHandle(pi.hThread);

    DWORD ret = ::WaitForSingleObject(pi.hProcess, INFINITE);
    DLOG_IF(ERROR, ret != WAIT_OBJECT_0)
        << "Unexpected return value from WaitForSingleObject: " << ret;
    if (::GetExitCodeProcess(pi.hProcess, &ret)) {
      DCHECK(ret != STILL_ACTIVE);
      success = true;
      if (exit_code)
        *exit_code = ret;
    } else {
      PLOG(ERROR) << "GetExitCodeProcess failed";
    }

    ::CloseHandle(pi.hProcess);
  }

  return success;
}

bool Product::SetMsiMarker(bool system_install, bool set) const {
  HKEY reg_root = system_install ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
  RegKey client_state_key;
  LONG result = client_state_key.Open(reg_root,
      distribution_->GetStateKey().c_str(), KEY_READ | KEY_WRITE);
  if (result == ERROR_SUCCESS) {
    result = client_state_key.WriteValue(google_update::kRegMSIField,
                                         set ? 1 : 0);
  }

  LOG_IF(ERROR, result != ERROR_SUCCESS) << "Failed to Open or Write MSI value"
      "to client state key. error: " << result;

  return (result == ERROR_SUCCESS);
}

bool Product::ShouldCreateUninstallEntry() const {
  return operations_->ShouldCreateUninstallEntry(options_);
}

void Product::AddKeyFiles(std::vector<FilePath>* key_files) const {
  operations_->AddKeyFiles(options_, key_files);
}

void Product::AddComDllList(std::vector<FilePath>* com_dll_list) const {
  operations_->AddComDllList(options_, com_dll_list);
}

void Product::AppendProductFlags(CommandLine* command_line) const {
  operations_->AppendProductFlags(options_, command_line);
}

bool Product::SetChannelFlags(bool set, ChannelInfo* channel_info) const {
  return operations_->SetChannelFlags(options_, set, channel_info);
}

}  // namespace installer
