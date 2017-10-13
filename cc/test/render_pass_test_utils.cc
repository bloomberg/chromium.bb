// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/render_pass_test_utils.h"

#include <stdint.h>

#include "base/bind.h"
#include "cc/resources/resource_provider.h"
#include "components/viz/common/quads/debug_border_draw_quad.h"
#include "components/viz/common/quads/render_pass_draw_quad.h"
#include "components/viz/common/quads/shared_quad_state.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/common/quads/stream_video_draw_quad.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "components/viz/common/quads/tile_draw_quad.h"
#include "components/viz/common/quads/yuv_video_draw_quad.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkImageFilter.h"
#include "ui/gfx/geometry/rect.h"

namespace cc {

viz::RenderPass* AddRenderPass(viz::RenderPassList* pass_list,
                               int render_pass_id,
                               const gfx::Rect& output_rect,
                               const gfx::Transform& root_transform,
                               const FilterOperations& filters) {
  std::unique_ptr<viz::RenderPass> pass(viz::RenderPass::Create());
  pass->SetNew(render_pass_id, output_rect, output_rect, root_transform);
  pass->filters = filters;
  viz::RenderPass* saved = pass.get();
  pass_list->push_back(std::move(pass));
  return saved;
}

viz::RenderPass* AddRenderPassWithDamage(viz::RenderPassList* pass_list,
                                         int render_pass_id,
                                         const gfx::Rect& output_rect,
                                         const gfx::Rect& damage_rect,
                                         const gfx::Transform& root_transform,
                                         const FilterOperations& filters) {
  std::unique_ptr<viz::RenderPass> pass(viz::RenderPass::Create());
  pass->SetNew(render_pass_id, output_rect, damage_rect, root_transform);
  pass->filters = filters;
  viz::RenderPass* saved = pass.get();
  pass_list->push_back(std::move(pass));
  return saved;
}

viz::SolidColorDrawQuad* AddQuad(viz::RenderPass* pass,
                                 const gfx::Rect& rect,
                                 SkColor color) {
  viz::SharedQuadState* shared_state = pass->CreateAndAppendSharedQuadState();
  shared_state->SetAll(gfx::Transform(), rect, rect, rect, false, false, 1,
                       SkBlendMode::kSrcOver, 0);
  auto* quad = pass->CreateAndAppendDrawQuad<viz::SolidColorDrawQuad>();
  quad->SetNew(shared_state, rect, rect, color, false);
  return quad;
}

viz::SolidColorDrawQuad* AddClippedQuad(viz::RenderPass* pass,
                                        const gfx::Rect& rect,
                                        SkColor color) {
  viz::SharedQuadState* shared_state = pass->CreateAndAppendSharedQuadState();
  shared_state->SetAll(gfx::Transform(), rect, rect, rect, true, false, 1,
                       SkBlendMode::kSrcOver, 0);
  auto* quad = pass->CreateAndAppendDrawQuad<viz::SolidColorDrawQuad>();
  quad->SetNew(shared_state, rect, rect, color, false);
  return quad;
}

viz::SolidColorDrawQuad* AddTransformedQuad(viz::RenderPass* pass,
                                            const gfx::Rect& rect,
                                            SkColor color,
                                            const gfx::Transform& transform) {
  viz::SharedQuadState* shared_state = pass->CreateAndAppendSharedQuadState();
  shared_state->SetAll(transform, rect, rect, rect, false, false, 1,

                       SkBlendMode::kSrcOver, 0);
  auto* quad = pass->CreateAndAppendDrawQuad<viz::SolidColorDrawQuad>();
  quad->SetNew(shared_state, rect, rect, color, false);
  return quad;
}

void AddRenderPassQuad(viz::RenderPass* to_pass,
                       viz::RenderPass* contributing_pass) {
  gfx::Rect output_rect = contributing_pass->output_rect;
  viz::SharedQuadState* shared_state =
      to_pass->CreateAndAppendSharedQuadState();
  shared_state->SetAll(gfx::Transform(), output_rect, output_rect, output_rect,
                       false, false, 1, SkBlendMode::kSrcOver, 0);
  auto* quad = to_pass->CreateAndAppendDrawQuad<viz::RenderPassDrawQuad>();
  quad->SetNew(shared_state, output_rect, output_rect, contributing_pass->id, 0,
               gfx::RectF(), gfx::Size(), gfx::Vector2dF(), gfx::PointF(),
               gfx::RectF(), false);
}

void AddRenderPassQuad(viz::RenderPass* to_pass,
                       viz::RenderPass* contributing_pass,
                       viz::ResourceId mask_resource_id,
                       gfx::Transform transform,
                       SkBlendMode blend_mode) {
  gfx::Rect output_rect = contributing_pass->output_rect;
  viz::SharedQuadState* shared_state =
      to_pass->CreateAndAppendSharedQuadState();
  shared_state->SetAll(transform, output_rect, output_rect, output_rect, false,
                       false, 1, blend_mode, 0);
  auto* quad = to_pass->CreateAndAppendDrawQuad<viz::RenderPassDrawQuad>();
  gfx::Size arbitrary_nonzero_size(1, 1);
  quad->SetNew(shared_state, output_rect, output_rect, contributing_pass->id,
               mask_resource_id, gfx::RectF(output_rect),
               arbitrary_nonzero_size, gfx::Vector2dF(), gfx::PointF(),
               gfx::RectF(), false);
}

static void EmptyReleaseCallback(const gpu::SyncToken& sync_token,
                                 bool lost_resource) {}

void AddOneOfEveryQuadType(viz::RenderPass* to_pass,
                           LayerTreeResourceProvider* resource_provider,
                           viz::RenderPassId child_pass_id,
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
      gfx::Size(45, 5), ResourceProvider::TEXTURE_HINT_DEFAULT,
      resource_provider->best_texture_format(), gfx::ColorSpace::CreateSRGB());
  resource_provider->AllocateForTesting(resource1);
  viz::ResourceId resource2 = resource_provider->CreateResource(
      gfx::Size(346, 61), ResourceProvider::TEXTURE_HINT_DEFAULT,
      resource_provider->best_texture_format(), gfx::ColorSpace::CreateSRGB());
  resource_provider->AllocateForTesting(resource2);
  viz::ResourceId resource3 = resource_provider->CreateResource(
      gfx::Size(12, 134), ResourceProvider::TEXTURE_HINT_DEFAULT,
      resource_provider->best_texture_format(), gfx::ColorSpace::CreateSRGB());
  resource_provider->AllocateForTesting(resource3);
  viz::ResourceId resource4 = resource_provider->CreateResource(
      gfx::Size(56, 12), ResourceProvider::TEXTURE_HINT_DEFAULT,
      resource_provider->best_texture_format(), gfx::ColorSpace::CreateSRGB());
  resource_provider->AllocateForTesting(resource4);
  gfx::Size resource5_size(73, 26);
  viz::ResourceId resource5 = resource_provider->CreateResource(
      resource5_size, ResourceProvider::TEXTURE_HINT_DEFAULT,
      resource_provider->best_texture_format(), gfx::ColorSpace::CreateSRGB());
  resource_provider->AllocateForTesting(resource5);
  viz::ResourceId resource6 = resource_provider->CreateResource(
      gfx::Size(64, 92), ResourceProvider::TEXTURE_HINT_DEFAULT,
      resource_provider->best_texture_format(), gfx::ColorSpace::CreateSRGB());
  resource_provider->AllocateForTesting(resource6);
  viz::ResourceId resource7 = resource_provider->CreateResource(
      gfx::Size(9, 14), ResourceProvider::TEXTURE_HINT_DEFAULT,
      resource_provider->best_texture_format(), gfx::ColorSpace::CreateSRGB());
  resource_provider->AllocateForTesting(resource7);

