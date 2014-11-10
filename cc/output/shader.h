// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_OUTPUT_SHADER_H_
#define CC_OUTPUT_SHADER_H_

#include <string>

#include "base/basictypes.h"
#include "cc/base/cc_export.h"

namespace gfx {
class Point;
class Size;
}

namespace gpu {
namespace gles2 {
class GLES2Interface;
}
}

namespace cc {

enum TexCoordPrecision {
  TexCoordPrecisionNA = 0,
  TexCoordPrecisionMedium = 1,
  TexCoordPrecisionHigh = 2,
  NumTexCoordPrecisions = 3
};

enum SamplerType {
  SamplerTypeNA = 0,
  SamplerType2D = 1,
  SamplerType2DRect = 2,
  SamplerTypeExternalOES = 3,
  NumSamplerTypes = 4
};

enum BlendMode {
  BlendModeNone,
  BlendModeNormal,
  BlendModeScreen,
  BlendModeOverlay,
  BlendModeDarken,
  BlendModeLighten,
  BlendModeColorDodge,
  BlendModeColorBurn,
  BlendModeHardLight,
  BlendModeSoftLight,
  BlendModeDifference,
  BlendModeExclusion,
  BlendModeMultiply,
  BlendModeHue,
  BlendModeSaturation,
  BlendModeColor,
  BlendModeLuminosity,
  NumBlendModes
};

// Note: The highp_threshold_cache must be provided by the caller to make
// the caching multi-thread/context safe in an easy low-overhead manner.
// The caller must make sure to clear highp_threshold_cache to 0, so it can be
// reinitialized, if a new or different context is used.
CC_EXPORT TexCoordPrecision
    TexCoordPrecisionRequired(gpu::gles2::GLES2Interface* context,
                              int* highp_threshold_cache,
                              int highp_threshold_min,
                              const gfx::Point& max_coordinate);

CC_EXPORT TexCoordPrecision TexCoordPrecisionRequired(
    gpu::gles2::GLES2Interface* context,
    int *highp_threshold_cache,
    int highp_threshold_min,
    const gfx::Size& max_size);

class VertexShaderPosTex {
 public:
  VertexShaderPosTex();

  void Init(gpu::gles2::GLES2Interface* context,
            unsigned program,
            int* base_uniform_index);
  std::string GetShaderString() const;

  int matrix_location() const { return matrix_location_; }

 private:
  int matrix_location_;

  DISALLOW_COPY_AND_ASSIGN(VertexShaderPosTex);
};

class VertexShaderPosTexYUVStretchOffset {
 public:
  VertexShaderPosTexYUVStretchOffset();

  void Init(gpu::gles2::GLES2Interface* context,
            unsigned program,
            int* base_uniform_index);
  std::string GetShaderString() const;

  int matrix_location() const { return matrix_location_; }
  int tex_scale_location() const { return tex_scale_location_; }
  int tex_offset_location() const { return tex_offset_location_; }

 private:
  int matrix_location_;
  int tex_scale_location_;
  int tex_offset_location_;

  DISALLOW_COPY_AND_ASSIGN(VertexShaderPosTexYUVStretchOffset);
};

class VertexShaderPos {
 public:
  VertexShaderPos();

  void Init(gpu::gles2::GLES2Interface* context,
            unsigned program,
            int* base_uniform_index);
  std::string GetShaderString() const;

  int matrix_location() const { return matrix_location_; }

 private:
  int matrix_location_;

  DISALLOW_COPY_AND_ASSIGN(VertexShaderPos);
};

class VertexShaderPosTexIdentity {
 public:
  void Init(gpu::gles2::GLES2Interface* context,
            unsigned program,
            int* base_uniform_index) {}
  std::string GetShaderString() const;
};

class VertexShaderPosTexTransform {
 public:
  VertexShaderPosTexTransform();

