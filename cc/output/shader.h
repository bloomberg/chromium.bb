// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_OUTPUT_SHADER_H_
#define CC_OUTPUT_SHADER_H_

#include <string>

#include "base/macros.h"
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
  TEX_COORD_PRECISION_NA = 0,
  TEX_COORD_PRECISION_MEDIUM = 1,
  TEX_COORD_PRECISION_HIGH = 2,
  LAST_TEX_COORD_PRECISION = 2
};

enum SamplerType {
  SAMPLER_TYPE_NA = 0,
  SAMPLER_TYPE_2D = 1,
  SAMPLER_TYPE_2D_RECT = 2,
  SAMPLER_TYPE_EXTERNAL_OES = 3,
  LAST_SAMPLER_TYPE = 3
};

enum BlendMode {
  BLEND_MODE_NONE,
  BLEND_MODE_NORMAL,
  BLEND_MODE_SCREEN,
  BLEND_MODE_OVERLAY,
  BLEND_MODE_DARKEN,
  BLEND_MODE_LIGHTEN,
  BLEND_MODE_COLOR_DODGE,
  BLEND_MODE_COLOR_BURN,
  BLEND_MODE_HARD_LIGHT,
  BLEND_MODE_SOFT_LIGHT,
  BLEND_MODE_DIFFERENCE,
  BLEND_MODE_EXCLUSION,
  BLEND_MODE_MULTIPLY,
  BLEND_MODE_HUE,
  BLEND_MODE_SATURATION,
  BLEND_MODE_COLOR,
  BLEND_MODE_LUMINOSITY,
  LAST_BLEND_MODE = BLEND_MODE_LUMINOSITY
};

enum InputColorSource {
  INPUT_COLOR_SOURCE_RGBA_TEXTURE,
  INPUT_COLOR_SOURCE_UNIFORM,
};

// TODO(ccameron): Merge this with BlendMode.
enum FragColorMode {
  FRAG_COLOR_MODE_DEFAULT,
  FRAG_COLOR_MODE_OPAQUE,
  FRAG_COLOR_MODE_APPLY_BLEND_MODE,
};

enum MaskMode {
  NO_MASK = 0,
  HAS_MASK = 1,
  LAST_MASK_VALUE = HAS_MASK
};

struct ShaderLocations {
  ShaderLocations();

  int sampler = -1;
  int quad = -1;
  int edge = -1;
  int viewport = -1;
  int mask_sampler = -1;
  int mask_tex_coord_scale = -1;
  int mask_tex_coord_offset = -1;
  int matrix = -1;
  int alpha = -1;
  int color_matrix = -1;
  int color_offset = -1;
  int tex_transform = -1;
  int backdrop = -1;
  int backdrop_rect = -1;
  int original_backdrop = -1;
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
  static std::string GetShaderHead();
  static std::string GetShaderBody();

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
  static std::string GetShaderHead();
  static std::string GetShaderBody();

  int matrix_location() const { return matrix_location_; }
  int ya_tex_scale_location() const { return ya_tex_scale_location_; }
  int ya_tex_offset_location() const { return ya_tex_offset_location_; }
  int uv_tex_scale_location() const { return uv_tex_scale_location_; }
  int uv_tex_offset_location() const { return uv_tex_offset_location_; }

 private:
  int matrix_location_;
  int ya_tex_scale_location_;
  int ya_tex_offset_location_;
  int uv_tex_scale_location_;
  int uv_tex_offset_location_;

  DISALLOW_COPY_AND_ASSIGN(VertexShaderPosTexYUVStretchOffset);
};

class VertexShaderPos {
 public:
  VertexShaderPos();

  void Init(gpu::gles2::GLES2Interface* context,
            unsigned program,
            int* base_uniform_index);
  std::string GetShaderString() const;
  static std::string GetShaderHead();
  static std::string GetShaderBody();

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
  static std::string GetShaderHead();
  static std::string GetShaderBody();
};

class VertexShaderPosTexTransform {
 public:
  VertexShaderPosTexTransform();

