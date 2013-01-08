// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_CACHING_BITMAP_CONTENT_LAYER_UPDATER_H_
#define CC_CACHING_BITMAP_CONTENT_LAYER_UPDATER_H_

#include "base/compiler_specific.h"
#include "cc/bitmap_content_layer_updater.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace cc {

class CachingBitmapContentLayerUpdater
    : public BitmapContentLayerUpdater {
 public:
  static scoped_refptr<CachingBitmapContentLayerUpdater> Create(
      scoped_ptr<LayerPainter>);

  virtual void prepareToUpdate(const gfx::Rect& content_rect,
                               const gfx::Size& tile_size,
                               float contents_width_scale,
                               float contents_height_scale,
                               gfx::Rect& resulting_opaque_rect,
                               RenderingStats&) OVERRIDE;

  bool pixelsDidChange() const;

 private:
  explicit CachingBitmapContentLayerUpdater(
      scoped_ptr<LayerPainter> painter);
  virtual ~CachingBitmapContentLayerUpdater();

  bool pixels_did_change_;
  SkBitmap cached_bitmap_;
};

}  // namespace cc

#endif  // CC_CACHING_BITMAP_CONTENT_LAYER_UPDATER_H_
