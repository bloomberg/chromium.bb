// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_devtools_browsertest.h"

#include "base/command_line.h"
#include "chrome/common/chrome_switches.h"

void ExtensionDevToolsBrowserTest::SetUpCommandLine(CommandLine* command_line) {
  ExtensionBrowserTest::SetUpCommandLine(command_line);

  command_line->AppendSwitch(switches::kEnableExtensionTimelineApi);
}


