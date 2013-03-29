// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/quads/draw_quad.h"

#include <algorithm>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "cc/base/math_util.h"
#include "cc/quads/checkerboard_draw_quad.h"
#include "cc/quads/debug_border_draw_quad.h"
#include "cc/quads/io_surface_draw_quad.h"
#include "cc/quads/picture_draw_quad.h"
#include "cc/quads/render_pass_draw_quad.h"
#include "cc/quads/solid_color_draw_quad.h"
#include "cc/quads/stream_video_draw_quad.h"
#include "cc/quads/texture_draw_quad.h"
#include "cc/quads/tile_draw_quad.h"
#include "cc/quads/yuv_video_draw_quad.h"
#include "cc/resources/picture_pile_impl.h"
#include "cc/test/geometry_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebFilterOperations.h"
#include "third_party/skia/include/effects/SkBlurImageFilter.h"
#include "ui/gfx/transform.h"

namespace cc {
namespace {

TEST(DrawQuadTest, CopySharedQuadState) {
  gfx::Transform quad_transform = gfx::Transform(1.0, 0.0, 0.5, 1.0, 0.5, 0.0);
  gfx::Size content_bounds(26, 28);
  gfx::Rect visible_content_rect(10, 12, 14, 16);
  gfx::Rect clip_rect(19, 21, 23, 25);
  bool is_clipped = true;
  float opacity = 0.25f;

  scoped_ptr<SharedQuadState> state(SharedQuadState::Create());
  state->SetAll(quad_transform,
                content_bounds,
                visible_content_rect,
                clip_rect,
                is_clipped,
                opacity);

  scoped_ptr<SharedQuadState> copy(state->Copy());
  EXPECT_EQ(quad_transform, copy->content_to_target_transform);
  EXPECT_RECT_EQ(visible_content_rect, copy->visible_content_rect);
  EXPECT_EQ(opacity, copy->opacity);
  EXPECT_RECT_EQ(clip_rect, copy->clip_rect);
  EXPECT_EQ(is_clipped, copy->is_clipped);
}

scoped_ptr<SharedQuadState> CreateSharedQuadState() {
  gfx::Transform quad_transform = gfx::Transform(1.0, 0.0, 0.5, 1.0, 0.5, 0.0);
  gfx::Size content_bounds(26, 28);
  gfx::Rect visible_content_rect(10, 12, 14, 16);
  gfx::Rect clip_rect(19, 21, 23, 25);
  bool is_clipped = false;
  float opacity = 1.f;

  scoped_ptr<SharedQuadState> state(SharedQuadState::Create());
  state->SetAll(quad_transform,
                content_bounds,
                visible_content_rect,
                clip_rect,
                is_clipped,
                opacity);
  return state.Pass();
}

void CompareDrawQuad(DrawQuad* quad,
                     DrawQuad* copy,
                     SharedQuadState* copy_shared_state) {
  EXPECT_EQ(quad->material, copy->material);
  EXPECT_RECT_EQ(quad->rect, copy->rect);
  EXPECT_RECT_EQ(quad->visible_rect, copy->visible_rect);
  EXPECT_RECT_EQ(quad->opaque_rect, copy->opaque_rect);
  EXPECT_EQ(quad->needs_blending, copy->needs_blending);
  EXPECT_EQ(copy_shared_state, copy->shared_quad_state);
}

#define CREATE_SHARED_STATE() \
    scoped_ptr<SharedQuadState> shared_state(CreateSharedQuadState()); \
    scoped_ptr<SharedQuadState> copy_shared_state(shared_state->Copy()); \

#define QUAD_DATA \
    gfx::Rect quad_rect(30, 40, 50, 60); \
    gfx::Rect quad_visible_rect(40, 50, 30, 20); \
    gfx::Rect ALLOW_UNUSED quad_opaque_rect(60, 55, 10, 10); \
    bool ALLOW_UNUSED needs_blending = true;

#define SETUP_AND_COPY_QUAD_NEW(Type, quad) \
    scoped_ptr<DrawQuad> copy_new(quad_new->Copy(copy_shared_state.get())); \
    CompareDrawQuad(quad_new.get(), copy_new.get(), copy_shared_state.get()); \
    const Type* ALLOW_UNUSED copy_quad = Type::MaterialCast(copy_new.get());

#define SETUP_AND_COPY_QUAD_ALL(Type, quad) \
    scoped_ptr<DrawQuad> copy_all(quad_all->Copy(copy_shared_state.get())); \
    CompareDrawQuad(quad_all.get(), copy_all.get(), copy_shared_state.get()); \
    copy_quad = Type::MaterialCast(copy_all.get());

#define SETUP_AND_COPY_QUAD_NEW_1(Type, quad, a) \
    scoped_ptr<DrawQuad> copy_new(quad_new->Copy(copy_shared_state.get(), a)); \
    CompareDrawQuad(quad_new.get(), copy_new.get(), copy_shared_state.get()); \
    const Type* ALLOW_UNUSED copy_quad = Type::MaterialCast(copy_new.get());

#define SETUP_AND_COPY_QUAD_ALL_1(Type, quad, a) \
    scoped_ptr<DrawQuad> copy_all(quad_all->Copy(copy_shared_state.get(), a)); \
    CompareDrawQuad(quad_all.get(), copy_all.get(), copy_shared_state.get()); \
    copy_quad = Type::MaterialCast(copy_all.get());

#define CREATE_QUAD_1_NEW(Type, a) \
    scoped_ptr<Type> quad_new(Type::Create()); \
    { \
      QUAD_DATA \
      quad_new->SetNew(shared_state.get(), quad_rect, a); \
    } \
    SETUP_AND_COPY_QUAD_NEW(Type, quad_new);

#define CREATE_QUAD_1_ALL(Type, a) \
    scoped_ptr<Type> quad_all(Type::Create()); \
    { \
      QUAD_DATA \
      quad_all->SetAll(shared_state.get(), quad_rect, quad_opaque_rect, \
                       quad_visible_rect, needs_blending, a); \
    } \
    SETUP_AND_COPY_QUAD_ALL(Type, quad_all);

#define CREATE_QUAD_2_NEW(Type, a, b) \
    scoped_ptr<Type> quad_new(Type::Create()); \
    { \
      QUAD_DATA \
      quad_new->SetNew(shared_state.get(), quad_rect, a, b); \
    } \
    SETUP_AND_COPY_QUAD_NEW(Type, quad_new);

#define CREATE_QUAD_2_ALL(Type, a, b) \
    scoped_ptr<Type> quad_all(Type::Create()); \
    { \
      QUAD_DATA \
      quad_all->SetAll(shared_state.get(), quad_rect, quad_opaque_rect, \
                       quad_visible_rect, needs_blending, a, b); \
    } \
    SETUP_AND_COPY_QUAD_ALL(Type, quad_all);

#define CREATE_QUAD_3_NEW(Type, a, b, c) \
    scoped_ptr<Type> quad_new(Type::Create()); \
    { \
      QUAD_DATA \
      quad_new->SetNew(shared_state.get(), quad_rect, a, b, c); \
    } \
    SETUP_AND_COPY_QUAD_NEW(Type, quad_new);

#define CREATE_QUAD_3_ALL(Type, a, b, c) \
    scoped_ptr<Type> quad_all(Type::Create()); \
    { \
      QUAD_DATA \
      quad_all->SetAll(shared_state.get(), quad_rect, quad_opaque_rect, \
                       quad_visible_rect, needs_blending, a, b, c); \
    } \
    SETUP_AND_COPY_QUAD_ALL(Type, quad_all);

#define CREATE_QUAD_4_NEW(Type, a, b, c, d) \
    scoped_ptr<Type> quad_new(Type::Create()); \
    { \
      QUAD_DATA \
      quad_new->SetNew(shared_state.get(), quad_rect, a, b, c, d); \
    } \
    SETUP_AND_COPY_QUAD_NEW(Type, quad_new);

#define CREATE_QUAD_4_ALL(Type, a, b, c, d) \
    scoped_ptr<Type> quad_all(Type::Create()); \
    { \
      QUAD_DATA \
      quad_all->SetAll(shared_state.get(), quad_rect, quad_opaque_rect, \
                       quad_visible_rect, needs_blending, a, b, c, d); \
    } \
    SETUP_AND_COPY_QUAD_ALL(Type, quad_all);

#define CREATE_QUAD_5_NEW(Type, a, b, c, d, e) \
    scoped_ptr<Type> quad_new(Type::Create()); \
    { \
      QUAD_DATA \
      quad_new->SetNew(shared_state.get(), quad_rect, a, b, c, d, e); \
    } \
    SETUP_AND_COPY_QUAD_NEW(Type, quad_new);

#define CREATE_QUAD_5_ALL(Type, a, b, c, d, e) \
    scoped_ptr<Type> quad_all(Type::Create()); \
    { \
      QUAD_DATA \
      quad_all->SetAll(shared_state.get(), quad_rect, quad_opaque_rect, \
                       quad_visible_rect, needs_blending, a, b, c, d, e); \
    } \
    SETUP_AND_COPY_QUAD_ALL(Type, quad_all);

#define CREATE_QUAD_5_NEW_1(Type, a, b, c, d, e, copy_a) \
    scoped_ptr<Type> quad_new(Type::Create()); \
    { \
      QUAD_DATA \
      quad_new->SetNew(shared_state.get(), quad_rect, a, b, c, d, e); \
    } \
    SETUP_AND_COPY_QUAD_NEW_1(Type, quad_new, copy_a);

#define CREATE_QUAD_5_ALL_1(Type, a, b, c, d, e, copy_a) \
    scoped_ptr<Type> quad_all(Type::Create()); \
    { \
      QUAD_DATA \
      quad_all->SetAll(shared_state.get(), quad_rect, quad_opaque_rect, \
                       quad_visible_rect, needs_blending, a, b, c, d, e); \
    } \
    SETUP_AND_COPY_QUAD_ALL_1(Type, quad_all, copy_a);

#define CREATE_QUAD_6_NEW(Type, a, b, c, d, e, f) \
    scoped_ptr<Type> quad_new(Type::Create()); \
    { \
      QUAD_DATA \
      quad_new->SetNew(shared_state.get(), quad_rect, a, b, c, d, e, f); \
    } \
    SETUP_AND_COPY_QUAD_NEW(Type, quad_new);

#define CREATE_QUAD_6_ALL(Type, a, b, c, d, e, f) \
    scoped_ptr<Type> quad_all(Type::Create()); \
    { \
      QUAD_DATA \
      quad_all->SetAll(shared_state.get(), quad_rect, quad_opaque_rect, \
                       quad_visible_rect, needs_blending, a, b, c, d, e, f); \
    } \
    SETUP_AND_COPY_QUAD_ALL(Type, quad_all);

#define CREATE_QUAD_7_NEW(Type, a, b, c, d, e, f, g) \
    scoped_ptr<Type> quad_new(Type::Create()); \
    { \
      QUAD_DATA \
      quad_new->SetNew(shared_state.get(), quad_rect, a, b, c, d, e, f, g); \
    } \
    SETUP_AND_COPY_QUAD_NEW(Type, quad_new);

#define CREATE_QUAD_7_ALL(Type, a, b, c, d, e, f, g) \
    scoped_ptr<Type> quad_all(Type::Create()); \
    { \
      QUAD_DATA \
      quad_all->SetAll(shared_state.get(), quad_rect, quad_opaque_rect, \
                       quad_visible_rect, needs_blending, \
                       a, b, c, d, e, f, g); \
    } \
    SETUP_AND_COPY_QUAD_ALL(Type, quad_all);

#define CREATE_QUAD_8_NEW(Type, a, b, c, d, e, f, g, h) \
    scoped_ptr<Type> quad_new(Type::Create()); \
    { \
      QUAD_DATA \
      quad_new->SetNew(shared_state.get(), quad_rect, a, b, c, d, e, f, g, h); \
    } \
    SETUP_AND_COPY_QUAD_NEW(Type, quad_new);

#define CREATE_QUAD_8_ALL(Type, a, b, c, d, e, f, g, h) \
    scoped_ptr<Type> quad_all(Type::Create()); \
    { \
      QUAD_DATA \
      quad_all->SetAll(shared_state.get(), quad_rect, quad_opaque_rect, \
                       quad_visible_rect, needs_blending, \
                       a, b, c, d, e, f, g, h); \
    } \
    SETUP_AND_COPY_QUAD_ALL(Type, quad_all);

#define CREATE_QUAD_8_NEW_1(Type, a, b, c, d, e, f, g, h, copy_a) \
    scoped_ptr<Type> quad_new(Type::Create()); \
    { \
      QUAD_DATA \
      quad_new->SetNew(shared_state.get(), quad_rect, a, b, c, d, e, f, g, h); \
    } \
    SETUP_AND_COPY_QUAD_NEW_1(Type, quad_new, copy_a);

#define CREATE_QUAD_8_ALL_1(Type, a, b, c, d, e, f, g, h, copy_a) \
    scoped_ptr<Type> quad_all(Type::Create()); \
    { \
      QUAD_DATA \
      quad_all->SetAll(shared_state.get(), quad_rect, quad_opaque_rect, \
                       quad_visible_rect, needs_blending, \
                       a, b, c, d, e, f, g, h); \
    } \
    SETUP_AND_COPY_QUAD_ALL_1(Type, quad_all, copy_a);

#define CREATE_QUAD_9_NEW(Type, a, b, c, d, e, f, g, h, i) \
    scoped_ptr<Type> quad_new(Type::Create()); \
    { \
      QUAD_DATA \
      quad_new->SetNew(shared_state.get(), quad_rect, \
                       a, b, c, d, e, f, g, h, i); \
    } \
    SETUP_AND_COPY_QUAD_NEW(Type, quad_new);

#define CREATE_QUAD_9_ALL(Type, a, b, c, d, e, f, g, h, i) \
    scoped_ptr<Type> quad_all(Type::Create()); \
    { \
      QUAD_DATA \
      quad_all->SetAll(shared_state.get(), quad_rect, quad_opaque_rect, \
                       quad_visible_rect, needs_blending, \
                       a, b, c, d, e, f, g, h, i); \
    } \
    SETUP_AND_COPY_QUAD_ALL(Type, quad_all);

TEST(DrawQuadTest, CopyCheckerboardDrawQuad) {
  SkColor color = 0xfabb0011;
  CREATE_SHARED_STATE();

  CREATE_QUAD_1_NEW(CheckerboardDrawQuad, color);
  EXPECT_EQ(DrawQuad::CHECKERBOARD, copy_quad->material);
  EXPECT_EQ(color, copy_quad->color);

  CREATE_QUAD_1_ALL(CheckerboardDrawQuad, color);
  EXPECT_EQ(DrawQuad::CHECKERBOARD, copy_quad->material);
  EXPECT_EQ(color, copy_quad->color);
}

TEST(DrawQuadTest, CopyDebugBorderDrawQuad) {
  SkColor color = 0xfabb0011;
  int width = 99;
  CREATE_SHARED_STATE();

  CREATE_QUAD_2_NEW(DebugBorderDrawQuad, color, width);
  EXPECT_EQ(DrawQuad::DEBUG_BORDER, copy_quad->material);
  EXPECT_EQ(color, copy_quad->color);
  EXPECT_EQ(width, copy_quad->width);

  CREATE_QUAD_2_ALL(DebugBorderDrawQuad, color, width);
  EXPECT_EQ(DrawQuad::DEBUG_BORDER, copy_quad->material);
  EXPECT_EQ(color, copy_quad->color);
  EXPECT_EQ(width, copy_quad->width);
}

TEST(DrawQuadTest, CopyIOSurfaceDrawQuad) {
  gfx::Rect opaque_rect(3, 7, 10, 12);
  gfx::Size size(58, 95);
  unsigned texture_id = 72;
  IOSurfaceDrawQuad::Orientation orientation = IOSurfaceDrawQuad::UNFLIPPED;
  CREATE_SHARED_STATE();

  CREATE_QUAD_4_NEW(
      IOSurfaceDrawQuad, opaque_rect, size, texture_id, orientation);
  EXPECT_EQ(DrawQuad::IO_SURFACE_CONTENT, copy_quad->material);
  EXPECT_RECT_EQ(opaque_rect, copy_quad->opaque_rect);
  EXPECT_EQ(size, copy_quad->io_surface_size);
  EXPECT_EQ(texture_id, copy_quad->io_surface_texture_id);
  EXPECT_EQ(orientation, copy_quad->orientation);

  CREATE_QUAD_3_ALL(IOSurfaceDrawQuad, size, texture_id, orientation);
  EXPECT_EQ(DrawQuad::IO_SURFACE_CONTENT, copy_quad->material);
  EXPECT_EQ(size, copy_quad->io_surface_size);
  EXPECT_EQ(texture_id, copy_quad->io_surface_texture_id);
  EXPECT_EQ(orientation, copy_quad->orientation);
}

TEST(DrawQuadTest, CopyRenderPassDrawQuad) {
  RenderPass::Id render_pass_id(22, 64);
  bool is_replica = true;
  ResourceProvider::ResourceId mask_resource_id = 78;
  gfx::Rect contents_changed_since_last_frame(42, 11, 74, 24);
  gfx::RectF mask_u_v_rect(-45.f, -21.f, 33.f, 19.f);
  WebKit::WebFilterOperations filters;
  filters.append(WebKit::WebFilterOperation::createBlurFilter(1.f));
  WebKit::WebFilterOperations background_filters;
  background_filters.append(
      WebKit::WebFilterOperation::createGrayscaleFilter(1.f));
  skia::RefPtr<SkImageFilter> filter =
      skia::AdoptRef(new SkBlurImageFilter(SK_Scalar1, SK_Scalar1));

  RenderPass::Id copied_render_pass_id(235, 11);
  CREATE_SHARED_STATE();

  CREATE_QUAD_8_NEW_1(RenderPassDrawQuad,
                      render_pass_id,
                      is_replica,
                      mask_resource_id,
                      contents_changed_since_last_frame,
                      mask_u_v_rect,
                      filters,
                      filter,
                      background_filters,
                      copied_render_pass_id);
  EXPECT_EQ(DrawQuad::RENDER_PASS, copy_quad->material);
  EXPECT_EQ(copied_render_pass_id, copy_quad->render_pass_id);
  EXPECT_EQ(is_replica, copy_quad->is_replica);
  EXPECT_EQ(mask_resource_id, copy_quad->mask_resource_id);
  EXPECT_RECT_EQ(contents_changed_since_last_frame,
                 copy_quad->contents_changed_since_last_frame);
  EXPECT_EQ(mask_u_v_rect.ToString(), copy_quad->mask_uv_rect.ToString());
  EXPECT_EQ(filters, copy_quad->filters);
  EXPECT_EQ(filter, copy_quad->filter);
  EXPECT_EQ(background_filters, copy_quad->background_filters);

  CREATE_QUAD_8_ALL_1(RenderPassDrawQuad,
                      render_pass_id,
                      is_replica,
                      mask_resource_id,
                      contents_changed_since_last_frame,
                      mask_u_v_rect,
                      filters,
                      filter,
                      background_filters,
                      copied_render_pass_id);
  EXPECT_EQ(DrawQuad::RENDER_PASS, copy_quad->material);
  EXPECT_EQ(copied_render_pass_id, copy_quad->render_pass_id);
  EXPECT_EQ(is_replica, copy_quad->is_replica);
  EXPECT_EQ(mask_resource_id, copy_quad->mask_resource_id);
  EXPECT_RECT_EQ(contents_changed_since_last_frame,
                 copy_quad->contents_changed_since_last_frame);
  EXPECT_EQ(mask_u_v_rect.ToString(), copy_quad->mask_uv_rect.ToString());
  EXPECT_EQ(filters, copy_quad->filters);
  EXPECT_EQ(filter, copy_quad->filter);
  EXPECT_EQ(background_filters, copy_quad->background_filters);
}

TEST(DrawQuadTest, CopySolidColorDrawQuad) {
  SkColor color = 0x49494949;
  CREATE_SHARED_STATE();

  CREATE_QUAD_1_NEW(SolidColorDrawQuad, color);
  EXPECT_EQ(DrawQuad::SOLID_COLOR, copy_quad->material);
  EXPECT_EQ(color, copy_quad->color);

  CREATE_QUAD_1_ALL(SolidColorDrawQuad, color);
  EXPECT_EQ(DrawQuad::SOLID_COLOR, copy_quad->material);
  EXPECT_EQ(color, copy_quad->color);
}

TEST(DrawQuadTest, CopyStreamVideoDrawQuad) {
  gfx::Rect opaque_rect(3, 7, 10, 12);
  unsigned texture_id = 64;
  gfx::Transform matrix = gfx::Transform(0.5, 0.25, 1, 0.75, 0, 1);
  CREATE_SHARED_STATE();

  CREATE_QUAD_3_NEW(StreamVideoDrawQuad, opaque_rect, texture_id, matrix);
  EXPECT_EQ(DrawQuad::STREAM_VIDEO_CONTENT, copy_quad->material);
  EXPECT_RECT_EQ(opaque_rect, copy_quad->opaque_rect);
  EXPECT_EQ(texture_id, copy_quad->texture_id);
  EXPECT_EQ(matrix, copy_quad->matrix);

  CREATE_QUAD_2_ALL(StreamVideoDrawQuad, texture_id, matrix);
  EXPECT_EQ(DrawQuad::STREAM_VIDEO_CONTENT, copy_quad->material);
  EXPECT_EQ(texture_id, copy_quad->texture_id);
  EXPECT_EQ(matrix, copy_quad->matrix);
}

TEST(DrawQuadTest, CopyTextureDrawQuad) {
  gfx::Rect opaque_rect(3, 7, 10, 12);
  unsigned resource_id = 82;
  bool premultiplied_alpha = true;
  gfx::PointF uv_top_left(0.5f, 224.f);
  gfx::PointF uv_bottom_right(51.5f, 260.f);
  const float vertex_opacity[] = { 1.0f, 1.0f, 1.0f, 1.0f };
  bool flipped = true;
  CREATE_SHARED_STATE();

  CREATE_QUAD_7_NEW(TextureDrawQuad,
                    opaque_rect,
                    resource_id,
                    premultiplied_alpha,
                    uv_top_left,
                    uv_bottom_right,
                    vertex_opacity,
                    flipped);
  EXPECT_EQ(DrawQuad::TEXTURE_CONTENT, copy_quad->material);
  EXPECT_RECT_EQ(opaque_rect, copy_quad->opaque_rect);
  EXPECT_EQ(resource_id, copy_quad->resource_id);
  EXPECT_EQ(premultiplied_alpha, copy_quad->premultiplied_alpha);
  EXPECT_EQ(uv_top_left, copy_quad->uv_top_left);
  EXPECT_EQ(uv_bottom_right, copy_quad->uv_bottom_right);
  EXPECT_FLOAT_ARRAY_EQ(vertex_opacity, copy_quad->vertex_opacity, 4);
  EXPECT_EQ(flipped, copy_quad->flipped);

  CREATE_QUAD_6_ALL(TextureDrawQuad,
                    resource_id,
                    premultiplied_alpha,
                    uv_top_left,
                    uv_bottom_right,
                    vertex_opacity,
                    flipped);
  EXPECT_EQ(DrawQuad::TEXTURE_CONTENT, copy_quad->material);
  EXPECT_EQ(resource_id, copy_quad->resource_id);
  EXPECT_EQ(premultiplied_alpha, copy_quad->premultiplied_alpha);
  EXPECT_EQ(uv_top_left, copy_quad->uv_top_left);
  EXPECT_EQ(uv_bottom_right, copy_quad->uv_bottom_right);
  EXPECT_FLOAT_ARRAY_EQ(vertex_opacity, copy_quad->vertex_opacity, 4);
  EXPECT_EQ(flipped, copy_quad->flipped);
}

TEST(DrawQuadTest, ClipTextureDrawQuad) {
  gfx::Rect opaque_rect(3, 7, 10, 12);
  unsigned resource_id = 82;
  bool premultiplied_alpha = true;
  bool flipped = true;
  CREATE_SHARED_STATE();
  // The original quad position is (30, 40) its size is 50*60.
  shared_state->content_to_target_transform =
      gfx::Transform(1.f, 0.f, 0.f, 1.f, 10.f, 20.f);
  // After transformation, the quad position is (40, 60) its size is 50*60.
  shared_state->clip_rect = gfx::Rect(50, 70, 30, 20);

  // The original quad is 'ABCD', the clipped quad is 'abcd':
  // 40 50       90
  //  B--:-------C 60
  //  |  b----c -|-70
  //  |  |    |  |
  //  |  a----d -|-90
  //  |          |
  //  A----------D 120
  // UV and vertex opacity are stored per vertex on the parent rectangle 'ABCD'.

  // This is the UV value for vertex 'B'.
  gfx::PointF uv_top_left(0.1f, 0.2f);
  // This is the UV value for vertex 'D'.
  gfx::PointF uv_bottom_right(0.9f, 0.8f);
  // This the vertex opacity for the vertices 'ABCD'.
  const float vertex_opacity[] = { 0.3f, 0.4f, 0.7f, 0.8f };
  {
    CREATE_QUAD_7_NEW(TextureDrawQuad,
                      opaque_rect,
                      resource_id,
                      premultiplied_alpha,
                      uv_top_left,
                      uv_bottom_right,
                      vertex_opacity,
                      flipped);
    CREATE_QUAD_6_ALL(TextureDrawQuad,
                      resource_id,
                      premultiplied_alpha,
                      uv_top_left,
                      uv_bottom_right,
                      vertex_opacity,
                      flipped);
    EXPECT_TRUE(quad_all->PerformClipping());

    // This is the expected UV value for vertex 'b'.
    // uv(b) = uv(B) + (Bb / BD) * (uv(D) - uv(B))
    // 0.3 = 0.2 + (10 / 60) * (0.8 - 0.2)
    gfx::PointF uv_top_left_clipped(0.26f, 0.3f);
    // This is the expected UV value for vertex 'd'.
    // uv(d) = uv(B) + (Bd / BD) * (uv(D) - uv(B))
    gfx::PointF uv_bottom_right_clipped(0.74f, 0.5f);
    // This the expected vertex opacity for the vertices 'abcd'.
    // They are computed with a bilinear interpolation of the corner values.
    const float vertex_opacity_clipped[] = { 0.43f, 0.45f, 0.65f, 0.67f };

    EXPECT_EQ(uv_top_left_clipped, quad_all->uv_top_left);
    EXPECT_EQ(uv_bottom_right_clipped, quad_all->uv_bottom_right);
    EXPECT_FLOAT_ARRAY_EQ(vertex_opacity_clipped, quad_all->vertex_opacity, 4);
  }

  uv_top_left = gfx::PointF(0.8f, 0.7f);
  uv_bottom_right = gfx::PointF(0.2f, 0.1f);
  {
    CREATE_QUAD_7_NEW(TextureDrawQuad,
                      opaque_rect,
                      resource_id,
                      premultiplied_alpha,
                      uv_top_left,
                      uv_bottom_right,
                      vertex_opacity,
                      flipped);
    CREATE_QUAD_6_ALL(TextureDrawQuad,
                      resource_id,
                      premultiplied_alpha,
                      uv_top_left,
                      uv_bottom_right,
                      vertex_opacity,
                      flipped);
    EXPECT_TRUE(quad_all->PerformClipping());

    // This is the expected UV value for vertex 'b'.
    gfx::PointF uv_top_left_clipped(0.68f, 0.6f);
    // This is the expected UV value for vertex 'd'.
    gfx::PointF uv_bottom_right_clipped(0.32f, 0.4f);

    EXPECT_EQ(uv_top_left_clipped, quad_all->uv_top_left);
    EXPECT_EQ(uv_bottom_right_clipped, quad_all->uv_bottom_right);
  }
}

TEST(DrawQuadTest, CopyTileDrawQuad) {
  gfx::Rect opaque_rect(33, 44, 22, 33);
  unsigned resource_id = 104;
  gfx::RectF tex_coord_rect(31.f, 12.f, 54.f, 20.f);
  gfx::Size texture_size(85, 32);
  bool swizzle_contents = true;
  CREATE_SHARED_STATE();

  CREATE_QUAD_5_NEW(TileDrawQuad,
                    opaque_rect,
                    resource_id,
                    tex_coord_rect,
                    texture_size,
                    swizzle_contents);
  EXPECT_EQ(DrawQuad::TILED_CONTENT, copy_quad->material);
  EXPECT_RECT_EQ(opaque_rect, copy_quad->opaque_rect);
  EXPECT_EQ(resource_id, copy_quad->resource_id);
  EXPECT_EQ(tex_coord_rect, copy_quad->tex_coord_rect);
  EXPECT_EQ(texture_size, copy_quad->texture_size);
  EXPECT_EQ(swizzle_contents, copy_quad->swizzle_contents);

  CREATE_QUAD_4_ALL(TileDrawQuad,
                    resource_id,
                    tex_coord_rect,
                    texture_size,
                    swizzle_contents);
  EXPECT_EQ(DrawQuad::TILED_CONTENT, copy_quad->material);
  EXPECT_EQ(resource_id, copy_quad->resource_id);
  EXPECT_EQ(tex_coord_rect, copy_quad->tex_coord_rect);
  EXPECT_EQ(texture_size, copy_quad->texture_size);
  EXPECT_EQ(swizzle_contents, copy_quad->swizzle_contents);
}

TEST(DrawQuadTest, CopyYUVVideoDrawQuad) {
  gfx::Rect opaque_rect(3, 7, 10, 12);
  gfx::SizeF tex_scale(0.75f, 0.5f);
  VideoLayerImpl::FramePlane y_plane;
  y_plane.resource_id = 45;
  y_plane.size = gfx::Size(34, 23);
  y_plane.format = 8;
  VideoLayerImpl::FramePlane u_plane;
  u_plane.resource_id = 532;
  u_plane.size = gfx::Size(134, 16);
  u_plane.format = 2;
  VideoLayerImpl::FramePlane v_plane;
  v_plane.resource_id = 4;
  v_plane.size = gfx::Size(456, 486);
  v_plane.format = 46;
  CREATE_SHARED_STATE();

  CREATE_QUAD_5_NEW(
      YUVVideoDrawQuad, opaque_rect, tex_scale, y_plane, u_plane, v_plane);
  EXPECT_EQ(DrawQuad::YUV_VIDEO_CONTENT, copy_quad->material);
  EXPECT_RECT_EQ(opaque_rect, copy_quad->opaque_rect);
  EXPECT_EQ(tex_scale, copy_quad->tex_scale);
  EXPECT_EQ(y_plane.resource_id, copy_quad->y_plane.resource_id);
  EXPECT_EQ(y_plane.size, copy_quad->y_plane.size);
  EXPECT_EQ(y_plane.format, copy_quad->y_plane.format);
  EXPECT_EQ(u_plane.resource_id, copy_quad->u_plane.resource_id);
  EXPECT_EQ(u_plane.size, copy_quad->u_plane.size);
  EXPECT_EQ(u_plane.format, copy_quad->u_plane.format);
  EXPECT_EQ(v_plane.resource_id, copy_quad->v_plane.resource_id);
  EXPECT_EQ(v_plane.size, copy_quad->v_plane.size);
  EXPECT_EQ(v_plane.format, copy_quad->v_plane.format);

  CREATE_QUAD_4_ALL(YUVVideoDrawQuad, tex_scale, y_plane, u_plane, v_plane);
  EXPECT_EQ(DrawQuad::YUV_VIDEO_CONTENT, copy_quad->material);
  EXPECT_EQ(tex_scale, copy_quad->tex_scale);
  EXPECT_EQ(y_plane.resource_id, copy_quad->y_plane.resource_id);
  EXPECT_EQ(y_plane.size, copy_quad->y_plane.size);
  EXPECT_EQ(y_plane.format, copy_quad->y_plane.format);
  EXPECT_EQ(u_plane.resource_id, copy_quad->u_plane.resource_id);
  EXPECT_EQ(u_plane.size, copy_quad->u_plane.size);
  EXPECT_EQ(u_plane.format, copy_quad->u_plane.format);
  EXPECT_EQ(v_plane.resource_id, copy_quad->v_plane.resource_id);
  EXPECT_EQ(v_plane.size, copy_quad->v_plane.size);
  EXPECT_EQ(v_plane.format, copy_quad->v_plane.format);
}

TEST(DrawQuadTest, CopyPictureDrawQuad) {
  gfx::Rect opaque_rect(33, 44, 22, 33);
  gfx::RectF tex_coord_rect(31.f, 12.f, 54.f, 20.f);
  gfx::Size texture_size(85, 32);
  bool swizzle_contents = true;
  gfx::Rect content_rect(30, 40, 20, 30);
  float contents_scale = 3.141592f;
  scoped_refptr<PicturePileImpl> picture_pile = PicturePileImpl::Create(false);
  CREATE_SHARED_STATE();

  CREATE_QUAD_7_NEW(PictureDrawQuad,
                    opaque_rect,
                    tex_coord_rect,
                    texture_size,
                    swizzle_contents,
                    content_rect,
                    contents_scale,
                    picture_pile);
  EXPECT_EQ(DrawQuad::PICTURE_CONTENT, copy_quad->material);
  EXPECT_RECT_EQ(opaque_rect, copy_quad->opaque_rect);
  EXPECT_EQ(tex_coord_rect, copy_quad->tex_coord_rect);
  EXPECT_EQ(texture_size, copy_quad->texture_size);
  EXPECT_EQ(swizzle_contents, copy_quad->swizzle_contents);
  EXPECT_RECT_EQ(content_rect, copy_quad->content_rect);
  EXPECT_EQ(contents_scale, copy_quad->contents_scale);
  EXPECT_EQ(picture_pile, copy_quad->picture_pile);

  CREATE_QUAD_6_ALL(PictureDrawQuad,
                    tex_coord_rect,
                    texture_size,
                    swizzle_contents,
                    content_rect,
                    contents_scale,
                    picture_pile);
  EXPECT_EQ(DrawQuad::PICTURE_CONTENT, copy_quad->material);
  EXPECT_EQ(tex_coord_rect, copy_quad->tex_coord_rect);
  EXPECT_EQ(texture_size, copy_quad->texture_size);
  EXPECT_EQ(swizzle_contents, copy_quad->swizzle_contents);
  EXPECT_RECT_EQ(content_rect, copy_quad->content_rect);
  EXPECT_EQ(contents_scale, copy_quad->contents_scale);
  EXPECT_EQ(picture_pile, copy_quad->picture_pile);
}

class DrawQuadIteratorTest : public testing::Test {
 protected:
  ResourceProvider::ResourceId IncrementResourceId(
      ResourceProvider::ResourceId id) {
    ++num_resources_;
    return id + 1;
  }

