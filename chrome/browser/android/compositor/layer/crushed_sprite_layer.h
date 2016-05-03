// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_COMPOSITOR_LAYER_CRUSHED_SPRITE_LAYER_H_
#define CHROME_BROWSER_ANDROID_COMPOSITOR_LAYER_CRUSHED_SPRITE_LAYER_H_

#include "base/macros.h"
#include "chrome/browser/android/compositor/layer/layer.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkRefCnt.h"

class SkCanvas;

namespace cc {
class UIResourceLayer;
}

namespace ui {
class CrushedSpriteResource;
class ResourceManager;
}

namespace chrome {
namespace android {

// A layer which manages drawing frames from a CrushedSpriteResource into an
// SkCanvas backed by an SkBitmap. The final SkBitmap is passed to a
// UIResourceLayer for display.
class CrushedSpriteLayer : public Layer {
 public:
  static scoped_refptr<CrushedSpriteLayer> Create();

  // Loads the resource, calculates the sprite frame to display based on
  // |completion_percentage|,  draws the rectangles for the frame on top
  // of the previous frame and sends to layer_ for display.
  void DrawSpriteFrame(ui::ResourceManager* resource_manager,
                       int bitmap_res_id,
                       int metadata_res_id,
                       float completion_percentage);

  // Layer overrides.
  scoped_refptr<cc::Layer> layer() override;

 protected:
  CrushedSpriteLayer();
  ~CrushedSpriteLayer() override;

 private:
  // Draws the rectangles for |frame| to |canvas|.
  void DrawRectanglesForFrame(ui::CrushedSpriteResource* resource,
                              int frame,
                              sk_sp<SkCanvas> canvas);

  scoped_refptr<cc::UIResourceLayer> layer_;
  int frame_count_;
  int previous_frame_;
  SkBitmap previous_frame_bitmap_;

  DISALLOW_COPY_AND_ASSIGN(CrushedSpriteLayer);
};

}  // namespace android
}  // namespace chrome

#endif  // CHROME_BROWSER_ANDROID_COMPOSITOR_LAYER_CRUSHED_SPRITE_LAYER_H_
