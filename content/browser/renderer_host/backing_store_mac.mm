// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "content/browser/renderer_host/backing_store_mac.h"

#include <cmath>

#include "base/logging.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/public/browser/render_widget_host_view.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/rect_conversions.h"
#include "ui/gfx/size_conversions.h"
#include "ui/gfx/scoped_cg_context_save_gstate_mac.h"
#include "ui/surface/transport_dib.h"

namespace content {

// Mac Backing Stores:
//
// Since backing stores are only ever written to or drawn into windows, we keep
// our backing store in a CGLayer that can get cached in GPU memory.  This
// allows acclerated drawing into the layer and lets scrolling and such happen
// all or mostly on the GPU, which is good for performance.

BackingStoreMac::BackingStoreMac(RenderWidgetHost* widget,
                                 const gfx::Size& size,
                                 float device_scale_factor)
    : BackingStore(widget, size), device_scale_factor_(device_scale_factor) {
  cg_layer_.reset(CreateCGLayer());
  if (!cg_layer_) {
    // The view isn't in a window yet.  Use a CGBitmapContext for now.
    cg_bitmap_.reset(CreateCGBitmapContext());
    CGContextScaleCTM(cg_bitmap_, device_scale_factor_, device_scale_factor_);
  }
}

BackingStoreMac::~BackingStoreMac() {
}

void BackingStoreMac::ScaleFactorChanged(float device_scale_factor) {
  if (device_scale_factor == device_scale_factor_)
    return;

  device_scale_factor_ = device_scale_factor;

  base::mac::ScopedCFTypeRef<CGLayerRef> new_layer(CreateCGLayer());
  // If we have a layer, copy the old contents. A pixelated flash is better
  // than a white flash.
  if (new_layer && cg_layer_) {
    CGContextRef layer = CGLayerGetContext(new_layer);
    CGContextDrawLayerAtPoint(layer, CGPointMake(0, 0), cg_layer_);
  }

  cg_layer_.swap(new_layer);
  if (!cg_layer_) {
    // The view isn't in a window yet.  Use a CGBitmapContext for now.
    cg_bitmap_.reset(CreateCGBitmapContext());
    CGContextScaleCTM(cg_bitmap_, device_scale_factor_, device_scale_factor_);
  }
}

size_t BackingStoreMac::MemorySize() {
  return gfx::ToFlooredSize(
      gfx::ScaleSize(size(), device_scale_factor_)).GetArea() * 4;
}

void BackingStoreMac::PaintToBackingStore(
    RenderProcessHost* process,
    TransportDIB::Id bitmap,
    const gfx::Rect& bitmap_rect,
    const std::vector<gfx::Rect>& copy_rects,
    float scale_factor,
    const base::Closure& completion_callback,
    bool* scheduled_completion_callback) {
  *scheduled_completion_callback = false;
  DCHECK_NE(static_cast<bool>(cg_layer()), static_cast<bool>(cg_bitmap()));

  TransportDIB* dib = process->GetTransportDIB(bitmap);
  if (!dib)
    return;

  gfx::Size pixel_size = gfx::ToFlooredSize(
      gfx::ScaleSize(size(), device_scale_factor_));
  gfx::Rect pixel_bitmap_rect = ToFlooredRectDeprecated(
      gfx::ScaleRect(bitmap_rect, scale_factor));

  size_t bitmap_byte_count =
      pixel_bitmap_rect.width() * pixel_bitmap_rect.height() * 4;
  DCHECK_GE(dib->size(), bitmap_byte_count);

  base::mac::ScopedCFTypeRef<CGDataProviderRef> data_provider(
      CGDataProviderCreateWithData(NULL, dib->memory(),
      bitmap_byte_count, NULL));

  base::mac::ScopedCFTypeRef<CGImageRef> bitmap_image(
      CGImageCreate(pixel_bitmap_rect.width(), pixel_bitmap_rect.height(),
          8, 32, 4 * pixel_bitmap_rect.width(),
          base::mac::GetSystemColorSpace(),
          kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Host,
          data_provider, NULL, false, kCGRenderingIntentDefault));

  for (size_t i = 0; i < copy_rects.size(); i++) {
    const gfx::Rect& copy_rect = copy_rects[i];
    gfx::Rect pixel_copy_rect = ToFlooredRectDeprecated(
        gfx::ScaleRect(copy_rect, scale_factor));

    // Only the subpixels given by copy_rect have pixels to copy.
    base::mac::ScopedCFTypeRef<CGImageRef> image(
        CGImageCreateWithImageInRect(bitmap_image, CGRectMake(
            pixel_copy_rect.x() - pixel_bitmap_rect.x(),
            pixel_copy_rect.y() - pixel_bitmap_rect.y(),
            pixel_copy_rect.width(),
            pixel_copy_rect.height())));

    if (!cg_layer()) {
      // The view may have moved to a window.  Try to get a CGLayer.
      cg_layer_.reset(CreateCGLayer());
      if (cg_layer()) {
        // Now that we have a layer, copy the cached image into it.
        base::mac::ScopedCFTypeRef<CGImageRef> bitmap_image(
            CGBitmapContextCreateImage(cg_bitmap_));
        CGContextDrawImage(CGLayerGetContext(cg_layer()),
                           CGRectMake(0, 0, size().width(), size().height()),
                           bitmap_image);
        // Discard the cache bitmap, since we no longer need it.
        cg_bitmap_.reset(NULL);
      }
    }

    DCHECK_NE(static_cast<bool>(cg_layer()), static_cast<bool>(cg_bitmap()));

    if (cg_layer()) {
      // The CGLayer's origin is in the lower left, but flipping the CTM would
      // cause the image to get drawn upside down.  So we move the rectangle
      // to the right position before drawing the image.
      CGContextRef layer = CGLayerGetContext(cg_layer());
      gfx::Rect paint_rect = copy_rect;
      paint_rect.set_y(size().height() - copy_rect.bottom());
      CGContextDrawImage(layer, paint_rect.ToCGRect(), image);
    } else {
      // The layer hasn't been created yet, so draw into the cache bitmap.
      gfx::Rect paint_rect = copy_rect;
      paint_rect.set_y(size().height() - copy_rect.bottom());
      CGContextDrawImage(cg_bitmap_, paint_rect.ToCGRect(), image);
    }
  }
}

bool BackingStoreMac::CopyFromBackingStore(const gfx::Rect& rect,
                                           skia::PlatformBitmap* output) {
  // TODO(thakis): Make sure this works with HiDPI backing stores.
  if (!output->Allocate(rect.width(), rect.height(), true))
    return false;

  CGContextRef temp_context = output->GetSurface();
  gfx::ScopedCGContextSaveGState save_gstate(temp_context);
  CGContextTranslateCTM(temp_context, 0.0, size().height());
  CGContextScaleCTM(temp_context, 1.0, -1.0);
  CGContextDrawLayerAtPoint(temp_context, CGPointMake(-rect.x(), -rect.y()),
                            cg_layer());
  return true;
}

// Scroll the contents of our CGLayer
void BackingStoreMac::ScrollBackingStore(const gfx::Vector2d& delta,
                                         const gfx::Rect& clip_rect,
                                         const gfx::Size& view_size) {
  DCHECK_NE(static_cast<bool>(cg_layer()), static_cast<bool>(cg_bitmap()));

  // "Scroll" the contents of the layer by creating a new CGLayer,
  // copying the contents of the old one into the new one offset by the scroll
  // amount, swapping in the new CGLayer, and then painting in the new data.
  //
  // The Windows code always sets the whole backing store as the source of the
  // scroll. Thus, we only have to worry about pixels which will end up inside
  // the clipping rectangle. (Note that the clipping rectangle is not
  // translated by the scroll.)

  // We assume |clip_rect| is contained within the backing store.
  DCHECK(clip_rect.bottom() <= size().height());
  DCHECK(clip_rect.right() <= size().width());

  if ((delta.x() || delta.y()) &&
       abs(delta.x()) < size().width() && abs(delta.y()) < size().height()) {
    if (cg_layer()) {
      CGContextRef layer = CGLayerGetContext(cg_layer());
      gfx::ScopedCGContextSaveGState save_gstate(layer);
      CGContextClipToRect(layer,
                          CGRectMake(clip_rect.x(),
                                     size().height() - clip_rect.bottom(),
                                     clip_rect.width(),
                                     clip_rect.height()));
      CGContextDrawLayerAtPoint(layer,
                                CGPointMake(delta.x(), -delta.y()), cg_layer());
    } else {
      // We don't have a layer, so scroll the contents of the CGBitmapContext.
      base::mac::ScopedCFTypeRef<CGImageRef> bitmap_image(
          CGBitmapContextCreateImage(cg_bitmap_));
      gfx::ScopedCGContextSaveGState save_gstate(cg_bitmap_);
      CGContextClipToRect(cg_bitmap_,
                          CGRectMake(clip_rect.x(),
                                     size().height() - clip_rect.bottom(),
                                     clip_rect.width(),
                                     clip_rect.height()));
      CGContextDrawImage(cg_bitmap_,
                         CGRectMake(delta.x(), -delta.y(),
                                    size().width(), size().height()),
                         bitmap_image);
    }
  }
}

void BackingStoreMac::CopyFromBackingStoreToCGContext(const CGRect& dest_rect,
                                                      CGContextRef context) {
  gfx::ScopedCGContextSaveGState save_gstate(context);
  CGContextSetInterpolationQuality(context, kCGInterpolationHigh);
  if (cg_layer_) {
    CGContextDrawLayerInRect(context, dest_rect, cg_layer_);
  } else {
    base::mac::ScopedCFTypeRef<CGImageRef> image(
        CGBitmapContextCreateImage(cg_bitmap_));
    CGContextDrawImage(context, dest_rect, image);
  }
}

CGLayerRef BackingStoreMac::CreateCGLayer() {
  // The CGLayer should be optimized for drawing into the containing window,
  // so extract a CGContext corresponding to the window to be passed to
  // CGLayerCreateWithContext.
  NSWindow* window = [render_widget_host()->GetView()->GetNativeView() window];
  if ([window windowNumber] <= 0) {
    // This catches a nil |window|, as well as windows that exist but that
    // aren't yet connected to WindowServer.
    return NULL;
  }

  NSGraphicsContext* ns_context = [window graphicsContext];
  DCHECK(ns_context);

  CGContextRef cg_context = static_cast<CGContextRef>(
      [ns_context graphicsPort]);
  DCHECK(cg_context);

  // Note: This takes the backingScaleFactor of cg_context into account. The
  // bitmap backing |layer| will be size() * 2 in HiDPI mode automatically.
  CGLayerRef layer = CGLayerCreateWithContext(cg_context,
                                              size().ToCGSize(),
                                              NULL);
  DCHECK(layer);

  return layer;
}

CGContextRef BackingStoreMac::CreateCGBitmapContext() {
  gfx::Size pixel_size = gfx::ToFlooredSize(
      gfx::ScaleSize(size(), device_scale_factor_));
  // A CGBitmapContext serves as a stand-in for the layer before the view is
  // in a containing window.
  CGContextRef context = CGBitmapContextCreate(NULL,
                                               pixel_size.width(),
                                               pixel_size.height(),
                                               8, pixel_size.width() * 4,
                                               base::mac::GetSystemColorSpace(),
                                               kCGImageAlphaPremultipliedFirst |
                                                   kCGBitmapByteOrder32Host);
  DCHECK(context);

  return context;
}

}  // namespace content
