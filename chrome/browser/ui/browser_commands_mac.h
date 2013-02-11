// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_COMMANDS_MAC_H_
#define CHROME_BROWSER_UI_BROWSER_COMMANDS_MAC_H_

class Browser;

namespace chrome {

// Enters Mac OSX 10.7 Lion Fullscreen mode, with browser chrome displayed.
// On earlier OSX versions, falls back to presentation mode.
void ToggleFullscreenWithChromeOrFallback(Browser* browser);

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_BROWSER_COMMANDS_MAC_H_
