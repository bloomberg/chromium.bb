// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/mock_quad_culler.h"

namespace cc {

MockQuadCuller::MockQuadCuller(
    RenderPass* render_pass,
    MockOcclusionTracker<LayerImpl>* occlusion_tracker)
    : QuadSink(render_pass, occlusion_tracker),
      occlusion_tracker_(occlusion_tracker) {
}

MockQuadCuller::~MockQuadCuller() {}

gfx::Rect MockQuadCuller::UnoccludedContentRect(
    const gfx::Rect& content_rect,
    const gfx::Transform& draw_transform) {
  return occlusion_tracker_->UnoccludedContentRect(content_rect,
                                                   draw_transform);
}

gfx::Rect MockQuadCuller::UnoccludedContributingSurfaceContentRect(
    const gfx::Rect& content_rect,
    const gfx::Transform& draw_transform) {
  return occlusion_tracker_->UnoccludedContributingSurfaceContentRect(
      content_rect, draw_transform);
}

}  // namespace cc

