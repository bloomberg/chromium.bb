// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/browser_plugin/browser_plugin_backing_store.h"

#include "ui/gfx/canvas.h"
#include "ui/gfx/rect.h"
#include "ui/surface/transport_dib.h"

namespace content {

// Max height and width for layers
static const int kMaxSize = 23170;

BrowserPluginBackingStore::BrowserPluginBackingStore(
    const gfx::Size& size,
    float scale_factor)
    : size_(size),
      scale_factor_(scale_factor) {
  gfx::Size pixel_size = size.Scale(scale_factor);
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
    TransportDIB* dib) {
  if (bitmap_rect.IsEmpty())
    return;

  gfx::Rect pixel_bitmap_rect = bitmap_rect.Scale(scale_factor_);

  const int width = pixel_bitmap_rect.width();
  const int height = pixel_bitmap_rect.height();

  if (width <= 0 || width > kMaxSize ||
      height <= 0 || height > kMaxSize)
    return;

  if (!dib)
    return;

  SkPaint copy_paint;
  copy_paint.setXfermodeMode(SkXfermode::kSrc_Mode);

  SkBitmap sk_bitmap;
  sk_bitmap.setConfig(SkBitmap::kARGB_8888_Config, width, height);
  sk_bitmap.setPixels(dib->memory());
  for (size_t i = 0; i < copy_rects.size(); i++) {
    const gfx::Rect& pixel_copy_rect = copy_rects[i].Scale(scale_factor_);
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
    int dx,
    int dy,
    const gfx::Rect& clip_rect,
    const gfx::Size& view_size) {
  gfx::Rect pixel_rect = clip_rect.Scale(scale_factor_);
  int pixel_dx = dx * scale_factor_;
  int pixel_dy = dy * scale_factor_;

  int x = std::min(pixel_rect.x(), pixel_rect.x() - pixel_dx);
  int y = std::min(pixel_rect.y(), pixel_rect.y() - pixel_dy);
  int w = pixel_rect.width() + abs(pixel_dx);
  int h = pixel_rect.height() + abs(pixel_dy);
  SkIRect rect = SkIRect::MakeXYWH(x, y, w, h);
  bitmap_.scrollRect(&rect, pixel_dx, pixel_dy);
}

}  // namespace content
