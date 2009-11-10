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
      size_(size) {
  cg_layer_.reset(CreateCGLayer());
  if (!cg_layer_) {
    // The view isn't in a window yet.  Use a CGBitmapContext for now.
    cg_bitmap_.reset(CreateCGBitmapContext());
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
  DCHECK_NE(static_cast<bool>(cg_layer()), static_cast<bool>(cg_bitmap()));

  scoped_cftyperef<CGDataProviderRef> data_provider(
      CGDataProviderCreateWithData(NULL, bitmap->memory(),
      bitmap_rect.width() * bitmap_rect.height() * 4, NULL));
  scoped_cftyperef<CGImageRef> image(
      CGImageCreate(bitmap_rect.width(), bitmap_rect.height(), 8, 32,
          4 * bitmap_rect.width(), mac_util::GetSystemColorSpace(),
          kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Host,
          data_provider, NULL, false, kCGRenderingIntentDefault));

  if (!cg_layer()) {
    // The view may have moved to a window.  Try to get a CGLayer.
    cg_layer_.reset(CreateCGLayer());
    if (cg_layer()) {
      // now that we have a layer, copy the cached image into it
      scoped_cftyperef<CGImageRef> bitmap_image(
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
  DCHECK(clip_rect.bottom() <= size_.height());
  DCHECK(clip_rect.right() <= size_.width());

  if ((dx || dy) && abs(dx) < size_.width() && abs(dy) < size_.height()) {
    if (cg_layer()) {
      scoped_cftyperef<CGLayerRef> new_layer(CreateCGLayer());

      // If the current view is in a window, the replacement must be too.
      DCHECK(new_layer);

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
      scoped_cftyperef<CGContextRef> new_bitmap(CreateCGBitmapContext());
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

CGLayerRef BackingStore::CreateCGLayer() {
  // The CGLayer should be optimized for drawing into the containing window,
  // so extract a CGContext corresponding to the window to be passed to
  // CGLayerCreateWithContext.
  NSWindow* window = [render_widget_host()->view()->GetNativeView() window];
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

  CGLayerRef layer = CGLayerCreateWithContext(cg_context,
                                              size_.ToCGSize(),
                                              NULL);
  DCHECK(layer);

  return layer;
}

CGContextRef BackingStore::CreateCGBitmapContext() {
  // A CGBitmapContext serves as a stand-in for the layer before the view is
  // in a containing window.
  CGContextRef context = CGBitmapContextCreate(NULL,
                                               size_.width(), size_.height(),
                                               8, size_.width() * 4,
                                               mac_util::GetSystemColorSpace(),
                                               kCGImageAlphaPremultipliedFirst |
                                                   kCGBitmapByteOrder32Host);
  DCHECK(context);

  return context;
}
