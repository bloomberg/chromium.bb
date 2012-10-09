// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_WINDOW_STATE_H_
#define CHROME_BROWSER_UI_BROWSER_WINDOW_STATE_H_

#include <string>

#include "ui/base/ui_base_types.h"

class Browser;

namespace gfx {
class Rect;
}

namespace chrome {

std::string GetWindowPlacementKey(const Browser* browser);

bool ShouldSaveWindowPlacement(const Browser* browser);

void SaveWindowPlacement(const Browser* browser,
                         const gfx::Rect& bounds,
                         ui::WindowShowState show_state);

// Return the |bounds| for the browser window to be used upon creation.
// The |show_state| variable will receive the desired initial show state for
// the window.
void GetSavedWindowBoundsAndShowState(const Browser* browser,
                                      gfx::Rect* bounds,
                                      ui::WindowShowState* show_state);

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_BROWSER_WINDOW_STATE_H_
