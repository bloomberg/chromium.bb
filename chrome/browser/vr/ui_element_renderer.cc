// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/ui_element_renderer.h"

#include <math.h>
#include <algorithm>
#include <string>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/vr/renderers/base_quad_renderer.h"
#include "chrome/browser/vr/renderers/base_renderer.h"
#include "chrome/browser/vr/renderers/external_textured_quad_renderer.h"
#include "chrome/browser/vr/renderers/gradient_quad_renderer.h"
#include "chrome/browser/vr/renderers/textured_quad_renderer.h"
#include "chrome/browser/vr/renderers/web_vr_renderer.h"
#include "chrome/browser/vr/vr_gl_util.h"
#include "ui/gfx/geometry/point3_f.h"
#include "ui/gfx/geometry/vector3d_f.h"

namespace vr {

UiElementRenderer::UiElementRenderer() : UiElementRenderer(true) {}

UiElementRenderer::UiElementRenderer(bool use_gl) {
  if (!use_gl)
    return;
  Init();
  BaseQuadRenderer::CreateBuffers();
  TexturedQuadRenderer::CreateBuffers();
}

UiElementRenderer::~UiElementRenderer() = default;

void UiElementRenderer::Init() {
  external_textured_quad_renderer_ =
      base::MakeUnique<ExternalTexturedQuadRenderer>();
  textured_quad_renderer_ = base::MakeUnique<TexturedQuadRenderer>();
  gradient_quad_renderer_ = base::MakeUnique<GradientQuadRenderer>();
  webvr_renderer_ = base::MakeUnique<WebVrRenderer>();
  reticle_renderer_ = base::MakeUnique<Reticle::Renderer>();
  laser_renderer_ = base::MakeUnique<Laser::Renderer>();
  controller_renderer_ = base::MakeUnique<Controller::Renderer>();
  gradient_grid_renderer_ = base::MakeUnique<Grid::Renderer>();
}

void UiElementRenderer::DrawTexturedQuad(
    int texture_data_handle,
    TextureLocation texture_location,
    const gfx::Transform& model_view_proj_matrix,
    const gfx::RectF& copy_rect,
    float opacity,
    gfx::SizeF element_size,
    float corner_radius) {
  // TODO(vollick): handle drawing this degenerate situation crbug.com/768922
  if (corner_radius * 2.0 > element_size.width() ||
      corner_radius * 2.0 > element_size.height()) {
    return;
  }
  TexturedQuadRenderer* renderer = texture_location == kTextureLocationExternal
                                       ? external_textured_quad_renderer_.get()
                                       : textured_quad_renderer_.get();
  FlushIfNecessary(renderer);
  renderer->AddQuad(texture_data_handle, model_view_proj_matrix, copy_rect,
                    opacity, element_size, corner_radius);
}

void UiElementRenderer::DrawGradientQuad(
    const gfx::Transform& model_view_proj_matrix,
    const SkColor edge_color,
    const SkColor center_color,
    float opacity,
    gfx::SizeF element_size,
    float corner_radius) {
  FlushIfNecessary(gradient_quad_renderer_.get());
  gradient_quad_renderer_->Draw(model_view_proj_matrix, edge_color,
                                center_color, opacity, element_size,
                                corner_radius);
}

void UiElementRenderer::DrawGradientGridQuad(
    const gfx::Transform& model_view_proj_matrix,
    const SkColor edge_color,
    const SkColor center_color,
    const SkColor grid_color,
    int gridline_count,
    float opacity) {
  FlushIfNecessary(gradient_grid_renderer_.get());
  gradient_grid_renderer_->Draw(model_view_proj_matrix, edge_color,
                                center_color, grid_color, gridline_count,
                                opacity);
}

void UiElementRenderer::DrawController(ControllerMesh::State state,
                                       float opacity,
                                       const gfx::Transform& view_proj_matrix) {
  if (!controller_renderer_->IsSetUp()) {
    return;
  }
  FlushIfNecessary(controller_renderer_.get());
  controller_renderer_->Draw(state, opacity, view_proj_matrix);
}

void UiElementRenderer::DrawLaser(float opacity,
                                  const gfx::Transform& view_proj_matrix) {
  FlushIfNecessary(laser_renderer_.get());
  laser_renderer_->Draw(opacity, view_proj_matrix);
}

void UiElementRenderer::DrawReticle(float opacity,
                                    const gfx::Transform& view_proj_matrix) {
  FlushIfNecessary(reticle_renderer_.get());
  reticle_renderer_->Draw(opacity, view_proj_matrix);
}

void UiElementRenderer::DrawWebVr(int texture_data_handle) {
  FlushIfNecessary(webvr_renderer_.get());
  webvr_renderer_->Draw(texture_data_handle);
}

void UiElementRenderer::Flush() {
  textured_quad_renderer_->Flush();
  external_textured_quad_renderer_->Flush();
  last_renderer_ = nullptr;
}

void UiElementRenderer::SetUpController(std::unique_ptr<ControllerMesh> mesh) {
  controller_renderer_->SetUp(std::move(mesh));
}

void UiElementRenderer::FlushIfNecessary(BaseRenderer* renderer) {
  if (last_renderer_ && renderer != last_renderer_) {
    last_renderer_->Flush();
  }
  last_renderer_ = renderer;
}

}  // namespace vr
