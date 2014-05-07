// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/mock_quad_culler.h"

#include "cc/base/math_util.h"
#include "cc/quads/draw_quad.h"

namespace cc {

MockQuadCuller::MockQuadCuller() {
  render_pass_storage_ = RenderPass::Create();
  active_render_pass_ = render_pass_storage_.get();
}

MockQuadCuller::MockQuadCuller(RenderPass* external_render_pass)
    : active_render_pass_(external_render_pass) {
}

MockQuadCuller::~MockQuadCuller() {}

SharedQuadState* MockQuadCuller::CreateSharedQuadState() {
  return active_render_pass_->CreateAndAppendSharedQuadState();
}

gfx::Rect MockQuadCuller::UnoccludedContentRect(
    const gfx::Rect& content_rect,
    const gfx::Transform& draw_transform) {
  DCHECK(draw_transform.IsIdentityOrIntegerTranslation() ||
         occluded_target_rect_.IsEmpty());
  gfx::Rect target_rect =
      MathUtil::MapEnclosingClippedRect(draw_transform, content_rect);
  target_rect.Subtract(occluded_target_rect_);
  gfx::Transform inverse_draw_transform(gfx::Transform::kSkipInitialization);
  if (!draw_transform.GetInverse(&inverse_draw_transform))
    NOTREACHED();
  gfx::Rect result = MathUtil::ProjectEnclosingClippedRect(
      inverse_draw_transform, target_rect);
  return result;
}

gfx::Rect MockQuadCuller::UnoccludedContributingSurfaceContentRect(
    const gfx::Rect& content_rect,
    const gfx::Transform& draw_transform) {
  DCHECK(draw_transform.IsIdentityOrIntegerTranslation() ||
         occluded_target_rect_for_contributing_surface_.IsEmpty());
  gfx::Rect target_rect =
      MathUtil::MapEnclosingClippedRect(draw_transform, content_rect);
  target_rect.Subtract(occluded_target_rect_for_contributing_surface_);
  gfx::Transform inverse_draw_transform(gfx::Transform::kSkipInitialization);
  if (!draw_transform.GetInverse(&inverse_draw_transform))
    NOTREACHED();
  gfx::Rect result = MathUtil::ProjectEnclosingClippedRect(
      inverse_draw_transform, target_rect);
  return result;
}

void MockQuadCuller::Append(scoped_ptr<DrawQuad> draw_quad) {
  DCHECK(!draw_quad->rect.IsEmpty());
  DCHECK(!draw_quad->visible_rect.IsEmpty());
  active_render_pass_->quad_list.push_back(draw_quad.Pass());
}

}  // namespace cc