  void Init(gpu::gles2::GLES2Interface* context,
            unsigned program,
            int* base_uniform_index);
  std::string GetShaderString() const;
  static std::string GetShaderHead();
  static std::string GetShaderBody();
  void FillLocations(ShaderLocations* locations) const;

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
  static std::string GetShaderHead();
  static std::string GetShaderBody();

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
  static std::string GetShaderHead();
  static std::string GetShaderBody();

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
  static std::string GetShaderHead();
  static std::string GetShaderBody();
  void FillLocations(ShaderLocations* locations) const;

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
  static std::string GetShaderHead();
  static std::string GetShaderBody();

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
  static std::string GetShaderHead();
  static std::string GetShaderBody();

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
  static std::string GetShaderHead();
  static std::string GetShaderBody();

  int matrix_location() const { return matrix_location_; }
  int tex_matrix_location() const { return tex_matrix_location_; }

 private:
  int matrix_location_;
  int tex_matrix_location_;

  DISALLOW_COPY_AND_ASSIGN(VertexShaderVideoTransform);
};

class FragmentShaderBase {
 public:
  virtual void Init(gpu::gles2::GLES2Interface* context,
                    unsigned program,
                    int* base_uniform_index);
  std::string GetShaderString(TexCoordPrecision precision,
                              SamplerType sampler) const;
  void FillLocations(ShaderLocations* locations) const;

  BlendMode blend_mode() const { return blend_mode_; }
  void set_blend_mode(BlendMode blend_mode) { blend_mode_ = blend_mode; }
  bool has_blend_mode() const { return blend_mode_ != BLEND_MODE_NONE; }
  void set_mask_for_background(bool mask_for_background) {
    mask_for_background_ = mask_for_background;
  }
  bool mask_for_background() const { return mask_for_background_; }

  int sampler_location() const { return sampler_location_; }
  int alpha_location() const { return alpha_location_; }
  int color_location() const { return color_location_; }
  int background_color_location() const { return background_color_location_; }
  int fragment_tex_transform_location() const {
    return fragment_tex_transform_location_;
  }

 protected:
  FragmentShaderBase();
  virtual std::string GetShaderSource() const;

  std::string SetBlendModeFunctions(const std::string& shader_string) const;

  // Settings that are modified by sub-classes.
  bool has_aa_ = false;
  bool has_varying_alpha_ = false;
  bool has_swizzle_ = false;
  bool has_premultiply_alpha_ = false;
  FragColorMode frag_color_mode_ = FRAG_COLOR_MODE_DEFAULT;
  InputColorSource input_color_type_ = INPUT_COLOR_SOURCE_RGBA_TEXTURE;

  // Used only if |blend_mode_| is not BLEND_MODE_NONE.
  int backdrop_location_ = -1;
  int original_backdrop_location_ = -1;
  int backdrop_rect_location_ = -1;

  // Used only if |input_color_type_| is INPUT_COLOR_SOURCE_RGBA_TEXTURE.
  bool has_rgba_fragment_tex_transform_ = false;
  int sampler_location_ = -1;
  int fragment_tex_transform_location_ = -1;

  // Always use sampler2D and texture2D for the RGBA texture, regardless of the
  // specified SamplerType.
  // TODO(ccameron): Change GLRenderer to always specify the correct
  // SamplerType.
  bool ignore_sampler_type_ = false;

  // Used only if |input_color_type_| is INPUT_COLOR_SOURCE_UNIFORM.
  int color_location_ = -1;

  bool has_mask_sampler_ = false;
  int mask_sampler_location_ = -1;
  int mask_tex_coord_scale_location_ = -1;
  int mask_tex_coord_offset_location_ = -1;

  bool has_color_matrix_ = false;
  int color_matrix_location_ = -1;
  int color_offset_location_ = -1;

  bool has_uniform_alpha_ = false;
  int alpha_location_ = -1;

  bool has_background_color_ = false;
  int background_color_location_ = -1;

 private:
  BlendMode blend_mode_ = BLEND_MODE_NONE;
  bool mask_for_background_ = false;

  std::string GetHelperFunctions() const;
  std::string GetBlendFunction() const;
  std::string GetBlendFunctionBodyForRGB() const;

  DISALLOW_COPY_AND_ASSIGN(FragmentShaderBase);
};

class FragmentShaderRGBATexVaryingAlpha : public FragmentShaderBase {
 public:
  FragmentShaderRGBATexVaryingAlpha() {
    has_varying_alpha_ = true;
    frag_color_mode_ = FRAG_COLOR_MODE_DEFAULT;
  }
};