  void Init(gpu::gles2::GLES2Interface* context,
            unsigned program,
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

class VertexShaderQuad {
 public:
  VertexShaderQuad();

  void Init(gpu::gles2::GLES2Interface* context,
           unsigned program,
           int* base_uniform_index);
  std::string GetShaderString() const;

  int matrix_location() const { return matrix_location_; }
  int viewport_location() const { return -1; }
  int quad_location() const { return quad_location_; }
  int edge_location() const { return -1; }

 private:
  int matrix_location_;
  int quad_location_;

  DISALLOW_COPY_AND_ASSIGN(VertexShaderQuad);
};

class VertexShaderQuadAA {
 public:
  VertexShaderQuadAA();

  void Init(gpu::gles2::GLES2Interface* context,
           unsigned program,
           int* base_uniform_index);
  std::string GetShaderString() const;

  int matrix_location() const { return matrix_location_; }
  int viewport_location() const { return viewport_location_; }
  int quad_location() const { return quad_location_; }
  int edge_location() const { return edge_location_; }

 private:
  int matrix_location_;
  int viewport_location_;
  int quad_location_;
  int edge_location_;

  DISALLOW_COPY_AND_ASSIGN(VertexShaderQuadAA);
};


class VertexShaderQuadTexTransformAA {
 public:
  VertexShaderQuadTexTransformAA();

  void Init(gpu::gles2::GLES2Interface* context,
           unsigned program,
           int* base_uniform_index);
  std::string GetShaderString() const;

  int matrix_location() const { return matrix_location_; }
  int viewport_location() const { return viewport_location_; }
  int quad_location() const { return quad_location_; }
  int edge_location() const { return edge_location_; }
  int tex_transform_location() const { return tex_transform_location_; }

 private:
  int matrix_location_;
  int viewport_location_;
  int quad_location_;
  int edge_location_;
  int tex_transform_location_;

  DISALLOW_COPY_AND_ASSIGN(VertexShaderQuadTexTransformAA);
};

class VertexShaderTile {
 public:
  VertexShaderTile();

  void Init(gpu::gles2::GLES2Interface* context,
            unsigned program,
            int* base_uniform_index);
  std::string GetShaderString() const;

  int matrix_location() const { return matrix_location_; }
  int viewport_location() const { return -1; }
  int quad_location() const { return quad_location_; }
  int edge_location() const { return -1; }
  int vertex_tex_transform_location() const {
    return vertex_tex_transform_location_;
  }

 private:
  int matrix_location_;
  int quad_location_;
  int vertex_tex_transform_location_;

  DISALLOW_COPY_AND_ASSIGN(VertexShaderTile);
};

class VertexShaderTileAA {
 public:
  VertexShaderTileAA();

  void Init(gpu::gles2::GLES2Interface* context,
            unsigned program,
            int* base_uniform_index);
  std::string GetShaderString() const;

  int matrix_location() const { return matrix_location_; }
  int viewport_location() const { return viewport_location_; }
  int quad_location() const { return quad_location_; }
  int edge_location() const { return edge_location_; }
  int vertex_tex_transform_location() const {
    return vertex_tex_transform_location_;
  }

 private:
  int matrix_location_;
  int viewport_location_;
  int quad_location_;
  int edge_location_;
  int vertex_tex_transform_location_;

  DISALLOW_COPY_AND_ASSIGN(VertexShaderTileAA);
};

class VertexShaderVideoTransform {
 public:
  VertexShaderVideoTransform();

  void Init(gpu::gles2::GLES2Interface* context,
            unsigned program,
            int* base_uniform_index);
  std::string GetShaderString() const;

  int matrix_location() const { return matrix_location_; }
  int tex_matrix_location() const { return tex_matrix_location_; }

 private:
  int matrix_location_;
  int tex_matrix_location_;

  DISALLOW_COPY_AND_ASSIGN(VertexShaderVideoTransform);
};

class FragmentTexBlendMode {
 public:
  int backdrop_location() const { return backdrop_location_; }
  int backdrop_rect_location() const { return backdrop_rect_location_; }

  BlendMode blend_mode() const { return blend_mode_; }
  void set_blend_mode(BlendMode blend_mode) { blend_mode_ = blend_mode; }
  bool has_blend_mode() const { return blend_mode_ != BlendModeNone; }

 protected:
  FragmentTexBlendMode();

  std::string SetBlendModeFunctions(std::string shader_string) const;

  int backdrop_location_;
  int backdrop_rect_location_;

 private:
  BlendMode blend_mode_;

  std::string GetHelperFunctions() const;
  std::string GetBlendFunction() const;
  std::string GetBlendFunctionBodyForRGB() const;
};

class FragmentTexAlphaBinding : public FragmentTexBlendMode {
 public:
  FragmentTexAlphaBinding();

  void Init(gpu::gles2::GLES2Interface* context,
            unsigned program,
            int* base_uniform_index);
  int alpha_location() const { return alpha_location_; }
  int fragment_tex_transform_location() const { return -1; }
  int sampler_location() const { return sampler_location_; }

 private:
  int sampler_location_;
  int alpha_location_;

  DISALLOW_COPY_AND_ASSIGN(FragmentTexAlphaBinding);
};

class FragmentTexColorMatrixAlphaBinding : public FragmentTexBlendMode {
 public:
    FragmentTexColorMatrixAlphaBinding();

    void Init(gpu::gles2::GLES2Interface* context,
              unsigned program,
              int* base_uniform_index);
    int alpha_location() const { return alpha_location_; }
    int color_matrix_location() const { return color_matrix_location_; }
    int color_offset_location() const { return color_offset_location_; }
    int fragment_tex_transform_location() const { return -1; }
    int sampler_location() const { return sampler_location_; }

 private:
    int sampler_location_;
    int alpha_location_;
    int color_matrix_location_;
    int color_offset_location_;
};

class FragmentTexOpaqueBinding : public FragmentTexBlendMode {
 public:
  FragmentTexOpaqueBinding();

  void Init(gpu::gles2::GLES2Interface* context,
            unsigned program,
            int* base_uniform_index);
  int alpha_location() const { return -1; }
  int fragment_tex_transform_location() const { return -1; }
  int background_color_location() const { return -1; }
  int sampler_location() const { return sampler_location_; }

 private:
  int sampler_location_;

  DISALLOW_COPY_AND_ASSIGN(FragmentTexOpaqueBinding);
};

class FragmentTexBackgroundBinding : public FragmentTexBlendMode {
 public:
  FragmentTexBackgroundBinding();

  void Init(gpu::gles2::GLES2Interface* context,
            unsigned program,
            int* base_uniform_index);
  int background_color_location() const { return background_color_location_; }
  int sampler_location() const { return sampler_location_; }

 private:
  int background_color_location_;
  int sampler_location_;

  DISALLOW_COPY_AND_ASSIGN(FragmentTexBackgroundBinding);
};

class FragmentShaderRGBATexVaryingAlpha : public FragmentTexOpaqueBinding {
 public:
  std::string GetShaderString(
      TexCoordPrecision precision, SamplerType sampler) const;
};

class FragmentShaderRGBATexPremultiplyAlpha : public FragmentTexOpaqueBinding {
 public:
  std::string GetShaderString(
      TexCoordPrecision precision, SamplerType sampler) const;
};

class FragmentShaderTexBackgroundVaryingAlpha
    : public FragmentTexBackgroundBinding {
 public:
  std::string GetShaderString(
      TexCoordPrecision precision, SamplerType sampler) const;
};

class FragmentShaderTexBackgroundPremultiplyAlpha
    : public FragmentTexBackgroundBinding {
 public:
  std::string GetShaderString(
      TexCoordPrecision precision, SamplerType sampler) const;
};

class FragmentShaderRGBATexAlpha : public FragmentTexAlphaBinding {
 public:
  std::string GetShaderString(
      TexCoordPrecision precision, SamplerType sampler) const;
};

class FragmentShaderRGBATexColorMatrixAlpha
    : public FragmentTexColorMatrixAlphaBinding {
 public:
    std::string GetShaderString(
        TexCoordPrecision precision, SamplerType sampler) const;
};

class FragmentShaderRGBATexOpaque : public FragmentTexOpaqueBinding {
 public:
  std::string GetShaderString(
      TexCoordPrecision precision, SamplerType sampler) const;
};

class FragmentShaderRGBATex : public FragmentTexOpaqueBinding {
 public:
  std::string GetShaderString(
      TexCoordPrecision precision, SamplerType sampler) const;
};

// Swizzles the red and blue component of sampled texel with alpha.
class FragmentShaderRGBATexSwizzleAlpha : public FragmentTexAlphaBinding {
 public:
  std::string GetShaderString(
      TexCoordPrecision precision, SamplerType sampler) const;
};

// Swizzles the red and blue component of sampled texel without alpha.
class FragmentShaderRGBATexSwizzleOpaque : public FragmentTexOpaqueBinding {
 public:
  std::string GetShaderString(
      TexCoordPrecision precision, SamplerType sampler) const;
};

class FragmentShaderRGBATexAlphaAA : public FragmentTexBlendMode {
 public:
  FragmentShaderRGBATexAlphaAA();

  void Init(gpu::gles2::GLES2Interface* context,
            unsigned program,
            int* base_uniform_index);
  std::string GetShaderString(
      TexCoordPrecision precision, SamplerType sampler) const;

  int alpha_location() const { return alpha_location_; }
  int sampler_location() const { return sampler_location_; }

 private:
  int sampler_location_;
  int alpha_location_;

  DISALLOW_COPY_AND_ASSIGN(FragmentShaderRGBATexAlphaAA);
};

class FragmentTexClampAlphaAABinding : public FragmentTexBlendMode {
 public:
  FragmentTexClampAlphaAABinding();

  void Init(gpu::gles2::GLES2Interface* context,
            unsigned program,
            int* base_uniform_index);
  int alpha_location() const { return alpha_location_; }
  int sampler_location() const { return sampler_location_; }
  int fragment_tex_transform_location() const {
    return fragment_tex_transform_location_;
  }

 private:
  int sampler_location_;
  int alpha_location_;
  int fragment_tex_transform_location_;

  DISALLOW_COPY_AND_ASSIGN(FragmentTexClampAlphaAABinding);
};

class FragmentShaderRGBATexClampAlphaAA
    : public FragmentTexClampAlphaAABinding {
 public:
  std::string GetShaderString(
      TexCoordPrecision precision, SamplerType sampler) const;
};

// Swizzles the red and blue component of sampled texel.
class FragmentShaderRGBATexClampSwizzleAlphaAA
    : public FragmentTexClampAlphaAABinding {
 public:
  std::string GetShaderString(
      TexCoordPrecision precision, SamplerType sampler) const;
};

class FragmentShaderRGBATexAlphaMask : public FragmentTexBlendMode {
 public:
  FragmentShaderRGBATexAlphaMask();
  std::string GetShaderString(
      TexCoordPrecision precision, SamplerType sampler) const;

  void Init(gpu::gles2::GLES2Interface* context,
            unsigned program,
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

class FragmentShaderRGBATexAlphaMaskAA : public FragmentTexBlendMode {
 public:
  FragmentShaderRGBATexAlphaMaskAA();
  std::string GetShaderString(
      TexCoordPrecision precision, SamplerType sampler) const;

  void Init(gpu::gles2::GLES2Interface* context,
            unsigned program,
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

  DISALLOW_COPY_AND_ASSIGN(FragmentShaderRGBATexAlphaMaskAA);
};

class FragmentShaderRGBATexAlphaMaskColorMatrixAA
    : public FragmentTexBlendMode {
 public:
  FragmentShaderRGBATexAlphaMaskColorMatrixAA();
  std::string GetShaderString(
      TexCoordPrecision precision, SamplerType sampler) const;

  void Init(gpu::gles2::GLES2Interface* context,
            unsigned program,
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

class FragmentShaderRGBATexAlphaColorMatrixAA : public FragmentTexBlendMode {
 public:
  FragmentShaderRGBATexAlphaColorMatrixAA();
  std::string GetShaderString(
      TexCoordPrecision precision, SamplerType sampler) const;

  void Init(gpu::gles2::GLES2Interface* context,
            unsigned program,
            int* base_uniform_index);
  int alpha_location() const { return alpha_location_; }
  int sampler_location() const { return sampler_location_; }
  int color_matrix_location() const { return color_matrix_location_; }
  int color_offset_location() const { return color_offset_location_; }

 private:
  int sampler_location_;
  int alpha_location_;
  int color_matrix_location_;
  int color_offset_location_;
};

class FragmentShaderRGBATexAlphaMaskColorMatrix : public FragmentTexBlendMode {
 public:
  FragmentShaderRGBATexAlphaMaskColorMatrix();
  std::string GetShaderString(
      TexCoordPrecision precision, SamplerType sampler) const;

  void Init(gpu::gles2::GLES2Interface* context,
            unsigned program,
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

class FragmentShaderYUVVideo : public FragmentTexBlendMode {
 public:
  FragmentShaderYUVVideo();
  std::string GetShaderString(
      TexCoordPrecision precision, SamplerType sampler) const;

  void Init(gpu::gles2::GLES2Interface* context,
            unsigned program,
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

class FragmentShaderYUVAVideo : public FragmentTexBlendMode {
 public:
  FragmentShaderYUVAVideo();
  std::string GetShaderString(
      TexCoordPrecision precision, SamplerType sampler) const;

  void Init(gpu::gles2::GLES2Interface* context,
            unsigned program,
            int* base_uniform_index);

  int y_texture_location() const { return y_texture_location_; }
  int u_texture_location() const { return u_texture_location_; }
  int v_texture_location() const { return v_texture_location_; }
  int a_texture_location() const { return a_texture_location_; }
  int alpha_location() const { return alpha_location_; }
  int yuv_matrix_location() const { return yuv_matrix_location_; }
  int yuv_adj_location() const { return yuv_adj_location_; }

 private:
  int y_texture_location_;
  int u_texture_location_;
  int v_texture_location_;
  int a_texture_location_;
  int alpha_location_;
  int yuv_matrix_location_;
  int yuv_adj_location_;

  DISALLOW_COPY_AND_ASSIGN(FragmentShaderYUVAVideo);
};

class FragmentShaderColor : public FragmentTexBlendMode {
 public:
  FragmentShaderColor();
  std::string GetShaderString(
      TexCoordPrecision precision, SamplerType sampler) const;

  void Init(gpu::gles2::GLES2Interface* context,
            unsigned program,
            int* base_uniform_index);
  int color_location() const { return color_location_; }

 private:
  int color_location_;

  DISALLOW_COPY_AND_ASSIGN(FragmentShaderColor);
};

class FragmentShaderColorAA : public FragmentTexBlendMode {
 public:
  FragmentShaderColorAA();
  std::string GetShaderString(
      TexCoordPrecision precision, SamplerType sampler) const;

  void Init(gpu::gles2::GLES2Interface* context,
            unsigned program,
            int* base_uniform_index);
  int color_location() const { return color_location_; }

 private:
  int color_location_;

  DISALLOW_COPY_AND_ASSIGN(FragmentShaderColorAA);
};

class FragmentShaderCheckerboard : public FragmentTexBlendMode {
 public:
  FragmentShaderCheckerboard();
  std::string GetShaderString(
      TexCoordPrecision precision, SamplerType sampler) const;

  void Init(gpu::gles2::GLES2Interface* context,
            unsigned program,
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
