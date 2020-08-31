// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/quads/draw_quad.h"

#include <stddef.h>

#include <algorithm>
#include <memory>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/unguessable_token.h"
#include "cc/base/math_util.h"
#include "cc/paint/filter_operations.h"
#include "cc/test/fake_raster_source.h"
#include "cc/test/geometry_test_utils.h"
#include "components/viz/common/quads/debug_border_draw_quad.h"
#include "components/viz/common/quads/largest_draw_quad.h"
#include "components/viz/common/quads/picture_draw_quad.h"
#include "components/viz/common/quads/render_pass.h"
#include "components/viz/common/quads/render_pass_draw_quad.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/common/quads/stream_video_draw_quad.h"
#include "components/viz/common/quads/surface_draw_quad.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "components/viz/common/quads/tile_draw_quad.h"
#include "components/viz/common/quads/video_hole_draw_quad.h"
#include "components/viz/common/quads/yuv_video_draw_quad.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/effects/SkBlurImageFilter.h"
#include "ui/gfx/transform.h"

namespace viz {
namespace {

static constexpr FrameSinkId kArbitraryFrameSinkId(1, 1);

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

  auto state = std::make_unique<SharedQuadState>();
  state->SetAll(quad_transform, layer_rect, visible_layer_rect, gfx::RRectF(),
                clip_rect, is_clipped, are_contents_opaque, opacity, blend_mode,
                sorting_context_id);

