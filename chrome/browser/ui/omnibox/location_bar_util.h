// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_OMNIBOX_LOCATION_BAR_UTIL_H_
#define CHROME_BROWSER_UI_OMNIBOX_LOCATION_BAR_UTIL_H_

#include "base/strings/string16.h"
#include "third_party/skia/include/core/SkColor.h"

class ExtensionAction;

namespace gfx {
class Canvas;
class Rect;
}

namespace location_bar_util {

// Build a short string to use in keyword-search when the field isn't very big.
string16 CalculateMinString(const string16& description);

// Paint the background and border for |action| on |tab_id|.  |bounds| should
// include the top and bottom of the location bar, and the middle column
// exactly between two ExtensionActions, so both ExtensionActions can draw on
// it.  This background is only drawn for script badges in the WANTS_ATTENTION
// state, and (when text is black and the background is white) consists of a
// dark grey, 1px border and a lighter grey background with a slight vertical
// gradient.
void PaintExtensionActionBackground(const ExtensionAction& action,
                                    int tab_id,
                                    gfx::Canvas* canvas,
                                    const gfx::Rect& bounds,
                                    SkColor text_color,
                                    SkColor background_color);

}  // namespace location_bar_util

#endif  // CHROME_BROWSER_UI_OMNIBOX_LOCATION_BAR_UTIL_H_
