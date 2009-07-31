// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/gfx/blit.h"

#if defined(OS_LINUX)
#include <cairo/cairo.h>
#endif

#include "base/gfx/point.h"
#include "base/gfx/rect.h"
#if defined(OS_MACOSX)
#include "base/scoped_cftyperef.h"
#endif
#include "skia/ext/platform_canvas.h"
#include "skia/ext/platform_device.h"

namespace gfx {

void BlitContextToContext(NativeDrawingContext dst_context,
                          const Rect& dst_rect,
                          NativeDrawingContext src_context,
                          const Point& src_origin) {
#if defined(OS_WIN)
  BitBlt(dst_context, dst_rect.x(), dst_rect.y(),
         dst_rect.width(), dst_rect.height(),
         src_context, src_origin.x(), src_origin.y(), SRCCOPY);
#elif defined(OS_MACOSX)
  // Only translations and/or vertical flips in the source context are
  // supported; more complex source context transforms will be ignored.

  // If there is a translation on the source context, we need to account for
  // it ourselves since CGBitmapContextCreateImage will bypass it.
  Rect src_rect(src_origin, dst_rect.size());
  CGAffineTransform transform = CGContextGetCTM(src_context);
  bool flipped = fabs(transform.d + 1) < 0.0001;
  CGFloat delta_y = flipped ? CGBitmapContextGetHeight(src_context) -
                              transform.ty
                            : transform.ty;
  src_rect.Offset(transform.tx, delta_y);

  scoped_cftyperef<CGImageRef>
      src_image(CGBitmapContextCreateImage(src_context));
  scoped_cftyperef<CGImageRef> src_sub_image(
      CGImageCreateWithImageInRect(src_image, src_rect.ToCGRect()));
  CGContextDrawImage(dst_context, dst_rect.ToCGRect(), src_sub_image);
#elif defined(OS_LINUX)
  // Only translations in the source context are supported; more complex
  // source context transforms will be ignored.
  cairo_save(dst_context);
  double surface_x = src_origin.x();
  double surface_y = src_origin.y();
  cairo_user_to_device(src_context, &surface_x, &surface_y);
  cairo_set_source_surface(dst_context, cairo_get_target(src_context),
                           dst_rect.x()-surface_x, dst_rect.y()-surface_y);
  cairo_rectangle(dst_context, dst_rect.x(), dst_rect.y(),
                  dst_rect.width(), dst_rect.height());
  cairo_clip(dst_context);
  cairo_paint(dst_context);
  cairo_restore(dst_context);
#endif
}

static NativeDrawingContext GetContextFromCanvas(
    skia::PlatformCanvas *canvas) {
  skia::PlatformDevice& device = canvas->getTopPlatformDevice();
#if defined(OS_WIN)
  return device.getBitmapDC();
#elif defined(OS_MACOSX)
  return device.GetBitmapContext();
#elif defined(OS_LINUX)
  return device.beginPlatformPaint();
#endif
}

void BlitContextToCanvas(skia::PlatformCanvas *dst_canvas,
                         const Rect& dst_rect,
                         NativeDrawingContext src_context,
                         const Point& src_origin) {
  BlitContextToContext(GetContextFromCanvas(dst_canvas), dst_rect,
                       src_context, src_origin);
}

void BlitCanvasToContext(NativeDrawingContext dst_context,
                         const Rect& dst_rect,
                         skia::PlatformCanvas *src_canvas,
                         const Point& src_origin) {
  BlitContextToContext(dst_context, dst_rect,
                       GetContextFromCanvas(src_canvas), src_origin);
}

void BlitCanvasToCanvas(skia::PlatformCanvas *dst_canvas,
                        const Rect& dst_rect,
                        skia::PlatformCanvas *src_canvas,
                        const Point& src_origin) {
  BlitContextToContext(GetContextFromCanvas(dst_canvas), dst_rect,
                       GetContextFromCanvas(src_canvas), src_origin);
}

}  // namespace gfx
