// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/first_run/upgrade.h"

#include "base/command_line.h"
#include "base/logging.h"

// static
void Upgrade::RelaunchChromeBrowserWithNewCommandLineIfNeeded() {
  if (new_command_line_) {
    if (!RelaunchChromeBrowser(*new_command_line_)) {
      DLOG(ERROR) << "Launching a new instance of the browser failed.";
    } else {
      DLOG(WARNING) << "Launched a new instance of the browser.";
    }
    delete new_command_line_;
    new_command_line_ = NULL;
  }
}
