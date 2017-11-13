// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_ELEMENTS_CONTROLLER_H_
#define CHROME_BROWSER_VR_ELEMENTS_CONTROLLER_H_

#include <vector>

#include "base/macros.h"
#include "chrome/browser/vr/controller_mesh.h"
#include "chrome/browser/vr/elements/ui_element.h"
#include "chrome/browser/vr/renderers/base_renderer.h"

namespace vr {

// This represents the controller.
class Controller : public UiElement {
 public:
  Controller();
  ~Controller() override;

  void set_touchpad_button_pressed(bool pressed) {
    touchpad_button_pressed_ = pressed;
  }

  void set_app_button_pressed(bool pressed) { app_button_pressed_ = pressed; }

  void set_home_button_pressed(bool pressed) { home_button_pressed_ = pressed; }

  void set_local_transform(const gfx::Transform& transform) {
    local_transform_ = transform;
  }

  class Renderer : public BaseRenderer {
   public:
    Renderer();
    ~Renderer() override;

    void SetUp(std::unique_ptr<ControllerMesh> model);
    void Draw(ControllerMesh::State state,
              float opacity,
              const gfx::Transform& view_proj_matrix);
    bool IsSetUp() const { return setup_; }

   private:
    GLuint model_view_proj_matrix_handle_;
    GLuint tex_coord_handle_;
    GLuint texture_handle_;
    GLuint opacity_handle_;
    GLuint indices_buffer_ = 0;
    GLuint vertex_buffer_ = 0;
    GLint position_components_ = 0;
    GLenum position_type_ = GL_FLOAT;
    GLsizei position_stride_ = 0;
    const GLvoid* position_offset_ = nullptr;
    GLint tex_coord_components_ = 0;
    GLenum tex_coord_type_ = GL_FLOAT;
    GLsizei tex_coord_stride_ = 0;
    const GLvoid* tex_coord_offset_ = nullptr;
    GLenum draw_mode_ = GL_TRIANGLES;
    GLsizei indices_count_ = 0;
    GLenum indices_type_ = GL_INT;
    const GLvoid* indices_offset_ = nullptr;
    std::vector<GLuint> texture_handles_;
    bool setup_ = false;

    DISALLOW_COPY_AND_ASSIGN(Renderer);
  };

 private:
  void Render(UiElementRenderer* renderer,
              const gfx::Transform& model_view_proj_matrix) const final;

  gfx::Transform LocalTransform() const override;

  bool touchpad_button_pressed_ = false;
  bool app_button_pressed_ = false;
  bool home_button_pressed_ = false;
  gfx::Transform local_transform_;

  DISALLOW_COPY_AND_ASSIGN(Controller);
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_ELEMENTS_CONTROLLER_H_
