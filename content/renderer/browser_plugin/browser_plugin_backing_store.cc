// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/browser_plugin/browser_plugin_backing_store.h"

#include "ui/gfx/canvas.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/rect_conversions.h"
#include "ui/gfx/size_conversions.h"
#include "ui/gfx/vector2d_conversions.h"
#include "ui/surface/transport_dib.h"

namespace content {

// Max height and width for layers
static const int kMaxSize = 23170;

BrowserPluginBackingStore::BrowserPluginBackingStore(
    const gfx::Size& size,
    float scale_factor)
    : size_(size),
      scale_factor_(scale_factor) {
  gfx::Size pixel_size = gfx::ToFlooredSize(gfx::ScaleSize(size, scale_factor));
  bitmap_.setConfig(SkBitmap::kARGB_8888_Config,
      pixel_size.width(), pixel_size.height());
  bitmap_.allocPixels();
  canvas_.reset(new SkCanvas(bitmap_));
}

BrowserPluginBackingStore::~BrowserPluginBackingStore() {
}

void BrowserPluginBackingStore::PaintToBackingStore(
    const gfx::Rect& bitmap_rect,
    const std::vector<gfx::Rect>& copy_rects,
    void* bitmap) {
  if (bitmap_rect.IsEmpty())
    return;

  gfx::Rect pixel_bitmap_rect = gfx::ToFlooredRectDeprecated(
      gfx::ScaleRect(bitmap_rect, scale_factor_));

  const int width = pixel_bitmap_rect.width();
  const int height = pixel_bitmap_rect.height();

  if (width <= 0 || width > kMaxSize ||
      height <= 0 || height > kMaxSize)
    return;

  if (!bitmap)
    return;

  SkPaint copy_paint;
  copy_paint.setXfermodeMode(SkXfermode::kSrc_Mode);

  SkBitmap sk_bitmap;
  sk_bitmap.setConfig(SkBitmap::kARGB_8888_Config, width, height);
  sk_bitmap.setPixels(bitmap);
  for (size_t i = 0; i < copy_rects.size(); i++) {
    const gfx::Rect& pixel_copy_rect = gfx::ToEnclosingRect(
        gfx::ScaleRect(copy_rects[i], scale_factor_));
    int x = pixel_copy_rect.x() - pixel_bitmap_rect.x();
    int y = pixel_copy_rect.y() - pixel_bitmap_rect.y();
    SkIRect srcrect = SkIRect::MakeXYWH(x, y,
        pixel_copy_rect.width(),
        pixel_copy_rect.height());

    SkRect dstrect = SkRect::MakeXYWH(
        SkIntToScalar(pixel_copy_rect.x()),
        SkIntToScalar(pixel_copy_rect.y()),
        SkIntToScalar(pixel_copy_rect.width()),
        SkIntToScalar(pixel_copy_rect.height()));
    canvas_.get()->drawBitmapRect(sk_bitmap, &srcrect, dstrect, &copy_paint);
  }
}

void BrowserPluginBackingStore::ScrollBackingStore(
    const gfx::Vector2d& delta,
    const gfx::Rect& clip_rect,
    const gfx::Size& view_size) {
  gfx::Rect pixel_rect = gfx::ToEnclosingRect(
      gfx::ScaleRect(clip_rect, scale_factor_));
  gfx::Vector2d pixel_delta = gfx::ToFlooredVector2d(
      gfx::ScaleVector2d(delta, scale_factor_));

  int x = std::min(pixel_rect.x(), pixel_rect.x() - pixel_delta.x());
  int y = std::min(pixel_rect.y(), pixel_rect.y() - pixel_delta.y());
  int w = pixel_rect.width() + abs(pixel_delta.x());
  int h = pixel_rect.height() + abs(pixel_delta.y());
  SkIRect rect = SkIRect::MakeXYWH(x, y, w, h);
  bitmap_.scrollRect(&rect, pixel_delta.x(), pixel_delta.y());
}

void BrowserPluginBackingStore::Clear(SkColor clear_color) {
  canvas_->clear(clear_color);
}

}  // namespace content