class FragmentShaderRGBATexPremultiplyAlpha : public FragmentShaderBase {
 public:
  FragmentShaderRGBATexPremultiplyAlpha() {
    has_varying_alpha_ = true;
    has_premultiply_alpha_ = true;
    frag_color_mode_ = FRAG_COLOR_MODE_DEFAULT;
  }
};

class FragmentShaderTexBackgroundVaryingAlpha : public FragmentShaderBase {
 public:
  FragmentShaderTexBackgroundVaryingAlpha() {
    has_background_color_ = true;
    has_varying_alpha_ = true;
    frag_color_mode_ = FRAG_COLOR_MODE_DEFAULT;
  }
};

class FragmentShaderTexBackgroundPremultiplyAlpha : public FragmentShaderBase {
 public:
  FragmentShaderTexBackgroundPremultiplyAlpha() {
    has_background_color_ = true;
    has_varying_alpha_ = true;
    has_premultiply_alpha_ = true;
    frag_color_mode_ = FRAG_COLOR_MODE_DEFAULT;
  }
};

class FragmentShaderRGBATexAlpha : public FragmentShaderBase {
 public:
  FragmentShaderRGBATexAlpha() {
    has_uniform_alpha_ = true;
    frag_color_mode_ = FRAG_COLOR_MODE_APPLY_BLEND_MODE;
  }
};

class FragmentShaderRGBATexColorMatrixAlpha : public FragmentShaderBase {
 public:
  FragmentShaderRGBATexColorMatrixAlpha() {
    has_uniform_alpha_ = true;
    has_color_matrix_ = true;
    frag_color_mode_ = FRAG_COLOR_MODE_APPLY_BLEND_MODE;
  }
};

class FragmentShaderRGBATexOpaque : public FragmentShaderBase {
 public:
  FragmentShaderRGBATexOpaque() { frag_color_mode_ = FRAG_COLOR_MODE_OPAQUE; }
};

class FragmentShaderRGBATex : public FragmentShaderBase {
 public:
  FragmentShaderRGBATex() { frag_color_mode_ = FRAG_COLOR_MODE_DEFAULT; }
};

// Swizzles the red and blue component of sampled texel with alpha.
class FragmentShaderRGBATexSwizzleAlpha : public FragmentShaderBase {
 public:
  FragmentShaderRGBATexSwizzleAlpha() {
    has_uniform_alpha_ = true;
    has_swizzle_ = true;
    frag_color_mode_ = FRAG_COLOR_MODE_DEFAULT;
  }
};

// Swizzles the red and blue component of sampled texel without alpha.
class FragmentShaderRGBATexSwizzleOpaque : public FragmentShaderBase {
 public:
  FragmentShaderRGBATexSwizzleOpaque() {
    has_swizzle_ = true;
    frag_color_mode_ = FRAG_COLOR_MODE_OPAQUE;
  }
};

class FragmentShaderRGBATexAlphaAA : public FragmentShaderBase {
 public:
  FragmentShaderRGBATexAlphaAA() {
    has_aa_ = true;
    has_uniform_alpha_ = true;
    frag_color_mode_ = FRAG_COLOR_MODE_APPLY_BLEND_MODE;
  }
};

class FragmentShaderRGBATexClampAlphaAA : public FragmentShaderBase {
 public:
  FragmentShaderRGBATexClampAlphaAA() {
    has_aa_ = true;
    has_uniform_alpha_ = true;
    has_rgba_fragment_tex_transform_ = true;
    frag_color_mode_ = FRAG_COLOR_MODE_DEFAULT;
  }
};

// Swizzles the red and blue component of sampled texel.
class FragmentShaderRGBATexClampSwizzleAlphaAA : public FragmentShaderBase {
 public:
  FragmentShaderRGBATexClampSwizzleAlphaAA() {
    has_aa_ = true;
    has_uniform_alpha_ = true;
    has_rgba_fragment_tex_transform_ = true;
    has_swizzle_ = true;
    frag_color_mode_ = FRAG_COLOR_MODE_DEFAULT;
  }
};

class FragmentShaderRGBATexAlphaMask : public FragmentShaderBase {
 public:
  FragmentShaderRGBATexAlphaMask() {
    has_uniform_alpha_ = true;
    has_mask_sampler_ = true;
    frag_color_mode_ = FRAG_COLOR_MODE_APPLY_BLEND_MODE;
    ignore_sampler_type_ = true;
  }
};