  unsigned target = GL_TEXTURE_2D;
  gpu::Mailbox gpu_mailbox;
  memcpy(gpu_mailbox.name, "Hello world", strlen("Hello world") + 1);
  std::unique_ptr<viz::SingleReleaseCallback> callback =
      viz::SingleReleaseCallback::Create(base::Bind(&EmptyReleaseCallback));
  viz::TextureMailbox mailbox(gpu_mailbox, kSyncTokenForMailboxTextureQuad,
                              target);
  viz::ResourceId resource8 =
      resource_provider->CreateResourceFromTextureMailbox(mailbox,
                                                          std::move(callback));
  resource_provider->AllocateForTesting(resource8);

  viz::SharedQuadState* shared_state =
      to_pass->CreateAndAppendSharedQuadState();
  shared_state->SetAll(gfx::Transform(), rect, rect, rect, false, false, 1,
                       SkBlendMode::kSrcOver, 0);

  auto* debug_border_quad =
      to_pass->CreateAndAppendDrawQuad<viz::DebugBorderDrawQuad>();
  debug_border_quad->SetNew(shared_state, rect, visible_rect, SK_ColorRED, 1);

  if (child_pass_id) {
    auto* render_pass_quad =
        to_pass->CreateAndAppendDrawQuad<viz::RenderPassDrawQuad>();
    render_pass_quad->SetNew(shared_state, rect, visible_rect, child_pass_id,
                             resource5, gfx::RectF(rect), resource5_size,
                             gfx::Vector2dF(), gfx::PointF(), gfx::RectF(),
                             false);
  }

