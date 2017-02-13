// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/renderer/window_features_converter.h"

namespace content {

blink::mojom::WindowFeaturesPtr ConvertWebWindowFeaturesToMojoWindowFeatures(
    const blink::WebWindowFeatures& web_window_features) {
  blink::mojom::WindowFeaturesPtr result = blink::mojom::WindowFeatures::New();
  result->x = web_window_features.x;
  result->has_x = web_window_features.xSet;
  result->y = web_window_features.y;
  result->has_y = web_window_features.ySet;
  result->width = web_window_features.width;
  result->has_width = web_window_features.widthSet;
  result->height = web_window_features.height;
  result->has_height = web_window_features.heightSet;
  result->menu_bar_visible = web_window_features.menuBarVisible;
  result->status_bar_visible = web_window_features.statusBarVisible;
  result->tool_bar_visible = web_window_features.toolBarVisible;
  result->location_bar_visible = web_window_features.locationBarVisible;
  result->scrollbars_visible = web_window_features.scrollbarsVisible;
  result->resizable = web_window_features.resizable;
  result->fullscreen = web_window_features.fullscreen;
  result->dialog = web_window_features.dialog;
  return result;
}

blink::WebWindowFeatures ConvertMojoWindowFeaturesToWebWindowFeatures(
    const blink::mojom::WindowFeatures& window_features) {
  blink::WebWindowFeatures result;
  result.x = window_features.x;
  result.xSet = window_features.has_x;
  result.y = window_features.y;
  result.ySet = window_features.has_y;
  result.width = window_features.width;
  result.widthSet = window_features.has_width;
  result.height = window_features.height;
  result.heightSet = window_features.has_height;
  result.menuBarVisible = window_features.menu_bar_visible;
  result.statusBarVisible = window_features.status_bar_visible;
  result.toolBarVisible = window_features.tool_bar_visible;
  result.locationBarVisible = window_features.location_bar_visible;
  result.scrollbarsVisible = window_features.scrollbars_visible;
  result.resizable = window_features.resizable;
  result.fullscreen = window_features.fullscreen;
  result.dialog = window_features.dialog;
  return result;
}

}  // namespace content
