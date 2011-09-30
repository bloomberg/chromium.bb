// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/system/touchpad_settings.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/process_util.h"
#include "base/stringprintf.h"
#include "chrome/browser/chromeos/system/runtime_environment.h"
#include "content/browser/browser_thread.h"

namespace chromeos {
namespace system {
namespace touchpad_settings {
namespace {
const char* kTpControl = "/opt/google/touchpad/tpcontrol";
}  // namespace

// Launches the tpcontrol command asynchronously, if it exists.
void LaunchTpControl(const std::vector<std::string>& argv) {
  if (!system::runtime_environment::IsRunningOnChromeOS()) {
    // Do nothing on Linux desktop, as the command does not exist.
    return;
  }

  if (!file_util::PathExists(FilePath(argv[0]))) {
    LOG(ERROR) << argv[0] << " not found";
    return;
  }

  base::LaunchOptions options;
  options.wait = false;  // Launch asynchronously.

  base::LaunchProcess(CommandLine(argv), options, NULL);
}

void SetSensitivity(int value) {
  // Run this on the FILE thread.
  if (!BrowserThread::CurrentlyOn(BrowserThread::FILE)) {
    BrowserThread::PostTask(
        BrowserThread::FILE, FROM_HERE,
        base::Bind(&SetSensitivity, value));
    return;
  }

  std::vector<std::string> argv;
  argv.push_back(kTpControl);
  argv.push_back("sensitivity");
  argv.push_back(StringPrintf("%d", value));

  LaunchTpControl(argv);
}

void SetTapToClick(bool enabled) {
  // Run this on the FILE thread.
  if (!BrowserThread::CurrentlyOn(BrowserThread::FILE)) {
    BrowserThread::PostTask(
        BrowserThread::FILE, FROM_HERE,
        base::Bind(&SetTapToClick, enabled));
    return;
  }

  std::vector<std::string> argv;
  argv.push_back(kTpControl);
  argv.push_back("taptoclick");
  argv.push_back(enabled ? "on" : "off");

  LaunchTpControl(argv);
}

}  // namespace touchpad_settings
}  // namespace system
}  // namespace chromeos
