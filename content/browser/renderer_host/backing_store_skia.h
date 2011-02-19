// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_BACKING_STORE_SKIA_H_
#define CONTENT_BROWSER_RENDERER_HOST_BACKING_STORE_SKIA_H_
#pragma once

#include "base/scoped_ptr.h"
#include "content/browser/renderer_host/backing_store.h"
#include "third_party/skia/include/core/SkBitmap.h"

class SkCanvas;

namespace gfx {
class Point;
class Canvas;
}

// A backing store that uses skia. This is a temporary backing store used by
// RenderWidgetHostViewViews. In time, only GPU rendering will be used for
// RWHVV, and then this backing store will be removed.
class BackingStoreSkia : public BackingStore {
 public:
  BackingStoreSkia(RenderWidgetHost* widget, const gfx::Size& size);
  virtual ~BackingStoreSkia();

  void SkiaShowRect(const gfx::Point& point, gfx::Canvas* canvas);

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
  SkBitmap bitmap_;

  scoped_ptr<SkCanvas> canvas_;

  DISALLOW_COPY_AND_ASSIGN(BackingStoreSkia);
};

#endif  // CONTENT_BROWSER_RENDERER_HOST_BACKING_STORE_SKIA_H_
