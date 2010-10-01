// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/test/test_backing_store.h"

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
    bool* painted_synchronously,
    bool* done_copying_bitmap) {
}

bool TestBackingStore::CopyFromBackingStore(const gfx::Rect& rect,
                                            skia::PlatformCanvas* output) {
  return false;
}

void TestBackingStore::ScrollBackingStore(int dx, int dy,
                                          const gfx::Rect& clip_rect,
                                          const gfx::Size& view_size) {
}
