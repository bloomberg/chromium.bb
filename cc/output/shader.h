// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_OUTPUT_SHADER_H_
#define CC_OUTPUT_SHADER_H_

#include <string>
#include "third_party/skia/include/core/SkColorPriv.h"

namespace WebKit {
class WebGraphicsContext3D;
}

namespace cc {

class VertexShaderPosTex {
 public:
  VertexShaderPosTex();

  void Init(WebKit::WebGraphicsContext3D*,
            unsigned program,
            bool using_bind_uniform,
            int* base_uniform_index);
  std::string GetShaderString() const;

  int matrix_location() const { return matrix_location_; }

 private:
  int matrix_location_;
};

class VertexShaderPosTexYUVStretch {
 public:
  VertexShaderPosTexYUVStretch();

  void Init(WebKit::WebGraphicsContext3D*,
            unsigned program,
            bool using_bind_uniform,
            int* base_uniform_index);
  std::string GetShaderString() const;

  int matrix_location() const { return matrix_location_; }
  int tex_scale_location() const { return tex_scale_location_; }

 private:
  int matrix_location_;
  int tex_scale_location_;
};

class VertexShaderPos {
 public:
  VertexShaderPos();

  void Init(WebKit::WebGraphicsContext3D*,
            unsigned program,
            bool using_bind_uniform,
            int* base_uniform_index);
  std::string GetShaderString() const;

  int matrix_location() const { return matrix_location_; }

 private:
  int matrix_location_;
};

class VertexShaderPosTexIdentity {
 public:
  void Init(WebKit::WebGraphicsContext3D*,
            unsigned program,
            bool using_bind_uniform,
            int* base_uniform_index) { }
  std::string GetShaderString() const;
};

class VertexShaderPosTexTransform {
 public:
  VertexShaderPosTexTransform();

  void Init(WebKit::WebGraphicsContext3D*,
            unsigned program,
            bool using_bind_uniform,
            int* base_uniform_index);
  std::string GetShaderString() const;

  int matrix_location() const { return matrix_location_; }
  int tex_transform_location() const { return tex_transform_location_; }
  int vertex_opacity_location() const { return vertex_opacity_location_; }

 private:
  int matrix_location_;
  int tex_transform_location_;
  int vertex_opacity_location_;
};

class VertexShaderPosTexTransformFlip : public VertexShaderPosTexTransform {
 public:
  std::string GetShaderString() const;
};

class VertexShaderQuad {
 public:
  VertexShaderQuad();

  void Init(WebKit::WebGraphicsContext3D*,
           unsigned program, 
           bool using_bind_uniform,
           int* base_uniform_index);
  std::string GetShaderString() const;

  int matrix_location() const { return matrix_location_; }
  int point_location() const { return point_location_; }
  int tex_scale_location() const { return tex_scale_location_; }

 private:
  int matrix_location_;
  int point_location_;
  int tex_scale_location_;
};

class VertexShaderTile {
 public:
  VertexShaderTile();

  void Init(WebKit::WebGraphicsContext3D*,
            unsigned program,
            bool using_bind_uniform,
            int* base_uniform_index);
  std::string GetShaderString() const;

  int matrix_location() const { return matrix_location_; }
  int point_location() const { return point_location_; }
  int vertex_tex_transform_location() const {
    return vertex_tex_transform_location_;
  }

 private:
  int matrix_location_;
  int point_location_;
  int vertex_tex_transform_location_;
};

class VertexShaderVideoTransform {
 public:
  VertexShaderVideoTransform();

  bool Init(WebKit::WebGraphicsContext3D*,
            unsigned program,
            bool using_bind_uniform,
            int* base_uniform_index);
  std::string GetShaderString() const;

  int matrix_location() const { return matrix_location_; }
  int tex_matrix_location() const { return tex_matrix_location_; }

 private:
  int matrix_location_;
  int tex_matrix_location_;
};

class FragmentTexAlphaBinding {
 public:
  FragmentTexAlphaBinding();

