// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_OPTIONS_OPTIONS_WINDOW_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_OPTIONS_OPTIONS_WINDOW_VIEW_H_

namespace chromeos {

// Closes the options dialog.
void CloseOptionsWindow();

// Get a proper parent for options dialogs. This returns the last active browser
// window for now.
gfx::NativeWindow GetOptionsViewParent();

}

#endif  // CHROME_BROWSER_CHROMEOS_OPTIONS_OPTIONS_WINDOW_VIEW_H_