  auto copy = std::make_unique<SharedQuadState>(*state);
  EXPECT_EQ(quad_transform, copy->quad_to_target_transform);
  EXPECT_EQ(visible_layer_rect, copy->visible_quad_layer_rect);
  EXPECT_EQ(opacity, copy->opacity);
  EXPECT_EQ(clip_rect, copy->clip_rect);
  EXPECT_EQ(is_clipped, copy->is_clipped);
  EXPECT_EQ(are_contents_opaque, copy->are_contents_opaque);
  EXPECT_EQ(blend_mode, copy->blend_mode);
}

SharedQuadState* CreateSharedQuadState(RenderPass* render_pass) {
  gfx::Transform quad_transform = gfx::Transform(1.0, 0.0, 0.5, 1.0, 0.5, 0.0);
  gfx::Rect layer_rect(26, 28);
  gfx::Rect visible_layer_rect(10, 12, 14, 16);
  gfx::Rect clip_rect(19, 21, 23, 25);
  bool is_clipped = false;
  bool are_contents_opaque = true;
  float opacity = 1.f;
  int sorting_context_id = 65536;
  SkBlendMode blend_mode = SkBlendMode::kSrcOver;

  SharedQuadState* state = render_pass->CreateAndAppendSharedQuadState();
  state->SetAll(quad_transform, layer_rect, visible_layer_rect, gfx::RRectF(),
                clip_rect, is_clipped, are_contents_opaque, opacity, blend_mode,
                sorting_context_id);
  return state;
}

void CompareSharedQuadState(const SharedQuadState* source_sqs,
                            const SharedQuadState* copy_sqs) {
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

void CompareDrawQuad(DrawQuad* quad, DrawQuad* copy) {
  EXPECT_EQ(quad->material, copy->material);
  EXPECT_EQ(quad->rect, copy->rect);
  EXPECT_EQ(quad->visible_rect, copy->visible_rect);
  EXPECT_EQ(quad->needs_blending, copy->needs_blending);
  CompareSharedQuadState(quad->shared_quad_state, copy->shared_quad_state);
}

#define CREATE_SHARED_STATE()                                              \
  std::unique_ptr<RenderPass> render_pass = RenderPass::Create();          \
  SharedQuadState* shared_state(CreateSharedQuadState(render_pass.get())); \
  SharedQuadState* copy_shared_state =                                     \
      render_pass->CreateAndAppendSharedQuadState();                       \
  *copy_shared_state = *shared_state;

#define QUAD_DATA                              \
  gfx::Rect quad_rect(30, 40, 50, 60);         \
  gfx::Rect quad_visible_rect(40, 50, 30, 20); \
  ALLOW_UNUSED_LOCAL(quad_visible_rect);       \
  bool needs_blending = true;                  \
  ALLOW_UNUSED_LOCAL(needs_blending);

#define SETUP_AND_COPY_QUAD_NEW(Type, quad)                              \
  DrawQuad* copy_new = render_pass->CopyFromAndAppendDrawQuad(quad_new); \
  CompareDrawQuad(quad_new, copy_new);                                   \
  const Type* copy_quad = Type::MaterialCast(copy_new);                  \
  ALLOW_UNUSED_LOCAL(copy_quad);

#define SETUP_AND_COPY_QUAD_ALL(Type, quad)                              \
  DrawQuad* copy_all = render_pass->CopyFromAndAppendDrawQuad(quad_all); \
  CompareDrawQuad(quad_all, copy_all);                                   \
  copy_quad = Type::MaterialCast(copy_all);

#define SETUP_AND_COPY_QUAD_NEW_RP(Type, quad, a)                    \
  DrawQuad* copy_new =                                               \
      render_pass->CopyFromAndAppendRenderPassDrawQuad(quad_new, a); \
  CompareDrawQuad(quad_new, copy_new);                               \
  const Type* copy_quad = Type::MaterialCast(copy_new);              \
  ALLOW_UNUSED_LOCAL(copy_quad);

#define SETUP_AND_COPY_QUAD_ALL_RP(Type, quad, a)                    \
  DrawQuad* copy_all =                                               \
      render_pass->CopyFromAndAppendRenderPassDrawQuad(quad_all, a); \
  CompareDrawQuad(quad_all, copy_all);                               \
  const Type* copy_quad = Type::MaterialCast(copy_all);

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

#define CREATE_QUAD_ALL_RP(Type, a, b, c, d, e, f, g, h, i, j, k, copy_a)     \
  Type* quad_all = render_pass->CreateAndAppendDrawQuad<Type>();              \
  {                                                                           \
    QUAD_DATA quad_all->SetAll(shared_state, quad_rect, a, needs_blending, b, \
                               c, d, e, f, g, h, i, j, k);                    \
  }                                                                           \
  SETUP_AND_COPY_QUAD_ALL_RP(Type, quad_all, copy_a);

#define CREATE_QUAD_NEW_RP(Type, a, b, c, d, e, f, g, h, i, j, copy_a)       \
  Type* quad_new = render_pass->CreateAndAppendDrawQuad<Type>();             \
  {                                                                          \
    QUAD_DATA quad_new->SetNew(shared_state, quad_rect, a, b, c, d, e, f, g, \
                               h, i, j);                                     \
  }                                                                          \
  SETUP_AND_COPY_QUAD_NEW_RP(Type, quad_new, copy_a);

TEST(DrawQuadTest, CopyDebugBorderDrawQuad) {
  gfx::Rect visible_rect(40, 50, 30, 20);
  SkColor color = 0xfabb0011;
  int width = 99;
  CREATE_SHARED_STATE();

  CREATE_QUAD_NEW(DebugBorderDrawQuad, visible_rect, color, width);
  EXPECT_EQ(DrawQuad::Material::kDebugBorder, copy_quad->material);
  EXPECT_EQ(visible_rect, copy_quad->visible_rect);
  EXPECT_EQ(color, copy_quad->color);
  EXPECT_EQ(width, copy_quad->width);

  CREATE_QUAD_ALL(DebugBorderDrawQuad, color, width);
  EXPECT_EQ(DrawQuad::Material::kDebugBorder, copy_quad->material);
  EXPECT_EQ(color, copy_quad->color);
  EXPECT_EQ(width, copy_quad->width);
}

TEST(DrawQuadTest, CopyRenderPassDrawQuad) {
  gfx::Rect visible_rect(40, 50, 30, 20);
  RenderPassId render_pass_id = 61;
  ResourceId mask_resource_id = 78;
  gfx::RectF mask_uv_rect(0, 0, 33.f, 19.f);
  gfx::Size mask_texture_size(128, 134);
  gfx::Vector2dF filters_scale;
  gfx::PointF filters_origin;
  gfx::RectF tex_coord_rect(1, 1, 255, 254);
  bool force_anti_aliasing_off = false;
  float backdrop_filter_quality = 1.0f;
  bool can_use_backdrop_filter_cache = true;

  RenderPassId copied_render_pass_id = 235;
  CREATE_SHARED_STATE();

  CREATE_QUAD_ALL_RP(RenderPassDrawQuad, visible_rect, render_pass_id,
                     mask_resource_id, mask_uv_rect, mask_texture_size,
                     filters_scale, filters_origin, tex_coord_rect,
                     force_anti_aliasing_off, backdrop_filter_quality,
                     can_use_backdrop_filter_cache, copied_render_pass_id);
  EXPECT_EQ(DrawQuad::Material::kRenderPass, copy_quad->material);
  EXPECT_EQ(visible_rect, copy_quad->visible_rect);
  EXPECT_EQ(copied_render_pass_id, copy_quad->render_pass_id);
  EXPECT_EQ(mask_resource_id, copy_quad->mask_resource_id());
  EXPECT_EQ(mask_uv_rect.ToString(), copy_quad->mask_uv_rect.ToString());
  EXPECT_EQ(mask_texture_size.ToString(),
            copy_quad->mask_texture_size.ToString());
  EXPECT_EQ(filters_scale, copy_quad->filters_scale);
  EXPECT_EQ(filters_origin, copy_quad->filters_origin);
  EXPECT_EQ(tex_coord_rect.ToString(), copy_quad->tex_coord_rect.ToString());
  EXPECT_EQ(force_anti_aliasing_off, copy_quad->force_anti_aliasing_off);
  EXPECT_EQ(backdrop_filter_quality, copy_quad->backdrop_filter_quality);
  EXPECT_EQ(can_use_backdrop_filter_cache,
            copy_quad->can_use_backdrop_filter_cache);
}

TEST(DrawQuadTest, CopySolidColorDrawQuad) {
  gfx::Rect visible_rect(40, 50, 30, 20);
  SkColor color = 0x49494949;
  bool force_anti_aliasing_off = false;
  CREATE_SHARED_STATE();

  CREATE_QUAD_NEW(SolidColorDrawQuad, visible_rect, color,
                  force_anti_aliasing_off);
  EXPECT_EQ(DrawQuad::Material::kSolidColor, copy_quad->material);
  EXPECT_EQ(visible_rect, copy_quad->visible_rect);
  EXPECT_EQ(color, copy_quad->color);
  EXPECT_EQ(force_anti_aliasing_off, copy_quad->force_anti_aliasing_off);

  CREATE_QUAD_ALL(SolidColorDrawQuad, color, force_anti_aliasing_off);
  EXPECT_EQ(DrawQuad::Material::kSolidColor, copy_quad->material);
  EXPECT_EQ(color, copy_quad->color);
  EXPECT_EQ(force_anti_aliasing_off, copy_quad->force_anti_aliasing_off);
}

TEST(DrawQuadTest, CopyStreamVideoDrawQuad) {
  gfx::Rect visible_rect(40, 50, 30, 20);
  bool needs_blending = true;
  ResourceId resource_id = 64;
  gfx::Size resource_size_in_pixels = gfx::Size(40, 41);
  gfx::PointF uv_top_left(0.25f, 0.3f);
  gfx::PointF uv_bottom_right(0.75f, 0.7f);
  CREATE_SHARED_STATE();

  CREATE_QUAD_NEW(StreamVideoDrawQuad, visible_rect, needs_blending,
                  resource_id, resource_size_in_pixels, uv_top_left,
                  uv_bottom_right);
  EXPECT_EQ(DrawQuad::Material::kStreamVideoContent, copy_quad->material);
  EXPECT_EQ(visible_rect, copy_quad->visible_rect);
  EXPECT_EQ(needs_blending, copy_quad->needs_blending);
  EXPECT_EQ(resource_id, copy_quad->resource_id());
  EXPECT_EQ(resource_size_in_pixels, copy_quad->resource_size_in_pixels());
  EXPECT_EQ(uv_top_left, copy_quad->uv_top_left);
  EXPECT_EQ(uv_bottom_right, copy_quad->uv_bottom_right);

  CREATE_QUAD_ALL(StreamVideoDrawQuad, resource_id, resource_size_in_pixels,
                  uv_top_left, uv_bottom_right);
  EXPECT_EQ(DrawQuad::Material::kStreamVideoContent, copy_quad->material);
  EXPECT_EQ(resource_id, copy_quad->resource_id());
  EXPECT_EQ(resource_size_in_pixels, copy_quad->resource_size_in_pixels());
  EXPECT_EQ(uv_top_left, copy_quad->uv_top_left);
  EXPECT_EQ(uv_bottom_right, copy_quad->uv_bottom_right);
}

TEST(DrawQuadTest, CopySurfaceDrawQuad) {
  gfx::Rect visible_rect(40, 50, 30, 20);
  SurfaceId primary_surface_id(
      kArbitraryFrameSinkId,
      LocalSurfaceId(1234, base::UnguessableToken::Create()));
  SurfaceId fallback_surface_id(
      kArbitraryFrameSinkId,
      LocalSurfaceId(5678, base::UnguessableToken::Create()));
  CREATE_SHARED_STATE();

  CREATE_QUAD_NEW(SurfaceDrawQuad, visible_rect,
                  SurfaceRange(fallback_surface_id, primary_surface_id),
                  SK_ColorWHITE, /*stretch_content_to_fill_bounds=*/true);
  EXPECT_EQ(DrawQuad::Material::kSurfaceContent, copy_quad->material);
  EXPECT_EQ(visible_rect, copy_quad->visible_rect);
  EXPECT_EQ(primary_surface_id, copy_quad->surface_range.end());
  EXPECT_EQ(fallback_surface_id, *copy_quad->surface_range.start());
  EXPECT_TRUE(copy_quad->stretch_content_to_fill_bounds);

  CREATE_QUAD_ALL(SurfaceDrawQuad,
                  SurfaceRange(fallback_surface_id, primary_surface_id),
                  SK_ColorWHITE, /*stretch_content_to_fill_bounds=*/false,
                  /*is_reflection=*/false, /*allow_merge=*/true);
  EXPECT_EQ(DrawQuad::Material::kSurfaceContent, copy_quad->material);
  EXPECT_EQ(primary_surface_id, copy_quad->surface_range.end());
  EXPECT_EQ(fallback_surface_id, *copy_quad->surface_range.start());
  EXPECT_FALSE(copy_quad->stretch_content_to_fill_bounds);
  EXPECT_FALSE(copy_quad->is_reflection);
}

TEST(DrawQuadTest, CopyTextureDrawQuad) {
  gfx::Rect visible_rect(40, 50, 30, 20);
  bool needs_blending = true;
  unsigned resource_id = 82;
  gfx::Size resource_size_in_pixels = gfx::Size(40, 41);
  bool premultiplied_alpha = true;
  gfx::PointF uv_top_left(0.5f, 224.f);
  gfx::PointF uv_bottom_right(51.5f, 260.f);
  const float vertex_opacity[] = {1.0f, 1.0f, 1.0f, 1.0f};
  bool y_flipped = true;
  bool nearest_neighbor = true;
  bool secure_output_only = true;
  gfx::ProtectedVideoType protected_video_type =
      gfx::ProtectedVideoType::kSoftwareProtected;
  CREATE_SHARED_STATE();

  CREATE_QUAD_NEW(TextureDrawQuad, visible_rect, needs_blending, resource_id,
                  premultiplied_alpha, uv_top_left, uv_bottom_right,
                  SK_ColorTRANSPARENT, vertex_opacity, y_flipped,
                  nearest_neighbor, secure_output_only, protected_video_type);
  EXPECT_EQ(DrawQuad::Material::kTextureContent, copy_quad->material);
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
  EXPECT_EQ(protected_video_type, copy_quad->protected_video_type);

  CREATE_QUAD_ALL(TextureDrawQuad, resource_id, resource_size_in_pixels,
                  premultiplied_alpha, uv_top_left, uv_bottom_right,
                  SK_ColorTRANSPARENT, vertex_opacity, y_flipped,
                  nearest_neighbor, secure_output_only, protected_video_type);
  EXPECT_EQ(DrawQuad::Material::kTextureContent, copy_quad->material);
  EXPECT_EQ(resource_id, copy_quad->resource_id());
  EXPECT_EQ(resource_size_in_pixels, copy_quad->resource_size_in_pixels());
  EXPECT_EQ(premultiplied_alpha, copy_quad->premultiplied_alpha);
  EXPECT_EQ(uv_top_left, copy_quad->uv_top_left);
  EXPECT_EQ(uv_bottom_right, copy_quad->uv_bottom_right);
  EXPECT_FLOAT_ARRAY_EQ(vertex_opacity, copy_quad->vertex_opacity, 4);
  EXPECT_EQ(y_flipped, copy_quad->y_flipped);
  EXPECT_EQ(nearest_neighbor, copy_quad->nearest_neighbor);
  EXPECT_EQ(secure_output_only, copy_quad->secure_output_only);
  EXPECT_EQ(protected_video_type, copy_quad->protected_video_type);
}

TEST(DrawQuadTest, CopyTileDrawQuad) {
  gfx::Rect visible_rect(40, 50, 30, 20);
  bool needs_blending = true;
  unsigned resource_id = 104;
  gfx::RectF tex_coord_rect(31.f, 12.f, 54.f, 20.f);
  gfx::Size texture_size(85, 32);
  bool contents_premultiplied = true;
  bool nearest_neighbor = true;
  bool force_anti_aliasing_off = false;
  CREATE_SHARED_STATE();

  CREATE_QUAD_NEW(TileDrawQuad, visible_rect, needs_blending, resource_id,
                  tex_coord_rect, texture_size, contents_premultiplied,
                  nearest_neighbor, force_anti_aliasing_off);
  EXPECT_EQ(DrawQuad::Material::kTiledContent, copy_quad->material);
  EXPECT_EQ(visible_rect, copy_quad->visible_rect);
  EXPECT_EQ(needs_blending, copy_quad->needs_blending);
  EXPECT_EQ(resource_id, copy_quad->resource_id());
  EXPECT_EQ(tex_coord_rect, copy_quad->tex_coord_rect);
  EXPECT_EQ(texture_size, copy_quad->texture_size);
  EXPECT_EQ(nearest_neighbor, copy_quad->nearest_neighbor);

  CREATE_QUAD_ALL(TileDrawQuad, resource_id, tex_coord_rect, texture_size,
                  contents_premultiplied, nearest_neighbor,
                  force_anti_aliasing_off);
  EXPECT_EQ(DrawQuad::Material::kTiledContent, copy_quad->material);
  EXPECT_EQ(resource_id, copy_quad->resource_id());
  EXPECT_EQ(tex_coord_rect, copy_quad->tex_coord_rect);
  EXPECT_EQ(texture_size, copy_quad->texture_size);
  EXPECT_EQ(nearest_neighbor, copy_quad->nearest_neighbor);
}

TEST(DrawQuadTest, CopyVideoHoleDrawQuad) {
  gfx::Rect visible_rect(40, 50, 30, 20);
  base::UnguessableToken overlay_plane_id = base::UnguessableToken::Create();
  CREATE_SHARED_STATE();

  CREATE_QUAD_NEW(VideoHoleDrawQuad, visible_rect, overlay_plane_id);
  EXPECT_EQ(DrawQuad::Material::kVideoHole, copy_quad->material);
  EXPECT_EQ(visible_rect, copy_quad->visible_rect);
  EXPECT_EQ(overlay_plane_id, copy_quad->overlay_plane_id);

  CREATE_QUAD_ALL(VideoHoleDrawQuad, overlay_plane_id);
  EXPECT_EQ(DrawQuad::Material::kVideoHole, copy_quad->material);
  EXPECT_EQ(overlay_plane_id, copy_quad->overlay_plane_id);
}

TEST(DrawQuadTest, CopyYUVVideoDrawQuad) {
  gfx::Rect visible_rect(40, 50, 30, 20);
  bool needs_blending = true;
  gfx::RectF ya_tex_coord_rect(40, 50, 30, 20);
  gfx::RectF uv_tex_coord_rect(20, 25, 15, 10);
  gfx::Size ya_tex_size(32, 68);
  gfx::Size uv_tex_size(41, 51);
  ResourceId y_plane_resource_id = 45;
  ResourceId u_plane_resource_id = 532;
  ResourceId v_plane_resource_id = 4;
  ResourceId a_plane_resource_id = 63;
  float resource_offset = 0.5f;
  float resource_multiplier = 2.001f;
  uint32_t bits_per_channel = 5;
  gfx::ProtectedVideoType protected_video_type =
      gfx::ProtectedVideoType::kHardwareProtected;
  gfx::ColorSpace video_color_space = gfx::ColorSpace::CreateJpeg();
  CREATE_SHARED_STATE();

  CREATE_QUAD_NEW(YUVVideoDrawQuad, visible_rect, needs_blending,
                  ya_tex_coord_rect, uv_tex_coord_rect, ya_tex_size,
                  uv_tex_size, y_plane_resource_id, u_plane_resource_id,
                  v_plane_resource_id, a_plane_resource_id, video_color_space,
                  resource_offset, resource_multiplier, bits_per_channel);
  EXPECT_EQ(DrawQuad::Material::kYuvVideoContent, copy_quad->material);
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
  EXPECT_EQ(resource_offset, copy_quad->resource_offset);
  EXPECT_EQ(resource_multiplier, copy_quad->resource_multiplier);
  EXPECT_EQ(bits_per_channel, copy_quad->bits_per_channel);
  EXPECT_EQ(gfx::ProtectedVideoType::kClear, copy_quad->protected_video_type);

  CREATE_QUAD_ALL(YUVVideoDrawQuad, ya_tex_coord_rect, uv_tex_coord_rect,
                  ya_tex_size, uv_tex_size, y_plane_resource_id,
                  u_plane_resource_id, v_plane_resource_id, a_plane_resource_id,
                  video_color_space, resource_offset, resource_multiplier,
                  bits_per_channel, protected_video_type);
  EXPECT_EQ(DrawQuad::Material::kYuvVideoContent, copy_quad->material);
  EXPECT_EQ(ya_tex_coord_rect, copy_quad->ya_tex_coord_rect);
  EXPECT_EQ(uv_tex_coord_rect, copy_quad->uv_tex_coord_rect);
  EXPECT_EQ(ya_tex_size, copy_quad->ya_tex_size);
  EXPECT_EQ(uv_tex_size, copy_quad->uv_tex_size);
  EXPECT_EQ(y_plane_resource_id, copy_quad->y_plane_resource_id());
  EXPECT_EQ(u_plane_resource_id, copy_quad->u_plane_resource_id());
  EXPECT_EQ(v_plane_resource_id, copy_quad->v_plane_resource_id());
  EXPECT_EQ(a_plane_resource_id, copy_quad->a_plane_resource_id());
  EXPECT_EQ(resource_offset, copy_quad->resource_offset);
  EXPECT_EQ(resource_multiplier, copy_quad->resource_multiplier);
  EXPECT_EQ(bits_per_channel, copy_quad->bits_per_channel);
  EXPECT_EQ(protected_video_type, copy_quad->protected_video_type);
}

TEST(DrawQuadTest, CopyPictureDrawQuad) {
  gfx::Rect visible_rect(40, 50, 30, 20);
  bool needs_blending = true;
  gfx::RectF tex_coord_rect(31.f, 12.f, 54.f, 20.f);
  gfx::Size texture_size(85, 32);
  bool nearest_neighbor = true;
  ResourceFormat texture_format = RGBA_8888;
  gfx::Rect content_rect(30, 40, 20, 30);
  float contents_scale = 3.141592f;
  scoped_refptr<cc::DisplayItemList> display_item_list =
      cc::FakeRasterSource::CreateEmpty(gfx::Size(100, 100))
          ->GetDisplayItemList();
  CREATE_SHARED_STATE();

  CREATE_QUAD_NEW(PictureDrawQuad, visible_rect, needs_blending, tex_coord_rect,
                  texture_size, nearest_neighbor, texture_format, content_rect,
                  contents_scale, {}, display_item_list);
  EXPECT_EQ(DrawQuad::Material::kPictureContent, copy_quad->material);
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
                  contents_scale, {}, display_item_list);
  EXPECT_EQ(DrawQuad::Material::kPictureContent, copy_quad->material);
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
  int IterateAndCount(DrawQuad* quad) {
    num_resources_ = 0;
    for (ResourceId& resource_id : quad->resources) {
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
  ResourceId mask_resource_id = 78;
  gfx::RectF mask_uv_rect(0.f, 0.f, 33.f, 19.f);
  gfx::Size mask_texture_size(128, 134);
  gfx::Vector2dF filters_scale(2.f, 3.f);
  gfx::PointF filters_origin(0.f, 0.f);
  gfx::RectF tex_coord_rect(1.f, 1.f, 33.f, 19.f);
  bool force_anti_aliasing_off = false;
  float backdrop_filter_quality = 1.0f;
  int copied_render_pass_id = 235;

  CREATE_SHARED_STATE();
  CREATE_QUAD_NEW_RP(RenderPassDrawQuad, visible_rect, render_pass_id,
                     mask_resource_id, mask_uv_rect, mask_texture_size,
                     filters_scale, filters_origin, tex_coord_rect,
                     force_anti_aliasing_off, backdrop_filter_quality,
                     copied_render_pass_id);
  EXPECT_EQ(mask_resource_id, quad_new->mask_resource_id());
  EXPECT_EQ(1, IterateAndCount(quad_new));
  EXPECT_EQ(mask_resource_id + 1, quad_new->mask_resource_id());

  ResourceId new_mask_resource_id = 0;
  gfx::Rect quad_rect(30, 40, 50, 60);
  quad_new->SetNew(shared_state, quad_rect, visible_rect, render_pass_id,
                   new_mask_resource_id, mask_uv_rect, mask_texture_size,
                   filters_scale, filters_origin, tex_coord_rect,
                   force_anti_aliasing_off, backdrop_filter_quality);
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
  ResourceId resource_id = 64;
  gfx::Size resource_size_in_pixels = gfx::Size(40, 41);
  gfx::PointF uv_top_left(0.25f, 0.3f);
  gfx::PointF uv_bottom_right(0.75f, 0.7f);

  CREATE_SHARED_STATE();
  CREATE_QUAD_NEW(StreamVideoDrawQuad, visible_rect, needs_blending,
                  resource_id, resource_size_in_pixels, uv_top_left,
                  uv_bottom_right);
  EXPECT_EQ(resource_id, quad_new->resource_id());
  EXPECT_EQ(resource_size_in_pixels, quad_new->resource_size_in_pixels());
  EXPECT_EQ(1, IterateAndCount(quad_new));
  EXPECT_EQ(resource_id + 1, quad_new->resource_id());
}

TEST_F(DrawQuadIteratorTest, SurfaceDrawQuad) {
  gfx::Rect visible_rect(40, 50, 30, 20);
  SurfaceId surface_id(kArbitraryFrameSinkId,
                       LocalSurfaceId(4321, base::UnguessableToken::Create()));

  CREATE_SHARED_STATE();
  CREATE_QUAD_NEW(SurfaceDrawQuad, visible_rect,
                  SurfaceRange(base::nullopt, surface_id), SK_ColorWHITE,
                  /*stretch_content_to_fill_bounds=*/false);
  EXPECT_EQ(0, IterateAndCount(quad_new));
}

TEST_F(DrawQuadIteratorTest, TextureDrawQuad) {
  gfx::Rect visible_rect(40, 50, 30, 20);
  unsigned resource_id = 82;
  bool premultiplied_alpha = true;
  gfx::PointF uv_top_left(0.5f, 224.f);
  gfx::PointF uv_bottom_right(51.5f, 260.f);
  const float vertex_opacity[] = {1.0f, 1.0f, 1.0f, 1.0f};
  bool y_flipped = true;
  bool nearest_neighbor = true;
  bool secure_output_only = true;
  gfx::ProtectedVideoType protected_video_type =
      gfx::ProtectedVideoType::kClear;

  CREATE_SHARED_STATE();
  CREATE_QUAD_NEW(TextureDrawQuad, visible_rect, needs_blending, resource_id,
                  premultiplied_alpha, uv_top_left, uv_bottom_right,
                  SK_ColorTRANSPARENT, vertex_opacity, y_flipped,
                  nearest_neighbor, secure_output_only, protected_video_type);
  EXPECT_EQ(resource_id, quad_new->resource_id());
  EXPECT_EQ(1, IterateAndCount(quad_new));
  EXPECT_EQ(resource_id + 1, quad_new->resource_id());
}

TEST_F(DrawQuadIteratorTest, TileDrawQuad) {
  gfx::Rect visible_rect(40, 50, 30, 20);
  unsigned resource_id = 104;
  gfx::RectF tex_coord_rect(31.f, 12.f, 54.f, 20.f);
  gfx::Size texture_size(85, 32);
  bool contents_premultiplied = true;
  bool nearest_neighbor = true;
  bool force_anti_aliasing_off = false;

  CREATE_SHARED_STATE();
  CREATE_QUAD_NEW(TileDrawQuad, visible_rect, needs_blending, resource_id,
                  tex_coord_rect, texture_size, contents_premultiplied,
                  nearest_neighbor, force_anti_aliasing_off);
  EXPECT_EQ(resource_id, quad_new->resource_id());
  EXPECT_EQ(1, IterateAndCount(quad_new));
  EXPECT_EQ(resource_id + 1, quad_new->resource_id());
}

TEST_F(DrawQuadIteratorTest, VideoHoleDrawQuad) {
  gfx::Rect visible_rect(40, 50, 30, 20);
  base::UnguessableToken overlay_plane_id = base::UnguessableToken::Create();

  CREATE_SHARED_STATE();
  CREATE_QUAD_NEW(VideoHoleDrawQuad, visible_rect, overlay_plane_id);
  EXPECT_EQ(0, IterateAndCount(quad_new));
}

TEST_F(DrawQuadIteratorTest, YUVVideoDrawQuad) {
  gfx::Rect visible_rect(40, 50, 30, 20);
  gfx::RectF ya_tex_coord_rect(0.0f, 0.0f, 0.75f, 0.5f);
  gfx::RectF uv_tex_coord_rect(0.0f, 0.0f, 0.375f, 0.25f);
  gfx::Size ya_tex_size(32, 68);
  gfx::Size uv_tex_size(41, 51);
  ResourceId y_plane_resource_id = 45;
  ResourceId u_plane_resource_id = 532;
  ResourceId v_plane_resource_id = 4;
  ResourceId a_plane_resource_id = 63;
  gfx::ColorSpace video_color_space = gfx::ColorSpace::CreateJpeg();

  CREATE_SHARED_STATE();
  CREATE_QUAD_NEW(YUVVideoDrawQuad, visible_rect, needs_blending,
                  ya_tex_coord_rect, uv_tex_coord_rect, ya_tex_size,
                  uv_tex_size, y_plane_resource_id, u_plane_resource_id,
                  v_plane_resource_id, a_plane_resource_id, video_color_space,
                  0.0, 1.0, 5);
  EXPECT_EQ(DrawQuad::Material::kYuvVideoContent, copy_quad->material);
  EXPECT_EQ(y_plane_resource_id, quad_new->y_plane_resource_id());
  EXPECT_EQ(u_plane_resource_id, quad_new->u_plane_resource_id());
  EXPECT_EQ(v_plane_resource_id, quad_new->v_plane_resource_id());
  EXPECT_EQ(a_plane_resource_id, quad_new->a_plane_resource_id());
  EXPECT_EQ(4, IterateAndCount(quad_new));
  EXPECT_EQ(y_plane_resource_id + 1, quad_new->y_plane_resource_id());
  EXPECT_EQ(u_plane_resource_id + 1, quad_new->u_plane_resource_id());
  EXPECT_EQ(v_plane_resource_id + 1, quad_new->v_plane_resource_id());
  EXPECT_EQ(a_plane_resource_id + 1, quad_new->a_plane_resource_id());
}

TEST(DrawQuadTest, LargestQuadType) {
  size_t largest = 0;

  for (int i = 0; i <= static_cast<int>(DrawQuad::Material::kMaxValue); ++i) {
    switch (static_cast<DrawQuad::Material>(i)) {
      case DrawQuad::Material::kDebugBorder:
        largest = std::max(largest, sizeof(DebugBorderDrawQuad));
        break;
      case DrawQuad::Material::kPictureContent:
        largest = std::max(largest, sizeof(PictureDrawQuad));
        break;
      case DrawQuad::Material::kTextureContent:
        largest = std::max(largest, sizeof(TextureDrawQuad));
        break;
      case DrawQuad::Material::kRenderPass:
        largest = std::max(largest, sizeof(RenderPassDrawQuad));
        break;
      case DrawQuad::Material::kSolidColor:
        largest = std::max(largest, sizeof(SolidColorDrawQuad));
        break;
      case DrawQuad::Material::kSurfaceContent:
        largest = std::max(largest, sizeof(SurfaceDrawQuad));
        break;
      case DrawQuad::Material::kTiledContent:
        largest = std::max(largest, sizeof(TileDrawQuad));
        break;
      case DrawQuad::Material::kStreamVideoContent:
        largest = std::max(largest, sizeof(StreamVideoDrawQuad));
        break;
      case DrawQuad::Material::kYuvVideoContent:
        largest = std::max(largest, sizeof(YUVVideoDrawQuad));
        break;
      case DrawQuad::Material::kVideoHole:
        largest = std::max(largest, sizeof(VideoHoleDrawQuad));
        break;
      case DrawQuad::Material::kInvalid:
        break;
    }
  }
  EXPECT_EQ(LargestDrawQuadSize(), largest);

  if (!HasFailure())
    return;

  // On failure, output the size of all quads for debugging.
  LOG(ERROR) << "largest " << largest;
  LOG(ERROR) << "kLargestDrawQuad " << LargestDrawQuadSize();
  for (int i = 0; i <= static_cast<int>(DrawQuad::Material::kMaxValue); ++i) {
    switch (static_cast<DrawQuad::Material>(i)) {
      case DrawQuad::Material::kDebugBorder:
        LOG(ERROR) << "DebugBorderDrawQuad " << sizeof(DebugBorderDrawQuad);
        break;
      case DrawQuad::Material::kPictureContent:
        LOG(ERROR) << "PictureDrawQuad " << sizeof(PictureDrawQuad);
        break;
      case DrawQuad::Material::kTextureContent:
        LOG(ERROR) << "TextureDrawQuad " << sizeof(TextureDrawQuad);
        break;
      case DrawQuad::Material::kRenderPass:
        LOG(ERROR) << "RenderPassDrawQuad " << sizeof(RenderPassDrawQuad);
        break;
      case DrawQuad::Material::kSolidColor:
        LOG(ERROR) << "SolidColorDrawQuad " << sizeof(SolidColorDrawQuad);
        break;
      case DrawQuad::Material::kSurfaceContent:
        LOG(ERROR) << "SurfaceDrawQuad " << sizeof(SurfaceDrawQuad);
        break;
      case DrawQuad::Material::kTiledContent:
        LOG(ERROR) << "TileDrawQuad " << sizeof(TileDrawQuad);
        break;
      case DrawQuad::Material::kStreamVideoContent:
        LOG(ERROR) << "StreamVideoDrawQuad " << sizeof(StreamVideoDrawQuad);
        break;
      case DrawQuad::Material::kYuvVideoContent:
        LOG(ERROR) << "YUVVideoDrawQuad " << sizeof(YUVVideoDrawQuad);
        break;
      case DrawQuad::Material::kVideoHole:
        LOG(ERROR) << "VideoHoleDrawQuad " << sizeof(VideoHoleDrawQuad);
        break;
      case DrawQuad::Material::kInvalid:
        break;
    }
  }
}

}  // namespace
}  // namespace viz
