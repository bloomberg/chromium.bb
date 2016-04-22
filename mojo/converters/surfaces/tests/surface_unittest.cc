// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <utility>

#include "cc/quads/debug_border_draw_quad.h"
#include "cc/quads/render_pass.h"
#include "cc/quads/solid_color_draw_quad.h"
#include "cc/quads/surface_draw_quad.h"
#include "cc/quads/texture_draw_quad.h"
#include "cc/resources/resource_provider.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/mailbox_holder.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "mojo/converters/geometry/geometry_type_converters.h"
#include "mojo/converters/surfaces/surfaces_type_converters.h"
#include "mojo/converters/transform/transform_type_converters.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkXfermode.h"

using mus::mojom::Color;
using mus::mojom::ColorPtr;
using mus::mojom::CompositorFrame;
using mus::mojom::CompositorFramePtr;
using mus::mojom::CompositorFrameMetadata;
using mus::mojom::CompositorFrameMetadataPtr;
using mus::mojom::DebugBorderQuadState;
using mus::mojom::DebugBorderQuadStatePtr;
using mus::mojom::Pass;
using mus::mojom::PassPtr;
using mus::mojom::Quad;
using mus::mojom::QuadPtr;
using mus::mojom::RenderPassId;
using mus::mojom::RenderPassIdPtr;
using mus::mojom::RenderPassQuadState;
using mus::mojom::RenderPassQuadStatePtr;
using mus::mojom::ResourceFormat;
using mus::mojom::ReturnedResource;
using mus::mojom::ReturnedResourcePtr;
using mus::mojom::SharedQuadState;
using mus::mojom::SharedQuadStatePtr;
using mus::mojom::SolidColorQuadState;
using mus::mojom::SolidColorQuadStatePtr;
using mus::mojom::SurfaceId;
using mus::mojom::SurfaceIdPtr;
using mus::mojom::SurfaceQuadState;
using mus::mojom::SurfaceQuadStatePtr;
using mus::mojom::TextureQuadState;
using mus::mojom::TextureQuadStatePtr;
using mus::mojom::TileQuadState;
using mus::mojom::TileQuadStatePtr;
using mus::mojom::TransferableResource;
using mus::mojom::TransferableResourcePtr;
using mus::mojom::YUVColorSpace;
using mus::mojom::YUVVideoQuadState;
using mus::mojom::YUVVideoQuadStatePtr;

namespace mojo {
namespace {

TEST(SurfaceLibTest, SurfaceIdConverterNullId) {
  cc::SurfaceId null_id;
  cc::SurfaceId round_trip = SurfaceId::From(null_id).To<cc::SurfaceId>();
  EXPECT_TRUE(round_trip.is_null());
}

TEST(SurfaceLibTest, SurfaceIdConverterValidId) {
  cc::SurfaceId valid_id(7);
  cc::SurfaceId round_trip = SurfaceId::From(valid_id).To<cc::SurfaceId>();
  EXPECT_FALSE(round_trip.is_null());
  EXPECT_EQ(valid_id, round_trip);
}

TEST(SurfaceLibTest, Color) {
  SkColor arbitrary_color = SK_ColorMAGENTA;
  SkColor round_trip = Color::From(arbitrary_color).To<SkColor>();
  EXPECT_EQ(arbitrary_color, round_trip);
}

class SurfaceLibQuadTest : public testing::Test {
 public:
  SurfaceLibQuadTest()
      : rect(5, 7, 13, 19),
        opaque_rect(rect),
        visible_rect(9, 11, 5, 7),
        needs_blending(false) {
    pass = cc::RenderPass::Create();
    sqs = pass->CreateAndAppendSharedQuadState();
  }

