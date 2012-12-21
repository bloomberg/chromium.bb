// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_BROWSER_PLUGIN_BROWSER_PLUGIN_BACKING_STORE_H__
#define CONTENT_RENDERER_BROWSER_PLUGIN_BROWSER_PLUGIN_BACKING_STORE_H__

#include <vector>

#include "base/memory/scoped_ptr.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/size.h"
#include "ui/gfx/vector2d.h"

class SkCanvas;
class TransportDIB;

namespace gfx {
class Canvas;
class Point;
class Rect;
}

namespace content {

// The BrowserPluginBackingStore is a wrapper around an SkBitmap that is used
// in the software rendering path of the browser plugin. The backing store has
// two write operations:
// 1. PaintToBackingStore copies pixel regions to the bitmap.
// 2. ScrollBackingStore scrolls a region of the bitmap by dx, and dy.
// These are called in response to changes in the guest relayed via
// BrowserPluginMsg_UpdateRect. See BrowserPlugin::UpdateRect.
class BrowserPluginBackingStore {
 public:
  BrowserPluginBackingStore(const gfx::Size& size, float scale_factor);
  virtual ~BrowserPluginBackingStore();

  void PaintToBackingStore(
      const gfx::Rect& bitmap_rect,
      const std::vector<gfx::Rect>& copy_rects,
      void* bitmap);

  void ScrollBackingStore(const gfx::Vector2d& delta,
                          const gfx::Rect& clip_rect,
                          const gfx::Size& view_size);

  void Clear(SkColor clear_color);

  const gfx::Size& GetSize() const { return size_; }

  const SkBitmap& GetBitmap() const { return bitmap_; }

  float GetScaleFactor() const { return scale_factor_; }

 private:
  gfx::Size size_;
  SkBitmap bitmap_;
  scoped_ptr<SkCanvas> canvas_;
  float scale_factor_;

  DISALLOW_COPY_AND_ASSIGN(BrowserPluginBackingStore);
};

}  // namespace content

#endif  // CONTENT_RENDERER_BROWSER_PLUGIN_BROWSER_PLUGIN_BACKING_STORE_H__
