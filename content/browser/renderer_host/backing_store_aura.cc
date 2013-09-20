// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/backing_store_aura.h"

#include "content/browser/renderer_host/dip_util.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/public/browser/render_widget_host.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/rect_conversions.h"
#include "ui/gfx/size_conversions.h"
#include "ui/gfx/vector2d_conversions.h"

namespace {

gfx::Size ToPixelSize(gfx::Size dipSize, float scale) {
  return gfx::ToCeiledSize(gfx::ScaleSize(dipSize, scale));
}

}  // namespace


namespace content {

// Assume that somewhere along the line, someone will do width * height * 4
// with signed numbers. If the maximum value is 2**31, then 2**31 / 4 =
// 2**29 and floor(sqrt(2**29)) = 23170.

// Max height and width for layers
static const int kMaxVideoLayerSize = 23170;

BackingStoreAura::BackingStoreAura(RenderWidgetHost* widget,
                                   const gfx::Size& size)
    : BackingStore(widget, size) {
  device_scale_factor_ =
      ui::GetScaleFactorScale(GetScaleFactorForView(widget->GetView()));
  gfx::Size pixel_size = ToPixelSize(size, device_scale_factor_);
  bitmap_.setConfig(SkBitmap::kARGB_8888_Config,
      pixel_size.width(), pixel_size.height());
  bitmap_.allocPixels();
  canvas_.reset(new SkCanvas(bitmap_));
}

BackingStoreAura::~BackingStoreAura() {
}

void BackingStoreAura::SkiaShowRect(const gfx::Point& point,
                                    gfx::Canvas* canvas) {
  gfx::ImageSkia image = gfx::ImageSkia(gfx::ImageSkiaRep(bitmap_,
      ui::GetScaleFactorFromScale(device_scale_factor_)));
  canvas->DrawImageInt(image, point.x(), point.y());
}

void BackingStoreAura::ScaleFactorChanged(float device_scale_factor) {
  if (device_scale_factor == device_scale_factor_)
    return;

  gfx::Size old_pixel_size = ToPixelSize(size(), device_scale_factor_);
  device_scale_factor_ = device_scale_factor;

  gfx::Size pixel_size = ToPixelSize(size(), device_scale_factor_);
  SkBitmap new_bitmap;
  new_bitmap.setConfig(SkBitmap::kARGB_8888_Config,
      pixel_size.width(), pixel_size.height());
  new_bitmap.allocPixels();
  scoped_ptr<SkCanvas> new_canvas(new SkCanvas(new_bitmap));

  // Copy old contents; a low-res flash is better than a black flash.
  SkPaint copy_paint;
  copy_paint.setXfermodeMode(SkXfermode::kSrc_Mode);
  SkIRect src_rect = SkIRect::MakeWH(old_pixel_size.width(),
                                     old_pixel_size.height());
  SkRect dst_rect = SkRect::MakeWH(pixel_size.width(), pixel_size.height());
  new_canvas.get()->drawBitmapRect(bitmap_, &src_rect, dst_rect, &copy_paint);

  canvas_.swap(new_canvas);
  bitmap_ = new_bitmap;
}

size_t BackingStoreAura::MemorySize() {
  // NOTE: The computation may be different when the canvas is a subrectangle of
  // a larger bitmap.
  return ToPixelSize(size(), device_scale_factor_).GetArea() * 4;
}

void BackingStoreAura::PaintToBackingStore(
    RenderProcessHost* process,
    TransportDIB::Id bitmap,
    const gfx::Rect& bitmap_rect,
    const std::vector<gfx::Rect>& copy_rects,
    float scale_factor,
    const base::Closure& completion_callback,
    bool* scheduled_completion_callback) {
  *scheduled_completion_callback = false;
  if (bitmap_rect.IsEmpty())
    return;

  gfx::Rect pixel_bitmap_rect = gfx::ToEnclosingRect(
      gfx::ScaleRect(bitmap_rect, scale_factor));

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
    const gfx::Rect pixel_copy_rect = gfx::ToEnclosingRect(
        gfx::ScaleRect(copy_rects[i], scale_factor));
    int x = pixel_copy_rect.x() - pixel_bitmap_rect.x();
    int y = pixel_copy_rect.y() - pixel_bitmap_rect.y();
    SkIRect srcrect = SkIRect::MakeXYWH(x, y,
        pixel_copy_rect.width(),
        pixel_copy_rect.height());

    const gfx::Rect pixel_copy_dst_rect = gfx::ToEnclosingRect(
        gfx::ScaleRect(copy_rects[i], device_scale_factor_));
    SkRect dstrect = SkRect::MakeXYWH(
        SkIntToScalar(pixel_copy_dst_rect.x()),
        SkIntToScalar(pixel_copy_dst_rect.y()),
        SkIntToScalar(pixel_copy_dst_rect.width()),
        SkIntToScalar(pixel_copy_dst_rect.height()));
    canvas_.get()->drawBitmapRect(sk_bitmap, &srcrect, dstrect, &copy_paint);
  }
}

void BackingStoreAura::ScrollBackingStore(const gfx::Vector2d& delta,
                                          const gfx::Rect& clip_rect,
                                          const gfx::Size& view_size) {
  gfx::Rect pixel_rect = gfx::ToEnclosingRect(
      gfx::ScaleRect(clip_rect, device_scale_factor_));
  gfx::Vector2d pixel_delta = gfx::ToFlooredVector2d(
      gfx::ScaleVector2d(delta, device_scale_factor_));

  int x = std::min(pixel_rect.x(), pixel_rect.x() - pixel_delta.x());
  int y = std::min(pixel_rect.y(), pixel_rect.y() - pixel_delta.y());
  int w = pixel_rect.width() + abs(pixel_delta.x());
  int h = pixel_rect.height() + abs(pixel_delta.y());
  SkIRect rect = SkIRect::MakeXYWH(x, y, w, h);
  bitmap_.scrollRect(&rect, pixel_delta.x(), pixel_delta.y());
}

bool BackingStoreAura::CopyFromBackingStore(const gfx::Rect& rect,
                                            skia::PlatformBitmap* output) {
  const int width =
      std::min(size().width(), rect.width()) * device_scale_factor_;
  const int height =
      std::min(size().height(), rect.height()) * device_scale_factor_;
  if (!output->Allocate(width, height, true))
    return false;

  SkIRect skrect = SkIRect::MakeXYWH(rect.x(), rect.y(), width, height);
  SkBitmap b;
  if (!canvas_->readPixels(skrect, &b))
    return false;
  SkCanvas(output->GetBitmap()).writePixels(b, rect.x(), rect.y());
  return true;
}

}  // namespace content