 protected:
  gfx::Rect rect;
  gfx::Rect opaque_rect;
  gfx::Rect visible_rect;
  bool needs_blending;
  std::unique_ptr<cc::RenderPass> pass;
  cc::SharedQuadState* sqs;
};

TEST_F(SurfaceLibQuadTest, ColorQuad) {
  cc::SolidColorDrawQuad* color_quad =
      pass->CreateAndAppendDrawQuad<cc::SolidColorDrawQuad>();
  SkColor arbitrary_color = SK_ColorGREEN;
  bool force_anti_aliasing_off = true;
  color_quad->SetAll(sqs,
                     rect,
                     opaque_rect,
                     visible_rect,
                     needs_blending,
                     arbitrary_color,
                     force_anti_aliasing_off);

  QuadPtr mus_quad = Quad::From<cc::DrawQuad>(*color_quad);
  ASSERT_FALSE(mus_quad.is_null());
  EXPECT_EQ(mus::mojom::Material::SOLID_COLOR, mus_quad->material);
  EXPECT_TRUE(Rect::From(rect).Equals(mus_quad->rect));
  EXPECT_TRUE(Rect::From(opaque_rect).Equals(mus_quad->opaque_rect));
  EXPECT_TRUE(Rect::From(visible_rect).Equals(mus_quad->visible_rect));
  EXPECT_EQ(needs_blending, mus_quad->needs_blending);
  ASSERT_TRUE(mus_quad->solid_color_quad_state);
  SolidColorQuadStatePtr& mus_color_state = mus_quad->solid_color_quad_state;
  EXPECT_TRUE(Color::From(arbitrary_color).Equals(mus_color_state->color));
  EXPECT_EQ(force_anti_aliasing_off, mus_color_state->force_anti_aliasing_off);
}

TEST_F(SurfaceLibQuadTest, SurfaceQuad) {
  cc::SurfaceDrawQuad* surface_quad =
      pass->CreateAndAppendDrawQuad<cc::SurfaceDrawQuad>();
  cc::SurfaceId arbitrary_id(5);
  surface_quad->SetAll(
      sqs, rect, opaque_rect, visible_rect, needs_blending, arbitrary_id);

  QuadPtr mus_quad = Quad::From<cc::DrawQuad>(*surface_quad);
  ASSERT_FALSE(mus_quad.is_null());
  EXPECT_EQ(mus::mojom::Material::SURFACE_CONTENT, mus_quad->material);
  ASSERT_TRUE(mus_quad->surface_quad_state);
  SurfaceQuadStatePtr& mus_surface_state = mus_quad->surface_quad_state;
  EXPECT_TRUE(SurfaceId::From(arbitrary_id).Equals(mus_surface_state->surface));
}

TEST_F(SurfaceLibQuadTest, TextureQuad) {
  cc::TextureDrawQuad* texture_quad =
      pass->CreateAndAppendDrawQuad<cc::TextureDrawQuad>();
  unsigned resource_id = 9;
  bool premultiplied_alpha = true;
  gfx::PointF uv_top_left(1.7f, 2.1f);
  gfx::PointF uv_bottom_right(-7.f, 16.3f);
  SkColor background_color = SK_ColorYELLOW;
  float vertex_opacity[4] = {0.1f, 0.5f, 0.4f, 0.8f};
  bool y_flipped = false;
  bool nearest_neighbor = false;
  texture_quad->SetAll(sqs, rect, opaque_rect, visible_rect, needs_blending,
                       resource_id, gfx::Size(), premultiplied_alpha,
                       uv_top_left, uv_bottom_right, background_color,
                       vertex_opacity, y_flipped, nearest_neighbor);

  QuadPtr mus_quad = Quad::From<cc::DrawQuad>(*texture_quad);
  ASSERT_FALSE(mus_quad.is_null());
  EXPECT_EQ(mus::mojom::Material::TEXTURE_CONTENT, mus_quad->material);
  ASSERT_TRUE(mus_quad->texture_quad_state);
  TextureQuadStatePtr& mus_texture_state = mus_quad->texture_quad_state;
  EXPECT_EQ(resource_id, mus_texture_state->resource_id);
  EXPECT_EQ(premultiplied_alpha, mus_texture_state->premultiplied_alpha);
  EXPECT_TRUE(PointF::From(uv_top_left).Equals(mus_texture_state->uv_top_left));
  EXPECT_TRUE(
      PointF::From(uv_bottom_right).Equals(mus_texture_state->uv_bottom_right));
  EXPECT_TRUE(Color::From(background_color)
                  .Equals(mus_texture_state->background_color));
  for (size_t i = 0; i < 4; ++i) {
    EXPECT_EQ(vertex_opacity[i], mus_texture_state->vertex_opacity[i]) << i;
  }
  EXPECT_EQ(y_flipped, mus_texture_state->y_flipped);
}

TEST_F(SurfaceLibQuadTest, TextureQuadEmptyVertexOpacity) {
  QuadPtr mus_texture_quad = Quad::New();
  mus_texture_quad->material = mus::mojom::Material::TEXTURE_CONTENT;
  TextureQuadStatePtr mus_texture_state = TextureQuadState::New();
  mus_texture_state->background_color = Color::New();
  mus_texture_quad->texture_quad_state = std::move(mus_texture_state);
  PassPtr mus_pass = Pass::New();
  mus_pass->id = RenderPassId::New();
  mus_pass->id->layer_id = 1;
  mus_pass->id->index = 1u;
  mus_pass->quads.push_back(std::move(mus_texture_quad));
  SharedQuadStatePtr mus_sqs = SharedQuadState::New();
  mus_pass->shared_quad_states.push_back(std::move(mus_sqs));

  std::unique_ptr<cc::RenderPass> pass =
      mus_pass.To<std::unique_ptr<cc::RenderPass>>();

  EXPECT_FALSE(pass);
}

TEST_F(SurfaceLibQuadTest, TextureQuadEmptyBackgroundColor) {
  QuadPtr mus_texture_quad = Quad::New();
  mus_texture_quad->material = mus::mojom::Material::TEXTURE_CONTENT;
  TextureQuadStatePtr mus_texture_state = TextureQuadState::New();
  mus_texture_state->vertex_opacity = mojo::Array<float>::New(4);
  mus_texture_quad->texture_quad_state = std::move(mus_texture_state);
  PassPtr mus_pass = Pass::New();
  mus_pass->id = RenderPassId::New();
  mus_pass->id->layer_id = 1;
  mus_pass->id->index = 1u;
  mus_pass->quads.push_back(std::move(mus_texture_quad));
  SharedQuadStatePtr mus_sqs = SharedQuadState::New();
  mus_pass->shared_quad_states.push_back(std::move(mus_sqs));

  std::unique_ptr<cc::RenderPass> pass =
      mus_pass.To<std::unique_ptr<cc::RenderPass>>();
  EXPECT_FALSE(pass);
}

TEST(SurfaceLibTest, SharedQuadState) {
  gfx::Transform quad_to_target_transform;
  quad_to_target_transform.Scale3d(0.3f, 0.7f, 0.9f);
  gfx::Size quad_layer_bounds(57, 39);
  gfx::Rect visible_quad_layer_rect(3, 7, 28, 42);
  gfx::Rect clip_rect(9, 12, 21, 31);
  bool is_clipped = true;
  float opacity = 0.65f;
  int sorting_context_id = 13;
  ::SkXfermode::Mode blend_mode = ::SkXfermode::kSrcOver_Mode;
  std::unique_ptr<cc::RenderPass> pass = cc::RenderPass::Create();
  cc::SharedQuadState* sqs = pass->CreateAndAppendSharedQuadState();
  sqs->SetAll(quad_to_target_transform, quad_layer_bounds,
              visible_quad_layer_rect, clip_rect, is_clipped, opacity,
              blend_mode, sorting_context_id);

  SharedQuadStatePtr mus_sqs = SharedQuadState::From(*sqs);
  ASSERT_FALSE(mus_sqs.is_null());
  EXPECT_TRUE(Transform::From(quad_to_target_transform)
                  .Equals(mus_sqs->quad_to_target_transform));
  EXPECT_TRUE(Size::From(quad_layer_bounds).Equals(mus_sqs->quad_layer_bounds));
  EXPECT_TRUE(Rect::From(visible_quad_layer_rect)
                  .Equals(mus_sqs->visible_quad_layer_rect));
  EXPECT_TRUE(Rect::From(clip_rect).Equals(mus_sqs->clip_rect));
  EXPECT_EQ(is_clipped, mus_sqs->is_clipped);
  EXPECT_EQ(opacity, mus_sqs->opacity);
  EXPECT_EQ(sorting_context_id, mus_sqs->sorting_context_id);
}

TEST(SurfaceLibTest, RenderPass) {
  std::unique_ptr<cc::RenderPass> pass = cc::RenderPass::Create();
  cc::RenderPassId pass_id(1, 6);
  gfx::Rect output_rect(4, 9, 13, 71);
  gfx::Rect damage_rect(9, 17, 41, 45);
  gfx::Transform transform_to_root_target;
  transform_to_root_target.Skew(0.0, 43.0);
  bool has_transparent_background = false;
  pass->SetAll(pass_id,
               output_rect,
               damage_rect,
               transform_to_root_target,
               has_transparent_background);

  gfx::Transform quad_to_target_transform;
  quad_to_target_transform.Scale3d(0.3f, 0.7f, 0.9f);
  gfx::Size quad_layer_bounds(57, 39);
  gfx::Rect visible_quad_layer_rect(3, 7, 28, 42);
  gfx::Rect clip_rect(9, 12, 21, 31);
  bool is_clipped = true;
  float opacity = 0.65f;
  int sorting_context_id = 13;
  ::SkXfermode::Mode blend_mode = ::SkXfermode::kSrcOver_Mode;
  cc::SharedQuadState* sqs = pass->CreateAndAppendSharedQuadState();
  sqs->SetAll(quad_to_target_transform, quad_layer_bounds,
              visible_quad_layer_rect, clip_rect, is_clipped, opacity,
              blend_mode, sorting_context_id);

  gfx::Rect rect(5, 7, 13, 19);
  gfx::Rect opaque_rect(rect);
  gfx::Rect visible_rect(9, 11, 5, 7);
  bool needs_blending = false;

  cc::SolidColorDrawQuad* color_quad =
      pass->CreateAndAppendDrawQuad<cc::SolidColorDrawQuad>();
  SkColor arbitrary_color = SK_ColorGREEN;
  bool force_anti_aliasing_off = true;
  color_quad->SetAll(pass->shared_quad_state_list.back(),
                     rect,
                     opaque_rect,
                     visible_rect,
                     needs_blending,
                     arbitrary_color,
                     force_anti_aliasing_off);

  cc::SurfaceDrawQuad* surface_quad =
      pass->CreateAndAppendDrawQuad<cc::SurfaceDrawQuad>();
  cc::SurfaceId arbitrary_id(5);
  surface_quad->SetAll(
      sqs, rect, opaque_rect, visible_rect, needs_blending, arbitrary_id);

  cc::TextureDrawQuad* texture_quad =
      pass->CreateAndAppendDrawQuad<cc::TextureDrawQuad>();
  unsigned resource_id = 9;
  bool premultiplied_alpha = true;
  gfx::PointF uv_top_left(1.7f, 2.1f);
  gfx::PointF uv_bottom_right(-7.f, 16.3f);
  SkColor background_color = SK_ColorYELLOW;
  float vertex_opacity[4] = {0.1f, 0.5f, 0.4f, 0.8f};
  bool y_flipped = false;
  bool nearest_neighbor = false;
  texture_quad->SetAll(sqs, rect, opaque_rect, visible_rect, needs_blending,
                       resource_id, gfx::Size(), premultiplied_alpha,
                       uv_top_left, uv_bottom_right, background_color,
                       vertex_opacity, y_flipped, nearest_neighbor);

  PassPtr mus_pass = Pass::From(*pass);
  ASSERT_FALSE(mus_pass.is_null());
  EXPECT_EQ(6u, mus_pass->id->index);
  EXPECT_TRUE(Rect::From(output_rect).Equals(mus_pass->output_rect));
  EXPECT_TRUE(Rect::From(damage_rect).Equals(mus_pass->damage_rect));
  EXPECT_TRUE(Transform::From(transform_to_root_target)
                  .Equals(mus_pass->transform_to_root_target));
  EXPECT_EQ(has_transparent_background, mus_pass->has_transparent_background);
  ASSERT_EQ(1u, mus_pass->shared_quad_states.size());
  ASSERT_EQ(3u, mus_pass->quads.size());
  EXPECT_EQ(0u, mus_pass->quads[0]->shared_quad_state_index);

  std::unique_ptr<cc::RenderPass> round_trip_pass =
      mus_pass.To<std::unique_ptr<cc::RenderPass>>();
  EXPECT_EQ(pass_id, round_trip_pass->id);
  EXPECT_EQ(output_rect, round_trip_pass->output_rect);
  EXPECT_EQ(damage_rect, round_trip_pass->damage_rect);
  EXPECT_EQ(transform_to_root_target,
            round_trip_pass->transform_to_root_target);
  EXPECT_EQ(has_transparent_background,
            round_trip_pass->has_transparent_background);
  ASSERT_EQ(1u, round_trip_pass->shared_quad_state_list.size());
  ASSERT_EQ(3u, round_trip_pass->quad_list.size());
  EXPECT_EQ(round_trip_pass->shared_quad_state_list.front(),
            round_trip_pass->quad_list.front()->shared_quad_state);

  cc::SharedQuadState* round_trip_sqs =
      round_trip_pass->shared_quad_state_list.front();
  EXPECT_EQ(quad_to_target_transform, round_trip_sqs->quad_to_target_transform);
  EXPECT_EQ(quad_layer_bounds, round_trip_sqs->quad_layer_bounds);
  EXPECT_EQ(visible_quad_layer_rect, round_trip_sqs->visible_quad_layer_rect);
  EXPECT_EQ(clip_rect, round_trip_sqs->clip_rect);
  EXPECT_EQ(is_clipped, round_trip_sqs->is_clipped);
  EXPECT_EQ(opacity, round_trip_sqs->opacity);
  EXPECT_EQ(sorting_context_id, round_trip_sqs->sorting_context_id);

  cc::DrawQuad* round_trip_quad = round_trip_pass->quad_list.front();
  // First is solid color quad.
  ASSERT_EQ(cc::DrawQuad::SOLID_COLOR, round_trip_quad->material);
  EXPECT_EQ(rect, round_trip_quad->rect);
  EXPECT_EQ(opaque_rect, round_trip_quad->opaque_rect);
  EXPECT_EQ(visible_rect, round_trip_quad->visible_rect);
  EXPECT_EQ(needs_blending, round_trip_quad->needs_blending);
  const cc::SolidColorDrawQuad* round_trip_color_quad =
      cc::SolidColorDrawQuad::MaterialCast(round_trip_quad);
  EXPECT_EQ(arbitrary_color, round_trip_color_quad->color);
  EXPECT_EQ(force_anti_aliasing_off,
            round_trip_color_quad->force_anti_aliasing_off);

  round_trip_quad = round_trip_pass->quad_list.ElementAt(1);
  // Second is surface quad.
  ASSERT_EQ(cc::DrawQuad::SURFACE_CONTENT, round_trip_quad->material);
  const cc::SurfaceDrawQuad* round_trip_surface_quad =
      cc::SurfaceDrawQuad::MaterialCast(round_trip_quad);
  EXPECT_EQ(arbitrary_id, round_trip_surface_quad->surface_id);

  round_trip_quad = round_trip_pass->quad_list.ElementAt(2);
  // Third is texture quad.
  ASSERT_EQ(cc::DrawQuad::TEXTURE_CONTENT, round_trip_quad->material);
  const cc::TextureDrawQuad* round_trip_texture_quad =
      cc::TextureDrawQuad::MaterialCast(round_trip_quad);
  EXPECT_EQ(resource_id, round_trip_texture_quad->resource_id());
  EXPECT_EQ(premultiplied_alpha, round_trip_texture_quad->premultiplied_alpha);
  EXPECT_EQ(uv_top_left, round_trip_texture_quad->uv_top_left);
  EXPECT_EQ(uv_bottom_right, round_trip_texture_quad->uv_bottom_right);
  EXPECT_EQ(background_color, round_trip_texture_quad->background_color);
  for (size_t i = 0; i < 4; ++i) {
    EXPECT_EQ(vertex_opacity[i], round_trip_texture_quad->vertex_opacity[i])
        << i;
  }
  EXPECT_EQ(y_flipped, round_trip_texture_quad->y_flipped);
}

TEST(SurfaceLibTest, TransferableResource) {
  uint32_t id = 7u;
  cc::ResourceFormat format = cc::BGRA_8888;
  uint32_t filter = 123u;
  gfx::Size size(17, 18);
  gpu::MailboxHolder mailbox_holder;
  bool is_software = false;
  cc::TransferableResource resource;
  resource.id = id;
  resource.format = format;
  resource.filter = filter;
  resource.size = size;
  resource.mailbox_holder = mailbox_holder;
  resource.is_software = is_software;

  TransferableResourcePtr mus_resource = TransferableResource::From(resource);
  EXPECT_EQ(id, mus_resource->id);
  EXPECT_EQ(static_cast<ResourceFormat>(format), mus_resource->format);
  EXPECT_EQ(filter, mus_resource->filter);
  EXPECT_TRUE(Size::From(size).Equals(mus_resource->size));
  EXPECT_EQ(is_software, mus_resource->is_software);

  cc::TransferableResource round_trip_resource =
      mus_resource.To<cc::TransferableResource>();
  EXPECT_EQ(id, round_trip_resource.id);
  EXPECT_EQ(format, round_trip_resource.format);
  EXPECT_EQ(filter, round_trip_resource.filter);
  EXPECT_EQ(size, round_trip_resource.size);
  EXPECT_EQ(mailbox_holder.mailbox, round_trip_resource.mailbox_holder.mailbox);
  EXPECT_EQ(mailbox_holder.texture_target,
            round_trip_resource.mailbox_holder.texture_target);
  EXPECT_EQ(mailbox_holder.sync_token,
            round_trip_resource.mailbox_holder.sync_token);
  EXPECT_EQ(is_software, round_trip_resource.is_software);
}

TEST(SurfaceLibTest, ReturnedResource) {
  uint32_t id = 5u;
  gpu::SyncToken sync_token(gpu::CommandBufferNamespace::GPU_IO, 0,
                            gpu::CommandBufferId::FromUnsafeValue(1), 24u);
  sync_token.SetVerifyFlush();
  int count = 2;
  bool lost = false;
  cc::ReturnedResource resource;
  resource.id = id;
  resource.sync_token = sync_token;
  resource.count = count;
  resource.lost = lost;

  ReturnedResourcePtr mus_resource = ReturnedResource::From(resource);
  EXPECT_EQ(id, mus_resource->id);
  EXPECT_EQ(sync_token, mus_resource->sync_token);
  EXPECT_EQ(count, mus_resource->count);
  EXPECT_EQ(lost, mus_resource->lost);

  cc::ReturnedResource round_trip_resource =
      mus_resource.To<cc::ReturnedResource>();
  EXPECT_EQ(id, round_trip_resource.id);
  EXPECT_EQ(sync_token, round_trip_resource.sync_token);
  EXPECT_EQ(count, round_trip_resource.count);
  EXPECT_EQ(lost, round_trip_resource.lost);
}

TEST_F(SurfaceLibQuadTest, DebugBorderQuad) {
  cc::DebugBorderDrawQuad* debug_border_quad =
      pass->CreateAndAppendDrawQuad<cc::DebugBorderDrawQuad>();
  const SkColor arbitrary_color = SK_ColorGREEN;
  const int width = 3;
  debug_border_quad->SetAll(sqs,
                            rect,
                            opaque_rect,
                            visible_rect,
                            needs_blending,
                            arbitrary_color,
                            width);

  QuadPtr mus_quad = Quad::From<cc::DrawQuad>(*debug_border_quad);
  ASSERT_FALSE(mus_quad.is_null());
  EXPECT_EQ(mus::mojom::Material::DEBUG_BORDER, mus_quad->material);
  EXPECT_TRUE(Rect::From(rect).Equals(mus_quad->rect));
  EXPECT_TRUE(Rect::From(opaque_rect).Equals(mus_quad->opaque_rect));
  EXPECT_TRUE(Rect::From(visible_rect).Equals(mus_quad->visible_rect));
  EXPECT_EQ(needs_blending, mus_quad->needs_blending);
  ASSERT_TRUE(mus_quad->debug_border_quad_state);
  DebugBorderQuadStatePtr& mus_debug_border_state =
      mus_quad->debug_border_quad_state;
  EXPECT_TRUE(
      Color::From(arbitrary_color).Equals(mus_debug_border_state->color));
  EXPECT_EQ(width, mus_debug_border_state->width);
}

}  // namespace
}  // namespace mojo
