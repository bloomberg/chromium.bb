// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_page_zoom.h"

#include <algorithm>
#include <cmath>

#include "base/prefs/pref_service.h"
#include "chrome/browser/chrome_page_zoom_constants.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/zoom/zoom_controller.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/host_zoom_map.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/page_zoom.h"
#include "content/public/common/renderer_preferences.h"

using base::UserMetricsAction;

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
      zoom_value = content::ZoomFactorToZoomLevel(zoom_value);
    if (content::ZoomValuesEqual(zoom_value, custom_value))
      found_custom = true;
    zoom_values.push_back(zoom_value);
  }
  // If the preset array did not contain the custom value, append it to the
  // vector and then sort.
  double min = value_type == PAGE_ZOOM_VALUE_TYPE_LEVEL ?
      content::ZoomFactorToZoomLevel(content::kMinimumZoomFactor) :
      content::kMinimumZoomFactor;
  double max = value_type == PAGE_ZOOM_VALUE_TYPE_LEVEL ?
      content::ZoomFactorToZoomLevel(content::kMaximumZoomFactor) :
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

void Zoom(content::WebContents* web_contents, content::PageZoom zoom) {
  ZoomController* zoom_controller =
      ZoomController::FromWebContents(web_contents);
  DCHECK(zoom_controller);

  double current_zoom_level = zoom_controller->GetZoomLevel();
  double default_zoom_level =
      Profile::FromBrowserContext(web_contents->GetBrowserContext())->
          GetPrefs()->GetDouble(prefs::kDefaultZoomLevel);

  if (zoom == content::PAGE_ZOOM_RESET) {
    zoom_controller->SetZoomLevel(default_zoom_level);
    content::RecordAction(UserMetricsAction("ZoomNormal"));
    return;
  }

  // Generate a vector of zoom levels from an array of known presets along with
  // the default level added if necessary.
  std::vector<double> zoom_levels = PresetZoomLevels(default_zoom_level);

  if (zoom == content::PAGE_ZOOM_OUT) {
    // Iterate through the zoom levels in reverse order to find the next
    // lower level based on the current zoom level for this page.
    for (std::vector<double>::reverse_iterator i = zoom_levels.rbegin();
         i != zoom_levels.rend(); ++i) {
      double zoom_level = *i;
      if (content::ZoomValuesEqual(zoom_level, current_zoom_level))
        continue;
      if (zoom_level < current_zoom_level) {
        zoom_controller->SetZoomLevel(zoom_level);
        content::RecordAction(UserMetricsAction("ZoomMinus"));
        return;
      }
      content::RecordAction(UserMetricsAction("ZoomMinus_AtMinimum"));
    }
  } else {
    // Iterate through the zoom levels in normal order to find the next
    // higher level based on the current zoom level for this page.
    for (std::vector<double>::const_iterator i = zoom_levels.begin();
         i != zoom_levels.end(); ++i) {
      double zoom_level = *i;
      if (content::ZoomValuesEqual(zoom_level, current_zoom_level))
        continue;
      if (zoom_level > current_zoom_level) {
        zoom_controller->SetZoomLevel(zoom_level);
        content::RecordAction(UserMetricsAction("ZoomPlus"));
        return;
      }
    }
    content::RecordAction(UserMetricsAction("ZoomPlus_AtMaximum"));
  }
}

}  // namespace chrome_page_zoom
