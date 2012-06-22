// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SEARCH_SEARCH_UI_H_
#define CHROME_BROWSER_UI_SEARCH_SEARCH_UI_H_
#pragma once

#include "third_party/skia/include/core/SkColor.h"

namespace gfx {
class Rect;
class Size;
}

namespace chrome {
namespace search {

// Background color of the NTP.
extern const SkColor kNTPBackgroundColor;

// Color for the separator between results and the page.
extern const SkColor kResultsSeparatorColor;

// Background color for search results.
extern const SkColor kSearchBackgroundColor;

// Y-coordinate of the omnibox when over the page.
extern const int kOmniboxYPosition;

// Initial height of the search results.
extern const int kSearchResultsHeight;

// Get location of NTP omnibox in |web_contents_size|.  A height of 0 is
// returned, it is platform-specific.
gfx::Rect GetNTPOmniboxBounds(const gfx::Size& web_contents_size);

}  // namespace search
}  // namespace chrome

#endif  // CHROME_BROWSER_UI_SEARCH_SEARCH_UI_H_
