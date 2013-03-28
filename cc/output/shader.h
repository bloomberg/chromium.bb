// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_OUTPUT_SHADER_H_
#define CC_OUTPUT_SHADER_H_

#include <string>

#include "base/basictypes.h"
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

  DISALLOW_COPY_AND_ASSIGN(VertexShaderPosTex);
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

  DISALLOW_COPY_AND_ASSIGN(VertexShaderPosTexYUVStretch);
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

  DISALLOW_COPY_AND_ASSIGN(VertexShaderPos);
};

class VertexShaderPosTexIdentity {
 public:
  void Init(WebKit::WebGraphicsContext3D*,
            unsigned program,
            bool using_bind_uniform,
            int* base_uniform_index) {}
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

  DISALLOW_COPY_AND_ASSIGN(VertexShaderPosTexTransform);
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

  DISALLOW_COPY_AND_ASSIGN(VertexShaderQuad);
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

  DISALLOW_COPY_AND_ASSIGN(VertexShaderTile);
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

  DISALLOW_COPY_AND_ASSIGN(VertexShaderVideoTransform);
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

  DISALLOW_COPY_AND_ASSIGN(FragmentTexAlphaBinding);
};

class FragmentTexColorMatrixAlphaBinding {
 public:
    FragmentTexColorMatrixAlphaBinding();

    void Init(WebKit::WebGraphicsContext3D*,
              unsigned program,
              bool usingBindUniform,
              int* baseUniformIndex);
    int alpha_location() const { return alpha_location_; }
    int color_matrix_location() const { return color_matrix_location_; }
    int color_offset_location() const { return color_offset_location_; }
    int edge_location() const { return -1; }
    int fragment_tex_transform_location() const { return -1; }
    int sampler_location() const { return sampler_location_; }

 private:
    int sampler_location_;
    int alpha_location_;
    int color_matrix_location_;
    int color_offset_location_;
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

  DISALLOW_COPY_AND_ASSIGN(FragmentTexOpaqueBinding);
};

class FragmentShaderRGBATexVaryingAlpha : public FragmentTexOpaqueBinding {
 public:
  std::string GetShaderString() const;
};

class FragmentShaderRGBATexAlpha : public FragmentTexAlphaBinding {
 public:
  std::string GetShaderString() const;
};

class FragmentShaderRGBATexColorMatrixAlpha
    : public FragmentTexColorMatrixAlphaBinding {
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
  FragmentShaderOESImageExternal();

  std::string GetShaderString() const;
  bool Init(WebKit::WebGraphicsContext3D*,
            unsigned program,
            bool using_bind_uniform,
            int* base_uniform_index);
 private:
  int sampler_location_;

  DISALLOW_COPY_AND_ASSIGN(FragmentShaderOESImageExternal);
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

  DISALLOW_COPY_AND_ASSIGN(FragmentShaderRGBATexAlphaAA);
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

  DISALLOW_COPY_AND_ASSIGN(FragmentTexClampAlphaAABinding);
};

class FragmentShaderRGBATexClampAlphaAA
    : public FragmentTexClampAlphaAABinding {
 public:
  std::string GetShaderString() const;
};

// Swizzles the red and blue component of sampled texel.
class FragmentShaderRGBATexClampSwizzleAlphaAA
    : public FragmentTexClampAlphaAABinding {
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

  DISALLOW_COPY_AND_ASSIGN(FragmentShaderRGBATexAlphaMask);
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

  DISALLOW_COPY_AND_ASSIGN(FragmentShaderRGBATexAlphaMaskAA);
};

class FragmentShaderRGBATexAlphaMaskColorMatrixAA {
 public:
  FragmentShaderRGBATexAlphaMaskColorMatrixAA();
  std::string GetShaderString() const;

  void Init(WebKit::WebGraphicsContext3D*,
    unsigned program,
    bool usingBindUniform,
    int* baseUniformIndex);
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
  int color_matrix_location() const { return color_matrix_location_; }
  int color_offset_location() const { return color_offset_location_; }

 private:
  int sampler_location_;
  int mask_sampler_location_;
  int alpha_location_;
  int edge_location_;
  int mask_tex_coord_scale_location_;
  int mask_tex_coord_offset_location_;
  int color_matrix_location_;
  int color_offset_location_;
};

class FragmentShaderRGBATexAlphaColorMatrixAA {
 public:
  FragmentShaderRGBATexAlphaColorMatrixAA();
  std::string GetShaderString() const;

  void Init(WebKit::WebGraphicsContext3D*,
            unsigned program,
            bool usingBindUniform,
            int* baseUniformIndex);
  int alpha_location() const { return alpha_location_; }
  int sampler_location() const { return sampler_location_; }
  int edge_location() const { return edge_location_; }
  int color_matrix_location() const { return color_matrix_location_; }
  int color_offset_location() const { return color_offset_location_; }

 private:
  int sampler_location_;
  int alpha_location_;
  int edge_location_;
  int color_matrix_location_;
  int color_offset_location_;
};

class FragmentShaderRGBATexAlphaMaskColorMatrix {
 public:
  FragmentShaderRGBATexAlphaMaskColorMatrix();
  std::string GetShaderString() const;

  void Init(WebKit::WebGraphicsContext3D*,
            unsigned program,
            bool usingBindUniform,
            int* baseUniformIndex);
  int alpha_location() const { return alpha_location_; }
  int sampler_location() const { return sampler_location_; }
  int mask_sampler_location() const { return mask_sampler_location_; }
  int mask_tex_coord_scale_location() const {
    return mask_tex_coord_scale_location_;
  }
  int mask_tex_coord_offset_location() const {
    return mask_tex_coord_offset_location_;
  }
  int color_matrix_location() const { return color_matrix_location_; }
  int color_offset_location() const { return color_offset_location_; }

 private:
  int sampler_location_;
  int mask_sampler_location_;
  int alpha_location_;
  int mask_tex_coord_scale_location_;
  int mask_tex_coord_offset_location_;
  int color_matrix_location_;
  int color_offset_location_;
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

  DISALLOW_COPY_AND_ASSIGN(FragmentShaderYUVVideo);
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

  DISALLOW_COPY_AND_ASSIGN(FragmentShaderColor);
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

  DISALLOW_COPY_AND_ASSIGN(FragmentShaderColorAA);
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

  DISALLOW_COPY_AND_ASSIGN(FragmentShaderCheckerboard);
};

}  // namespace cc

#endif  // CC_OUTPUT_SHADER_H_
