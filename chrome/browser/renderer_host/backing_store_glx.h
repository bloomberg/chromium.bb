// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_HOST_BACKING_STORE_GLX_H_
#define CHROME_BROWSER_RENDERER_HOST_BACKING_STORE_GLX_H_

#include "base/basictypes.h"
#include "build/build_config.h"
#include "chrome/browser/renderer_host/backing_store.h"
#include "chrome/common/x11_util.h"

class BackingStoreGLX : public BackingStore {
 public:
  // Create a backing store on the X server. The visual is an Xlib Visual
  // describing the format of the target window and the depth is the color
  // depth of the X window which will be drawn into.
  BackingStoreGLX(RenderWidgetHost* widget,
                  const gfx::Size& size,
                  void* visual,
                  int depth);

  // This is for unittesting only. An object constructed using this constructor
  // will silently ignore all paints
  BackingStoreGLX(RenderWidgetHost* widget, const gfx::Size& size);

  virtual ~BackingStoreGLX();

  Display* display() const { return display_; }
  XID root_window() const { return root_window_; }

  // Copy from the server-side backing store to the target window
  //   display: the display of the backing store and target window
  //   damage: the area to copy
  //   target: the X id of the target window
  void ShowRect(const gfx::Rect& damage, XID target);

  // Paints the server-side backing store data to a SkBitmap. On failure, the
  // return bitmap will be isNull().
  SkBitmap PaintRectToBitmap(const gfx::Rect& rect);

#if defined(TOOLKIT_GTK)
  // Paint the backing store into the target's |dest_rect|.
  void PaintToRect(const gfx::Rect& dest_rect, GdkDrawable* target);
#endif

  // BackingStore implementation.
  virtual size_t MemorySize();
  virtual void PaintRect(base::ProcessHandle process,
                         TransportDIB* bitmap,
                         const gfx::Rect& bitmap_rect,
                         const gfx::Rect& copy_rect);
  virtual void ScrollRect(int dx, int dy,
                          const gfx::Rect& clip_rect,
                          const gfx::Size& view_size);

 private:
  Display* const display_;

  // The parent window for this backing store.
  const XID root_window_;

  unsigned int texture_id_;  // 0 when uninitialized.

  // The size of the texture loaded into GL. This is 0x0 when there is no
  // texture loaded. This may be different than the size of the backing store
  // because we could have been resized without yet getting the updated
  // bitmap.
  gfx::Size texture_size_;

  DISALLOW_COPY_AND_ASSIGN(BackingStoreGLX);
};

#endif  // CHROME_BROWSER_RENDERER_HOST_BACKING_STORE_GLX_H_
