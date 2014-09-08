// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_commands_mac.h"

#include "base/command_line.h"
#include "base/mac/mac_util.h"
#include "chrome/browser/fullscreen.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/fullscreen/fullscreen_controller.h"
#include "chrome/common/chrome_switches.h"

namespace chrome {

void ToggleFullscreenWithChromeOrFallback(Browser* browser) {
  DCHECK(browser);
  if (chrome::mac::SupportsSystemFullscreen())
    browser->fullscreen_controller()->ToggleBrowserFullscreenWithChrome();
  else
    ToggleFullscreenMode(browser);
}

}  // namespace chrome
