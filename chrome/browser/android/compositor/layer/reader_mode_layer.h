// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_COMPOSITOR_LAYER_READER_MODE_LAYER_H_
#define CHROME_BROWSER_ANDROID_COMPOSITOR_LAYER_READER_MODE_LAYER_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/android/compositor/layer/layer.h"

namespace cc {
class Layer;
class NinePatchLayer;
class SolidColorLayer;
class UIResourceLayer;
}

namespace content {
class ContentViewCore;
}

namespace ui {
class ResourceManager;
}

namespace chrome {
namespace android {

class ReaderModeLayer : public Layer {
 public:
  static scoped_refptr<ReaderModeLayer> Create(
      ui::ResourceManager* resource_manager);

  void SetProperties(int panel_background_resource_id,
                     int panel_text_resource_id,
                     content::ContentViewCore* reader_mode_content_view_core,
                     float panel_y,
                     float panel_width,
                     float panel_margin_top,
                     float panel_height,
                     float distilled_y,
                     float distilled_height,
                     float x,
                     float panel_text_opacity,
                     int header_background_color);

  scoped_refptr<cc::Layer> layer() override;

 protected:
  explicit ReaderModeLayer(ui::ResourceManager* resource_manager);
  ~ReaderModeLayer() override;

 private:
  ui::ResourceManager* resource_manager_;

  scoped_refptr<cc::Layer> layer_;
  scoped_refptr<cc::NinePatchLayer> panel_background_;
  scoped_refptr<cc::UIResourceLayer> panel_text_;
  scoped_refptr<cc::NinePatchLayer> content_shadow_;
  scoped_refptr<cc::SolidColorLayer> content_solid_;
  scoped_refptr<cc::Layer> content_view_container_;
};

}  //  namespace android
}  //  namespace chrome

#endif  // CHROME_BROWSER_ANDROID_COMPOSITOR_LAYER_READER_MODE_LAYER_H_
