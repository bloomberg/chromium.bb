// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/backing_store_skia.h"

#include "content/browser/renderer_host/render_process_host_impl.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/canvas_skia.h"
#include "ui/gfx/rect.h"

// Assume that somewhere along the line, someone will do width * height * 4
// with signed numbers. If the maximum value is 2**31, then 2**31 / 4 =
// 2**29 and floor(sqrt(2**29)) = 23170.

// Max height and width for layers
static const int kMaxVideoLayerSize = 23170;

BackingStoreSkia::BackingStoreSkia(RenderWidgetHost* widget,
                                   const gfx::Size& size)
    : BackingStore(widget, size) {
  bitmap_.setConfig(SkBitmap::kARGB_8888_Config, size.width(), size.height());
  bitmap_.allocPixels();
  canvas_.reset(new SkCanvas(bitmap_));
}

BackingStoreSkia::~BackingStoreSkia() {
}

void BackingStoreSkia::SkiaShowRect(const gfx::Point& point,
                                    gfx::Canvas* canvas) {
  canvas->GetSkCanvas()->drawBitmap(bitmap_,
      SkIntToScalar(point.x()), SkIntToScalar(point.y()));
}

size_t BackingStoreSkia::MemorySize() {
  // NOTE: The computation may be different when the canvas is a subrectangle of
  // a larger bitmap.
  return size().GetArea() * 4;
}

void BackingStoreSkia::PaintToBackingStore(
    content::RenderProcessHost* process,
    TransportDIB::Id bitmap,
    const gfx::Rect& bitmap_rect,
    const std::vector<gfx::Rect>& copy_rects,
    const base::Closure& completion_callback,
    bool* scheduled_completion_callback) {
  *scheduled_completion_callback = false;
  if (bitmap_rect.IsEmpty())
    return;

  const int width = bitmap_rect.width();
  const int height = bitmap_rect.height();

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
    const gfx::Rect& copy_rect = copy_rects[i];
    int x = copy_rect.x() - bitmap_rect.x();
    int y = copy_rect.y() - bitmap_rect.y();
    int w = copy_rect.width();
    int h = copy_rect.height();
    SkIRect srcrect = SkIRect::MakeXYWH(x, y, w, h);
    SkRect dstrect = SkRect::MakeXYWH(
        SkIntToScalar(copy_rect.x()), SkIntToScalar(copy_rect.y()),
        SkIntToScalar(w), SkIntToScalar(h));
    canvas_.get()->drawBitmapRect(sk_bitmap, &srcrect, dstrect, &copy_paint);
  }
}

void BackingStoreSkia::ScrollBackingStore(int dx, int dy,
                                          const gfx::Rect& clip_rect,
                                          const gfx::Size& view_size) {
  int x = std::min(clip_rect.x(), clip_rect.x() - dx);
  int y = std::min(clip_rect.y(), clip_rect.y() - dy);
  int w = clip_rect.width() + abs(dx);
  int h = clip_rect.height() + abs(dy);
  SkIRect rect = SkIRect::MakeXYWH(x, y, w, h);
  bitmap_.scrollRect(&rect, dx, dy);
}

bool BackingStoreSkia::CopyFromBackingStore(const gfx::Rect& rect,
                                            skia::PlatformCanvas* output) {
  const int width = std::min(size().width(), rect.width());
  const int height = std::min(size().height(), rect.height());
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
