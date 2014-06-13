// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/mock_quad_culler.h"

#include "cc/base/math_util.h"
#include "cc/layers/layer_impl.h"
#include "cc/quads/draw_quad.h"

namespace cc {

MockQuadCuller::MockQuadCuller() {
  render_pass_storage_ = RenderPass::Create();
  active_render_pass_ = render_pass_storage_.get();
  occlusion_tracker_storage_ = make_scoped_ptr(
      new MockOcclusionTracker<LayerImpl>(gfx::Rect(0, 0, 1000, 1000)));
  occlusion_tracker_ = occlusion_tracker_storage_.get();
}

MockQuadCuller::MockQuadCuller(RenderPass* external_render_pass)
    : active_render_pass_(external_render_pass) {
  occlusion_tracker_storage_ = make_scoped_ptr(
      new MockOcclusionTracker<LayerImpl>(gfx::Rect(0, 0, 1000, 1000)));
  occlusion_tracker_ = occlusion_tracker_storage_.get();
}

MockQuadCuller::MockQuadCuller(
    MockOcclusionTracker<LayerImpl>* occlusion_tracker)
    : occlusion_tracker_(occlusion_tracker) {
  render_pass_storage_ = RenderPass::Create();
  active_render_pass_ = render_pass_storage_.get();
}

MockQuadCuller::MockQuadCuller(
    RenderPass* external_render_pass,
    MockOcclusionTracker<LayerImpl>* occlusion_tracker)
    : active_render_pass_(external_render_pass),
      occlusion_tracker_(occlusion_tracker) {
}

MockQuadCuller::~MockQuadCuller() {}

SharedQuadState* MockQuadCuller::CreateSharedQuadState() {
  return active_render_pass_->CreateAndAppendSharedQuadState();
}

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

void MockQuadCuller::Append(scoped_ptr<DrawQuad> draw_quad) {
  DCHECK(!draw_quad->rect.IsEmpty());
  DCHECK(!draw_quad->visible_rect.IsEmpty());
  active_render_pass_->quad_list.push_back(draw_quad.Pass());
}

}  // namespace cc
