// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_VR_SHELL_RENDERER_H_
#define CHROME_BROWSER_VR_VR_SHELL_RENDERER_H_

#include <memory>
#include <queue>
#include <vector>

#include "base/macros.h"
#include "chrome/browser/vr/ui_element_renderer.h"
#include "chrome/browser/vr/vr_controller_model.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/transform.h"
#include "ui/gl/gl_bindings.h"

namespace vr {

class BaseRenderer;
class BaseQuadRenderer;
class ControllerRenderer;
class ExternalTexturedQuadRenderer;
class GradientGridRenderer;
class GradientQuadRenderer;
class LaserRenderer;
class ReticleRenderer;
class TexturedQuadRenderer;
class WebVrRenderer;

class VrShellRenderer : public vr::UiElementRenderer {
 public:
  VrShellRenderer();
  ~VrShellRenderer() override;

  // vr::UiElementRenderer interface (exposed to UI elements).
  void DrawTexturedQuad(int texture_data_handle,
                        TextureLocation texture_location,
                        const gfx::Transform& view_proj_matrix,
                        const gfx::RectF& copy_rect,
                        float opacity,
                        gfx::SizeF element_size,
                        float corner_radius) override;
  void DrawGradientQuad(const gfx::Transform& view_proj_matrix,
                        const SkColor edge_color,
                        const SkColor center_color,
                        float opacity) override;

  // VrShell's internal GL rendering API.
  ExternalTexturedQuadRenderer* GetExternalTexturedQuadRenderer();
  TexturedQuadRenderer* GetTexturedQuadRenderer();
  WebVrRenderer* GetWebVrRenderer();
  ReticleRenderer* GetReticleRenderer();
  LaserRenderer* GetLaserRenderer();
  ControllerRenderer* GetControllerRenderer();
  GradientQuadRenderer* GetGradientQuadRenderer();
  GradientGridRenderer* GetGradientGridRenderer();

  void Flush();

  gfx::Size surface_texture_size() const { return surface_texture_size_; }
  void set_surface_texture_size(const gfx::Size& surface_texture_size) {
    surface_texture_size_ = surface_texture_size;
  }

 private:
  void FlushIfNecessary(BaseRenderer* renderer);

  BaseRenderer* last_renderer_ = nullptr;

  std::unique_ptr<ExternalTexturedQuadRenderer>
      external_textured_quad_renderer_;
  std::unique_ptr<TexturedQuadRenderer> textured_quad_renderer_;
  std::unique_ptr<WebVrRenderer> webvr_renderer_;
  std::unique_ptr<ReticleRenderer> reticle_renderer_;
  std::unique_ptr<LaserRenderer> laser_renderer_;
  std::unique_ptr<ControllerRenderer> controller_renderer_;
  std::unique_ptr<GradientQuadRenderer> gradient_quad_renderer_;
  std::unique_ptr<GradientGridRenderer> gradient_grid_renderer_;

  gfx::Size surface_texture_size_;

  DISALLOW_COPY_AND_ASSIGN(VrShellRenderer);
};

// TODO(vollick): move these into separate headers.
class BaseRenderer {
 public:
  virtual ~BaseRenderer();

  virtual void Flush();

 protected:
  BaseRenderer(const char* vertex_src, const char* fragment_src);

  GLuint program_handle_;
  GLuint position_handle_;

  DISALLOW_COPY_AND_ASSIGN(BaseRenderer);
};

class BaseQuadRenderer : public BaseRenderer {
 public:
  BaseQuadRenderer(const char* vertex_src, const char* fragment_src);
  ~BaseQuadRenderer() override;

  static void SetVertexBuffer();

 protected:
  void PrepareToDraw(GLuint view_proj_matrix_handle,
                     const gfx::Transform& view_proj_matrix);

  static GLuint vertex_buffer_;
  static GLuint index_buffer_;

  DISALLOW_COPY_AND_ASSIGN(BaseQuadRenderer);
};

class TexturedQuadRenderer : public BaseRenderer {
 public:
  TexturedQuadRenderer(const char* vertex_src, const char* fragment_src);
  TexturedQuadRenderer();
  ~TexturedQuadRenderer() override;

