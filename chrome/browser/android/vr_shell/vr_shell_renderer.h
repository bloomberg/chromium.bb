// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_SHELL_VR_SHELL_RENDERER_H_
#define CHROME_BROWSER_ANDROID_VR_SHELL_VR_SHELL_RENDERER_H_

#include <memory>

#include "base/macros.h"
#include "chrome/browser/android/vr_shell/vr_math.h"
#include "third_party/gvr-android-sdk/src/ndk/include/vr/gvr/capi/include/gvr_types.h"
#include "ui/gl/gl_bindings.h"

namespace vr_shell {

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
  SHADER_ID_MAX
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

class TexturedQuadRenderer : public BaseRenderer {
 public:
  TexturedQuadRenderer();
  ~TexturedQuadRenderer() override;

  // Draw the content rect in the texture quad.
  void Draw(int texture_data_handle, const gvr::Mat4f& combined_matrix,
            const Rectf& copy_rect);

 private:
  GLuint combined_matrix_handle_;
  GLuint copy_rect_uniform_handle_;
  GLuint tex_uniform_handle_;

  DISALLOW_COPY_AND_ASSIGN(TexturedQuadRenderer);
};

// Renders a page-generated stereo VR view.
class WebVrRenderer : public BaseRenderer {
 public:
  WebVrRenderer();
  ~WebVrRenderer() override;

  void Draw(int texture_handle);

  void UpdateTextureBounds(int eye, const gvr::Rectf& bounds);

 private:
  static constexpr size_t VERTEX_STRIDE = sizeof(float) * 4;
  static constexpr size_t POSITION_ELEMENTS = 2;
  static constexpr size_t TEXCOORD_ELEMENTS = 2;
  static constexpr size_t POSITION_OFFSET = 0;
  static constexpr size_t TEXCOORD_OFFSET = sizeof(float) * 2;

  GLuint src_rect_uniform_handle_;
  GLuint tex_uniform_handle_;
  GLuint vertex_buffer_;

  gvr::Rectf left_bounds_;
  gvr::Rectf right_bounds_;

  DISALLOW_COPY_AND_ASSIGN(WebVrRenderer);
};

class ReticleRenderer : public BaseRenderer {
 public:
  ReticleRenderer();
  ~ReticleRenderer() override;

  void Draw(const gvr::Mat4f& combined_matrix);

 private:
  GLuint combined_matrix_handle_;
  GLuint color_handle_;
  GLuint ring_diameter_handle_;
  GLuint inner_hole_handle_;
  GLuint inner_ring_end_handle_;
  GLuint inner_ring_thickness_handle_;
  GLuint mid_ring_end_handle_;
  GLuint mid_ring_opacity_handle_;

  DISALLOW_COPY_AND_ASSIGN(ReticleRenderer);
};

class LaserRenderer : public BaseRenderer {
 public:
  LaserRenderer();
  ~LaserRenderer() override;

  void Draw(const gvr::Mat4f& combined_matrix);

 private:
  GLuint combined_matrix_handle_;
  GLuint texture_unit_handle_;
  GLuint texture_data_handle_;
  GLuint color_handle_;
  GLuint fade_point_handle_;
  GLuint fade_end_handle_;

  DISALLOW_COPY_AND_ASSIGN(LaserRenderer);
};

class VrShellRenderer {
 public:
  VrShellRenderer();
  ~VrShellRenderer();

  TexturedQuadRenderer* GetTexturedQuadRenderer() {
    return textured_quad_renderer_.get();
  }

  WebVrRenderer* GetWebVrRenderer() {
    return webvr_renderer_.get();
  }

  ReticleRenderer* GetReticleRenderer() {
    return reticle_renderer_.get();
  }

  LaserRenderer* GetLaserRenderer() {
    return laser_renderer_.get();
  }

 private:
  std::unique_ptr<TexturedQuadRenderer> textured_quad_renderer_;
  std::unique_ptr<WebVrRenderer> webvr_renderer_;
  std::unique_ptr<ReticleRenderer> reticle_renderer_;
  std::unique_ptr<LaserRenderer> laser_renderer_;

  DISALLOW_COPY_AND_ASSIGN(VrShellRenderer);
};

}  // namespace vr_shell

#endif  // CHROME_BROWSER_ANDROID_VR_SHELL_VR_SHELL_RENDERER_H_
