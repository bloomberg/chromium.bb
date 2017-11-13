// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/renderers/gradient_quad_renderer.h"

#include "chrome/browser/vr/renderers/textured_quad_renderer.h"
#include "chrome/browser/vr/vr_gl_util.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/transform.h"

namespace vr {

namespace {

static constexpr int kPositionDataSize = 2;
static constexpr size_t kPositionDataOffset = 0;
static constexpr int kOffsetScaleDataSize = 2;
static constexpr size_t kOffsetScaleDataOffset = 2 * sizeof(float);
static constexpr int kCornerPositionDataSize = 2;
static constexpr size_t kCornerPositionDataOffset = 4 * sizeof(float);
static constexpr size_t kDataStride = 6 * sizeof(float);

static constexpr size_t kInnerRectOffset = 6 * sizeof(GLushort);

// clang-format off
static constexpr char const* kGradientQuadVertexShader = SHADER(
  precision mediump float;
  uniform mat4 u_ModelViewProjMatrix;
  uniform vec2 u_CornerOffset;
  attribute vec4 a_Position;
  attribute vec2 a_CornerPosition;
  attribute vec2 a_OffsetScale;
  varying vec2 v_CornerPosition;
  varying vec2 v_Position;

  void main() {
    v_CornerPosition = a_CornerPosition;
    vec4 position = vec4(
        a_Position[0] + u_CornerOffset[0] * a_OffsetScale[0],
        a_Position[1] + u_CornerOffset[1] * a_OffsetScale[1],
        a_Position[2],
        a_Position[3]);
    v_Position = position.xy;
    gl_Position = u_ModelViewProjMatrix * position;
  }
);

static constexpr char const* kGradientQuadFragmentShader = SHADER(
  precision highp float;
  varying vec2 v_CornerPosition;
  varying vec2 v_Position;
  uniform mediump float u_Opacity;
  uniform vec4 u_CenterColor;
  uniform vec4 u_EdgeColor;
  void main() {
    // NB: this is on the range [0, 1] and hits its extrema at the horizontal
    // and vertical edges of the quad, regardless of its aspect ratio. If we
    // want to have a true circular gradient, we will need to do some extra
    // math.
    float edge_color_weight = clamp(2.0 * length(v_Position), 0.0, 1.0);
    float center_color_weight = 1.0 - edge_color_weight;
    vec4 color = u_CenterColor * center_color_weight + u_EdgeColor *
        edge_color_weight;
    float mask = 1.0 - step(1.0, length(v_CornerPosition));
    // Add some noise to prevent banding artifacts in the gradient.
    float n = (fract(dot(v_Position.xy, vec2(12345.67, 456.7))) - 0.5) / 255.0;

    color = color + n;
    color = vec4(color.rgb * color.a, color.a);
    gl_FragColor = color * u_Opacity * mask;
  }
);
// clang-format on

}  // namespace

GradientQuadRenderer::GradientQuadRenderer()
    : BaseRenderer(kGradientQuadVertexShader, kGradientQuadFragmentShader) {
  model_view_proj_matrix_handle_ =
      glGetUniformLocation(program_handle_, "u_ModelViewProjMatrix");
  corner_offset_handle_ =
      glGetUniformLocation(program_handle_, "u_CornerOffset");
  corner_position_handle_ =
      glGetAttribLocation(program_handle_, "a_CornerPosition");
  offset_scale_handle_ = glGetAttribLocation(program_handle_, "a_OffsetScale");
  opacity_handle_ = glGetUniformLocation(program_handle_, "u_Opacity");
  center_color_handle_ = glGetUniformLocation(program_handle_, "u_CenterColor");
  edge_color_handle_ = glGetUniformLocation(program_handle_, "u_EdgeColor");
}

GradientQuadRenderer::~GradientQuadRenderer() = default;

void GradientQuadRenderer::Draw(const gfx::Transform& model_view_proj_matrix,
                                SkColor edge_color,
                                SkColor center_color,
                                float opacity,
                                const gfx::SizeF& element_size,
                                float corner_radius) {
  glUseProgram(program_handle_);
  glEnable(GL_BLEND);
  glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

  glBindBuffer(GL_ARRAY_BUFFER, TexturedQuadRenderer::VertexBuffer());
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, TexturedQuadRenderer::IndexBuffer());

  // Set up position attribute.
  glVertexAttribPointer(position_handle_, kPositionDataSize, GL_FLOAT, false,
                        kDataStride, VOID_OFFSET(kPositionDataOffset));
  glEnableVertexAttribArray(position_handle_);

  // Set up offset scale attribute.
  glVertexAttribPointer(offset_scale_handle_, kOffsetScaleDataSize, GL_FLOAT,
                        false, kDataStride,
                        VOID_OFFSET(kOffsetScaleDataOffset));
  glEnableVertexAttribArray(offset_scale_handle_);

  // Set up corner position attribute.
  glVertexAttribPointer(corner_position_handle_, kCornerPositionDataSize,
                        GL_FLOAT, false, kDataStride,
                        VOID_OFFSET(kCornerPositionDataOffset));
  glEnableVertexAttribArray(corner_position_handle_);

  if (corner_radius == 0.0f) {
    glUniform2f(corner_offset_handle_, 0.0, 0.0);
  } else {
    glUniform2f(corner_offset_handle_, corner_radius / element_size.width(),
                corner_radius / element_size.height());
  }

  // Set the edge color to the fog color so that it seems to fade out.
  SetColorUniform(edge_color_handle_, edge_color);
  SetColorUniform(center_color_handle_, center_color);
  glUniform1f(opacity_handle_, opacity);

  // Pass in model view project matrix.
  glUniformMatrix4fv(model_view_proj_matrix_handle_, 1, false,
                     MatrixToGLArray(model_view_proj_matrix).data());

  if (corner_radius == 0.0f) {
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT,
                   VOID_OFFSET(kInnerRectOffset));
  } else {
    glDrawElements(GL_TRIANGLES, TexturedQuadRenderer::NumQuadIndices(),
                   GL_UNSIGNED_SHORT, 0);
  }

  glDisableVertexAttribArray(position_handle_);
  glDisableVertexAttribArray(offset_scale_handle_);
  glDisableVertexAttribArray(corner_position_handle_);
}

}  // namespace vr
