// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/quads/draw_quad.h"

#include <stddef.h>

#include <algorithm>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "cc/base/filter_operations.h"
#include "cc/base/math_util.h"
#include "cc/quads/debug_border_draw_quad.h"
#include "cc/quads/largest_draw_quad.h"
#include "cc/quads/picture_draw_quad.h"
#include "cc/quads/render_pass.h"
#include "cc/quads/render_pass_draw_quad.h"
#include "cc/quads/solid_color_draw_quad.h"
#include "cc/quads/stream_video_draw_quad.h"
#include "cc/quads/surface_draw_quad.h"
#include "cc/quads/texture_draw_quad.h"
#include "cc/quads/tile_draw_quad.h"
#include "cc/quads/yuv_video_draw_quad.h"
#include "cc/test/fake_raster_source.h"
#include "cc/test/geometry_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/effects/SkBlurImageFilter.h"
#include "ui/gfx/transform.h"

namespace cc {
namespace {

static constexpr viz::FrameSinkId kArbitraryFrameSinkId(1, 1);

TEST(DrawQuadTest, CopySharedQuadState) {
  gfx::Transform quad_transform = gfx::Transform(1.0, 0.0, 0.5, 1.0, 0.5, 0.0);
  gfx::Rect layer_rect(26, 28);
  gfx::Rect visible_layer_rect(10, 12, 14, 16);
  gfx::Rect clip_rect(19, 21, 23, 25);
  bool is_clipped = true;
  bool are_contents_opaque = true;
  float opacity = 0.25f;
  SkBlendMode blend_mode = SkBlendMode::kMultiply;
  int sorting_context_id = 65536;

  auto state = std::make_unique<viz::SharedQuadState>();
  state->SetAll(quad_transform, layer_rect, visible_layer_rect, clip_rect,
                is_clipped, are_contents_opaque, opacity, blend_mode,
                sorting_context_id);

  auto copy = std::make_unique<viz::SharedQuadState>(*state);
  EXPECT_EQ(quad_transform, copy->quad_to_target_transform);
  EXPECT_EQ(visible_layer_rect, copy->visible_quad_layer_rect);
  EXPECT_EQ(opacity, copy->opacity);
  EXPECT_EQ(clip_rect, copy->clip_rect);
  EXPECT_EQ(is_clipped, copy->is_clipped);
  EXPECT_EQ(are_contents_opaque, copy->are_contents_opaque);
  EXPECT_EQ(blend_mode, copy->blend_mode);
}

viz::SharedQuadState* CreateSharedQuadState(RenderPass* render_pass) {
  gfx::Transform quad_transform = gfx::Transform(1.0, 0.0, 0.5, 1.0, 0.5, 0.0);
  gfx::Rect layer_rect(26, 28);
  gfx::Rect visible_layer_rect(10, 12, 14, 16);
  gfx::Rect clip_rect(19, 21, 23, 25);
  bool is_clipped = false;
  bool are_contents_opaque = true;
  float opacity = 1.f;
  int sorting_context_id = 65536;
  SkBlendMode blend_mode = SkBlendMode::kSrcOver;

  viz::SharedQuadState* state = render_pass->CreateAndAppendSharedQuadState();
  state->SetAll(quad_transform, layer_rect, visible_layer_rect, clip_rect,
                is_clipped, are_contents_opaque, opacity, blend_mode,
                sorting_context_id);
  return state;
}

void CompareSharedQuadState(const viz::SharedQuadState* source_sqs,
                            const viz::SharedQuadState* copy_sqs) {
  EXPECT_EQ(source_sqs->quad_to_target_transform,
            copy_sqs->quad_to_target_transform);
  EXPECT_EQ(source_sqs->quad_layer_rect, copy_sqs->quad_layer_rect);
  EXPECT_EQ(source_sqs->visible_quad_layer_rect,
            copy_sqs->visible_quad_layer_rect);
  EXPECT_EQ(source_sqs->clip_rect, copy_sqs->clip_rect);
  EXPECT_EQ(source_sqs->is_clipped, copy_sqs->is_clipped);
  EXPECT_EQ(source_sqs->opacity, copy_sqs->opacity);
  EXPECT_EQ(source_sqs->blend_mode, copy_sqs->blend_mode);
  EXPECT_EQ(source_sqs->sorting_context_id, copy_sqs->sorting_context_id);
}

void CompareDrawQuad(viz::DrawQuad* quad, viz::DrawQuad* copy) {
  EXPECT_EQ(quad->material, copy->material);
  EXPECT_EQ(quad->rect, copy->rect);
  EXPECT_EQ(quad->visible_rect, copy->visible_rect);
  EXPECT_EQ(quad->needs_blending, copy->needs_blending);
  CompareSharedQuadState(quad->shared_quad_state, copy->shared_quad_state);
}

#define CREATE_SHARED_STATE()                                     \
  std::unique_ptr<RenderPass> render_pass = RenderPass::Create(); \
  viz::SharedQuadState* shared_state(                             \
      CreateSharedQuadState(render_pass.get()));                  \
  viz::SharedQuadState* copy_shared_state =                       \
      render_pass->CreateAndAppendSharedQuadState();              \
  *copy_shared_state = *shared_state;

#define QUAD_DATA                              \
  gfx::Rect quad_rect(30, 40, 50, 60);         \
  gfx::Rect quad_visible_rect(40, 50, 30, 20); \
  bool needs_blending = true;                  \
  ALLOW_UNUSED_LOCAL(needs_blending);

#define SETUP_AND_COPY_QUAD_NEW(Type, quad)                                   \
  viz::DrawQuad* copy_new = render_pass->CopyFromAndAppendDrawQuad(quad_new); \
  CompareDrawQuad(quad_new, copy_new);                                        \
  const Type* copy_quad = Type::MaterialCast(copy_new);                       \
  ALLOW_UNUSED_LOCAL(copy_quad);

#define SETUP_AND_COPY_QUAD_ALL(Type, quad)                                   \
  viz::DrawQuad* copy_all = render_pass->CopyFromAndAppendDrawQuad(quad_all); \
  CompareDrawQuad(quad_all, copy_all);                                        \
  copy_quad = Type::MaterialCast(copy_all);

#define SETUP_AND_COPY_QUAD_NEW_RP(Type, quad, a)                    \
  viz::DrawQuad* copy_new =                                          \
      render_pass->CopyFromAndAppendRenderPassDrawQuad(quad_new, a); \
  CompareDrawQuad(quad_new, copy_new);                               \
  const Type* copy_quad = Type::MaterialCast(copy_new);              \
  ALLOW_UNUSED_LOCAL(copy_quad);

#define SETUP_AND_COPY_QUAD_ALL_RP(Type, quad, a)                    \
  viz::DrawQuad* copy_all =                                          \
      render_pass->CopyFromAndAppendRenderPassDrawQuad(quad_all, a); \
  CompareDrawQuad(quad_all, copy_all);                               \
  copy_quad = Type::MaterialCast(copy_all);

#define CREATE_QUAD_ALL(Type, ...)                                         \
  Type* quad_all = render_pass->CreateAndAppendDrawQuad<Type>();           \
  {                                                                        \
    QUAD_DATA quad_all->SetAll(shared_state, quad_rect, quad_visible_rect, \
                               needs_blending, __VA_ARGS__);               \
  }                                                                        \
  SETUP_AND_COPY_QUAD_ALL(Type, quad_all);

#define CREATE_QUAD_NEW(Type, ...)                                      \
  Type* quad_new = render_pass->CreateAndAppendDrawQuad<Type>();        \
  { QUAD_DATA quad_new->SetNew(shared_state, quad_rect, __VA_ARGS__); } \
  SETUP_AND_COPY_QUAD_NEW(Type, quad_new);

#define CREATE_QUAD_ALL_RP(Type, a, b, c, d, e, f, g, copy_a)              \
  Type* quad_all = render_pass->CreateAndAppendDrawQuad<Type>();           \
  {                                                                        \
    QUAD_DATA quad_all->SetAll(shared_state, quad_rect, quad_visible_rect, \
                               needs_blending, a, b, c, d, e, f, g);       \
  }                                                                        \
  SETUP_AND_COPY_QUAD_ALL_RP(Type, quad_all, copy_a);

#define CREATE_QUAD_NEW_RP(Type, a, b, c, d, e, f, g, h, copy_a)             \
  Type* quad_new = render_pass->CreateAndAppendDrawQuad<Type>();             \
  {                                                                          \
    QUAD_DATA quad_new->SetNew(shared_state, quad_rect, a, b, c, d, e, f, g, \
                               h);                                           \
  }                                                                          \
  SETUP_AND_COPY_QUAD_NEW_RP(Type, quad_new, copy_a);

TEST(DrawQuadTest, CopyDebugBorderDrawQuad) {
  gfx::Rect visible_rect(40, 50, 30, 20);
  SkColor color = 0xfabb0011;
  int width = 99;
  CREATE_SHARED_STATE();

  CREATE_QUAD_NEW(DebugBorderDrawQuad, visible_rect, color, width);
  EXPECT_EQ(viz::DrawQuad::DEBUG_BORDER, copy_quad->material);
  EXPECT_EQ(visible_rect, copy_quad->visible_rect);
  EXPECT_EQ(color, copy_quad->color);
  EXPECT_EQ(width, copy_quad->width);

  CREATE_QUAD_ALL(DebugBorderDrawQuad, color, width);
  EXPECT_EQ(viz::DrawQuad::DEBUG_BORDER, copy_quad->material);
  EXPECT_EQ(color, copy_quad->color);
  EXPECT_EQ(width, copy_quad->width);
}

TEST(DrawQuadTest, CopyRenderPassDrawQuad) {
  gfx::Rect visible_rect(40, 50, 30, 20);
  RenderPassId render_pass_id = 61;
  viz::ResourceId mask_resource_id = 78;
  gfx::RectF mask_uv_rect(0, 0, 33.f, 19.f);
  gfx::Size mask_texture_size(128, 134);
  gfx::Vector2dF filters_scale;
  gfx::PointF filters_origin;
  gfx::RectF tex_coord_rect(1, 1, 255, 254);

  RenderPassId copied_render_pass_id = 235;
  CREATE_SHARED_STATE();

  CREATE_QUAD_NEW_RP(RenderPassDrawQuad, visible_rect, render_pass_id,
                     mask_resource_id, mask_uv_rect, mask_texture_size,
                     filters_scale, filters_origin, tex_coord_rect,
                     copied_render_pass_id);
  EXPECT_EQ(viz::DrawQuad::RENDER_PASS, copy_quad->material);
  EXPECT_EQ(visible_rect, copy_quad->visible_rect);
  EXPECT_EQ(copied_render_pass_id, copy_quad->render_pass_id);
  EXPECT_EQ(mask_resource_id, copy_quad->mask_resource_id());
  EXPECT_EQ(mask_uv_rect.ToString(), copy_quad->mask_uv_rect.ToString());
  EXPECT_EQ(mask_texture_size.ToString(),
            copy_quad->mask_texture_size.ToString());
  EXPECT_EQ(filters_scale, copy_quad->filters_scale);
  EXPECT_EQ(filters_origin, copy_quad->filters_origin);
  EXPECT_EQ(tex_coord_rect.ToString(), copy_quad->tex_coord_rect.ToString());

  CREATE_QUAD_ALL_RP(RenderPassDrawQuad, render_pass_id, mask_resource_id,
                     mask_uv_rect, mask_texture_size, filters_scale,
                     filters_origin, tex_coord_rect, copied_render_pass_id);
  EXPECT_EQ(viz::DrawQuad::RENDER_PASS, copy_quad->material);
  EXPECT_EQ(copied_render_pass_id, copy_quad->render_pass_id);
  EXPECT_EQ(mask_resource_id, copy_quad->mask_resource_id());
  EXPECT_EQ(mask_uv_rect.ToString(), copy_quad->mask_uv_rect.ToString());
  EXPECT_EQ(mask_texture_size.ToString(),
            copy_quad->mask_texture_size.ToString());
  EXPECT_EQ(filters_scale, copy_quad->filters_scale);
  EXPECT_EQ(filters_origin, copy_quad->filters_origin);
  EXPECT_EQ(tex_coord_rect.ToString(), copy_quad->tex_coord_rect.ToString());
}

TEST(DrawQuadTest, CopySolidColorDrawQuad) {
  gfx::Rect visible_rect(40, 50, 30, 20);
  SkColor color = 0x49494949;
  bool force_anti_aliasing_off = false;
  CREATE_SHARED_STATE();

  CREATE_QUAD_NEW(SolidColorDrawQuad, visible_rect, color,
                  force_anti_aliasing_off);
  EXPECT_EQ(viz::DrawQuad::SOLID_COLOR, copy_quad->material);
  EXPECT_EQ(visible_rect, copy_quad->visible_rect);
  EXPECT_EQ(color, copy_quad->color);
  EXPECT_EQ(force_anti_aliasing_off, copy_quad->force_anti_aliasing_off);

  CREATE_QUAD_ALL(SolidColorDrawQuad, color, force_anti_aliasing_off);
  EXPECT_EQ(viz::DrawQuad::SOLID_COLOR, copy_quad->material);
  EXPECT_EQ(color, copy_quad->color);
  EXPECT_EQ(force_anti_aliasing_off, copy_quad->force_anti_aliasing_off);
}

TEST(DrawQuadTest, CopyStreamVideoDrawQuad) {
  gfx::Rect visible_rect(40, 50, 30, 20);
  bool needs_blending = true;
  viz::ResourceId resource_id = 64;
  gfx::Size resource_size_in_pixels = gfx::Size(40, 41);
  gfx::Transform matrix = gfx::Transform(0.5, 0.25, 1, 0.75, 0, 1);
  CREATE_SHARED_STATE();

  CREATE_QUAD_NEW(StreamVideoDrawQuad, visible_rect, needs_blending,
                  resource_id, resource_size_in_pixels, matrix);
  EXPECT_EQ(viz::DrawQuad::STREAM_VIDEO_CONTENT, copy_quad->material);
  EXPECT_EQ(visible_rect, copy_quad->visible_rect);
  EXPECT_EQ(needs_blending, copy_quad->needs_blending);
  EXPECT_EQ(resource_id, copy_quad->resource_id());
  EXPECT_EQ(resource_size_in_pixels, copy_quad->resource_size_in_pixels());
  EXPECT_EQ(matrix, copy_quad->matrix);

  CREATE_QUAD_ALL(StreamVideoDrawQuad, resource_id, resource_size_in_pixels,
                  matrix);
  EXPECT_EQ(viz::DrawQuad::STREAM_VIDEO_CONTENT, copy_quad->material);
  EXPECT_EQ(resource_id, copy_quad->resource_id());
  EXPECT_EQ(resource_size_in_pixels, copy_quad->resource_size_in_pixels());
  EXPECT_EQ(matrix, copy_quad->matrix);
}

TEST(DrawQuadTest, CopySurfaceDrawQuad) {
  gfx::Rect visible_rect(40, 50, 30, 20);
  viz::SurfaceId surface_id(
      kArbitraryFrameSinkId,
      viz::LocalSurfaceId(1234, base::UnguessableToken::Create()));
  CREATE_SHARED_STATE();

  CREATE_QUAD_NEW(SurfaceDrawQuad, visible_rect, surface_id,
                  SurfaceDrawQuadType::PRIMARY, nullptr);
  EXPECT_EQ(viz::DrawQuad::SURFACE_CONTENT, copy_quad->material);
  EXPECT_EQ(visible_rect, copy_quad->visible_rect);
  EXPECT_EQ(surface_id, copy_quad->surface_id);

  CREATE_QUAD_ALL(SurfaceDrawQuad, surface_id, SurfaceDrawQuadType::PRIMARY,
                  nullptr);
  EXPECT_EQ(viz::DrawQuad::SURFACE_CONTENT, copy_quad->material);
  EXPECT_EQ(surface_id, copy_quad->surface_id);
}


TEST(DrawQuadTest, CopyTextureDrawQuad) {
  gfx::Rect visible_rect(40, 50, 30, 20);
  bool needs_blending = true;
  unsigned resource_id = 82;
  gfx::Size resource_size_in_pixels = gfx::Size(40, 41);
  bool premultiplied_alpha = true;
  gfx::PointF uv_top_left(0.5f, 224.f);
  gfx::PointF uv_bottom_right(51.5f, 260.f);
  const float vertex_opacity[] = { 1.0f, 1.0f, 1.0f, 1.0f };
  bool y_flipped = true;
  bool nearest_neighbor = true;
  bool secure_output_only = true;
  CREATE_SHARED_STATE();

  CREATE_QUAD_NEW(TextureDrawQuad, visible_rect, needs_blending, resource_id,
                  premultiplied_alpha, uv_top_left, uv_bottom_right,
                  SK_ColorTRANSPARENT, vertex_opacity, y_flipped,
                  nearest_neighbor, secure_output_only);
  EXPECT_EQ(viz::DrawQuad::TEXTURE_CONTENT, copy_quad->material);
  EXPECT_EQ(visible_rect, copy_quad->visible_rect);
  EXPECT_EQ(needs_blending, copy_quad->needs_blending);
  EXPECT_EQ(resource_id, copy_quad->resource_id());
  EXPECT_EQ(premultiplied_alpha, copy_quad->premultiplied_alpha);
  EXPECT_EQ(uv_top_left, copy_quad->uv_top_left);
  EXPECT_EQ(uv_bottom_right, copy_quad->uv_bottom_right);
  EXPECT_FLOAT_ARRAY_EQ(vertex_opacity, copy_quad->vertex_opacity, 4);
  EXPECT_EQ(y_flipped, copy_quad->y_flipped);
  EXPECT_EQ(nearest_neighbor, copy_quad->nearest_neighbor);
  EXPECT_EQ(secure_output_only, copy_quad->secure_output_only);

  CREATE_QUAD_ALL(TextureDrawQuad, resource_id, resource_size_in_pixels,
                  premultiplied_alpha, uv_top_left, uv_bottom_right,
                  SK_ColorTRANSPARENT, vertex_opacity, y_flipped,
                  nearest_neighbor, secure_output_only);
  EXPECT_EQ(viz::DrawQuad::TEXTURE_CONTENT, copy_quad->material);
  EXPECT_EQ(resource_id, copy_quad->resource_id());
  EXPECT_EQ(resource_size_in_pixels, copy_quad->resource_size_in_pixels());
  EXPECT_EQ(premultiplied_alpha, copy_quad->premultiplied_alpha);
  EXPECT_EQ(uv_top_left, copy_quad->uv_top_left);
  EXPECT_EQ(uv_bottom_right, copy_quad->uv_bottom_right);
  EXPECT_FLOAT_ARRAY_EQ(vertex_opacity, copy_quad->vertex_opacity, 4);
  EXPECT_EQ(y_flipped, copy_quad->y_flipped);
  EXPECT_EQ(nearest_neighbor, copy_quad->nearest_neighbor);
  EXPECT_EQ(secure_output_only, copy_quad->secure_output_only);
}

TEST(DrawQuadTest, CopyTileDrawQuad) {
  gfx::Rect visible_rect(40, 50, 30, 20);
  bool needs_blending = true;
  unsigned resource_id = 104;
  gfx::RectF tex_coord_rect(31.f, 12.f, 54.f, 20.f);
  gfx::Size texture_size(85, 32);
  bool swizzle_contents = true;
  bool nearest_neighbor = true;
  CREATE_SHARED_STATE();

  CREATE_QUAD_NEW(TileDrawQuad, visible_rect, needs_blending, resource_id,
                  tex_coord_rect, texture_size, swizzle_contents,
                  nearest_neighbor);
  EXPECT_EQ(viz::DrawQuad::TILED_CONTENT, copy_quad->material);
  EXPECT_EQ(visible_rect, copy_quad->visible_rect);
  EXPECT_EQ(needs_blending, copy_quad->needs_blending);
  EXPECT_EQ(resource_id, copy_quad->resource_id());
  EXPECT_EQ(tex_coord_rect, copy_quad->tex_coord_rect);
  EXPECT_EQ(texture_size, copy_quad->texture_size);
  EXPECT_EQ(swizzle_contents, copy_quad->swizzle_contents);
  EXPECT_EQ(nearest_neighbor, copy_quad->nearest_neighbor);

  CREATE_QUAD_ALL(TileDrawQuad, resource_id, tex_coord_rect, texture_size,
                  swizzle_contents, nearest_neighbor);
  EXPECT_EQ(viz::DrawQuad::TILED_CONTENT, copy_quad->material);
  EXPECT_EQ(resource_id, copy_quad->resource_id());
  EXPECT_EQ(tex_coord_rect, copy_quad->tex_coord_rect);
  EXPECT_EQ(texture_size, copy_quad->texture_size);
  EXPECT_EQ(swizzle_contents, copy_quad->swizzle_contents);
  EXPECT_EQ(nearest_neighbor, copy_quad->nearest_neighbor);
}

TEST(DrawQuadTest, CopyYUVVideoDrawQuad) {
  gfx::Rect visible_rect(40, 50, 30, 20);
  bool needs_blending = true;
  gfx::RectF ya_tex_coord_rect(40, 50, 30, 20);
  gfx::RectF uv_tex_coord_rect(20, 25, 15, 10);
  gfx::Size ya_tex_size(32, 68);
  gfx::Size uv_tex_size(41, 51);
  viz::ResourceId y_plane_resource_id = 45;
  viz::ResourceId u_plane_resource_id = 532;
  viz::ResourceId v_plane_resource_id = 4;
  viz::ResourceId a_plane_resource_id = 63;
  float resource_offset = 0.5f;
  float resource_multiplier = 2.001f;
  uint32_t bits_per_channel = 5;
  bool require_overlay = true;
  YUVVideoDrawQuad::ColorSpace color_space = YUVVideoDrawQuad::JPEG;
  gfx::ColorSpace video_color_space = gfx::ColorSpace::CreateJpeg();
  CREATE_SHARED_STATE();

  CREATE_QUAD_NEW(YUVVideoDrawQuad, visible_rect, needs_blending,
                  ya_tex_coord_rect, uv_tex_coord_rect, ya_tex_size,
                  uv_tex_size, y_plane_resource_id, u_plane_resource_id,
                  v_plane_resource_id, a_plane_resource_id, color_space,
                  video_color_space, resource_offset, resource_multiplier,
                  bits_per_channel);
  EXPECT_EQ(viz::DrawQuad::YUV_VIDEO_CONTENT, copy_quad->material);
  EXPECT_EQ(visible_rect, copy_quad->visible_rect);
  EXPECT_EQ(needs_blending, copy_quad->needs_blending);
  EXPECT_EQ(ya_tex_coord_rect, copy_quad->ya_tex_coord_rect);
  EXPECT_EQ(uv_tex_coord_rect, copy_quad->uv_tex_coord_rect);
  EXPECT_EQ(ya_tex_size, copy_quad->ya_tex_size);
  EXPECT_EQ(uv_tex_size, copy_quad->uv_tex_size);
  EXPECT_EQ(y_plane_resource_id, copy_quad->y_plane_resource_id());
  EXPECT_EQ(u_plane_resource_id, copy_quad->u_plane_resource_id());
  EXPECT_EQ(v_plane_resource_id, copy_quad->v_plane_resource_id());
  EXPECT_EQ(a_plane_resource_id, copy_quad->a_plane_resource_id());
  EXPECT_EQ(color_space, copy_quad->color_space);
  EXPECT_EQ(resource_offset, copy_quad->resource_offset);
  EXPECT_EQ(resource_multiplier, copy_quad->resource_multiplier);
  EXPECT_EQ(bits_per_channel, copy_quad->bits_per_channel);
  EXPECT_FALSE(copy_quad->require_overlay);

  CREATE_QUAD_ALL(YUVVideoDrawQuad, ya_tex_coord_rect, uv_tex_coord_rect,
                  ya_tex_size, uv_tex_size, y_plane_resource_id,
                  u_plane_resource_id, v_plane_resource_id, a_plane_resource_id,
                  color_space, video_color_space, resource_offset,
                  resource_multiplier, bits_per_channel, require_overlay);
  EXPECT_EQ(viz::DrawQuad::YUV_VIDEO_CONTENT, copy_quad->material);
  EXPECT_EQ(ya_tex_coord_rect, copy_quad->ya_tex_coord_rect);
  EXPECT_EQ(uv_tex_coord_rect, copy_quad->uv_tex_coord_rect);
  EXPECT_EQ(ya_tex_size, copy_quad->ya_tex_size);
  EXPECT_EQ(uv_tex_size, copy_quad->uv_tex_size);
  EXPECT_EQ(y_plane_resource_id, copy_quad->y_plane_resource_id());
  EXPECT_EQ(u_plane_resource_id, copy_quad->u_plane_resource_id());
  EXPECT_EQ(v_plane_resource_id, copy_quad->v_plane_resource_id());
  EXPECT_EQ(a_plane_resource_id, copy_quad->a_plane_resource_id());
  EXPECT_EQ(color_space, copy_quad->color_space);
  EXPECT_EQ(resource_offset, copy_quad->resource_offset);
  EXPECT_EQ(resource_multiplier, copy_quad->resource_multiplier);
  EXPECT_EQ(bits_per_channel, copy_quad->bits_per_channel);
  EXPECT_EQ(require_overlay, copy_quad->require_overlay);
}

TEST(DrawQuadTest, CopyPictureDrawQuad) {
  gfx::Rect visible_rect(40, 50, 30, 20);
  bool needs_blending = true;
  gfx::RectF tex_coord_rect(31.f, 12.f, 54.f, 20.f);
  gfx::Size texture_size(85, 32);
  bool nearest_neighbor = true;
  viz::ResourceFormat texture_format = viz::RGBA_8888;
  gfx::Rect content_rect(30, 40, 20, 30);
  float contents_scale = 3.141592f;
  scoped_refptr<DisplayItemList> display_item_list =
      FakeRasterSource::CreateEmpty(gfx::Size(100, 100))->GetDisplayItemList();
  CREATE_SHARED_STATE();

  CREATE_QUAD_NEW(PictureDrawQuad, visible_rect, needs_blending, tex_coord_rect,
                  texture_size, nearest_neighbor, texture_format, content_rect,
                  contents_scale, display_item_list);
  EXPECT_EQ(viz::DrawQuad::PICTURE_CONTENT, copy_quad->material);
  EXPECT_EQ(visible_rect, copy_quad->visible_rect);
  EXPECT_EQ(needs_blending, copy_quad->needs_blending);
  EXPECT_EQ(tex_coord_rect, copy_quad->tex_coord_rect);
  EXPECT_EQ(texture_size, copy_quad->texture_size);
  EXPECT_EQ(nearest_neighbor, copy_quad->nearest_neighbor);
  EXPECT_EQ(texture_format, copy_quad->texture_format);
  EXPECT_EQ(content_rect, copy_quad->content_rect);
  EXPECT_EQ(contents_scale, copy_quad->contents_scale);
  EXPECT_EQ(display_item_list, copy_quad->display_item_list);

  CREATE_QUAD_ALL(PictureDrawQuad, tex_coord_rect, texture_size,
                  nearest_neighbor, texture_format, content_rect,
                  contents_scale, display_item_list);
  EXPECT_EQ(viz::DrawQuad::PICTURE_CONTENT, copy_quad->material);
  EXPECT_EQ(tex_coord_rect, copy_quad->tex_coord_rect);
  EXPECT_EQ(texture_size, copy_quad->texture_size);
  EXPECT_EQ(nearest_neighbor, copy_quad->nearest_neighbor);
  EXPECT_EQ(texture_format, copy_quad->texture_format);
  EXPECT_EQ(content_rect, copy_quad->content_rect);
  EXPECT_EQ(contents_scale, copy_quad->contents_scale);
  EXPECT_EQ(display_item_list, copy_quad->display_item_list);
}

class DrawQuadIteratorTest : public testing::Test {
 protected:
  int IterateAndCount(viz::DrawQuad* quad) {
    num_resources_ = 0;
    for (viz::ResourceId& resource_id : quad->resources) {
      ++num_resources_;
      ++resource_id;
    }
    return num_resources_;
  }

