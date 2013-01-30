// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_commands_mac.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/fullscreen/fullscreen_controller.h"

namespace chrome {

void ToggleFullscreenWithChrome(Browser* browser) {
  browser->fullscreen_controller()->ToggleFullscreenWithChrome();
}

}  // namespace chrome
