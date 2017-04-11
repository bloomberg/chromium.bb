// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_SHELL_VR_SHELL_RENDERER_H_
#define CHROME_BROWSER_ANDROID_VR_SHELL_VR_SHELL_RENDERER_H_

#include <memory>
#include <queue>
#include <vector>

#include "base/macros.h"
#include "chrome/browser/android/vr_shell/vr_controller_model.h"
#include "device/vr/vr_types.h"
#include "ui/gl/gl_bindings.h"

namespace vr_shell {

// TODO(tiborg): set background color through JS API.
constexpr float kFogBrightness = 0.57f;

typedef unsigned int GLuint;

enum ShaderID {
  SHADER_UNRECOGNIZED = 0,
  TEXTURE_QUAD_VERTEX_SHADER,
  TEXTURE_QUAD_FRAGMENT_SHADER,
  WEBVR_VERTEX_SHADER,
  WEBVR_FRAGMENT_SHADER,
  RETICLE_VERTEX_SHADER,
  RETICLE_FRAGMENT_SHADER,
  LASER_VERTEX_SHADER,
  LASER_FRAGMENT_SHADER,
  GRADIENT_QUAD_VERTEX_SHADER,
  GRADIENT_QUAD_FRAGMENT_SHADER,
  GRADIENT_GRID_VERTEX_SHADER,
  GRADIENT_GRID_FRAGMENT_SHADER,
  CONTROLLER_VERTEX_SHADER,
  CONTROLLER_FRAGMENT_SHADER,
  SHADER_ID_MAX
};

struct Vertex3d {
  float x;
  float y;
  float z;
};

struct RectF {
  float x;
  float y;
  float width;
  float height;
};

struct Line3d {
  Vertex3d start;
  Vertex3d end;
};

struct TexturedQuad {
  int texture_data_handle;
  vr::Mat4f view_proj_matrix;
  RectF copy_rect;
  float opacity;
};

class BaseRenderer {
 public:
  virtual ~BaseRenderer();

 protected:
  BaseRenderer(ShaderID vertex_id, ShaderID fragment_id);

  GLuint program_handle_;
  GLuint position_handle_;
  GLuint tex_coord_handle_;

  DISALLOW_COPY_AND_ASSIGN(BaseRenderer);
};

class BaseQuadRenderer : public BaseRenderer {
 public:
  BaseQuadRenderer(ShaderID vertex_id, ShaderID fragment_id);
  ~BaseQuadRenderer() override;

  static void SetVertexBuffer();

 protected:
  void PrepareToDraw(GLuint view_proj_matrix_handle,
                     const vr::Mat4f& view_proj_matrix);

  static GLuint vertex_buffer_;

  DISALLOW_COPY_AND_ASSIGN(BaseQuadRenderer);
};

class TexturedQuadRenderer : public BaseQuadRenderer {
 public:
  TexturedQuadRenderer();
  ~TexturedQuadRenderer() override;

  // Draw the content rect in the texture quad.
  void AddQuad(int texture_data_handle,
               const vr::Mat4f& view_proj_matrix,
               const gfx::RectF& copy_rect,
               float opacity);

  void Flush();

 private:
  GLuint model_view_proj_matrix_handle_;
  GLuint copy_rect_uniform_handle_;
  GLuint tex_uniform_handle_;
  GLuint opacity_handle_;

  std::queue<TexturedQuad> quad_queue_;

  DISALLOW_COPY_AND_ASSIGN(TexturedQuadRenderer);
};

// Renders a page-generated stereo VR view.
class WebVrRenderer : public BaseQuadRenderer {
 public:
  WebVrRenderer();
  ~WebVrRenderer() override;

  void Draw(int texture_handle);

 private:
  GLuint tex_uniform_handle_;

  DISALLOW_COPY_AND_ASSIGN(WebVrRenderer);
};

class ReticleRenderer : public BaseQuadRenderer {
 public:
  ReticleRenderer();
  ~ReticleRenderer() override;

  void Draw(const vr::Mat4f& view_proj_matrix);

 private:
  GLuint model_view_proj_matrix_handle_;
  GLuint color_handle_;
  GLuint ring_diameter_handle_;
  GLuint inner_hole_handle_;
  GLuint inner_ring_end_handle_;
  GLuint inner_ring_thickness_handle_;
  GLuint mid_ring_end_handle_;
  GLuint mid_ring_opacity_handle_;

