// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/controller.h"

#include "base/trace_event/trace_event.h"
#include "chrome/browser/vr/controller_mesh.h"
#include "chrome/browser/vr/ui_element_renderer.h"
#include "chrome/browser/vr/vr_gl_util.h"

namespace vr {

namespace {

// clang-format off
static constexpr char const* kVertexShader = SHADER(
  precision mediump float;
  uniform mat4 u_ModelViewProjMatrix;
  attribute vec4 a_Position;
  attribute vec2 a_TexCoordinate;
  varying vec2 v_TexCoordinate;

  void main() {
    v_TexCoordinate = a_TexCoordinate;
    gl_Position = u_ModelViewProjMatrix * a_Position;
  }
);

static constexpr char const* kFragmentShader = SHADER(
    precision mediump float;
    uniform sampler2D u_texture;
  varying vec2 v_TexCoordinate;
  uniform mediump float u_Opacity;

  void main() {
    lowp vec4 texture_color = texture2D(u_texture, v_TexCoordinate);
    gl_FragColor = texture_color * u_Opacity;
  }
);
// clang-format on

}  // namespace

Controller::Controller() {
  set_name(kController);
  set_hit_testable(false);
  SetVisible(true);
}

Controller::~Controller() = default;

void Controller::Render(UiElementRenderer* renderer,
                        const gfx::Transform& model_view_proj_matrix) const {
  ControllerMesh::State state;
  if (touchpad_button_pressed_) {
    state = ControllerMesh::TOUCHPAD;
  } else if (app_button_pressed_) {
    state = ControllerMesh::APP;
  } else if (home_button_pressed_) {
    state = ControllerMesh::SYSTEM;
  } else {
    state = ControllerMesh::IDLE;
  }

  renderer->DrawController(state, computed_opacity(), model_view_proj_matrix);
}

gfx::Transform Controller::LocalTransform() const {
  return local_transform_;
}

Controller::Renderer::Renderer()
    : BaseRenderer(kVertexShader, kFragmentShader),
      texture_handles_(ControllerMesh::STATE_COUNT) {
  model_view_proj_matrix_handle_ =
      glGetUniformLocation(program_handle_, "u_ModelViewProjMatrix");
  tex_coord_handle_ = glGetAttribLocation(program_handle_, "a_TexCoordinate");
  texture_handle_ = glGetUniformLocation(program_handle_, "u_Texture");
  opacity_handle_ = glGetUniformLocation(program_handle_, "u_Opacity");
}

Controller::Renderer::~Renderer() = default;

void Controller::Renderer::SetUp(std::unique_ptr<ControllerMesh> model) {
  TRACE_EVENT0("gpu", "ControllerRenderer::SetUp");
  glGenBuffersARB(1, &indices_buffer_);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indices_buffer_);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, model->IndicesBufferSize(),
               model->IndicesBuffer(), GL_STATIC_DRAW);

  glGenBuffersARB(1, &vertex_buffer_);
  glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer_);
  glBufferData(GL_ARRAY_BUFFER, model->ElementsBufferSize(),
               model->ElementsBuffer(), GL_STATIC_DRAW);

  glGenTextures(ControllerMesh::STATE_COUNT, texture_handles_.data());
  for (int i = 0; i < ControllerMesh::STATE_COUNT; i++) {
    sk_sp<SkImage> texture = model->GetTexture(i);
    SkPixmap pixmap;
    if (!texture->peekPixels(&pixmap)) {
      LOG(ERROR) << "Failed to read controller texture pixels";
      continue;
    }
    glBindTexture(GL_TEXTURE_2D, texture_handles_[i]);
    SetTexParameters(GL_TEXTURE_2D);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, pixmap.width(), pixmap.height(), 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, pixmap.addr());
  }

  const vr::gltf::Accessor* accessor = model->PositionAccessor();
  position_components_ = vr::gltf::GetTypeComponents(accessor->type);
  position_type_ = accessor->component_type;
  position_stride_ = accessor->byte_stride;
  position_offset_ = VOID_OFFSET(accessor->byte_offset);

  accessor = model->TextureCoordinateAccessor();
  tex_coord_components_ = vr::gltf::GetTypeComponents(accessor->type);
  tex_coord_type_ = accessor->component_type;
  tex_coord_stride_ = accessor->byte_stride;
  tex_coord_offset_ = VOID_OFFSET(accessor->byte_offset);

  draw_mode_ = model->DrawMode();
  accessor = model->IndicesAccessor();
  indices_count_ = accessor->count;
  indices_type_ = accessor->component_type;
  indices_offset_ = VOID_OFFSET(accessor->byte_offset);
  setup_ = true;
}

void Controller::Renderer::Draw(ControllerMesh::State state,
                                float opacity,
                                const gfx::Transform& view_proj_matrix) {
  glUseProgram(program_handle_);

  glUniform1f(opacity_handle_, opacity);

  glUniformMatrix4fv(model_view_proj_matrix_handle_, 1, false,
                     MatrixToGLArray(view_proj_matrix).data());

  glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer_);

  glVertexAttribPointer(position_handle_, position_components_, position_type_,
                        GL_FALSE, position_stride_, position_offset_);
  glEnableVertexAttribArray(position_handle_);

  glVertexAttribPointer(tex_coord_handle_, tex_coord_components_,
                        tex_coord_type_, GL_FALSE, tex_coord_stride_,
                        tex_coord_offset_);
  glEnableVertexAttribArray(tex_coord_handle_);

  glEnable(GL_BLEND);
  glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indices_buffer_);

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, texture_handles_[state]);
  glUniform1i(texture_handle_, 0);

  glDrawElements(draw_mode_, indices_count_, indices_type_, indices_offset_);
}

}  // namespace vr
