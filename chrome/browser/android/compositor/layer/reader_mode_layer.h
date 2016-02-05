// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_COMPOSITOR_LAYER_READER_MODE_LAYER_H_
#define CHROME_BROWSER_ANDROID_COMPOSITOR_LAYER_READER_MODE_LAYER_H_

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/android/compositor/layer/overlay_panel_layer.h"

namespace content {
class ContentViewCore;
}

namespace ui {
class ResourceManager;
}

namespace chrome {
namespace android {

class ReaderModeLayer : public OverlayPanelLayer {
 public:
  static scoped_refptr<ReaderModeLayer> Create(
      ui::ResourceManager* resource_manager);

  void SetProperties(float dp_to_px,
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
                     float bar_shadow_opacity);

 protected:
  explicit ReaderModeLayer(ui::ResourceManager* resource_manager);
  ~ReaderModeLayer() override;
};

}  //  namespace android
}  //  namespace chrome

#endif  // CHROME_BROWSER_ANDROID_COMPOSITOR_LAYER_READER_MODE_LAYER_H_
