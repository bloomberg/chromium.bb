// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_BACKING_STORE_AURA_H_
#define CONTENT_BROWSER_RENDERER_HOST_BACKING_STORE_AURA_H_

#include "base/memory/scoped_ptr.h"
#include "content/browser/renderer_host/backing_store.h"
#include "content/common/content_export.h"
#include "third_party/skia/include/core/SkBitmap.h"

class SkCanvas;

namespace gfx {
class Point;
class Canvas;
}

namespace content {
class RenderProcessHost;

// A backing store that uses skia. This is the backing store used by
// RenderWidgetHostViewAura.
class BackingStoreAura : public BackingStore {
 public:
  CONTENT_EXPORT BackingStoreAura(
      RenderWidgetHost* widget,
      const gfx::Size& size);

  virtual ~BackingStoreAura();

  CONTENT_EXPORT void SkiaShowRect(const gfx::Point& point,
                                   gfx::Canvas* canvas);

  // Called when the view's backing scale factor changes.
  void ScaleFactorChanged(float device_scale_factor);

  // BackingStore implementation.
  virtual size_t MemorySize() OVERRIDE;
  virtual void PaintToBackingStore(
      RenderProcessHost* process,
      TransportDIB::Id bitmap,
      const gfx::Rect& bitmap_rect,
      const std::vector<gfx::Rect>& copy_rects,
      float scale_factor,
      const base::Closure& completion_callback,
      bool* scheduled_completion_callback) OVERRIDE;
  virtual bool CopyFromBackingStore(const gfx::Rect& rect,
                                    skia::PlatformBitmap* output) OVERRIDE;
  virtual void ScrollBackingStore(const gfx::Vector2d& delta,
                                  const gfx::Rect& clip_rect,
                                  const gfx::Size& view_size) OVERRIDE;

 private:
  SkBitmap bitmap_;

  scoped_ptr<SkCanvas> canvas_;
  int device_scale_factor_;

  DISALLOW_COPY_AND_ASSIGN(BackingStoreAura);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_BACKING_STORE_AURA_H_
