// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_list.h"

#include "base/command_line.h"
#include "chrome/common/chrome_switches.h"

// static
void BrowserList::HandleAppExitingForPlatform() {
#if defined(OS_CHROMEOS)
  if (!CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableZeroBrowsersOpenForTests)) {
    // App is exiting, call EndKeepAlive() on behalf of Aura Shell.
    BrowserList::EndKeepAlive();
  }
#endif // OS_CHROMEOS
}
