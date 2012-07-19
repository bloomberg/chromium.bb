// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/file_path.h"
#include "base/logging.h"
#include "base/process_util.h"
#include "chrome/installer/launcher_support/chrome_launcher_support.h"

namespace {

const wchar_t kChromeExe[] = L"chrome.exe";

}  // namespace

int main(int /* argc */, char* /* argv[] */) {
  base::AtExitManager exit_manager;

  // Initialize the commandline singleton from the environment.
  CommandLine::Init(0, NULL);

  FilePath chrome_exe(chrome_launcher_support::GetAnyChromePath());

  if (chrome_exe.empty()) {
    LOG(ERROR) << "No Chrome executable could be found.";
    // TODO(erikwright): install Chrome.
    return 1;
  }

  CommandLine chrome_exe_command_line(chrome_exe);
  chrome_exe_command_line.AppendArguments(
      *CommandLine::ForCurrentProcess(), false);

  if (base::LaunchProcess(chrome_exe_command_line,
                          base::LaunchOptions(),
                          NULL)) {
    LOG(INFO) << "Delegated to Chrome executable at " << chrome_exe.value();
    return 0;
  } else {
    LOG(INFO) << "Failed to launch Chrome executable at " << chrome_exe.value();
    return 1;
  }
}
