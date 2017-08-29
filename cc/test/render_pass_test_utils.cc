// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/render_pass_test_utils.h"

#include <stdint.h>

#include "base/bind.h"
#include "cc/quads/debug_border_draw_quad.h"
#include "cc/quads/render_pass_draw_quad.h"
#include "cc/quads/solid_color_draw_quad.h"
#include "cc/quads/stream_video_draw_quad.h"
#include "cc/quads/texture_draw_quad.h"
#include "cc/quads/tile_draw_quad.h"
#include "cc/quads/yuv_video_draw_quad.h"
#include "cc/resources/resource_provider.h"
#include "components/viz/common/quads/shared_quad_state.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkImageFilter.h"
#include "ui/gfx/geometry/rect.h"

namespace cc {

RenderPass* AddRenderPass(RenderPassList* pass_list,
                          int render_pass_id,
                          const gfx::Rect& output_rect,
                          const gfx::Transform& root_transform,
                          const FilterOperations& filters) {
  std::unique_ptr<RenderPass> pass(RenderPass::Create());
  pass->SetNew(render_pass_id, output_rect, output_rect, root_transform);
  pass->filters = filters;
  RenderPass* saved = pass.get();
  pass_list->push_back(std::move(pass));
  return saved;
}

SolidColorDrawQuad* AddQuad(RenderPass* pass,
                            const gfx::Rect& rect,
                            SkColor color) {
  viz::SharedQuadState* shared_state = pass->CreateAndAppendSharedQuadState();
  shared_state->SetAll(gfx::Transform(), rect, rect, rect, false, 1,
                       SkBlendMode::kSrcOver, 0);
  SolidColorDrawQuad* quad =
      pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  quad->SetNew(shared_state, rect, rect, color, false);
  return quad;
}

SolidColorDrawQuad* AddClippedQuad(RenderPass* pass,
                                   const gfx::Rect& rect,
                                   SkColor color) {
  viz::SharedQuadState* shared_state = pass->CreateAndAppendSharedQuadState();
  shared_state->SetAll(gfx::Transform(), rect, rect, rect, true, 1,
                       SkBlendMode::kSrcOver, 0);
  SolidColorDrawQuad* quad =
      pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  quad->SetNew(shared_state, rect, rect, color, false);
  return quad;
}

SolidColorDrawQuad* AddTransformedQuad(RenderPass* pass,
                                       const gfx::Rect& rect,
                                       SkColor color,
                                       const gfx::Transform& transform) {
  viz::SharedQuadState* shared_state = pass->CreateAndAppendSharedQuadState();
  shared_state->SetAll(transform, rect, rect, rect, false, 1,
                       SkBlendMode::kSrcOver, 0);
  SolidColorDrawQuad* quad =
      pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  quad->SetNew(shared_state, rect, rect, color, false);
  return quad;
}

void AddRenderPassQuad(RenderPass* to_pass, RenderPass* contributing_pass) {
  gfx::Rect output_rect = contributing_pass->output_rect;
  viz::SharedQuadState* shared_state =
      to_pass->CreateAndAppendSharedQuadState();
  shared_state->SetAll(gfx::Transform(), output_rect, output_rect, output_rect,
                       false, 1, SkBlendMode::kSrcOver, 0);
  RenderPassDrawQuad* quad =
      to_pass->CreateAndAppendDrawQuad<RenderPassDrawQuad>();
  quad->SetNew(shared_state, output_rect, output_rect, contributing_pass->id, 0,
               gfx::RectF(), gfx::Size(), gfx::Vector2dF(), gfx::PointF(),
               gfx::RectF());
}

void AddRenderPassQuad(RenderPass* to_pass,
                       RenderPass* contributing_pass,
                       viz::ResourceId mask_resource_id,
                       gfx::Transform transform,
                       SkBlendMode blend_mode) {
  gfx::Rect output_rect = contributing_pass->output_rect;
  viz::SharedQuadState* shared_state =
      to_pass->CreateAndAppendSharedQuadState();
  shared_state->SetAll(transform, output_rect, output_rect, output_rect, false,
                       1, blend_mode, 0);
  RenderPassDrawQuad* quad =
      to_pass->CreateAndAppendDrawQuad<RenderPassDrawQuad>();
  gfx::Size arbitrary_nonzero_size(1, 1);
  quad->SetNew(shared_state, output_rect, output_rect, contributing_pass->id,
               mask_resource_id, gfx::RectF(output_rect),
               arbitrary_nonzero_size, gfx::Vector2dF(), gfx::PointF(),
               gfx::RectF());
}

static void EmptyReleaseCallback(const gpu::SyncToken& sync_token,
                                 bool lost_resource,
                                 BlockingTaskRunner* main_thread_task_runner) {}

void AddOneOfEveryQuadType(RenderPass* to_pass,
                           ResourceProvider* resource_provider,
                           RenderPassId child_pass_id,
                           gpu::SyncToken* sync_token_for_mailbox_tebxture) {
  gfx::Rect rect(0, 0, 100, 100);
  gfx::Rect visible_rect(0, 0, 100, 100);
  bool needs_blending = true;
  const float vertex_opacity[] = {1.0f, 1.0f, 1.0f, 1.0f};

  static const gpu::SyncToken kSyncTokenForMailboxTextureQuad(
      gpu::CommandBufferNamespace::GPU_IO, 0,
      gpu::CommandBufferId::FromUnsafeValue(0x123), 30);
  *sync_token_for_mailbox_tebxture = kSyncTokenForMailboxTextureQuad;

  viz::ResourceId resource1 = resource_provider->CreateResource(
      gfx::Size(45, 5), ResourceProvider::TEXTURE_HINT_IMMUTABLE,
      resource_provider->best_texture_format(), gfx::ColorSpace());
  resource_provider->AllocateForTesting(resource1);
  viz::ResourceId resource2 = resource_provider->CreateResource(
      gfx::Size(346, 61), ResourceProvider::TEXTURE_HINT_IMMUTABLE,
      resource_provider->best_texture_format(), gfx::ColorSpace());
  resource_provider->AllocateForTesting(resource2);
  viz::ResourceId resource3 = resource_provider->CreateResource(
      gfx::Size(12, 134), ResourceProvider::TEXTURE_HINT_IMMUTABLE,
      resource_provider->best_texture_format(), gfx::ColorSpace());
  resource_provider->AllocateForTesting(resource3);
  viz::ResourceId resource4 = resource_provider->CreateResource(
      gfx::Size(56, 12), ResourceProvider::TEXTURE_HINT_IMMUTABLE,
      resource_provider->best_texture_format(), gfx::ColorSpace());
  resource_provider->AllocateForTesting(resource4);
  gfx::Size resource5_size(73, 26);
  viz::ResourceId resource5 = resource_provider->CreateResource(
      resource5_size, ResourceProvider::TEXTURE_HINT_IMMUTABLE,
      resource_provider->best_texture_format(), gfx::ColorSpace());
  resource_provider->AllocateForTesting(resource5);
  viz::ResourceId resource6 = resource_provider->CreateResource(
      gfx::Size(64, 92), ResourceProvider::TEXTURE_HINT_IMMUTABLE,
      resource_provider->best_texture_format(), gfx::ColorSpace());
  resource_provider->AllocateForTesting(resource6);
  viz::ResourceId resource7 = resource_provider->CreateResource(
      gfx::Size(9, 14), ResourceProvider::TEXTURE_HINT_IMMUTABLE,
      resource_provider->best_texture_format(), gfx::ColorSpace());
  resource_provider->AllocateForTesting(resource7);

  unsigned target = GL_TEXTURE_2D;
  gpu::Mailbox gpu_mailbox;
  memcpy(gpu_mailbox.name, "Hello world", strlen("Hello world") + 1);
  std::unique_ptr<SingleReleaseCallbackImpl> callback =
      SingleReleaseCallbackImpl::Create(base::Bind(&EmptyReleaseCallback));
  viz::TextureMailbox mailbox(gpu_mailbox, kSyncTokenForMailboxTextureQuad,
                              target);
  viz::ResourceId resource8 =
      resource_provider->CreateResourceFromTextureMailbox(mailbox,
                                                          std::move(callback));
  resource_provider->AllocateForTesting(resource8);

  viz::SharedQuadState* shared_state =
      to_pass->CreateAndAppendSharedQuadState();
  shared_state->SetAll(gfx::Transform(), rect, rect, rect, false, 1,
                       SkBlendMode::kSrcOver, 0);

  DebugBorderDrawQuad* debug_border_quad =
      to_pass->CreateAndAppendDrawQuad<DebugBorderDrawQuad>();
  debug_border_quad->SetNew(shared_state, rect, visible_rect, SK_ColorRED, 1);

  if (child_pass_id) {
    RenderPassDrawQuad* render_pass_quad =
        to_pass->CreateAndAppendDrawQuad<RenderPassDrawQuad>();
    render_pass_quad->SetNew(shared_state, rect, visible_rect, child_pass_id,
                             resource5, gfx::RectF(rect), resource5_size,
                             gfx::Vector2dF(), gfx::PointF(), gfx::RectF());
  }

  SolidColorDrawQuad* solid_color_quad =
      to_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  solid_color_quad->SetNew(shared_state, rect, visible_rect, SK_ColorRED,
                           false);

  StreamVideoDrawQuad* stream_video_quad =
      to_pass->CreateAndAppendDrawQuad<StreamVideoDrawQuad>();
  stream_video_quad->SetNew(shared_state, rect, visible_rect, needs_blending,
                            resource6, gfx::Size(), gfx::Transform());

  TextureDrawQuad* texture_quad =
      to_pass->CreateAndAppendDrawQuad<TextureDrawQuad>();
  texture_quad->SetNew(shared_state, rect, visible_rect, needs_blending,
                       resource1, false, gfx::PointF(0.f, 0.f),
                       gfx::PointF(1.f, 1.f), SK_ColorTRANSPARENT,
                       vertex_opacity, false, false, false);

  TextureDrawQuad* mailbox_texture_quad =
      to_pass->CreateAndAppendDrawQuad<TextureDrawQuad>();
  mailbox_texture_quad->SetNew(shared_state, rect, visible_rect, needs_blending,
                               resource8, false, gfx::PointF(0.f, 0.f),
                               gfx::PointF(1.f, 1.f), SK_ColorTRANSPARENT,
                               vertex_opacity, false, false, false);

  TileDrawQuad* scaled_tile_quad =
      to_pass->CreateAndAppendDrawQuad<TileDrawQuad>();
  scaled_tile_quad->SetNew(shared_state, rect, visible_rect, needs_blending,
                           resource2, gfx::RectF(0, 0, 50, 50),
                           gfx::Size(50, 50), false, false);

  viz::SharedQuadState* transformed_state =
      to_pass->CreateAndAppendSharedQuadState();
  *transformed_state = *shared_state;
  gfx::Transform rotation;
  rotation.Rotate(45);
  transformed_state->quad_to_target_transform =
      transformed_state->quad_to_target_transform * rotation;
  TileDrawQuad* transformed_tile_quad =
      to_pass->CreateAndAppendDrawQuad<TileDrawQuad>();
  transformed_tile_quad->SetNew(
      transformed_state, rect, visible_rect, needs_blending, resource3,
      gfx::RectF(0, 0, 100, 100), gfx::Size(100, 100), false, false);

  viz::SharedQuadState* shared_state2 =
      to_pass->CreateAndAppendSharedQuadState();
  shared_state->SetAll(gfx::Transform(), rect, rect, rect, false, 1,
                       SkBlendMode::kSrcOver, 0);

  TileDrawQuad* tile_quad = to_pass->CreateAndAppendDrawQuad<TileDrawQuad>();
  tile_quad->SetNew(shared_state2, rect, visible_rect, needs_blending,
                    resource4, gfx::RectF(0, 0, 100, 100), gfx::Size(100, 100),
                    false, false);

  viz::ResourceId plane_resources[4];
  for (int i = 0; i < 4; ++i) {
    plane_resources[i] = resource_provider->CreateResource(
        gfx::Size(20, 12), ResourceProvider::TEXTURE_HINT_IMMUTABLE,
        resource_provider->best_texture_format(), gfx::ColorSpace());
    resource_provider->AllocateForTesting(plane_resources[i]);
  }
  YUVVideoDrawQuad::ColorSpace color_space = YUVVideoDrawQuad::REC_601;

  YUVVideoDrawQuad* yuv_quad =
      to_pass->CreateAndAppendDrawQuad<YUVVideoDrawQuad>();
  yuv_quad->SetNew(shared_state2, rect, visible_rect, needs_blending,
                   gfx::RectF(.0f, .0f, 100.0f, 100.0f),
                   gfx::RectF(.0f, .0f, 50.0f, 50.0f), gfx::Size(100, 100),
                   gfx::Size(50, 50), plane_resources[0], plane_resources[1],
                   plane_resources[2], plane_resources[3], color_space,
                   gfx::ColorSpace::CreateJpeg(), 0.0, 1.0, 8);
}

}  // namespace cc
