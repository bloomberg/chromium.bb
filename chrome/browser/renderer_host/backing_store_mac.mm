// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "chrome/browser/renderer_host/backing_store.h"

#include "base/logging.h"
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
      cg_layer_(CGLayerCreateWithContext(static_cast<CGContextRef>(
          [[NSGraphicsContext currentContext] graphicsPort]), size.ToCGSize(),
          NULL)) {
  CHECK(cg_layer_.get() != NULL);
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
  scoped_cftyperef<CGColorSpaceRef> color_space(CGColorSpaceCreateDeviceRGB());

  scoped_cftyperef<CGDataProviderRef> data_provider(
      CGDataProviderCreateWithData(NULL, bitmap->memory(),
          bitmap_rect.width() * bitmap_rect.height() * 4, NULL));

  scoped_cftyperef<CGImageRef> image(CGImageCreate(bitmap_rect.width(),
      bitmap_rect.height(), 8, 32, 4 * bitmap_rect.width(), color_space,
      kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Host, data_provider,
      NULL, false, kCGRenderingIntentDefault));

  // The CGLayer's origin is in the lower left, but flipping the CTM would
  // cause the image to get drawn upside down.  So we move the rectangle
  // to the right position before drawing the image.
  CGContextRef layer = CGLayerGetContext(cg_layer());
  gfx::Rect paint_rect = bitmap_rect;
  paint_rect.set_y(size_.height() - bitmap_rect.bottom());
  CGContextDrawImage(layer, paint_rect.ToCGRect(), image);
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
    //
    scoped_cftyperef<CGLayerRef> new_layer(CGLayerCreateWithContext(
        CGLayerGetContext(cg_layer()), layer_size, NULL));
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
  }
  // Now paint the new bitmap data into the CGLayer
  PaintRect(process, bitmap, bitmap_rect);
  return;
}
