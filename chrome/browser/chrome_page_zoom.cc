// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_page_zoom.h"

#include <algorithm>
#include <cmath>

#include "chrome/browser/chrome_page_zoom_constants.h"
#include "content/public/common/page_zoom.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"

namespace chrome_page_zoom {

enum PageZoomValueType {
  PAGE_ZOOM_VALUE_TYPE_FACTOR,
  PAGE_ZOOM_VALUE_TYPE_LEVEL,
};

std::vector<double> PresetZoomValues(PageZoomValueType value_type,
                                     double custom_value) {
  // Generate a vector of zoom values from an array of known preset
  // factors. The values in content::kPresetZoomFactors will already be in
  // sorted order.
  std::vector<double> zoom_values;
  bool found_custom = false;
  for (size_t i = 0; i < kPresetZoomFactorsSize; i++) {
    double zoom_value = kPresetZoomFactors[i];
    if (value_type == PAGE_ZOOM_VALUE_TYPE_LEVEL)
      zoom_value = WebKit::WebView::zoomFactorToZoomLevel(zoom_value);
    if (content::ZoomValuesEqual(zoom_value, custom_value))
      found_custom = true;
    zoom_values.push_back(zoom_value);
  }
  // If the preset array did not contain the custom value, append it to the
  // vector and then sort.
  double min = value_type == PAGE_ZOOM_VALUE_TYPE_LEVEL ?
      WebKit::WebView::zoomFactorToZoomLevel(content::kMinimumZoomFactor) :
      content::kMinimumZoomFactor;
  double max = value_type == PAGE_ZOOM_VALUE_TYPE_LEVEL ?
      WebKit::WebView::zoomFactorToZoomLevel(content::kMaximumZoomFactor) :
      content::kMaximumZoomFactor;
  if (!found_custom && custom_value > min && custom_value < max) {
    zoom_values.push_back(custom_value);
    std::sort(zoom_values.begin(), zoom_values.end());
  }
  return zoom_values;
}

std::vector<double> PresetZoomFactors(double custom_factor) {
  return PresetZoomValues(PAGE_ZOOM_VALUE_TYPE_FACTOR, custom_factor);
}

std::vector<double> PresetZoomLevels(double custom_level) {
  return PresetZoomValues(PAGE_ZOOM_VALUE_TYPE_LEVEL, custom_level);
}

}  // namespace chrome_page_zoom