class FragmentShaderRGBATexAlphaMaskAA : public FragmentShaderBase {
 public:
  FragmentShaderRGBATexAlphaMaskAA() {
    has_aa_ = true;
    has_uniform_alpha_ = true;
    has_mask_sampler_ = true;
    frag_color_mode_ = FRAG_COLOR_MODE_APPLY_BLEND_MODE;
    ignore_sampler_type_ = true;
  }
};

class FragmentShaderRGBATexAlphaMaskColorMatrixAA : public FragmentShaderBase {
 public:
  FragmentShaderRGBATexAlphaMaskColorMatrixAA() {
    has_aa_ = true;
    has_uniform_alpha_ = true;
    has_mask_sampler_ = true;
    has_color_matrix_ = true;
    frag_color_mode_ = FRAG_COLOR_MODE_APPLY_BLEND_MODE;
    ignore_sampler_type_ = true;
  }
};

class FragmentShaderRGBATexAlphaColorMatrixAA : public FragmentShaderBase {
 public:
  FragmentShaderRGBATexAlphaColorMatrixAA() {
    has_aa_ = true;
    has_uniform_alpha_ = true;
    has_color_matrix_ = true;
    frag_color_mode_ = FRAG_COLOR_MODE_APPLY_BLEND_MODE;
  }
};

class FragmentShaderRGBATexAlphaMaskColorMatrix : public FragmentShaderBase {
 public:
  FragmentShaderRGBATexAlphaMaskColorMatrix() {
    has_uniform_alpha_ = true;
    has_mask_sampler_ = true;
    has_color_matrix_ = true;
    frag_color_mode_ = FRAG_COLOR_MODE_APPLY_BLEND_MODE;
    ignore_sampler_type_ = true;
  }
};

class FragmentShaderYUVVideo : public FragmentShaderBase {
 public:
  FragmentShaderYUVVideo();
  void SetFeatures(bool use_alpha_texture, bool use_nv12, bool use_color_lut);

  void Init(gpu::gles2::GLES2Interface* context,
            unsigned program,
            int* base_uniform_index) override;
  int y_texture_location() const { return y_texture_location_; }
  int u_texture_location() const { return u_texture_location_; }
  int v_texture_location() const { return v_texture_location_; }
  int uv_texture_location() const { return uv_texture_location_; }
  int a_texture_location() const { return a_texture_location_; }
  int lut_texture_location() const { return lut_texture_location_; }
  int alpha_location() const { return alpha_location_; }
  int yuv_matrix_location() const { return yuv_matrix_location_; }
  int yuv_adj_location() const { return yuv_adj_location_; }
  int ya_clamp_rect_location() const { return ya_clamp_rect_location_; }
  int uv_clamp_rect_location() const { return uv_clamp_rect_location_; }
  int resource_multiplier_location() const {
    return resource_multiplier_location_;
  }
  int resource_offset_location() const { return resource_offset_location_; }

 private:
  std::string GetShaderSource() const override;

  bool use_alpha_texture_ = false;
  bool use_nv12_ = false;
  bool use_color_lut_ = false;

  int y_texture_location_ = -1;
  int u_texture_location_ = -1;
  int v_texture_location_ = -1;
  int uv_texture_location_ = -1;
  int a_texture_location_ = -1;
  int lut_texture_location_ = -1;
  int alpha_location_ = -1;
  int yuv_matrix_location_ = -1;
  int yuv_adj_location_ = -1;
  int ya_clamp_rect_location_ = -1;
  int uv_clamp_rect_location_ = -1;
  int resource_multiplier_location_ = -1;
  int resource_offset_location_ = -1;
};

class FragmentShaderColor : public FragmentShaderBase {
 public:
  FragmentShaderColor() {
    input_color_type_ = INPUT_COLOR_SOURCE_UNIFORM;
    frag_color_mode_ = FRAG_COLOR_MODE_DEFAULT;
  }
};

class FragmentShaderColorAA : public FragmentShaderBase {
 public:
  FragmentShaderColorAA() {
    input_color_type_ = INPUT_COLOR_SOURCE_UNIFORM;
    has_aa_ = true;
    frag_color_mode_ = FRAG_COLOR_MODE_DEFAULT;
  }
};

}  // namespace cc

#endif  // CC_OUTPUT_SHADER_H_
