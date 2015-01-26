// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_commands_mac.h"

#include "base/command_line.h"
#include "chrome/browser/fullscreen.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller.h"
#include "chrome/common/chrome_switches.h"

namespace chrome {

void ToggleFullscreenWithToolbarOrFallback(Browser* browser) {
  DCHECK(browser);
  if (chrome::mac::SupportsSystemFullscreen())
    browser->exclusive_access_manager()
        ->fullscreen_controller()
        ->ToggleBrowserFullscreenWithToolbar();
  else
    ToggleFullscreenMode(browser);
}

}  // namespace chrome
