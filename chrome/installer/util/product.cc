// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/product.h"

#include <string>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/process/launch.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/chrome_browser_operations.h"
#include "chrome/installer/util/product_operations.h"

namespace installer {

Product::Product(BrowserDistribution* distribution)
    : distribution_(distribution),
      operations_(std::make_unique<ChromeBrowserOperations>()) {}

Product::~Product() {
}

bool Product::LaunchChrome(const base::FilePath& application_path) const {
  bool success = !application_path.empty();
  if (success) {
    base::CommandLine cmd(application_path.Append(installer::kChromeExe));
    success = base::LaunchProcess(cmd, base::LaunchOptions()).IsValid();
  }
  return success;
}

bool Product::LaunchChromeAndWait(const base::FilePath& application_path,
                                  const base::CommandLine& options,
                                  int32_t* exit_code) const {
  if (application_path.empty())
    return false;

  base::CommandLine cmd(application_path.Append(installer::kChromeExe));
  cmd.AppendArguments(options, false);

  bool success = false;
  STARTUPINFOW si = { sizeof(si) };
  PROCESS_INFORMATION pi = {0};
  // Create a writable copy of the command line string, since CreateProcess may
  // modify the string (insert \0 to separate the program from the arguments).
  std::wstring writable_command_line_string(cmd.GetCommandLineString());
  if (!::CreateProcess(cmd.GetProgram().value().c_str(),
                       &writable_command_line_string[0],
                       NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL,
                       &si, &pi)) {
    PLOG(ERROR) << "Failed to launch: " << cmd.GetCommandLineString();
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

void Product::AddKeyFiles(std::vector<base::FilePath>* key_files) const {
  operations_->AddKeyFiles(key_files);
}

}  // namespace installer