  void Init(WebKit::WebGraphicsContext3D*,
            unsigned program,
            bool using_bind_uniform,
            int* base_uniform_index);
  int alpha_location() const { return alpha_location_; }
  int edge_location() const { return -1; }
  int fragment_tex_transform_location() const { return -1; }
  int sampler_location() const { return sampler_location_; }

 private:
  int sampler_location_;
  int alpha_location_;
};

class FragmentTexOpaqueBinding {
 public:
  FragmentTexOpaqueBinding();

  void Init(WebKit::WebGraphicsContext3D*,
            unsigned program,
            bool using_bind_uniform,
            int* base_uniform_index);
  int alpha_location() const { return -1; }
  int edge_location() const { return -1; }
  int fragment_tex_transform_location() const { return -1; }
  int sampler_location() const { return sampler_location_; }

 private:
  int sampler_location_;
};

class FragmentShaderRGBATexVaryingAlpha : public FragmentTexOpaqueBinding {
 public:
  std::string GetShaderString() const;
};

class FragmentShaderRGBATexAlpha : public FragmentTexAlphaBinding {
public:
  std::string GetShaderString() const;
};

class FragmentShaderRGBATexRectVaryingAlpha : public FragmentTexAlphaBinding {
 public:
  std::string GetShaderString() const;
};

class FragmentShaderRGBATexOpaque : public FragmentTexOpaqueBinding {
 public:
  std::string GetShaderString() const;
};

class FragmentShaderRGBATex : public FragmentTexOpaqueBinding {
 public:
  std::string GetShaderString() const;
};

// Swizzles the red and blue component of sampled texel with alpha.
class FragmentShaderRGBATexSwizzleAlpha : public FragmentTexAlphaBinding {
 public:
  std::string GetShaderString() const;
};

// Swizzles the red and blue component of sampled texel without alpha.
class FragmentShaderRGBATexSwizzleOpaque : public FragmentTexOpaqueBinding {
 public:
  std::string GetShaderString() const;
};

// Fragment shader for external textures.
class FragmentShaderOESImageExternal : public FragmentTexAlphaBinding {
 public:
  std::string GetShaderString() const;
  bool Init(WebKit::WebGraphicsContext3D*,
            unsigned program,
            bool using_bind_uniform,
            int* base_uniform_index);
 private:
  int sampler_location_;
};

class FragmentShaderRGBATexAlphaAA {
 public:
  FragmentShaderRGBATexAlphaAA();

  void Init(WebKit::WebGraphicsContext3D*,
            unsigned program,
            bool using_bind_uniform,
            int* base_uniform_index);
  std::string GetShaderString() const;

  int alpha_location() const { return alpha_location_; }
  int sampler_location() const { return sampler_location_; }
  int edge_location() const { return edge_location_; }

 private:
  int sampler_location_;
  int alpha_location_;
  int edge_location_;
};

class FragmentTexClampAlphaAABinding {
 public:
  FragmentTexClampAlphaAABinding();

  void Init(WebKit::WebGraphicsContext3D*,
            unsigned program,
            bool using_bind_uniform,
            int* base_uniform_index);
  int alpha_location() const { return alpha_location_; }
  int sampler_location() const { return sampler_location_; }
  int fragment_tex_transform_location() const {
    return fragment_tex_transform_location_;
  }
  int edge_location() const { return edge_location_; }

 private:
  int sampler_location_;
  int alpha_location_;
  int fragment_tex_transform_location_;
  int edge_location_;
};

class FragmentShaderRGBATexClampAlphaAA :
  public FragmentTexClampAlphaAABinding {
 public:
  std::string GetShaderString() const;
};

// Swizzles the red and blue component of sampled texel.
class FragmentShaderRGBATexClampSwizzleAlphaAA :
  public FragmentTexClampAlphaAABinding {
 public:
  std::string GetShaderString() const;
};

class FragmentShaderRGBATexAlphaMask {
 public:
  FragmentShaderRGBATexAlphaMask();
  std::string GetShaderString() const;

