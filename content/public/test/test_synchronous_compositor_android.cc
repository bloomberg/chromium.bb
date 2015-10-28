// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/test_synchronous_compositor_android.h"

#include "cc/output/compositor_frame.h"

namespace content {

TestSynchronousCompositor::TestSynchronousCompositor() : client_(NULL) {
}

TestSynchronousCompositor::~TestSynchronousCompositor() {
  SetClient(NULL);
}

void TestSynchronousCompositor::SetClient(SynchronousCompositorClient* client) {
  if (client_)
    client_->DidDestroyCompositor(this);
  client_ = client;
  if (client_)
    client_->DidInitializeCompositor(this);
}

scoped_ptr<cc::CompositorFrame> TestSynchronousCompositor::DemandDrawHw(
    const gfx::Size& surface_size,
    const gfx::Transform& transform,
    const gfx::Rect& viewport,
    const gfx::Rect& clip,
    const gfx::Rect& viewport_rect_for_tile_priority,
    const gfx::Transform& transform_for_tile_priority) {
  return hardware_frame_.Pass();
}

bool TestSynchronousCompositor::DemandDrawSw(SkCanvas* canvas) {
  DCHECK(canvas);
  return true;
}

void TestSynchronousCompositor::SetHardwareFrame(
    scoped_ptr<cc::CompositorFrame> frame) {
  hardware_frame_ = frame.Pass();
}

}  // namespace content
