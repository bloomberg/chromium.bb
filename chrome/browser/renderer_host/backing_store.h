// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_HOST_BACKING_STORE_H_
#define CHROME_BROWSER_RENDERER_HOST_BACKING_STORE_H_

#include "base/basictypes.h"
#include "base/gfx/rect.h"
#include "base/gfx/size.h"
#include "base/process.h"
#include "build/build_config.h"

#if defined(OS_WIN)
#include <windows.h>
#elif defined(OS_MACOSX)
#include "base/scoped_cftyperef.h"
#include "skia/ext/platform_canvas.h"
#elif defined(OS_LINUX)
#include "chrome/common/x11_util.h"
#endif

class RenderWidgetHost;
class SkBitmap;
class TransportDIB;
typedef struct _GdkDrawable GdkDrawable;

// BackingStore ----------------------------------------------------------------

// Represents a backing store for the pixels in a RenderWidgetHost.
class BackingStore {
 public:
#if defined(OS_WIN) || defined(OS_MACOSX)
  BackingStore(RenderWidgetHost* widget, const gfx::Size& size);
#elif defined(OS_LINUX)
  // Create a backing store on the X server. The visual is an Xlib Visual
  // describing the format of the target window and the depth is the color
  // depth of the X window which will be drawn into.
  BackingStore(RenderWidgetHost* widget,
               const gfx::Size& size,
               void* visual,
               int depth);

  // This is for unittesting only. An object constructed using this constructor
  // will silently ignore all paints
  BackingStore(RenderWidgetHost* widget, const gfx::Size& size);
#endif
  ~BackingStore();

  RenderWidgetHost* render_widget_host() const { return render_widget_host_; }
  const gfx::Size& size() { return size_; }

  // The number of bytes that this backing store consumes.  This should roughly
  // be size_.GetArea() * bytes per pixel.
  size_t MemorySize();

#if defined(OS_WIN)
  HDC hdc() { return hdc_; }

  // Returns true if we should convert to the monitor profile when painting.
  static bool ColorManagementEnabled();
#elif defined(OS_MACOSX)
  // A CGLayer that stores the contents of the backing store, cached in GPU
  // memory if possible.
  CGLayerRef cg_layer() { return cg_layer_; }
  // A CGBitmapContext that stores the contents of the backing store if the
  // corresponding Cocoa view has not been inserted into an NSWindow yet.
  CGContextRef cg_bitmap() { return cg_bitmap_; }

  // Paint the layer into a graphics context--if the target is a window,
  // this should be a GPU->GPU copy (and therefore very fast).
  void PaintToRect(const gfx::Rect& dest_rect, CGContextRef target);
#elif defined(OS_LINUX)
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
#endif

#if defined(TOOLKIT_GTK)
  // Paint the backing store into the target's |dest_rect|.
  void PaintToRect(const gfx::Rect& dest_rect, GdkDrawable* target);
#endif

  // Paints the bitmap from the renderer onto the backing store.  bitmap_rect
  // gives the location of bitmap, and copy_rect specifies the subregion of
  // the backingstore to be painted from the bitmap.
  void PaintRect(base::ProcessHandle process,
                 TransportDIB* bitmap,
                 const gfx::Rect& bitmap_rect,
                 const gfx::Rect& copy_rect);

  // Scrolls the contents of clip_rect in the backing store by dx or dy (but dx
  // and dy cannot both be non-zero).
  void ScrollRect(int dx, int dy,
                  const gfx::Rect& clip_rect,
                  const gfx::Size& view_size);

 private:
#if defined(OS_MACOSX)
  // Creates a CGLayer associated with its owner view's window's graphics
  // context, sized properly for the backing store.  Returns NULL if the owner
  // is not in a window with a CGContext.  cg_layer_ is assigned this method's
  // result.
  CGLayerRef CreateCGLayer();

  // Creates a CGBitmapContext sized properly for the backing store.  The
  // owner view need not be in a window.  cg_bitmap_ is assigned this method's
  // result.
  CGContextRef CreateCGBitmapContext();
#endif

  // The owner of this backing store.
  RenderWidgetHost* render_widget_host_;

  // The size of the backing store.
  gfx::Size size_;

#if defined(OS_WIN)
  // The backing store dc.
  HDC hdc_;
  // Handle to the backing store dib.
  HANDLE backing_store_dib_;
  // Handle to the original bitmap in the dc.
  HANDLE original_bitmap_;
  // Number of bits per pixel of the screen.
  int color_depth_;
#elif defined(OS_MACOSX)
  scoped_cftyperef<CGContextRef> cg_bitmap_;
  scoped_cftyperef<CGLayerRef> cg_layer_;
#elif defined(OS_LINUX) && defined(USE_GL)
  Display* const display_;

  // The parent window for this backing store.
  const XID root_window_;

  unsigned int texture_id_;  // 0 when uninitialized.

  // The size of the texture loaded into GL. This is 0x0 when there is no
  // texture loaded. This may be different than the size of the backing store
  // because we could have been resized without yet getting the updated
  // bitmap.
  gfx::Size texture_size_;

#elif defined(OS_LINUX) && !defined(USE_GL)
  // Paints the bitmap from the renderer onto the backing store without
  // using Xrender to composite the pixmaps.
  void PaintRectWithoutXrender(TransportDIB* bitmap,
                               const gfx::Rect& bitmap_rect,
                               const gfx::Rect& copy_rect);

  // This is the connection to the X server where this backing store will be
  // displayed.
  Display* const display_;
  // If this is true, then |connection_| is good for MIT-SHM (X shared memory).
  const bool use_shared_memory_;
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
#endif

  DISALLOW_COPY_AND_ASSIGN(BackingStore);
};

#endif  // CHROME_BROWSER_RENDERER_HOST_BACKING_STORE_H_
