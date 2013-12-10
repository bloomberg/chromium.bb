// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/render_pass_test_common.h"

#include "cc/quads/checkerboard_draw_quad.h"
#include "cc/quads/debug_border_draw_quad.h"
#include "cc/quads/io_surface_draw_quad.h"
#include "cc/quads/render_pass_draw_quad.h"
#include "cc/quads/shared_quad_state.h"
#include "cc/quads/solid_color_draw_quad.h"
#include "cc/quads/stream_video_draw_quad.h"
#include "cc/quads/texture_draw_quad.h"
#include "cc/quads/tile_draw_quad.h"
#include "cc/quads/yuv_video_draw_quad.h"
#include "cc/resources/resource_provider.h"
#include "ui/gfx/transform.h"

namespace cc {

void TestRenderPass::AppendQuad(scoped_ptr<DrawQuad> quad) {
  quad_list.push_back(quad.Pass());
}

void TestRenderPass::AppendSharedQuadState(scoped_ptr<SharedQuadState> state) {
  shared_quad_state_list.push_back(state.Pass());
}

void TestRenderPass::AppendOneOfEveryQuadType(
    ResourceProvider* resource_provider,
    RenderPass::Id child_pass) {
  gfx::Rect rect(0, 0, 100, 100);
  gfx::Rect opaque_rect(10, 10, 80, 80);
  const float vertex_opacity[] = {1.0f, 1.0f, 1.0f, 1.0f};
  ResourceProvider::ResourceId resource1 = resource_provider->CreateResource(
      gfx::Size(45, 5),
      GL_CLAMP_TO_EDGE,
      ResourceProvider::TextureUsageAny,
      resource_provider->best_texture_format());
  resource_provider->AllocateForTesting(resource1);
  ResourceProvider::ResourceId resource2 = resource_provider->CreateResource(
      gfx::Size(346, 61),
      GL_CLAMP_TO_EDGE,
      ResourceProvider::TextureUsageAny,
      resource_provider->best_texture_format());
  resource_provider->AllocateForTesting(resource2);
  ResourceProvider::ResourceId resource3 = resource_provider->CreateResource(
      gfx::Size(12, 134),
      GL_CLAMP_TO_EDGE,
      ResourceProvider::TextureUsageAny,
      resource_provider->best_texture_format());
  resource_provider->AllocateForTesting(resource3);
  ResourceProvider::ResourceId resource4 = resource_provider->CreateResource(
      gfx::Size(56, 12),
      GL_CLAMP_TO_EDGE,
      ResourceProvider::TextureUsageAny,
      resource_provider->best_texture_format());
  resource_provider->AllocateForTesting(resource4);
  ResourceProvider::ResourceId resource5 = resource_provider->CreateResource(
      gfx::Size(73, 26),
      GL_CLAMP_TO_EDGE,
      ResourceProvider::TextureUsageAny,
      resource_provider->best_texture_format());
  resource_provider->AllocateForTesting(resource5);
  ResourceProvider::ResourceId resource6 = resource_provider->CreateResource(
      gfx::Size(64, 92),
      GL_CLAMP_TO_EDGE,
      ResourceProvider::TextureUsageAny,
      resource_provider->best_texture_format());
  resource_provider->AllocateForTesting(resource6);
  ResourceProvider::ResourceId resource7 = resource_provider->CreateResource(
      gfx::Size(9, 14),
      GL_CLAMP_TO_EDGE,
      ResourceProvider::TextureUsageAny,
      resource_provider->best_texture_format());
  resource_provider->AllocateForTesting(resource7);

  scoped_ptr<SharedQuadState> shared_state = SharedQuadState::Create();
  shared_state->SetAll(gfx::Transform(),
                       rect.size(),
                       rect,
                       rect,
                       false,
                       1,
                       SkXfermode::kSrcOver_Mode);

  scoped_ptr<CheckerboardDrawQuad> checkerboard_quad =
      CheckerboardDrawQuad::Create();
  checkerboard_quad->SetNew(shared_state.get(),
                            rect,
                            SK_ColorRED);
  AppendQuad(checkerboard_quad.PassAs<DrawQuad>());

  scoped_ptr<DebugBorderDrawQuad> debug_border_quad =
      DebugBorderDrawQuad::Create();
  debug_border_quad->SetNew(shared_state.get(),
                            rect,
                            SK_ColorRED,
                            1);
  AppendQuad(debug_border_quad.PassAs<DrawQuad>());

  scoped_ptr<IOSurfaceDrawQuad> io_surface_quad = IOSurfaceDrawQuad::Create();
  io_surface_quad->SetNew(shared_state.get(),
                          rect,
                          opaque_rect,
                          gfx::Size(50, 50),
                          resource7,
                          IOSurfaceDrawQuad::FLIPPED);
  AppendQuad(io_surface_quad.PassAs<DrawQuad>());

  if (child_pass.layer_id) {
    scoped_ptr<RenderPassDrawQuad> render_pass_quad =
        RenderPassDrawQuad::Create();
    render_pass_quad->SetNew(shared_state.get(),
                             rect,
                             child_pass,
                             false,
                             resource5,
                             rect,
                             gfx::RectF(),
                             FilterOperations(),
                             FilterOperations());
    AppendQuad(render_pass_quad.PassAs<DrawQuad>());

    scoped_ptr<RenderPassDrawQuad> render_pass_replica_quad =
        RenderPassDrawQuad::Create();
    render_pass_replica_quad->SetNew(shared_state.get(),
                                     rect,
                                     child_pass,
                                     true,
                                     resource5,
                                     rect,
                                     gfx::RectF(),
                                     FilterOperations(),
                                     FilterOperations());
    AppendQuad(render_pass_replica_quad.PassAs<DrawQuad>());
  }

  scoped_ptr<SolidColorDrawQuad> solid_color_quad =
      SolidColorDrawQuad::Create();
  solid_color_quad->SetNew(shared_state.get(),
                           rect,
                           SK_ColorRED,
                           false);
  AppendQuad(solid_color_quad.PassAs<DrawQuad>());

  scoped_ptr<StreamVideoDrawQuad> stream_video_quad =
      StreamVideoDrawQuad::Create();
  stream_video_quad->SetNew(shared_state.get(),
                            rect,
                            opaque_rect,
                            resource6,
                            gfx::Transform());
  AppendQuad(stream_video_quad.PassAs<DrawQuad>());

  scoped_ptr<TextureDrawQuad> texture_quad = TextureDrawQuad::Create();
  texture_quad->SetNew(shared_state.get(),
                       rect,
                       opaque_rect,
                       resource1,
                       false,
                       gfx::PointF(0.f, 0.f),
                       gfx::PointF(1.f, 1.f),
                       SK_ColorTRANSPARENT,
                       vertex_opacity,
                       false);
  AppendQuad(texture_quad.PassAs<DrawQuad>());

  scoped_ptr<TileDrawQuad> scaled_tile_quad = TileDrawQuad::Create();
  scaled_tile_quad->SetNew(shared_state.get(),
                           rect,
                           opaque_rect,
                           resource2,
                           gfx::RectF(0, 0, 50, 50),
                           gfx::Size(50, 50),
                           false);
  AppendQuad(scaled_tile_quad.PassAs<DrawQuad>());

  scoped_ptr<SharedQuadState> transformed_state = shared_state->Copy();
  gfx::Transform rotation;
  rotation.Rotate(45);
  transformed_state->content_to_target_transform =
      transformed_state->content_to_target_transform * rotation;
  scoped_ptr<TileDrawQuad> transformed_tile_quad = TileDrawQuad::Create();
  transformed_tile_quad->SetNew(transformed_state.get(),
                                rect,
                                opaque_rect,
                                resource3,
                                gfx::RectF(0, 0, 100, 100),
                                gfx::Size(100, 100),
                                false);
  AppendQuad(transformed_tile_quad.PassAs<DrawQuad>());

  scoped_ptr<SharedQuadState> shared_state2 = SharedQuadState::Create();
  shared_state->SetAll(gfx::Transform(),
                       rect.size(),
                       rect,
                       rect,
                       false,
                       1,
                       SkXfermode::kSrcOver_Mode);

  scoped_ptr<TileDrawQuad> tile_quad = TileDrawQuad::Create();
  tile_quad->SetNew(shared_state2.get(),
                    rect,
                    opaque_rect,
                    resource4,
                    gfx::RectF(0, 0, 100, 100),
                    gfx::Size(100, 100),
                    false);
  AppendQuad(tile_quad.PassAs<DrawQuad>());

  ResourceProvider::ResourceId plane_resources[4];
  for (int i = 0; i < 4; ++i) {
    plane_resources[i] =
        resource_provider->CreateResource(
            gfx::Size(20, 12),
            GL_CLAMP_TO_EDGE,
            ResourceProvider::TextureUsageAny,
            resource_provider->best_texture_format());
    resource_provider->AllocateForTesting(plane_resources[i]);
  }
  scoped_ptr<YUVVideoDrawQuad> yuv_quad = YUVVideoDrawQuad::Create();
  yuv_quad->SetNew(shared_state2.get(),
                   rect,
                   opaque_rect,
                   gfx::Size(100, 100),
                   plane_resources[0],
                   plane_resources[1],
                   plane_resources[2],
                   plane_resources[3]);
  AppendQuad(yuv_quad.PassAs<DrawQuad>());

  AppendSharedQuadState(shared_state.Pass());
  AppendSharedQuadState(transformed_state.Pass());
  AppendSharedQuadState(shared_state2.Pass());
}

}  // namespace cc
