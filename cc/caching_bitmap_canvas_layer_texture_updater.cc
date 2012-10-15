// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "caching_bitmap_canvas_layer_texture_updater.h"

#include "LayerPainterChromium.h"
#include "skia/ext/platform_canvas.h"

namespace cc {

PassRefPtr<CachingBitmapCanvasLayerTextureUpdater>
CachingBitmapCanvasLayerTextureUpdater::Create(
    scoped_ptr<LayerPainterChromium> painter) {
  return adoptRef(new CachingBitmapCanvasLayerTextureUpdater(painter.Pass()));
}

CachingBitmapCanvasLayerTextureUpdater::CachingBitmapCanvasLayerTextureUpdater(
    scoped_ptr<LayerPainterChromium> painter)
    : BitmapCanvasLayerTextureUpdater(painter.Pass()),
      pixels_did_change_(false) {
}

void CachingBitmapCanvasLayerTextureUpdater::prepareToUpdate(
    const IntRect& content_rect,
    const IntSize& tile_size,
    float contents_width_scale,
    float contents_height_scale,
    IntRect& resulting_opaque_rect,
    CCRenderingStats& stats) {
  BitmapCanvasLayerTextureUpdater::prepareToUpdate(content_rect,
                                                   tile_size,
                                                   contents_width_scale,
                                                   contents_height_scale,
                                                   resulting_opaque_rect,
                                                   stats);

  const SkBitmap& new_bitmap = m_canvas->getDevice()->accessBitmap(false);
  SkAutoLockPixels lock(new_bitmap);
  ASSERT(new_bitmap.bytesPerPixel() > 0);
  pixels_did_change_ = new_bitmap.config() != cached_bitmap_.config() ||
                       new_bitmap.height() != cached_bitmap_.height() ||
                       new_bitmap.width() != cached_bitmap_.width() ||
                       memcmp(new_bitmap.getPixels(),
                              cached_bitmap_.getPixels(),
                              new_bitmap.getSafeSize());

  if (pixels_did_change_)
    new_bitmap.deepCopyTo(&cached_bitmap_, new_bitmap.config());
}

bool CachingBitmapCanvasLayerTextureUpdater::pixelsDidChange() const
{
  return pixels_did_change_;
}

}  // namespace cc