  auto* solid_color_quad =
      to_pass->CreateAndAppendDrawQuad<viz::SolidColorDrawQuad>();
  solid_color_quad->SetNew(shared_state, rect, visible_rect, SK_ColorRED,
                           false);

  auto* stream_video_quad =
      to_pass->CreateAndAppendDrawQuad<viz::StreamVideoDrawQuad>();
  stream_video_quad->SetNew(shared_state, rect, visible_rect, needs_blending,
                            resource6, gfx::Size(), gfx::Transform());

  auto* texture_quad = to_pass->CreateAndAppendDrawQuad<viz::TextureDrawQuad>();
  texture_quad->SetNew(shared_state, rect, visible_rect, needs_blending,
                       resource1, false, gfx::PointF(0.f, 0.f),
                       gfx::PointF(1.f, 1.f), SK_ColorTRANSPARENT,
                       vertex_opacity, false, false, false);

  auto* mailbox_texture_quad =
      to_pass->CreateAndAppendDrawQuad<viz::TextureDrawQuad>();
  mailbox_texture_quad->SetNew(shared_state, rect, visible_rect, needs_blending,
                               resource8, false, gfx::PointF(0.f, 0.f),
                               gfx::PointF(1.f, 1.f), SK_ColorTRANSPARENT,
                               vertex_opacity, false, false, false);

  auto* scaled_tile_quad =
      to_pass->CreateAndAppendDrawQuad<viz::TileDrawQuad>();
  scaled_tile_quad->SetNew(shared_state, rect, visible_rect, needs_blending,
                           resource2, gfx::RectF(0, 0, 50, 50),
                           gfx::Size(50, 50), false, false, false);

  viz::SharedQuadState* transformed_state =
      to_pass->CreateAndAppendSharedQuadState();
  *transformed_state = *shared_state;
  gfx::Transform rotation;
  rotation.Rotate(45);
  transformed_state->quad_to_target_transform =
      transformed_state->quad_to_target_transform * rotation;
  auto* transformed_tile_quad =
      to_pass->CreateAndAppendDrawQuad<viz::TileDrawQuad>();
  transformed_tile_quad->SetNew(
      transformed_state, rect, visible_rect, needs_blending, resource3,
      gfx::RectF(0, 0, 100, 100), gfx::Size(100, 100), false, false, false);

  viz::SharedQuadState* shared_state2 =
      to_pass->CreateAndAppendSharedQuadState();
  shared_state->SetAll(gfx::Transform(), rect, rect, rect, false, false, 1,
                       SkBlendMode::kSrcOver, 0);

  auto* tile_quad = to_pass->CreateAndAppendDrawQuad<viz::TileDrawQuad>();
  tile_quad->SetNew(shared_state2, rect, visible_rect, needs_blending,
                    resource4, gfx::RectF(0, 0, 100, 100), gfx::Size(100, 100),
                    false, false, false);

