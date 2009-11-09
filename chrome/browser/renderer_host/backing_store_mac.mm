// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "chrome/browser/renderer_host/backing_store.h"
#include "chrome/browser/renderer_host/render_widget_host.h"
#include "chrome/browser/renderer_host/render_widget_host_view.h"

#include "base/logging.h"
#include "base/mac_util.h"
#include "chrome/common/transport_dib.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"

// Mac Backing Stores:
//
// Since backing stores are only ever written to or drawn into windows, we keep
// our backing store in a CGLayer that can get cached in GPU memory.  This
// allows acclerated drawing into the layer and lets scrolling and such happen
// all or mostly on the GPU, which is good for performance.

BackingStore::BackingStore(RenderWidgetHost* widget, const gfx::Size& size)
    : render_widget_host_(widget),
      size_(size),
      cg_layer_(NULL) {
  // We want our CGLayer to be optimized for drawing into our containing
  // window, so extract a CGContext corresponding to that window that we can
  // pass to CGLayerCreateWithContext.
  NSWindow* containing_window = [widget->view()->GetNativeView() window];
  if (!containing_window) {
    // If we are not in a containing window yet, create a CGBitmapContext
    // to use as a stand-in for the layer.
    cg_bitmap_.reset(CGBitmapContextCreate(NULL, size.width(), size.height(),
        8, size.width() * 4, mac_util::GetSystemColorSpace(),
        kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Host));
  } else {
    CGContextRef context = static_cast<CGContextRef>(
        [[containing_window graphicsContext] graphicsPort]);
    CGLayerRef layer = CGLayerCreateWithContext(context, size.ToCGSize(), NULL);
    cg_layer_.reset(layer);
  }
}

BackingStore::~BackingStore() {
}

size_t BackingStore::MemorySize() {
  // Estimate memory usage as 4 bytes per pixel.
  return size_.GetArea() * 4;
}

// Paint the contents of a TransportDIB into a rectangle of our CGLayer
void BackingStore::PaintRect(base::ProcessHandle process,
                             TransportDIB* bitmap,
                             const gfx::Rect& bitmap_rect) {
  scoped_cftyperef<CGDataProviderRef> data_provider(
      CGDataProviderCreateWithData(NULL, bitmap->memory(),
      bitmap_rect.width() * bitmap_rect.height() * 4, NULL));
  scoped_cftyperef<CGImageRef> image(
      CGImageCreate(bitmap_rect.width(), bitmap_rect.height(), 8, 32,
          4 * bitmap_rect.width(), mac_util::GetSystemColorSpace(),
          kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Host,
          data_provider, NULL, false, kCGRenderingIntentDefault));

  if (!cg_layer()) {
    // we don't have a CGLayer yet, so see if we can create one.
    NSWindow* containing_window =
        [render_widget_host()->view()->GetNativeView() window];
    if (containing_window) {
      CGContextRef context = static_cast<CGContextRef>(
          [[containing_window graphicsContext] graphicsPort]);
      CGLayerRef layer =
          CGLayerCreateWithContext(context, size().ToCGSize(), NULL);
      cg_layer_.reset(layer);
      // now that we have a layer, copy the cached image into it
      scoped_cftyperef<CGImageRef> bitmap_image(
          CGBitmapContextCreateImage(cg_bitmap_));
      CGContextDrawImage(CGLayerGetContext(layer),
                         CGRectMake(0, 0, size().width(), size().height()),
                         bitmap_image);
      // Discard the cache bitmap, since we no longer need it.
      cg_bitmap_.reset(NULL);
    }
  }

  if (cg_layer()) {
    // The CGLayer's origin is in the lower left, but flipping the CTM would
    // cause the image to get drawn upside down.  So we move the rectangle
    // to the right position before drawing the image.
    CGContextRef layer = CGLayerGetContext(cg_layer());
    gfx::Rect paint_rect = bitmap_rect;
    paint_rect.set_y(size_.height() - bitmap_rect.bottom());
    CGContextDrawImage(layer, paint_rect.ToCGRect(), image);
  } else {
    // The layer hasn't been created yet, so draw into the cache bitmap.
    gfx::Rect paint_rect = bitmap_rect;
    paint_rect.set_y(size_.height() - bitmap_rect.bottom());
    CGContextDrawImage(cg_bitmap_, paint_rect.ToCGRect(), image);
  }
}

// Scroll the contents of our CGLayer
void BackingStore::ScrollRect(base::ProcessHandle process,
                              TransportDIB* bitmap,
                              const gfx::Rect& bitmap_rect,
                              int dx, int dy,
                              const gfx::Rect& clip_rect,
                              const gfx::Size& view_size) {
  // "Scroll" the contents of the layer by creating a new CGLayer,
  // copying the contents of the old one into the new one offset by the scroll
  // amount, swapping in the new CGLayer, and then painting in the new data.
  //
  // The Windows code always sets the whole backing store as the source of the
  // scroll. Thus, we only have to worry about pixels which will end up inside
  // the clipping rectangle. (Note that the clipping rectangle is not
  // translated by the scroll.)

  // We assume |clip_rect| is contained within the backing store.
  CGSize layer_size = CGLayerGetSize(cg_layer());
  DCHECK(clip_rect.bottom() <= layer_size.height);
  DCHECK(clip_rect.right() <= layer_size.width);

  if ((dx && abs(dx) < layer_size.width) ||
      (dy && abs(dy) < layer_size.height)) {
    if (cg_layer()) {
      CGContextRef context = CGLayerGetContext(cg_layer());
      scoped_cftyperef<CGLayerRef> new_layer(
          CGLayerCreateWithContext(context, layer_size, NULL));
      CGContextRef layer = CGLayerGetContext(new_layer);
      CGContextDrawLayerAtPoint(layer, CGPointMake(0, 0), cg_layer());
      CGContextSaveGState(layer);
      CGContextClipToRect(layer, CGRectMake(clip_rect.x(),
                                            size_.height() - clip_rect.bottom(),
                                            clip_rect.width(),
                                            clip_rect.height()));
      CGContextDrawLayerAtPoint(layer, CGPointMake(dx, -dy), cg_layer());
      CGContextRestoreGState(layer);
      cg_layer_.swap(new_layer);
    } else {
      // We don't have a layer, so scroll the contents of the CGBitmapContext.
      scoped_cftyperef<CGContextRef> new_bitmap(
          CGBitmapContextCreate(NULL, size_.width(), size_.height(), 8,
              size_.width() * 4, mac_util::GetSystemColorSpace(),
              kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Host));
      scoped_cftyperef<CGImageRef> bitmap_image(
          CGBitmapContextCreateImage(cg_bitmap_));
      CGContextDrawImage(new_bitmap,
                         CGRectMake(0, 0, size_.width(), size_.height()),
                         bitmap_image);
      CGContextSaveGState(new_bitmap);
      CGContextClipToRect(new_bitmap,
                          CGRectMake(clip_rect.x(),
                                     size_.height() - clip_rect.bottom(),
                                     clip_rect.width(),
                                     clip_rect.height()));
      CGContextDrawImage(new_bitmap,
                         CGRectMake(dx, -dy, size_.width(), size_.height()),
                         bitmap_image);
      CGContextRestoreGState(new_bitmap);
      cg_bitmap_.swap(new_bitmap);
    }
  }
  // Now paint the new bitmap data
  PaintRect(process, bitmap, bitmap_rect);
  return;
}
