// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_SURFACE_AGGREGATOR_TEST_HELPERS_H_
#define CC_TEST_SURFACE_AGGREGATOR_TEST_HELPERS_H_

#include <stddef.h>

#include <vector>

#include "cc/quads/draw_quad.h"
#include "cc/surfaces/surface_id.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/size.h"

namespace cc {

class RenderPass;
class TestRenderPass;

typedef std::vector<std::unique_ptr<RenderPass>> RenderPassList;

namespace test {

struct Quad {
  static Quad SolidColorQuad(SkColor color) {
    Quad quad;
    quad.material = DrawQuad::SOLID_COLOR;
    quad.color = color;
    return quad;
  }

  static Quad SurfaceQuad(const SurfaceId& primary_surface_id,
                          const SurfaceId& fallback_surface_id,
                          float opacity) {
    Quad quad;
    quad.material = DrawQuad::SURFACE_CONTENT;
    quad.opacity = opacity;
    quad.primary_surface_id = primary_surface_id;
    quad.fallback_surface_id = fallback_surface_id;
    return quad;
  }

  static Quad RenderPassQuad(int id) {
    Quad quad;
    quad.material = DrawQuad::RENDER_PASS;
    quad.render_pass_id = id;
    return quad;
  }

  DrawQuad::Material material;
  // Set when material==DrawQuad::SURFACE_CONTENT.
  SurfaceId primary_surface_id;
  SurfaceId fallback_surface_id;
  float opacity;
  // Set when material==DrawQuad::SOLID_COLOR.
  SkColor color;
  // Set when material==DrawQuad::RENDER_PASS.
  int render_pass_id;

 private:
  Quad() : material(DrawQuad::INVALID), opacity(1.f), color(SK_ColorWHITE) {}
};

struct Pass {
  Pass(Quad* quads, size_t quad_count, int id)
      : quads(quads), quad_count(quad_count), id(id) {}
  Pass(Quad* quads, size_t quad_count) : quads(quads), quad_count(quad_count) {}

  Quad* quads;
  size_t quad_count;
  int id = 1;
};

void AddQuadInPass(TestRenderPass* pass, Quad desc);

void AddPasses(RenderPassList* pass_list,
               const gfx::Rect& output_rect,
               Pass* passes,
               size_t pass_count);

void TestQuadMatchesExpectations(Quad expected_quad, const DrawQuad* quad);

void TestPassMatchesExpectations(Pass expected_pass, const RenderPass* pass);

void TestPassesMatchExpectations(Pass* expected_passes,
                                 size_t expected_pass_count,
                                 const RenderPassList* passes);

}  // namespace test
}  // namespace cc

#endif  // CC_TEST_SURFACE_AGGREGATOR_TEST_HELPERS_H_
