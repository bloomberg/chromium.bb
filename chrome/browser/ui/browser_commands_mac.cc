// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_commands_mac.h"

#include "base/mac/mac_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/fullscreen/fullscreen_controller.h"

namespace chrome {

void ToggleFullscreenWithChromeOrFallback(Browser* browser) {
  if (base::mac::IsOSLionOrLater())
    browser->fullscreen_controller()->ToggleFullscreenWithChrome();
  else
    ToggleFullscreenMode(browser);
}

}  // namespace chrome
