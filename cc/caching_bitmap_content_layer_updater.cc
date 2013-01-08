// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "caching_bitmap_content_layer_updater.h"

#include "base/logging.h"
#include "cc/layer_painter.h"
#include "skia/ext/platform_canvas.h"

namespace cc {

scoped_refptr<CachingBitmapContentLayerUpdater>
CachingBitmapContentLayerUpdater::Create(
    scoped_ptr<LayerPainter> painter) {
  return make_scoped_refptr(new CachingBitmapContentLayerUpdater(
      painter.Pass()));
}

CachingBitmapContentLayerUpdater::CachingBitmapContentLayerUpdater(
    scoped_ptr<LayerPainter> painter)
    : BitmapContentLayerUpdater(painter.Pass()),
      pixels_did_change_(false) {
}

CachingBitmapContentLayerUpdater::
  ~CachingBitmapContentLayerUpdater()
{
}

void CachingBitmapContentLayerUpdater::prepareToUpdate(
    const gfx::Rect& content_rect,
    const gfx::Size& tile_size,
    float contents_width_scale,
    float contents_height_scale,
    gfx::Rect& resulting_opaque_rect,
    RenderingStats& stats) {
  BitmapContentLayerUpdater::prepareToUpdate(
      content_rect,
      tile_size,
      contents_width_scale,
      contents_height_scale,
      resulting_opaque_rect,
      stats);

  const SkBitmap& new_bitmap = m_canvas->getDevice()->accessBitmap(false);
  SkAutoLockPixels lock(new_bitmap);
  DCHECK(new_bitmap.bytesPerPixel() > 0);
  pixels_did_change_ = new_bitmap.config() != cached_bitmap_.config() ||
                       new_bitmap.height() != cached_bitmap_.height() ||
                       new_bitmap.width() != cached_bitmap_.width() ||
                       memcmp(new_bitmap.getPixels(),
                              cached_bitmap_.getPixels(),
                              new_bitmap.getSafeSize());

  if (pixels_did_change_)
    new_bitmap.deepCopyTo(&cached_bitmap_, new_bitmap.config());
}

bool CachingBitmapContentLayerUpdater::pixelsDidChange() const
{
  return pixels_did_change_;
}

}  // namespace cc
