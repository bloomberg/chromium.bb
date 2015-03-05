// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file can be empty. Its purpose is to contain the relatively short lived
// definitions required for experimental flags.

#include "ios/chrome/browser/experimental_flags.h"

#include "base/command_line.h"
#include "ios/chrome/browser/chrome_switches.h"

namespace experimental_flags {

bool IsOpenFromClipboardEnabled() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  return command_line->HasSwitch(switches::kEnableIOSOpenFromClipboard);
}

}  // namespace experimental_flags
