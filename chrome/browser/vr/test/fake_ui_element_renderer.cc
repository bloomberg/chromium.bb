// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/test/fake_ui_element_renderer.h"

#include "ui/gfx/geometry/size_f.h"

namespace vr {

FakeUiElementRenderer::FakeUiElementRenderer() : UiElementRenderer(false) {}
FakeUiElementRenderer::~FakeUiElementRenderer() {}

void FakeUiElementRenderer::DrawTexturedQuad(
    int texture_data_handle,
    int overlay_texture_data_handle,
    TextureLocation texture_location,
    const gfx::Transform& view_proj_matrix,
    const gfx::RectF& copy_rect,
    float opacity,
    const gfx::SizeF& element_size,
    float corner_radius) {
  opacity_ = opacity;
  called_ = true;
}

void FakeUiElementRenderer::DrawGradientQuad(
    const gfx::Transform& view_proj_matrix,
    const SkColor edge_color,
    const SkColor center_color,
    float opacity,
    const gfx::SizeF& element_size,
    const CornerRadii& corner_radii) {
  opacity_ = opacity;
  called_ = true;
}

void FakeUiElementRenderer::DrawGradientGridQuad(
    const gfx::Transform& view_proj_matrix,
    const SkColor edge_color,
    const SkColor center_color,
    const SkColor grid_color,
    int gridline_count,
    float opacity) {
  opacity_ = opacity;
  called_ = true;
}

void FakeUiElementRenderer::DrawController(
    float opacity,
    const gfx::Transform& model_view_proj_matrix) {
  opacity_ = opacity;
  called_ = true;
}

void FakeUiElementRenderer::DrawLaser(float opacity,
                                      const gfx::Transform& view_proj_matrix) {
  opacity_ = opacity;
  called_ = true;
}

void FakeUiElementRenderer::DrawReticle(
    float opacity,
    const gfx::Transform& view_proj_matrix) {
  opacity_ = opacity;
  called_ = true;
}

void FakeUiElementRenderer::DrawShadow(
    const gfx::Transform& model_view_proj_matrix,
    const gfx::SizeF& element_size,
    float x_padding,
    float y_padding,
    float y_offset,
    SkColor color,
    float opacity,
    float corner_radius) {
  // We do not verify the opacity used by shadows -- they adjust this at the
  // last moment before calling into the UiElementRenderer.
}

void FakeUiElementRenderer::DrawStars(
    float t,
    const gfx::Transform& model_view_proj_matrix) {}

}  // namespace vr
