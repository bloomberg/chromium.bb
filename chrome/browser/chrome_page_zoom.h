// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROME_PAGE_ZOOM_H_
#define CHROME_BROWSER_CHROME_PAGE_ZOOM_H_

#include <vector>
#include "content/public/common/page_zoom.h"

namespace content {
class WebContents;
}

namespace chrome_page_zoom {

// Return a sorted vector of zoom factors. The vector will consist of preset
// values along with a custom value (if the custom value is not already
// represented.)
std::vector<double> PresetZoomFactors(double custom_factor);

// Return a sorted vector of zoom levels. The vector will consist of preset
// values along with a custom value (if the custom value is not already
// represented.)
std::vector<double> PresetZoomLevels(double custom_level);

// Adjusts the zoom level of |web_contents|.
void Zoom(content::WebContents* web_contents, content::PageZoom zoom);

}  // namespace chrome_page_zoom

#endif  // CHROME_BROWSER_CHROME_PAGE_ZOOM_H_