  viz::ResourceId plane_resources[4];
  for (int i = 0; i < 4; ++i) {
    plane_resources[i] = resource_provider->CreateResource(
        gfx::Size(20, 12), ResourceProvider::TEXTURE_HINT_DEFAULT,
        resource_provider->best_texture_format(),
        gfx::ColorSpace::CreateREC601());
    resource_provider->AllocateForTesting(plane_resources[i]);
  }
  auto color_space = viz::YUVVideoDrawQuad::REC_601;

  auto* yuv_quad = to_pass->CreateAndAppendDrawQuad<viz::YUVVideoDrawQuad>();
  yuv_quad->SetNew(shared_state2, rect, visible_rect, needs_blending,
                   gfx::RectF(.0f, .0f, 100.0f, 100.0f),
                   gfx::RectF(.0f, .0f, 50.0f, 50.0f), gfx::Size(100, 100),
                   gfx::Size(50, 50), plane_resources[0], plane_resources[1],
                   plane_resources[2], plane_resources[3], color_space,
                   gfx::ColorSpace::CreateREC601(), 0.0, 1.0, 8);
}

static void CollectResources(
    std::vector<viz::ReturnedResource>* array,
    const std::vector<viz::ReturnedResource>& returned) {}

void AddOneOfEveryQuadTypeInDisplayResourceProvider(
    viz::RenderPass* to_pass,
    DisplayResourceProvider* resource_provider,
    LayerTreeResourceProvider* child_resource_provider,
    viz::RenderPassId child_pass_id,
    gpu::SyncToken* sync_token_for_mailbox_tebxture) {
  gfx::Rect rect(0, 0, 100, 100);
  gfx::Rect visible_rect(0, 0, 100, 100);
  bool needs_blending = true;
  const float vertex_opacity[] = {1.0f, 1.0f, 1.0f, 1.0f};

  static const gpu::SyncToken kSyncTokenForMailboxTextureQuad(
      gpu::CommandBufferNamespace::GPU_IO, 0,
      gpu::CommandBufferId::FromUnsafeValue(0x123), 30);
  *sync_token_for_mailbox_tebxture = kSyncTokenForMailboxTextureQuad;

  viz::ResourceId resource1 = child_resource_provider->CreateResource(
      gfx::Size(45, 5), ResourceProvider::TEXTURE_HINT_DEFAULT,
      child_resource_provider->best_texture_format(),
      gfx::ColorSpace::CreateSRGB());
  child_resource_provider->AllocateForTesting(resource1);
  viz::ResourceId resource2 = child_resource_provider->CreateResource(
      gfx::Size(346, 61), ResourceProvider::TEXTURE_HINT_DEFAULT,
      child_resource_provider->best_texture_format(),
      gfx::ColorSpace::CreateSRGB());
  child_resource_provider->AllocateForTesting(resource2);
  viz::ResourceId resource3 = child_resource_provider->CreateResource(
      gfx::Size(12, 134), ResourceProvider::TEXTURE_HINT_DEFAULT,
      child_resource_provider->best_texture_format(),
      gfx::ColorSpace::CreateSRGB());
  child_resource_provider->AllocateForTesting(resource3);
  viz::ResourceId resource4 = child_resource_provider->CreateResource(
      gfx::Size(56, 12), ResourceProvider::TEXTURE_HINT_DEFAULT,
      child_resource_provider->best_texture_format(),
      gfx::ColorSpace::CreateSRGB());
  child_resource_provider->AllocateForTesting(resource4);
  gfx::Size resource5_size(73, 26);
  viz::ResourceId resource5 = child_resource_provider->CreateResource(
      resource5_size, ResourceProvider::TEXTURE_HINT_DEFAULT,
      child_resource_provider->best_texture_format(),
      gfx::ColorSpace::CreateSRGB());
  child_resource_provider->AllocateForTesting(resource5);
  viz::ResourceId resource6 = child_resource_provider->CreateResource(
      gfx::Size(64, 92), ResourceProvider::TEXTURE_HINT_DEFAULT,
      child_resource_provider->best_texture_format(),
      gfx::ColorSpace::CreateSRGB());
  child_resource_provider->AllocateForTesting(resource6);
  viz::ResourceId resource7 = child_resource_provider->CreateResource(
      gfx::Size(9, 14), ResourceProvider::TEXTURE_HINT_DEFAULT,
      child_resource_provider->best_texture_format(),
      gfx::ColorSpace::CreateSRGB());
  child_resource_provider->AllocateForTesting(resource7);

  unsigned target = GL_TEXTURE_2D;
  gpu::Mailbox gpu_mailbox;
  memcpy(gpu_mailbox.name, "Hello world", strlen("Hello world") + 1);
  std::unique_ptr<viz::SingleReleaseCallback> callback =
      viz::SingleReleaseCallback::Create(base::Bind(&EmptyReleaseCallback));
  viz::TextureMailbox mailbox(gpu_mailbox, kSyncTokenForMailboxTextureQuad,
                              target);
  viz::ResourceId resource8 =
      child_resource_provider->CreateResourceFromTextureMailbox(
          mailbox, std::move(callback));
  child_resource_provider->AllocateForTesting(resource8);

  // Transfer resource to the parent.
  ResourceProvider::ResourceIdArray resource_ids_to_transfer;
  resource_ids_to_transfer.push_back(resource1);
  resource_ids_to_transfer.push_back(resource2);
  resource_ids_to_transfer.push_back(resource3);
  resource_ids_to_transfer.push_back(resource4);
  resource_ids_to_transfer.push_back(resource5);
  resource_ids_to_transfer.push_back(resource6);
  resource_ids_to_transfer.push_back(resource7);
  resource_ids_to_transfer.push_back(resource8);

  viz::ResourceId plane_resources[4];
  for (int i = 0; i < 4; ++i) {
    plane_resources[i] = child_resource_provider->CreateResource(
        gfx::Size(20, 12), ResourceProvider::TEXTURE_HINT_DEFAULT,
        child_resource_provider->best_texture_format(),
        gfx::ColorSpace::CreateREC601());
    child_resource_provider->AllocateForTesting(plane_resources[i]);
    resource_ids_to_transfer.push_back(plane_resources[i]);
  }

  std::vector<viz::ReturnedResource> returned_to_child;
  int child_id = resource_provider->CreateChild(
      base::Bind(&CollectResources, &returned_to_child));

  // Transfer resource to the parent.
  std::vector<viz::TransferableResource> list;
  child_resource_provider->PrepareSendToParent(resource_ids_to_transfer, &list);
  resource_provider->ReceiveFromChild(child_id, list);

  // Before create DrawQuad in DisplayResourceProvider's namespace, get the
  // mapped resource id first.
  ResourceProvider::ResourceIdMap resource_map =
      resource_provider->GetChildToParentMap(child_id);
  viz::ResourceId mapped_resource1 = resource_map[resource1];
  viz::ResourceId mapped_resource2 = resource_map[resource2];
  viz::ResourceId mapped_resource4 = resource_map[resource4];
  viz::ResourceId mapped_resource5 = resource_map[resource5];
  viz::ResourceId mapped_resource6 = resource_map[resource6];
  viz::ResourceId mapped_resource8 = resource_map[resource8];
  viz::ResourceId mapped_plane_resources[4];
  for (int i = 0; i < 4; ++i) {
    mapped_plane_resources[i] = resource_map[plane_resources[i]];
  }

  viz::SharedQuadState* shared_state =
      to_pass->CreateAndAppendSharedQuadState();
  shared_state->SetAll(gfx::Transform(), rect, rect, rect, false, false, 1,
                       SkBlendMode::kSrcOver, 0);

  viz::DebugBorderDrawQuad* debug_border_quad =
      to_pass->CreateAndAppendDrawQuad<viz::DebugBorderDrawQuad>();
  debug_border_quad->SetNew(shared_state, rect, visible_rect, SK_ColorRED, 1);

  if (child_pass_id) {
    viz::RenderPassDrawQuad* render_pass_quad =
        to_pass->CreateAndAppendDrawQuad<viz::RenderPassDrawQuad>();
    render_pass_quad->SetNew(shared_state, rect, visible_rect, child_pass_id,
                             mapped_resource5, gfx::RectF(rect), resource5_size,
                             gfx::Vector2dF(), gfx::PointF(), gfx::RectF(),
                             false);
  }

  viz::SolidColorDrawQuad* solid_color_quad =
      to_pass->CreateAndAppendDrawQuad<viz::SolidColorDrawQuad>();
  solid_color_quad->SetNew(shared_state, rect, visible_rect, SK_ColorRED,
                           false);

  viz::StreamVideoDrawQuad* stream_video_quad =
      to_pass->CreateAndAppendDrawQuad<viz::StreamVideoDrawQuad>();
  stream_video_quad->SetNew(shared_state, rect, visible_rect, needs_blending,
                            mapped_resource6, gfx::Size(), gfx::Transform());

  viz::TextureDrawQuad* texture_quad =
      to_pass->CreateAndAppendDrawQuad<viz::TextureDrawQuad>();
  texture_quad->SetNew(shared_state, rect, visible_rect, needs_blending,
                       mapped_resource1, false, gfx::PointF(0.f, 0.f),
                       gfx::PointF(1.f, 1.f), SK_ColorTRANSPARENT,
                       vertex_opacity, false, false, false);

  viz::TextureDrawQuad* mailbox_texture_quad =
      to_pass->CreateAndAppendDrawQuad<viz::TextureDrawQuad>();
  mailbox_texture_quad->SetNew(shared_state, rect, visible_rect, needs_blending,
                               mapped_resource8, false, gfx::PointF(0.f, 0.f),
                               gfx::PointF(1.f, 1.f), SK_ColorTRANSPARENT,
                               vertex_opacity, false, false, false);

  viz::TileDrawQuad* scaled_tile_quad =
      to_pass->CreateAndAppendDrawQuad<viz::TileDrawQuad>();
  scaled_tile_quad->SetNew(shared_state, rect, visible_rect, needs_blending,
                           mapped_resource2, gfx::RectF(0, 0, 50, 50),
                           gfx::Size(50, 50), false, false, false);

  viz::SharedQuadState* transformed_state =
      to_pass->CreateAndAppendSharedQuadState();
  *transformed_state = *shared_state;
  gfx::Transform rotation;
  rotation.Rotate(45);
  transformed_state->quad_to_target_transform =
      transformed_state->quad_to_target_transform * rotation;
  viz::TileDrawQuad* transformed_tile_quad =
      to_pass->CreateAndAppendDrawQuad<viz::TileDrawQuad>();
  transformed_tile_quad->SetNew(
      transformed_state, rect, visible_rect, needs_blending, resource3,
      gfx::RectF(0, 0, 100, 100), gfx::Size(100, 100), false, false, false);

  viz::SharedQuadState* shared_state2 =
      to_pass->CreateAndAppendSharedQuadState();
  shared_state->SetAll(gfx::Transform(), rect, rect, rect, false, false, 1,
                       SkBlendMode::kSrcOver, 0);

  viz::TileDrawQuad* tile_quad =
      to_pass->CreateAndAppendDrawQuad<viz::TileDrawQuad>();
  tile_quad->SetNew(shared_state2, rect, visible_rect, needs_blending,
                    mapped_resource4, gfx::RectF(0, 0, 100, 100),
                    gfx::Size(100, 100), false, false, false);

  viz::YUVVideoDrawQuad::ColorSpace color_space =
      viz::YUVVideoDrawQuad::REC_601;

  viz::YUVVideoDrawQuad* yuv_quad =
      to_pass->CreateAndAppendDrawQuad<viz::YUVVideoDrawQuad>();
  yuv_quad->SetNew(shared_state2, rect, visible_rect, needs_blending,
                   gfx::RectF(.0f, .0f, 100.0f, 100.0f),
                   gfx::RectF(.0f, .0f, 50.0f, 50.0f), gfx::Size(100, 100),
                   gfx::Size(50, 50), mapped_plane_resources[0],
                   mapped_plane_resources[1], mapped_plane_resources[2],
                   mapped_plane_resources[3], color_space,
                   gfx::ColorSpace::CreateREC601(), 0.0, 1.0, 8);
}

}  // namespace cc