  int IterateAndCount(DrawQuad* quad) {
    num_resources_ = 0;
    quad->IterateResources(base::Bind(
        &DrawQuadIteratorTest::IncrementResourceId, base::Unretained(this)));
    return num_resources_;
  }

 private:
  int num_resources_;
};

TEST_F(DrawQuadIteratorTest, CheckerboardDrawQuad) {
  SkColor color = 0xfabb0011;

  CREATE_SHARED_STATE();
  CREATE_QUAD_1_NEW(CheckerboardDrawQuad, color);
  EXPECT_EQ(0, IterateAndCount(quad_new.get()));
}

TEST_F(DrawQuadIteratorTest, DebugBorderDrawQuad) {
  SkColor color = 0xfabb0011;
  int width = 99;

  CREATE_SHARED_STATE();
  CREATE_QUAD_2_NEW(DebugBorderDrawQuad, color, width);
  EXPECT_EQ(0, IterateAndCount(quad_new.get()));
}

TEST_F(DrawQuadIteratorTest, IOSurfaceDrawQuad) {
  gfx::Rect opaque_rect(3, 7, 10, 12);
  gfx::Size size(58, 95);
  unsigned texture_id = 72;
  IOSurfaceDrawQuad::Orientation orientation = IOSurfaceDrawQuad::UNFLIPPED;

  CREATE_SHARED_STATE();
  CREATE_QUAD_4_NEW(
      IOSurfaceDrawQuad, opaque_rect, size, texture_id, orientation);
  EXPECT_EQ(0, IterateAndCount(quad_new.get()));
}

TEST_F(DrawQuadIteratorTest, RenderPassDrawQuad) {
  RenderPass::Id render_pass_id(22, 64);
  bool is_replica = true;
  ResourceProvider::ResourceId mask_resource_id = 78;
  gfx::Rect contents_changed_since_last_frame(42, 11, 74, 24);
  gfx::RectF mask_u_v_rect(-45.f, -21.f, 33.f, 19.f);
  WebKit::WebFilterOperations filters;
  filters.append(WebKit::WebFilterOperation::createBlurFilter(1.f));
  WebKit::WebFilterOperations background_filters;
  background_filters.append(
      WebKit::WebFilterOperation::createGrayscaleFilter(1.f));
  skia::RefPtr<SkImageFilter> filter =
      skia::AdoptRef(new SkBlurImageFilter(SK_Scalar1, SK_Scalar1));

  RenderPass::Id copied_render_pass_id(235, 11);

  CREATE_SHARED_STATE();
  CREATE_QUAD_8_NEW_1(RenderPassDrawQuad,
                      render_pass_id,
                      is_replica,
                      mask_resource_id,
                      contents_changed_since_last_frame,
                      mask_u_v_rect,
                      filters,
                      filter,
                      background_filters,
                      copied_render_pass_id);
  EXPECT_EQ(mask_resource_id, quad_new->mask_resource_id);
  EXPECT_EQ(1, IterateAndCount(quad_new.get()));
  EXPECT_EQ(mask_resource_id + 1, quad_new->mask_resource_id);
}

TEST_F(DrawQuadIteratorTest, SolidColorDrawQuad) {
  SkColor color = 0x49494949;

  CREATE_SHARED_STATE();
  CREATE_QUAD_1_NEW(SolidColorDrawQuad, color);
  EXPECT_EQ(0, IterateAndCount(quad_new.get()));
}

TEST_F(DrawQuadIteratorTest, StreamVideoDrawQuad) {
  gfx::Rect opaque_rect(3, 7, 10, 12);
  unsigned texture_id = 64;
  gfx::Transform matrix = gfx::Transform(0.5, 0.25, 1, 0.75, 0, 1);

  CREATE_SHARED_STATE();
  CREATE_QUAD_3_NEW(StreamVideoDrawQuad, opaque_rect, texture_id, matrix);
  EXPECT_EQ(0, IterateAndCount(quad_new.get()));
}

TEST_F(DrawQuadIteratorTest, TextureDrawQuad) {
  gfx::Rect opaque_rect(3, 7, 10, 12);
  unsigned resource_id = 82;
  bool premultiplied_alpha = true;
  gfx::PointF uv_top_left(0.5f, 224.f);
  gfx::PointF uv_bottom_right(51.5f, 260.f);
  const float vertex_opacity[] = { 1.0f, 1.0f, 1.0f, 1.0f };
  bool flipped = true;

  CREATE_SHARED_STATE();
  CREATE_QUAD_7_NEW(TextureDrawQuad,
                    opaque_rect,
                    resource_id,
                    premultiplied_alpha,
                    uv_top_left,
                    uv_bottom_right,
                    vertex_opacity,
                    flipped);
  EXPECT_EQ(resource_id, quad_new->resource_id);
  EXPECT_EQ(1, IterateAndCount(quad_new.get()));
  EXPECT_EQ(resource_id + 1, quad_new->resource_id);
}

TEST_F(DrawQuadIteratorTest, TileDrawQuad) {
  gfx::Rect opaque_rect(33, 44, 22, 33);
  unsigned resource_id = 104;
  gfx::RectF tex_coord_rect(31.f, 12.f, 54.f, 20.f);
  gfx::Size texture_size(85, 32);
  bool swizzle_contents = true;

  CREATE_SHARED_STATE();
  CREATE_QUAD_5_NEW(TileDrawQuad,
                    opaque_rect,
                    resource_id,
                    tex_coord_rect,
                    texture_size,
                    swizzle_contents);
  EXPECT_EQ(resource_id, quad_new->resource_id);
  EXPECT_EQ(1, IterateAndCount(quad_new.get()));
  EXPECT_EQ(resource_id + 1, quad_new->resource_id);
}

TEST_F(DrawQuadIteratorTest, YUVVideoDrawQuad) {
  gfx::Rect opaque_rect(3, 7, 10, 12);
  gfx::SizeF tex_scale(0.75f, 0.5f);
  VideoLayerImpl::FramePlane y_plane;
  y_plane.resource_id = 45;
  y_plane.size = gfx::Size(34, 23);
  y_plane.format = 8;
  VideoLayerImpl::FramePlane u_plane;
  u_plane.resource_id = 532;
  u_plane.size = gfx::Size(134, 16);
  u_plane.format = 2;
  VideoLayerImpl::FramePlane v_plane;
  v_plane.resource_id = 4;
  v_plane.size = gfx::Size(456, 486);
  v_plane.format = 46;

  CREATE_SHARED_STATE();
  CREATE_QUAD_5_NEW(
      YUVVideoDrawQuad, opaque_rect, tex_scale, y_plane, u_plane, v_plane);
  EXPECT_EQ(DrawQuad::YUV_VIDEO_CONTENT, copy_quad->material);
  EXPECT_EQ(y_plane.resource_id, quad_new->y_plane.resource_id);
  EXPECT_EQ(u_plane.resource_id, quad_new->u_plane.resource_id);
  EXPECT_EQ(v_plane.resource_id, quad_new->v_plane.resource_id);
  EXPECT_EQ(3, IterateAndCount(quad_new.get()));
  EXPECT_EQ(y_plane.resource_id + 1, quad_new->y_plane.resource_id);
  EXPECT_EQ(u_plane.resource_id + 1, quad_new->u_plane.resource_id);
  EXPECT_EQ(v_plane.resource_id + 1, quad_new->v_plane.resource_id);
}

TEST_F(DrawQuadIteratorTest, PictureDrawQuad) {
  gfx::Rect opaque_rect(33, 44, 22, 33);
  gfx::RectF tex_coord_rect(31.f, 12.f, 54.f, 20.f);
  gfx::Size texture_size(85, 32);
  bool swizzle_contents = true;
  gfx::Rect content_rect(30, 40, 20, 30);
  float contents_scale = 3.141592f;
  scoped_refptr<PicturePileImpl> picture_pile = PicturePileImpl::Create(false);

  CREATE_SHARED_STATE();
  CREATE_QUAD_7_NEW(PictureDrawQuad,
                    opaque_rect,
                    tex_coord_rect,
                    texture_size,
                    swizzle_contents,
                    content_rect,
                    contents_scale,
                    picture_pile);
  EXPECT_EQ(0, IterateAndCount(quad_new.get()));
}

}  // namespace
}  // namespace cc
