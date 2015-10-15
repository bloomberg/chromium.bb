// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_dialogs.h"

#include "base/command_line.h"
#include "chrome/common/chrome_switches.h"

namespace chrome {

bool ToolkitViewsDialogsEnabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableMacViewsDialogs);
}

}  // namespace chrome
