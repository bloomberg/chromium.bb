// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/system/touchpad_settings.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/message_loop.h"
#include "base/process_util.h"
#include "base/stringprintf.h"
#include "content/browser/browser_thread.h"

namespace chromeos {
namespace system {
namespace touchpad_settings {
namespace {
const char* kTpControl = "/opt/google/touchpad/tpcontrol";
}  // namespace


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

  base::LaunchOptions options;
  options.wait = false;  // Launch asynchronously.

  base::LaunchProcess(CommandLine(argv), options, NULL);
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

  base::LaunchOptions options;
  options.wait = false;  // Launch asynchronously.

  base::LaunchProcess(CommandLine(argv), options, NULL);
}

}  // namespace touchpad_settings
}  // namespace system
}  // namespace chromeos
