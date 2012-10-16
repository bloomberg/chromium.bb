// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CACHING_BITMAP_CANVAS_LAYER_TEXTURE_UPDATER_H_
#define CACHING_BITMAP_CANVAS_LAYER_TEXTURE_UPDATER_H_

#include "base/compiler_specific.h"
#include "cc/bitmap_canvas_layer_texture_updater.h"

namespace cc {

class CachingBitmapCanvasLayerTextureUpdater
    : public BitmapCanvasLayerTextureUpdater {
 public:
  static scoped_refptr<CachingBitmapCanvasLayerTextureUpdater> Create(
      scoped_ptr<LayerPainterChromium>);

  virtual void prepareToUpdate(const IntRect& content_rect,
                               const IntSize& tile_size,
                               float contents_width_scale,
                               float contents_height_scale,
                               IntRect& resulting_opaque_rect,
                               CCRenderingStats&) OVERRIDE;

  bool pixelsDidChange() const;

 private:
  explicit CachingBitmapCanvasLayerTextureUpdater(
      scoped_ptr<LayerPainterChromium> painter);
  virtual ~CachingBitmapCanvasLayerTextureUpdater();

  bool pixels_did_change_;
  SkBitmap cached_bitmap_;
};

}  // namespace cc

#endif  // CACHING_BITMAP_CANVAS_LAYER_TEXTURE_UPDATER_H_
