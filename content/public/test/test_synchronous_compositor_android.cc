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
    gfx::Size surface_size,
    const gfx::Transform& transform,
    gfx::Rect viewport,
    gfx::Rect clip,
    gfx::Rect viewport_rect_for_tile_priority,
    const gfx::Transform& transform_for_tile_priority) {
  scoped_ptr<cc::CompositorFrame> compositor_frame(new cc::CompositorFrame);
  scoped_ptr<cc::DelegatedFrameData> frame(new cc::DelegatedFrameData);
  scoped_ptr<cc::RenderPass> root_pass(cc::RenderPass::Create());
  root_pass->SetNew(cc::RenderPassId(1, 1), viewport, viewport,
                    gfx::Transform());
  frame->render_pass_list.push_back(root_pass.Pass());
  compositor_frame->delegated_frame_data = frame.Pass();
  return compositor_frame.Pass();
}

bool TestSynchronousCompositor::DemandDrawSw(SkCanvas* canvas) {
  DCHECK(canvas);
  return true;
}

}  // namespace content
