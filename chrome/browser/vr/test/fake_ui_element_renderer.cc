// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/test/fake_ui_element_renderer.h"

#include "ui/gfx/geometry/size_f.h"

namespace vr {

FakeUiElementRenderer::FakeUiElementRenderer() {}
FakeUiElementRenderer::~FakeUiElementRenderer() {}

void FakeUiElementRenderer::DrawTexturedQuad(
    int texture_data_handle,
    TextureLocation texture_location,
    const gfx::Transform& view_proj_matrix,
    const gfx::RectF& copy_rect,
    float opacity,
    gfx::SizeF element_size,
    float corner_radius) {
  texture_opacity_ = opacity;
}

void FakeUiElementRenderer::DrawGradientQuad(
    const gfx::Transform& view_proj_matrix,
    const SkColor edge_color,
    const SkColor center_color,
    float opacity,
    gfx::SizeF element_size,
    float corner_radius) {
  gradient_opacity_ = opacity;
}

void FakeUiElementRenderer::DrawGradientGridQuad(
    const gfx::Transform& view_proj_matrix,
    const SkColor edge_color,
    const SkColor center_color,
    const SkColor grid_color,
    int gridline_count,
    float opacity) {
  grid_opacity_ = opacity;
}

}  // namespace vr
