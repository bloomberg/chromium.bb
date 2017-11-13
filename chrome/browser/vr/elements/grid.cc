// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/grid.h"

#include "chrome/browser/vr/target_property.h"
#include "chrome/browser/vr/ui_element_renderer.h"
#include "chrome/browser/vr/vr_gl_util.h"

namespace vr {

namespace {

// clang-format off
static constexpr char const* kVertexShader = SHADER(
  precision mediump float;
  uniform mat4 u_ModelViewProjMatrix;
  uniform float u_SceneRadius;
  attribute vec4 a_Position;
  varying vec2 v_GridPosition;

  void main() {
    v_GridPosition = a_Position.xy / u_SceneRadius;
    gl_Position = u_ModelViewProjMatrix * a_Position;
  }
);

static constexpr char const* kFragmentShader = SHADER(
  precision highp float;
  varying vec2 v_GridPosition;
  uniform vec4 u_CenterColor;
  uniform vec4 u_EdgeColor;
  uniform vec4 u_GridColor;
  uniform mediump float u_Opacity;
  uniform float u_LinesCount;

  void main() {
    float edgeColorWeight = clamp(length(v_GridPosition), 0.0, 1.0);
    float centerColorWeight = 1.0 - edgeColorWeight;
    vec4 bg_color = u_CenterColor * centerColorWeight +
        u_EdgeColor * edgeColorWeight;
    bg_color = vec4(bg_color.xyz * bg_color.w, bg_color.w);
    float linesCountF = u_LinesCount * 0.5;
    float pos_x = abs(v_GridPosition.x) * linesCountF;
    float pos_y = abs(v_GridPosition.y) * linesCountF;
    float diff_x = abs(pos_x - floor(pos_x + 0.5));
    float diff_y = abs(pos_y - floor(pos_y + 0.5));
    float diff = min(diff_x, diff_y);
    if (diff < 0.01) {
      float opacity = 1.0 - diff / 0.01;
      opacity = opacity * opacity * centerColorWeight * u_GridColor.w;
      vec3 grid_color = u_GridColor.xyz * opacity;
      gl_FragColor = vec4(
          grid_color.xyz + bg_color.xyz * (1.0 - opacity),
          opacity + bg_color.w * (1.0 - opacity));
    } else {
      // Add some noise to prevent banding artifacts in the gradient.
      float n =
          (fract(dot(v_GridPosition.xy, vec2(12345.67, 456.7))) - 0.5) / 255.0;
      gl_FragColor = bg_color + n;
    }
  }
);
// clang-format on

static constexpr float kSceneRadius = 0.5f;

}  // namespace

Grid::Grid() {}
Grid::~Grid() {}

void Grid::SetGridColor(SkColor color) {
  animation_player().TransitionColorTo(last_frame_time(), GRID_COLOR,
                                       grid_color_, color);
}

void Grid::NotifyClientColorAnimated(SkColor color,
                                     int target_property_id,
                                     cc::Animation* animation) {
  if (target_property_id == GRID_COLOR) {
    grid_color_ = color;
  } else {
    Rect::NotifyClientColorAnimated(color, target_property_id, animation);
  }
}

void Grid::Render(UiElementRenderer* renderer,
                  const gfx::Transform& model_view_proj_matrix) const {
  renderer->DrawGradientGridQuad(model_view_proj_matrix, edge_color(),
                                 center_color(), grid_color_, gridline_count_,
                                 computed_opacity());
}

Grid::Renderer::~Renderer() = default;

Grid::Renderer::Renderer() : BaseQuadRenderer(kVertexShader, kFragmentShader) {
  model_view_proj_matrix_handle_ =
      glGetUniformLocation(program_handle_, "u_ModelViewProjMatrix");
  scene_radius_handle_ = glGetUniformLocation(program_handle_, "u_SceneRadius");
  center_color_handle_ = glGetUniformLocation(program_handle_, "u_CenterColor");
  edge_color_handle_ = glGetUniformLocation(program_handle_, "u_EdgeColor");
  grid_color_handle_ = glGetUniformLocation(program_handle_, "u_GridColor");
  opacity_handle_ = glGetUniformLocation(program_handle_, "u_Opacity");
  lines_count_handle_ = glGetUniformLocation(program_handle_, "u_LinesCount");
}

void Grid::Renderer::Draw(const gfx::Transform& model_view_proj_matrix,
                          SkColor edge_color,
                          SkColor center_color,
                          SkColor grid_color,
                          int gridline_count,
                          float opacity) {
  PrepareToDraw(model_view_proj_matrix_handle_, model_view_proj_matrix);

  // Tell shader the grid size so that it can calculate the fading.
  glUniform1f(scene_radius_handle_, kSceneRadius);
  glUniform1f(lines_count_handle_, static_cast<float>(gridline_count));

  // Set the edge color to the fog color so that it seems to fade out.
  SetColorUniform(edge_color_handle_, edge_color);
  SetColorUniform(center_color_handle_, center_color);
  SetColorUniform(grid_color_handle_, grid_color);
  glUniform1f(opacity_handle_, opacity);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer_);
  glDrawElements(GL_TRIANGLES, BaseQuadRenderer::NumQuadIndices(),
                 GL_UNSIGNED_SHORT, 0);

  glDisableVertexAttribArray(position_handle_);
}

}  // namespace vr
