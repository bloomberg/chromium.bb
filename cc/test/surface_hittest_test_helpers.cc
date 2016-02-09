// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/surface_hittest_test_helpers.h"

#include "cc/output/compositor_frame.h"
#include "cc/output/delegated_frame_data.h"
#include "cc/quads/render_pass_draw_quad.h"
#include "cc/quads/shared_quad_state.h"
#include "cc/quads/solid_color_draw_quad.h"
#include "cc/quads/surface_draw_quad.h"

namespace cc {
namespace test {

void CreateSharedQuadState(RenderPass* pass,
                           const gfx::Transform& transform,
                           const gfx::Rect& root_rect) {
  SharedQuadState* child_shared_state = pass->CreateAndAppendSharedQuadState();
  child_shared_state->SetAll(transform, root_rect.size(), root_rect, root_rect,
                             false, 1.0f, SkXfermode::kSrcOver_Mode, 0);
}

void CreateSolidColorDrawQuad(RenderPass* pass,
                              const gfx::Transform& transform,
                              const gfx::Rect& root_rect,
                              const gfx::Rect& quad_rect) {
  CreateSharedQuadState(pass, transform, root_rect);
  SolidColorDrawQuad* color_quad =
      pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  color_quad->SetNew(pass->shared_quad_state_list.back(), quad_rect, quad_rect,
                     SK_ColorYELLOW, false);
}

void CreateRenderPassDrawQuad(RenderPass* pass,
                              const gfx::Transform& transform,
                              const gfx::Rect& root_rect,
                              const gfx::Rect& quad_rect,
                              const RenderPassId& render_pass_id) {
  CreateSharedQuadState(pass, transform, root_rect);
  RenderPassDrawQuad* render_pass_quad =
      pass->CreateAndAppendDrawQuad<RenderPassDrawQuad>();
  render_pass_quad->SetNew(pass->shared_quad_state_list.back(), quad_rect,
                           quad_rect, render_pass_id, ResourceId(),
                           gfx::Vector2dF(), gfx::Size(), FilterOperations(),
                           gfx::Vector2dF(), FilterOperations());
}

void CreateSurfaceDrawQuad(RenderPass* pass,
                           const gfx::Transform& transform,
                           const gfx::Rect& root_rect,
                           const gfx::Rect& quad_rect,
                           SurfaceId surface_id) {
  CreateSharedQuadState(pass, transform, root_rect);
  SurfaceDrawQuad* surface_quad =
      pass->CreateAndAppendDrawQuad<SurfaceDrawQuad>();
  surface_quad->SetNew(pass->shared_quad_state_list.back(), quad_rect,
                       quad_rect, surface_id);
}

void CreateRenderPass(const RenderPassId& render_pass_id,
                      const gfx::Rect& rect,
                      const gfx::Transform& transform_to_root_target,
                      RenderPassList* render_pass_list) {
  scoped_ptr<RenderPass> render_pass = RenderPass::Create();
  render_pass->SetNew(render_pass_id, rect, rect, transform_to_root_target);
  render_pass_list->push_back(std::move(render_pass));
}

scoped_ptr<CompositorFrame> CreateCompositorFrameWithRenderPassList(
    RenderPassList* render_pass_list) {
  scoped_ptr<DelegatedFrameData> root_delegated_frame_data(
      new DelegatedFrameData);
  root_delegated_frame_data->render_pass_list.swap(*render_pass_list);
  scoped_ptr<CompositorFrame> root_frame(new CompositorFrame);
  root_frame->delegated_frame_data = std::move(root_delegated_frame_data);
  return root_frame;
}

scoped_ptr<CompositorFrame> CreateCompositorFrame(const gfx::Rect& root_rect,
                                                  RenderPass** render_pass) {
  RenderPassList render_pass_list;
  RenderPassId root_id(1, 1);
  CreateRenderPass(root_id, root_rect, gfx::Transform(), &render_pass_list);

  scoped_ptr<CompositorFrame> root_frame =
      CreateCompositorFrameWithRenderPassList(&render_pass_list);

  *render_pass =
      root_frame->delegated_frame_data->render_pass_list.back().get();
  return root_frame;
}

TestSurfaceHittestDelegate::TestSurfaceHittestDelegate()
    : reject_target_overrides_(0), accept_target_overrides_(0) {}

TestSurfaceHittestDelegate::~TestSurfaceHittestDelegate() {}

void TestSurfaceHittestDelegate::AddInsetsForRejectSurface(
    const SurfaceId& surface_id,
    const gfx::Insets& inset) {
  insets_for_reject_.insert(std::make_pair(surface_id, inset));
}

void TestSurfaceHittestDelegate::AddInsetsForAcceptSurface(
    const SurfaceId& surface_id,
    const gfx::Insets& inset) {
  insets_for_accept_.insert(std::make_pair(surface_id, inset));
}

bool TestSurfaceHittestDelegate::RejectHitTarget(
    const SurfaceDrawQuad* surface_quad,
    const gfx::Point& point_in_quad_space) {
  if (!insets_for_reject_.count(surface_quad->surface_id))
    return false;
  gfx::Rect bounds(surface_quad->rect);
  bounds.Inset(insets_for_reject_[surface_quad->surface_id]);
  // If the point provided falls outside the inset, then we skip this surface.
  if (!bounds.Contains(point_in_quad_space)) {
    if (surface_quad->rect.Contains(point_in_quad_space))
      ++reject_target_overrides_;
    return true;
  }
  return false;
}

bool TestSurfaceHittestDelegate::AcceptHitTarget(
    const SurfaceDrawQuad* surface_quad,
    const gfx::Point& point_in_quad_space) {
  if (!insets_for_accept_.count(surface_quad->surface_id))
    return false;
  gfx::Rect bounds(surface_quad->rect);
  bounds.Inset(insets_for_accept_[surface_quad->surface_id]);
  // If the point provided falls outside the inset, then we accept this surface.
  if (!bounds.Contains(point_in_quad_space)) {
    ++accept_target_overrides_;
    return true;
  }
  return false;
}

}  // namespace test
}  // namespace cc
