// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_HOST_BACKING_STORE_X_H_
#define CHROME_BROWSER_RENDERER_HOST_BACKING_STORE_X_H_
#pragma once

#include "base/basictypes.h"
#include "build/build_config.h"
#include "chrome/browser/renderer_host/backing_store.h"
#include "ui/base/x/x11_util.h"

namespace gfx {
class Point;
class Rect;
}  // namespace gfx

typedef struct _GdkDrawable GdkDrawable;
class SkBitmap;

class BackingStoreX : public BackingStore {
 public:
  // Create a backing store on the X server. The visual is an Xlib Visual
  // describing the format of the target window and the depth is the color
  // depth of the X window which will be drawn into.
  BackingStoreX(RenderWidgetHost* widget,
               const gfx::Size& size,
               void* visual,
               int depth);

  // This is for unittesting only. An object constructed using this constructor
  // will silently ignore all paints
  BackingStoreX(RenderWidgetHost* widget, const gfx::Size& size);

  virtual ~BackingStoreX();

  Display* display() const { return display_; }
  XID root_window() const { return root_window_; }

  // Copy from the server-side backing store to the target window
  //   origin: the destination rectangle origin
  //   damage: the area to copy
  //   target: the X id of the target window
  void XShowRect(const gfx::Point &origin, const gfx::Rect& damage,
                 XID target);

  // As above, but use Cairo instead of Xlib.
  void CairoShowRect(const gfx::Rect& damage, GdkDrawable* drawable);

#if defined(TOOLKIT_GTK)
  // Paint the backing store into the target's |dest_rect|.
  void PaintToRect(const gfx::Rect& dest_rect, GdkDrawable* target);
#endif

  // BackingStore implementation.
  virtual size_t MemorySize();
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
  // Paints the bitmap from the renderer onto the backing store without
  // using Xrender to composite the pixmaps.
  void PaintRectWithoutXrender(TransportDIB* bitmap,
                               const gfx::Rect& bitmap_rect,
                               const std::vector<gfx::Rect>& copy_rects);

  // This is the connection to the X server where this backing store will be
  // displayed.
  Display* const display_;
  // What flavor, if any, MIT-SHM (X shared memory) support we have.
  const ui::SharedMemorySupport shared_memory_support_;
  // If this is true, then we can use Xrender to composite our pixmaps.
  const bool use_render_;
  // If |use_render_| is false, this is the number of bits-per-pixel for |depth|
  int pixmap_bpp_;
  // if |use_render_| is false, we need the Visual to get the RGB masks.
  void* const visual_;
  // This is the depth of the target window.
  const int visual_depth_;
  // The parent window (probably a GtkDrawingArea) for this backing store.
  const XID root_window_;
  // This is a handle to the server side pixmap which is our backing store.
  XID pixmap_;
  // This is the RENDER picture pointing at |pixmap_|.
  XID picture_;
  // This is a default graphic context, used in XCopyArea
  void* pixmap_gc_;

  DISALLOW_COPY_AND_ASSIGN(BackingStoreX);
};

#endif  // CHROME_BROWSER_RENDERER_HOST_BACKING_STORE_X_H_
