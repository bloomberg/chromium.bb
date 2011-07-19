// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_BACKING_STORE_MAC_H_
#define CONTENT_BROWSER_RENDERER_HOST_BACKING_STORE_MAC_H_
#pragma once

#include "base/basictypes.h"
#include "base/mac/scoped_cftyperef.h"
#include "content/browser/renderer_host/backing_store.h"

class BackingStoreMac : public BackingStore {
 public:
  BackingStoreMac(RenderWidgetHost* widget, const gfx::Size& size);
  virtual ~BackingStoreMac();

  // A CGLayer that stores the contents of the backing store, cached in GPU
  // memory if possible.
  CGLayerRef cg_layer() { return cg_layer_; }

  // A CGBitmapContext that stores the contents of the backing store if the
  // corresponding Cocoa view has not been inserted into an NSWindow yet.
  CGContextRef cg_bitmap() { return cg_bitmap_; }

  // BackingStore implementation.
  virtual void PaintToBackingStore(
      RenderProcessHost* process,
      TransportDIB::Id bitmap,
      const gfx::Rect& bitmap_rect,
      const std::vector<gfx::Rect>& copy_rects);
  virtual bool CopyFromBackingStore(const gfx::Rect& rect,
                                    skia::PlatformCanvas* output);
  virtual void ScrollBackingStore(int dx, int dy,
                                  const gfx::Rect& clip_rect,
                                  const gfx::Size& view_size);

 private:
  // Creates a CGLayer associated with its owner view's window's graphics
  // context, sized properly for the backing store.  Returns NULL if the owner
  // is not in a window with a CGContext.  cg_layer_ is assigned this method's
  // result.
  CGLayerRef CreateCGLayer();

  // Creates a CGBitmapContext sized properly for the backing store.  The
  // owner view need not be in a window.  cg_bitmap_ is assigned this method's
  // result.
  CGContextRef CreateCGBitmapContext();

  base::mac::ScopedCFTypeRef<CGContextRef> cg_bitmap_;
  base::mac::ScopedCFTypeRef<CGLayerRef> cg_layer_;

  DISALLOW_COPY_AND_ASSIGN(BackingStoreMac);
};

#endif  // CONTENT_BROWSER_RENDERER_HOST_BACKING_STORE_MAC_H_