  DISALLOW_COPY_AND_ASSIGN(ReticleRenderer);
};

class LaserRenderer : public BaseQuadRenderer {
 public:
  LaserRenderer();
  ~LaserRenderer() override;

  void Draw(const vr::Mat4f& view_proj_matrix);

 private:
  GLuint model_view_proj_matrix_handle_;
  GLuint texture_unit_handle_;
  GLuint texture_data_handle_;
  GLuint color_handle_;
  GLuint fade_point_handle_;
  GLuint fade_end_handle_;

  DISALLOW_COPY_AND_ASSIGN(LaserRenderer);
};

class ControllerRenderer : public BaseRenderer {
 public:
  ControllerRenderer();
  ~ControllerRenderer() override;

  void SetUp(std::unique_ptr<VrControllerModel> model);
  void Draw(VrControllerModel::State state, const vr::Mat4f& view_proj_matrix);
  bool IsSetUp() const { return setup_; }

 private:
  GLuint model_view_proj_matrix_handle_;
  GLuint tex_uniform_handle_;
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

  DISALLOW_COPY_AND_ASSIGN(ControllerRenderer);
};

class GradientQuadRenderer : public BaseQuadRenderer {
 public:
  GradientQuadRenderer();
  ~GradientQuadRenderer() override;

  void Draw(const vr::Mat4f& view_proj_matrix,
            const vr::Colorf& edge_color,
            const vr::Colorf& center_color,
            float opacity);

 private:
  GLuint model_view_proj_matrix_handle_;
  GLuint scene_radius_handle_;
  GLuint center_color_handle_;
  GLuint edge_color_handle_;
  GLuint opacity_handle_;

  DISALLOW_COPY_AND_ASSIGN(GradientQuadRenderer);
};

class GradientGridRenderer : public BaseRenderer {
 public:
  GradientGridRenderer();
  ~GradientGridRenderer() override;

  void Draw(const vr::Mat4f& view_proj_matrix,
            const vr::Colorf& edge_color,
            const vr::Colorf& center_color,
            int gridline_count,
            float opacity);

 private:
  void MakeGridLines(int gridline_count);

  GLuint vertex_buffer_ = 0;
  GLuint model_view_proj_matrix_handle_;
  GLuint scene_radius_handle_;
  GLuint center_color_handle_;
  GLuint edge_color_handle_;
  GLuint opacity_handle_;
  std::vector<Line3d> grid_lines_;

  DISALLOW_COPY_AND_ASSIGN(GradientGridRenderer);
};

class VrShellRenderer {
 public:
  VrShellRenderer();
  ~VrShellRenderer();

  TexturedQuadRenderer* GetTexturedQuadRenderer() {
    return textured_quad_renderer_.get();
  }

  WebVrRenderer* GetWebVrRenderer() { return webvr_renderer_.get(); }

  ReticleRenderer* GetReticleRenderer() { return reticle_renderer_.get(); }

  LaserRenderer* GetLaserRenderer() { return laser_renderer_.get(); }

  ControllerRenderer* GetControllerRenderer() {
    return controller_renderer_.get();
  }

  GradientQuadRenderer* GetGradientQuadRenderer() {
    return gradient_quad_renderer_.get();
  }

  GradientGridRenderer* GetGradientGridRenderer() {
    return gradient_grid_renderer_.get();
  }

 private:
  std::unique_ptr<TexturedQuadRenderer> textured_quad_renderer_;
  std::unique_ptr<WebVrRenderer> webvr_renderer_;
  std::unique_ptr<ReticleRenderer> reticle_renderer_;
  std::unique_ptr<LaserRenderer> laser_renderer_;
  std::unique_ptr<ControllerRenderer> controller_renderer_;
  std::unique_ptr<GradientQuadRenderer> gradient_quad_renderer_;
  std::unique_ptr<GradientGridRenderer> gradient_grid_renderer_;

  DISALLOW_COPY_AND_ASSIGN(VrShellRenderer);
};

}  // namespace vr_shell

#endif  // CHROME_BROWSER_ANDROID_VR_SHELL_VR_SHELL_RENDERER_H_