  void Init(WebKit::WebGraphicsContext3D*,
            unsigned program,
            bool using_bind_uniform,
            int* base_uniform_index);
  int alpha_location() const { return alpha_location_; }
  int sampler_location() const { return sampler_location_; }
  int mask_sampler_location() const { return mask_sampler_location_; }
  int mask_tex_coord_scale_location() const {
    return mask_tex_coord_scale_location_;
  }
  int mask_tex_coord_offset_location() const {
    return mask_tex_coord_offset_location_;
  }

 private:
  int sampler_location_;
  int mask_sampler_location_;
  int alpha_location_;
  int mask_tex_coord_scale_location_;
  int mask_tex_coord_offset_location_;
};

class FragmentShaderRGBATexAlphaMaskAA {
 public:
  FragmentShaderRGBATexAlphaMaskAA();
  std::string GetShaderString() const;

  void Init(WebKit::WebGraphicsContext3D*,
            unsigned program,
            bool using_bind_uniform,
            int* base_uniform_index);
  int alpha_location() const { return alpha_location_; }
  int sampler_location() const { return sampler_location_; }
  int mask_sampler_location() const { return mask_sampler_location_; }
  int edge_location() const { return edge_location_; }
  int mask_tex_coord_scale_location() const {
    return mask_tex_coord_scale_location_;
  }
  int mask_tex_coord_offset_location() const {
    return mask_tex_coord_offset_location_;
  }

 private:
  int sampler_location_;
  int mask_sampler_location_;
  int alpha_location_;
  int edge_location_;
  int mask_tex_coord_scale_location_;
  int mask_tex_coord_offset_location_;
};

class FragmentShaderYUVVideo {
 public:
  FragmentShaderYUVVideo();
  std::string GetShaderString() const;

  void Init(WebKit::WebGraphicsContext3D*,
            unsigned program,
            bool using_bind_uniform,
            int* base_uniform_index);
  int y_texture_location() const { return y_texture_location_; }
  int u_texture_location() const { return u_texture_location_; }
  int v_texture_location() const { return v_texture_location_; }
  int alpha_location() const { return alpha_location_; }
  int yuv_matrix_location() const { return yuv_matrix_location_; }
  int yuv_adj_location() const { return yuv_adj_location_; }

 private:
  int y_texture_location_;
  int u_texture_location_;
  int v_texture_location_;
  int alpha_location_;
  int yuv_matrix_location_;
  int yuv_adj_location_;
};

class FragmentShaderColor {
 public:
  FragmentShaderColor();
  std::string GetShaderString() const;

  void Init(WebKit::WebGraphicsContext3D*,
            unsigned program,
            bool using_bind_uniform,
            int* base_uniform_index);
  int edge_location() const { return -1; }
  int color_location() const { return color_location_; }

 private:
  int color_location_;
};

class FragmentShaderColorAA {
 public:
  FragmentShaderColorAA();
  std::string GetShaderString() const;

  void Init(WebKit::WebGraphicsContext3D*,
            unsigned program,
            bool using_bind_uniform,
            int* base_uniform_index);
  int edge_location() const { return edge_location_; }
  int color_location() const { return color_location_; }

 private:
  int edge_location_;
  int color_location_;
};

class FragmentShaderCheckerboard {
 public:
  FragmentShaderCheckerboard();
  std::string GetShaderString() const;

  void Init(WebKit::WebGraphicsContext3D*,
            unsigned program,
            bool using_bind_uniform,
            int* base_uniform_index);
  int alpha_location() const { return alpha_location_; }
  int tex_transform_location() const { return tex_transform_location_; }
  int frequency_location() const { return frequency_location_; }
  int color_location() const { return color_location_; }

 private:
  int alpha_location_;
  int tex_transform_location_;
  int frequency_location_;
  int color_location_;
};

} // namespace cc

#endif  // CC_OUTPUT_SHADER_H_
