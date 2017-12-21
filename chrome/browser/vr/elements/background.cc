// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/background.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/vr/skia_surface_provider.h"
#include "chrome/browser/vr/ui_element_renderer.h"
#include "chrome/browser/vr/vr_gl_util.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkSurface.h"

namespace vr {

namespace {

// On the background sphere, this is the number of faces between poles.
int kSteps = 20;

// clang-format off
constexpr char const* kVertexShader = SHADER(
  precision mediump float;
  uniform mat4 u_ModelViewProjMatrix;
  attribute vec2 a_TexCoordinate;
  varying vec2 v_TexCoordinate;

  void main() {
    vec4 sphereVertex;
    float theta = a_TexCoordinate.x * 6.28319;
    float phi = a_TexCoordinate.y * 3.14159;

    // Place the background at 1000 m, an arbitrary large distance. This
    // nullifies the translational portion of eye transforms, which is what we
    // want for ODS backgrounds.
    sphereVertex.x = 1000. * -cos(theta) * sin(phi);
    sphereVertex.y = 1000. * cos(phi);
    sphereVertex.z = 1000. * -sin(theta) * sin(phi);
    sphereVertex.w = 1.0;

    gl_Position = u_ModelViewProjMatrix * sphereVertex;
    v_TexCoordinate = a_TexCoordinate;
  }
);

constexpr char const* kFragmentShader = SHADER(
  precision mediump float;
  uniform sampler2D u_Texture;
  varying vec2 v_TexCoordinate;

  void main() {
    gl_FragColor = texture2D(u_Texture, v_TexCoordinate);
  }
);
/* clang-format on */

}  // namespace

Background::Background() {
  set_hit_testable(false);
}
Background::~Background() = default;

void Background::Render(UiElementRenderer* renderer,
                        const CameraModel& model) const {
  if (texture_handle_ != 0) {
    renderer->DrawBackground(model.view_proj_matrix * world_space_transform(),
                             texture_handle_);
  }
}

void Background::Initialize(SkiaSurfaceProvider* provider) {
  provider_ = provider;
  if (initialization_bitmap_)
    CreateTexture();
}

void Background::SetImage(std::unique_ptr<SkBitmap> bitmap) {
  initialization_bitmap_ = std::move(bitmap);
  if (provider_)
    CreateTexture();
}

void Background::CreateTexture() {
  DCHECK(provider_);
  DCHECK(initialization_bitmap_);

  // The bitmap data is thrown away after the texture is generated.
  auto bitmap = std::move(initialization_bitmap_);

  surface_ = provider_->MakeSurface({bitmap->width(), bitmap->height()});
  DCHECK(surface_.get());
  SkCanvas* canvas = surface_->getCanvas();
  canvas->drawBitmap(*bitmap, 0, 0);
  texture_handle_ = provider_->FlushSurface(surface_.get(), texture_handle_);
}

Background::Renderer::Renderer()
    : BaseRenderer(kVertexShader, kFragmentShader) {
  model_view_proj_matrix_handle_ =
      glGetUniformLocation(program_handle_, "u_ModelViewProjMatrix");
  position_handle_ = glGetAttribLocation(program_handle_, "a_TexCoordinate");
  tex_uniform_handle_ = glGetUniformLocation(program_handle_, "u_Texture");

  // Build the set of texture points representing the sphere.
  std::vector<float> vertices;
  for (int x = 0; x <= 2 * kSteps; x++) {
    for (int y = 0; y <= kSteps; y++) {
      vertices.push_back(static_cast<float>(x) / (kSteps * 2));
      vertices.push_back(static_cast<float>(y) / kSteps);
    }
  }
  std::vector<GLushort> indices;
  int y_stride = kSteps + 1;
  for (int x = 0; x < 2 * kSteps; x++) {
    for (int y = 0; y < kSteps; y++) {
      GLushort p0 = x * y_stride + y;
      GLushort p1 = x * y_stride + y + 1;
      GLushort p2 = (x + 1) * y_stride + y;
      GLushort p3 = (x + 1) * y_stride + y + 1;
      indices.push_back(p0);
      indices.push_back(p1);
      indices.push_back(p3);
      indices.push_back(p0);
      indices.push_back(p3);
      indices.push_back(p2);
    }
  }

  GLuint buffers[2];
  glGenBuffersARB(2, buffers);
  vertex_buffer_ = buffers[0];
  index_buffer_ = buffers[1];
  index_count_ = indices.size();

  glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer_);
  glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float),
               vertices.data(), GL_STATIC_DRAW);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer_);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(GLushort),
               indices.data(), GL_STATIC_DRAW);
}

Background::Renderer::~Renderer() = default;

void Background::Renderer::Draw(const gfx::Transform& view_proj_matrix,
                                int texture_data_handle) {
  glUseProgram(program_handle_);

  // Pass in model view project matrix.
  glUniformMatrix4fv(model_view_proj_matrix_handle_, 1, false,
                     MatrixToGLArray(view_proj_matrix).data());

  // Set up vertex attributes.
  glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer_);
  glVertexAttribPointer(position_handle_, 2, GL_FLOAT, GL_FALSE, 0,
                        VOID_OFFSET(0));
  glEnableVertexAttribArray(position_handle_);

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, texture_data_handle);

  // Set up uniforms.
  glUniform1i(tex_uniform_handle_, 0);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer_);
  glDrawElements(GL_TRIANGLES, index_count_, GL_UNSIGNED_SHORT, 0);

  glDisableVertexAttribArray(position_handle_);
}

}  // namespace vr