 private:
  int num_resources_;
};

TEST_F(DrawQuadIteratorTest, DebugBorderDrawQuad) {
  gfx::Rect visible_rect(40, 50, 30, 20);
  SkColor color = 0xfabb0011;
  int width = 99;

  CREATE_SHARED_STATE();
  CREATE_QUAD_NEW(DebugBorderDrawQuad, visible_rect, color, width);
  EXPECT_EQ(0, IterateAndCount(quad_new));
}

TEST_F(DrawQuadIteratorTest, RenderPassDrawQuad) {
  gfx::Rect visible_rect(40, 50, 30, 20);
  int render_pass_id = 61;
  viz::ResourceId mask_resource_id = 78;
  gfx::RectF mask_uv_rect(0.f, 0.f, 33.f, 19.f);
  gfx::Size mask_texture_size(128, 134);
  gfx::Vector2dF filters_scale(2.f, 3.f);
  gfx::PointF filters_origin(0.f, 0.f);
  gfx::RectF tex_coord_rect(1.f, 1.f, 33.f, 19.f);

  int copied_render_pass_id = 235;

  CREATE_SHARED_STATE();
  CREATE_QUAD_NEW_RP(RenderPassDrawQuad, visible_rect, render_pass_id,
                     mask_resource_id, mask_uv_rect, mask_texture_size,
                     filters_scale, filters_origin, tex_coord_rect,
                     copied_render_pass_id);
  EXPECT_EQ(mask_resource_id, quad_new->mask_resource_id());
  EXPECT_EQ(1, IterateAndCount(quad_new));
  EXPECT_EQ(mask_resource_id + 1, quad_new->mask_resource_id());

  viz::ResourceId new_mask_resource_id = 0;
  gfx::Rect quad_rect(30, 40, 50, 60);
  quad_new->SetNew(shared_state, quad_rect, visible_rect, render_pass_id,
                   new_mask_resource_id, mask_uv_rect, mask_texture_size,
                   filters_scale, filters_origin, tex_coord_rect);
  EXPECT_EQ(0, IterateAndCount(quad_new));
  EXPECT_EQ(0u, quad_new->mask_resource_id());
}

TEST_F(DrawQuadIteratorTest, SolidColorDrawQuad) {
  gfx::Rect visible_rect(40, 50, 30, 20);
  SkColor color = 0x49494949;
  bool force_anti_aliasing_off = false;

  CREATE_SHARED_STATE();
  CREATE_QUAD_NEW(SolidColorDrawQuad, visible_rect, color,
                  force_anti_aliasing_off);
  EXPECT_EQ(0, IterateAndCount(quad_new));
}

TEST_F(DrawQuadIteratorTest, StreamVideoDrawQuad) {
  gfx::Rect visible_rect(40, 50, 30, 20);
  viz::ResourceId resource_id = 64;
  gfx::Size resource_size_in_pixels = gfx::Size(40, 41);
  gfx::Transform matrix = gfx::Transform(0.5, 0.25, 1, 0.75, 0, 1);

  CREATE_SHARED_STATE();
  CREATE_QUAD_NEW(StreamVideoDrawQuad, visible_rect, needs_blending,
                  resource_id, resource_size_in_pixels, matrix);
  EXPECT_EQ(resource_id, quad_new->resource_id());
  EXPECT_EQ(resource_size_in_pixels, quad_new->resource_size_in_pixels());
  EXPECT_EQ(1, IterateAndCount(quad_new));
  EXPECT_EQ(resource_id + 1, quad_new->resource_id());
}

TEST_F(DrawQuadIteratorTest, SurfaceDrawQuad) {
  gfx::Rect visible_rect(40, 50, 30, 20);
  viz::SurfaceId surface_id(
      kArbitraryFrameSinkId,
      viz::LocalSurfaceId(4321, base::UnguessableToken::Create()));

  CREATE_SHARED_STATE();
  CREATE_QUAD_NEW(SurfaceDrawQuad, visible_rect, surface_id,
                  SurfaceDrawQuadType::PRIMARY, nullptr);
  EXPECT_EQ(0, IterateAndCount(quad_new));
}

TEST_F(DrawQuadIteratorTest, TextureDrawQuad) {
  gfx::Rect visible_rect(40, 50, 30, 20);
  unsigned resource_id = 82;
  bool premultiplied_alpha = true;
  gfx::PointF uv_top_left(0.5f, 224.f);
  gfx::PointF uv_bottom_right(51.5f, 260.f);
  const float vertex_opacity[] = { 1.0f, 1.0f, 1.0f, 1.0f };
  bool y_flipped = true;
  bool nearest_neighbor = true;
  bool secure_output_only = true;

  CREATE_SHARED_STATE();
  CREATE_QUAD_NEW(TextureDrawQuad, visible_rect, needs_blending, resource_id,
                  premultiplied_alpha, uv_top_left, uv_bottom_right,
                  SK_ColorTRANSPARENT, vertex_opacity, y_flipped,
                  nearest_neighbor, secure_output_only);
  EXPECT_EQ(resource_id, quad_new->resource_id());
  EXPECT_EQ(1, IterateAndCount(quad_new));
  EXPECT_EQ(resource_id + 1, quad_new->resource_id());
}

TEST_F(DrawQuadIteratorTest, TileDrawQuad) {
  gfx::Rect visible_rect(40, 50, 30, 20);
  unsigned resource_id = 104;
  gfx::RectF tex_coord_rect(31.f, 12.f, 54.f, 20.f);
  gfx::Size texture_size(85, 32);
  bool swizzle_contents = true;
  bool nearest_neighbor = true;

  CREATE_SHARED_STATE();
  CREATE_QUAD_NEW(TileDrawQuad, visible_rect, needs_blending, resource_id,
                  tex_coord_rect, texture_size, swizzle_contents,
                  nearest_neighbor);
  EXPECT_EQ(resource_id, quad_new->resource_id());
  EXPECT_EQ(1, IterateAndCount(quad_new));
  EXPECT_EQ(resource_id + 1, quad_new->resource_id());
}

TEST_F(DrawQuadIteratorTest, YUVVideoDrawQuad) {
  gfx::Rect visible_rect(40, 50, 30, 20);
  gfx::RectF ya_tex_coord_rect(0.0f, 0.0f, 0.75f, 0.5f);
  gfx::RectF uv_tex_coord_rect(0.0f, 0.0f, 0.375f, 0.25f);
  gfx::Size ya_tex_size(32, 68);
  gfx::Size uv_tex_size(41, 51);
  viz::ResourceId y_plane_resource_id = 45;
  viz::ResourceId u_plane_resource_id = 532;
  viz::ResourceId v_plane_resource_id = 4;
  viz::ResourceId a_plane_resource_id = 63;
  YUVVideoDrawQuad::ColorSpace color_space = YUVVideoDrawQuad::JPEG;
  gfx::ColorSpace video_color_space = gfx::ColorSpace::CreateJpeg();

  CREATE_SHARED_STATE();
  CREATE_QUAD_NEW(YUVVideoDrawQuad, visible_rect, needs_blending,
                  ya_tex_coord_rect, uv_tex_coord_rect, ya_tex_size,
                  uv_tex_size, y_plane_resource_id, u_plane_resource_id,
                  v_plane_resource_id, a_plane_resource_id, color_space,
                  video_color_space, 0.0, 1.0, 5);
  EXPECT_EQ(viz::DrawQuad::YUV_VIDEO_CONTENT, copy_quad->material);
  EXPECT_EQ(y_plane_resource_id, quad_new->y_plane_resource_id());
  EXPECT_EQ(u_plane_resource_id, quad_new->u_plane_resource_id());
  EXPECT_EQ(v_plane_resource_id, quad_new->v_plane_resource_id());
  EXPECT_EQ(a_plane_resource_id, quad_new->a_plane_resource_id());
  EXPECT_EQ(color_space, quad_new->color_space);
  EXPECT_EQ(4, IterateAndCount(quad_new));
  EXPECT_EQ(y_plane_resource_id + 1, quad_new->y_plane_resource_id());
  EXPECT_EQ(u_plane_resource_id + 1, quad_new->u_plane_resource_id());
  EXPECT_EQ(v_plane_resource_id + 1, quad_new->v_plane_resource_id());
  EXPECT_EQ(a_plane_resource_id + 1, quad_new->a_plane_resource_id());
}

TEST(DrawQuadTest, LargestQuadType) {
  size_t largest = 0;

  for (int i = 0; i <= viz::DrawQuad::MATERIAL_LAST; ++i) {
    switch (static_cast<viz::DrawQuad::Material>(i)) {
      case viz::DrawQuad::DEBUG_BORDER:
        largest = std::max(largest, sizeof(DebugBorderDrawQuad));
        break;
      case viz::DrawQuad::PICTURE_CONTENT:
        largest = std::max(largest, sizeof(PictureDrawQuad));
        break;
      case viz::DrawQuad::TEXTURE_CONTENT:
        largest = std::max(largest, sizeof(TextureDrawQuad));
        break;
      case viz::DrawQuad::RENDER_PASS:
        largest = std::max(largest, sizeof(RenderPassDrawQuad));
        break;
      case viz::DrawQuad::SOLID_COLOR:
        largest = std::max(largest, sizeof(SolidColorDrawQuad));
        break;
      case viz::DrawQuad::SURFACE_CONTENT:
        largest = std::max(largest, sizeof(SurfaceDrawQuad));
        break;
      case viz::DrawQuad::TILED_CONTENT:
        largest = std::max(largest, sizeof(TileDrawQuad));
        break;
      case viz::DrawQuad::STREAM_VIDEO_CONTENT:
        largest = std::max(largest, sizeof(StreamVideoDrawQuad));
        break;
      case viz::DrawQuad::YUV_VIDEO_CONTENT:
        largest = std::max(largest, sizeof(YUVVideoDrawQuad));
        break;
      case viz::DrawQuad::INVALID:
        break;
    }
  }
  EXPECT_EQ(LargestDrawQuadSize(), largest);

  if (!HasFailure())
    return;

  // On failure, output the size of all quads for debugging.
  LOG(ERROR) << "largest " << largest;
  LOG(ERROR) << "kLargestDrawQuad " << LargestDrawQuadSize();
  for (int i = 0; i <= viz::DrawQuad::MATERIAL_LAST; ++i) {
    switch (static_cast<viz::DrawQuad::Material>(i)) {
      case viz::DrawQuad::DEBUG_BORDER:
        LOG(ERROR) << "DebugBorderDrawQuad " << sizeof(DebugBorderDrawQuad);
        break;
      case viz::DrawQuad::PICTURE_CONTENT:
        LOG(ERROR) << "PictureDrawQuad " << sizeof(PictureDrawQuad);
        break;
      case viz::DrawQuad::TEXTURE_CONTENT:
        LOG(ERROR) << "TextureDrawQuad " << sizeof(TextureDrawQuad);
        break;
      case viz::DrawQuad::RENDER_PASS:
        LOG(ERROR) << "RenderPassDrawQuad " << sizeof(RenderPassDrawQuad);
        break;
      case viz::DrawQuad::SOLID_COLOR:
        LOG(ERROR) << "SolidColorDrawQuad " << sizeof(SolidColorDrawQuad);
        break;
      case viz::DrawQuad::SURFACE_CONTENT:
        LOG(ERROR) << "SurfaceDrawQuad " << sizeof(SurfaceDrawQuad);
        break;
      case viz::DrawQuad::TILED_CONTENT:
        LOG(ERROR) << "TileDrawQuad " << sizeof(TileDrawQuad);
        break;
      case viz::DrawQuad::STREAM_VIDEO_CONTENT:
        LOG(ERROR) << "StreamVideoDrawQuad " << sizeof(StreamVideoDrawQuad);
        break;
      case viz::DrawQuad::YUV_VIDEO_CONTENT:
        LOG(ERROR) << "YUVVideoDrawQuad " << sizeof(YUVVideoDrawQuad);
        break;
      case viz::DrawQuad::INVALID:
        break;
    }
  }
}

}  // namespace
}  // namespace cc
