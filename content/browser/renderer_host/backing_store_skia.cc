// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/backing_store_skia.h"

#include "content/browser/renderer_host/dip_util.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/public/browser/render_widget_host.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/rect.h"

// Assume that somewhere along the line, someone will do width * height * 4
// with signed numbers. If the maximum value is 2**31, then 2**31 / 4 =
// 2**29 and floor(sqrt(2**29)) = 23170.

// Max height and width for layers
static const int kMaxVideoLayerSize = 23170;

BackingStoreSkia::BackingStoreSkia(content::RenderWidgetHost* widget,
                                   const gfx::Size& size)
    : BackingStore(widget, size),
      device_scale_factor_(content::GetDIPScaleFactor(widget->GetView())) {
  gfx::Size pixel_size = size.Scale(device_scale_factor_);
  bitmap_.setConfig(SkBitmap::kARGB_8888_Config,
      pixel_size.width(), pixel_size.height());
  bitmap_.allocPixels();
  canvas_.reset(new SkCanvas(bitmap_));
}

BackingStoreSkia::~BackingStoreSkia() {
}

void BackingStoreSkia::SkiaShowRect(const gfx::Point& point,
                                    gfx::Canvas* canvas) {
  const gfx::Point scaled_point = point.Scale(device_scale_factor_);
  canvas->sk_canvas()->drawBitmap(bitmap_,
      SkIntToScalar(scaled_point.x()), SkIntToScalar(scaled_point.y()));
}

size_t BackingStoreSkia::MemorySize() {
  // NOTE: The computation may be different when the canvas is a subrectangle of
  // a larger bitmap.
  return size().Scale(device_scale_factor_).GetArea() * 4;
}

void BackingStoreSkia::PaintToBackingStore(
    content::RenderProcessHost* process,
    TransportDIB::Id bitmap,
    const gfx::Rect& bitmap_rect,
    const std::vector<gfx::Rect>& copy_rects,
    float scale_factor,
    const base::Closure& completion_callback,
    bool* scheduled_completion_callback) {
  *scheduled_completion_callback = false;
  if (bitmap_rect.IsEmpty())
    return;

  gfx::Rect pixel_bitmap_rect = bitmap_rect.Scale(scale_factor);

  const int width = pixel_bitmap_rect.width();
  const int height = pixel_bitmap_rect.height();

  if (width <= 0 || width > kMaxVideoLayerSize ||
      height <= 0 || height > kMaxVideoLayerSize)
    return;

  TransportDIB* dib = process->GetTransportDIB(bitmap);
  if (!dib)
    return;

  SkPaint copy_paint;
  copy_paint.setXfermodeMode(SkXfermode::kSrc_Mode);

  SkBitmap sk_bitmap;
  sk_bitmap.setConfig(SkBitmap::kARGB_8888_Config, width, height);
  sk_bitmap.setPixels(dib->memory());
  for (size_t i = 0; i < copy_rects.size(); i++) {
    const gfx::Rect pixel_copy_rect = copy_rects[i].Scale(scale_factor);
    int x = pixel_copy_rect.x() - pixel_bitmap_rect.x();
    int y = pixel_copy_rect.y() - pixel_bitmap_rect.y();
    SkIRect srcrect = SkIRect::MakeXYWH(x, y,
        pixel_copy_rect.width(),
        pixel_copy_rect.height());

    const gfx::Rect pixel_copy_dst_rect =
        copy_rects[i].Scale(device_scale_factor_);
    SkRect dstrect = SkRect::MakeXYWH(
        SkIntToScalar(pixel_copy_dst_rect.x()),
        SkIntToScalar(pixel_copy_dst_rect.y()),
        SkIntToScalar(pixel_copy_dst_rect.width()),
        SkIntToScalar(pixel_copy_dst_rect.height()));
    canvas_.get()->drawBitmapRect(sk_bitmap, &srcrect, dstrect, &copy_paint);
  }
}

void BackingStoreSkia::ScrollBackingStore(int dx, int dy,
                                          const gfx::Rect& clip_rect,
                                          const gfx::Size& view_size) {
  gfx::Rect pixel_rect = clip_rect.Scale(device_scale_factor_);
  int x = std::min(pixel_rect.x(), pixel_rect.x() - dx);
  int y = std::min(pixel_rect.y(), pixel_rect.y() - dy);
  int w = pixel_rect.width() + abs(dx);
  int h = pixel_rect.height() + abs(dy);
  SkIRect rect = SkIRect::MakeXYWH(x, y, w, h);
  bitmap_.scrollRect(&rect, dx, dy);
}

bool BackingStoreSkia::CopyFromBackingStore(const gfx::Rect& rect,
                                            skia::PlatformCanvas* output) {
  const int width =
      std::min(size().width(), rect.width()) * device_scale_factor_;
  const int height =
      std::min(size().height(), rect.height()) * device_scale_factor_;
  if (!output->initialize(width, height, true))
    return false;

  SkBitmap bitmap = skia::GetTopDevice(*output)->accessBitmap(true);
  SkIRect skrect = SkIRect::MakeXYWH(rect.x(), rect.y(), width, height);
  SkBitmap b;
  if (!canvas_->readPixels(skrect, &b))
    return false;
  output->writePixels(b, rect.x(), rect.y());
  return true;
}
