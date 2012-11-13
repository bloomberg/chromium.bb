// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/test_backing_store.h"

namespace content {

TestBackingStore::TestBackingStore(RenderWidgetHost* widget,
                                   const gfx::Size& size)
    : BackingStore(widget, size) {
}

TestBackingStore::~TestBackingStore() {
}

void TestBackingStore::PaintToBackingStore(
    RenderProcessHost* process,
    TransportDIB::Id bitmap,
    const gfx::Rect& bitmap_rect,
    const std::vector<gfx::Rect>& copy_rects,
    float scale_factor,
    const base::Closure& completion_callback,
    bool* scheduled_completion_callback) {
  *scheduled_completion_callback = false;
}

bool TestBackingStore::CopyFromBackingStore(const gfx::Rect& rect,
                                            skia::PlatformBitmap* output) {
  return false;
}

void TestBackingStore::ScrollBackingStore(const gfx::Vector2d& delta,
                                          const gfx::Rect& clip_rect,
                                          const gfx::Size& view_size) {
}

}  // namespace content
