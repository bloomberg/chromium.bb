// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search/search_ui.h"

#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"

namespace chrome {
namespace search {

const SkColor kNTPBackgroundColor = SkColorSetRGB(0xF5, 0xF5, 0xF5);
const SkColor kResultsSeparatorColor = SkColorSetRGB(226, 226, 226);
const SkColor kSearchBackgroundColor = SK_ColorWHITE;

const int kOmniboxYPosition = 310;
const int kSearchResultsHeight = 122;

gfx::Rect GetNTPOmniboxBounds(const gfx::Size& web_contents_size) {
  const double kNTPPageWidthRatio = 0.73f;
  if (web_contents_size.IsEmpty())
    return gfx::Rect();
  int width = static_cast<int>(kNTPPageWidthRatio *
                               static_cast<double>(web_contents_size.width()));
  int x = (web_contents_size.width() - width) / 2;
  return gfx::Rect(x, kOmniboxYPosition, width, 0);
}

} //  namespace search
} //  namespace chrome
