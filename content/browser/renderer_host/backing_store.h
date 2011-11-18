// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_BACKING_STORE_H_
#define CONTENT_BROWSER_RENDERER_HOST_BACKING_STORE_H_
#pragma once

#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "content/common/content_export.h"
#include "ui/gfx/size.h"
#include "ui/gfx/surface/transport_dib.h"

class RenderProcessHost;
class RenderWidgetHost;

namespace gfx {
class Rect;
}

namespace content {
class RenderProcessHost;
}

namespace skia {
class PlatformCanvas;
}

// Represents a backing store for the pixels in a RenderWidgetHost.
class CONTENT_EXPORT BackingStore {
 public:
  virtual ~BackingStore();

  RenderWidgetHost* render_widget_host() const { return render_widget_host_; }
  const gfx::Size& size() { return size_; }

  // The number of bytes that this backing store consumes. The default
  // implementation just assumes there's 32 bits per pixel over the current
  // size of the screen. Implementations may override this if they have more
  // information about the color depth.
  virtual size_t MemorySize();

  // Paints the bitmap from the renderer onto the backing store.  bitmap_rect
  // gives the location of bitmap, and copy_rects specifies the subregion(s) of
  // the backingstore to be painted from the bitmap.
  //
  // PaintToBackingStore does not need to guarantee that this has happened by
  // the time it returns, in which case it will set |scheduled_callback| to
  // true and will call |callback| when completed.
  virtual void PaintToBackingStore(
      content::RenderProcessHost* process,
      TransportDIB::Id bitmap,
      const gfx::Rect& bitmap_rect,
      const std::vector<gfx::Rect>& copy_rects,
      const base::Closure& completion_callback,
      bool* scheduled_completion_callback) = 0;

  // Extracts the gives subset of the backing store and copies it to the given
  // PlatformCanvas. The PlatformCanvas should not be initialized. This function
  // will call initialize() with the correct size. The return value indicates
  // success.
  virtual bool CopyFromBackingStore(const gfx::Rect& rect,
                                    skia::PlatformCanvas* output) = 0;

  // Scrolls the contents of clip_rect in the backing store by dx or dy (but dx
  // and dy cannot both be non-zero).
  virtual void ScrollBackingStore(int dx, int dy,
                                  const gfx::Rect& clip_rect,
                                  const gfx::Size& view_size) = 0;
 protected:
  // Can only be constructed via subclasses.
  BackingStore(RenderWidgetHost* widget, const gfx::Size& size);

 private:
  // The owner of this backing store.
  RenderWidgetHost* render_widget_host_;

  // The size of the backing store.
  gfx::Size size_;

  DISALLOW_COPY_AND_ASSIGN(BackingStore);
};

#endif  // CONTENT_BROWSER_RENDERER_HOST_BACKING_STORE_H_