  // Enqueues a textured quad for rendering. The GL will ultimately be issued
  // in |Flush|.
  void AddQuad(int texture_data_handle,
               const gfx::Transform& view_proj_matrix,
               const gfx::RectF& copy_rect,
               float opacity,
               gfx::Size surface_texture_size,
               gfx::SizeF element_size,
               float corner_radius);

  void Flush() override;

  static void SetVertexBuffer();

 protected:
  virtual GLenum TextureType() const;

 private:
  struct QuadData {
    int texture_data_handle;
    gfx::Transform view_proj_matrix;
    gfx::RectF copy_rect;
    float opacity;
    gfx::Size surface_texture_size;
    gfx::SizeF element_size;
    float corner_radius;
  };

  float ComputePhysicalPixelWidth(const QuadData& quad) const;

  static GLuint vertex_buffer_;
  static GLuint index_buffer_;

  // Uniforms
  GLuint model_view_proj_matrix_handle_;
  GLuint corner_offset_handle_;
  GLuint corner_scale_handle_;
  GLuint opacity_handle_;
  GLuint texture_handle_;
  GLuint copy_rect_handler_;

  // Attributes
  GLuint corner_position_handle_;
  GLuint offset_scale_handle_;

  std::queue<QuadData> quad_queue_;

  DISALLOW_COPY_AND_ASSIGN(TexturedQuadRenderer);
};

class ExternalTexturedQuadRenderer : public TexturedQuadRenderer {
 public:
  ExternalTexturedQuadRenderer();
  ~ExternalTexturedQuadRenderer() override;

 private:
  GLenum TextureType() const override;

  DISALLOW_COPY_AND_ASSIGN(ExternalTexturedQuadRenderer);
};

// Renders a page-generated stereo VR view.
class WebVrRenderer : public BaseQuadRenderer {
 public:
  WebVrRenderer();
  ~WebVrRenderer() override;

  void Draw(int texture_handle);

 private:
  GLuint texture_handle_;

  DISALLOW_COPY_AND_ASSIGN(WebVrRenderer);
};

class ReticleRenderer : public BaseQuadRenderer {
 public:
  ReticleRenderer();
  ~ReticleRenderer() override;

  void Draw(const gfx::Transform& view_proj_matrix);

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

  void Draw(float opacity, const gfx::Transform& view_proj_matrix);

 private:
  GLuint model_view_proj_matrix_handle_;
  GLuint texture_unit_handle_;
  GLuint texture_data_handle_;
  GLuint color_handle_;
  GLuint fade_point_handle_;
  GLuint fade_end_handle_;
  GLuint opacity_handle_;

  DISALLOW_COPY_AND_ASSIGN(LaserRenderer);
};

class ControllerRenderer : public BaseRenderer {
 public:
  ControllerRenderer();
  ~ControllerRenderer() override;

  void SetUp(std::unique_ptr<VrControllerModel> model);
  void Draw(VrControllerModel::State state,
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

  DISALLOW_COPY_AND_ASSIGN(ControllerRenderer);
};

class GradientQuadRenderer : public BaseQuadRenderer {
 public:
  GradientQuadRenderer();
  ~GradientQuadRenderer() override;

  void Draw(const gfx::Transform& view_proj_matrix,
            SkColor edge_color,
            SkColor center_color,
            float opacity);

 private:
  GLuint model_view_proj_matrix_handle_;
  GLuint scene_radius_handle_;
  GLuint center_color_handle_;
  GLuint edge_color_handle_;
  GLuint opacity_handle_;

  DISALLOW_COPY_AND_ASSIGN(GradientQuadRenderer);
};

class GradientGridRenderer : public BaseQuadRenderer {
 public:
  GradientGridRenderer();
  ~GradientGridRenderer() override;

  void Draw(const gfx::Transform& view_proj_matrix,
            SkColor edge_color,
            SkColor center_color,
            SkColor grid_color,
            int gridline_count,
            float opacity);

 private:
  GLuint model_view_proj_matrix_handle_;
  GLuint scene_radius_handle_;
  GLuint center_color_handle_;
  GLuint edge_color_handle_;
  GLuint grid_color_handle_;
  GLuint opacity_handle_;
  GLuint lines_count_handle_;

  DISALLOW_COPY_AND_ASSIGN(GradientGridRenderer);
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_VR_SHELL_RENDERER_H_
