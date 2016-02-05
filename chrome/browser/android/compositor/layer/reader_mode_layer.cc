// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/compositor/layer/reader_mode_layer.h"

#include "cc/layers/layer.h"
#include "cc/resources/scoped_ui_resource.h"
#include "content/public/browser/android/compositor.h"
#include "content/public/browser/android/content_view_core.h"
#include "ui/android/resources/resource_manager.h"

namespace chrome {
namespace android {

// static
scoped_refptr<ReaderModeLayer> ReaderModeLayer::Create(
    ui::ResourceManager* resource_manager) {
  return make_scoped_refptr(new ReaderModeLayer(resource_manager));
}

void ReaderModeLayer::SetProperties(
    float dp_to_px,
    content::ContentViewCore* content_view_core,
    float panel_x,
    float panel_y,
    float panel_width,
    float panel_height,
    float bar_margin_side,
    float bar_height,
    float text_opacity,
    bool bar_border_visible,
    float bar_border_height,
    bool bar_shadow_visible,
    float bar_shadow_opacity) {
  // Round values to avoid pixel gap between layers.
  bar_height = floor(bar_height);

  OverlayPanelLayer::SetProperties(
      dp_to_px,
      content_view_core,
      bar_height,
      panel_x,
      panel_y,
      panel_width,
      panel_height,
      bar_margin_side,
      bar_height,
      0.0f,
      text_opacity,
      bar_border_visible,
      bar_border_height,
      bar_shadow_visible,
      bar_shadow_opacity,
      1.0f);
}

ReaderModeLayer::ReaderModeLayer(
    ui::ResourceManager* resource_manager)
    : OverlayPanelLayer(resource_manager) {
}

ReaderModeLayer::~ReaderModeLayer() {
}

}  //  namespace android
}  //  namespace chrome
