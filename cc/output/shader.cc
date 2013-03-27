// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/output/shader.h"

#include "base/basictypes.h"
#include "base/logging.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebGraphicsContext3D.h"

#define SHADER0(Src) #Src
#define SHADER(Src) SHADER0(Src)

using WebKit::WebGraphicsContext3D;

namespace cc {

namespace {

static void GetProgramUniformLocations(WebGraphicsContext3D* context,
                                       unsigned program,
                                       const char** shader_uniforms,
                                       size_t count,
                                       size_t max_locations,
                                       int* locations,
                                       bool using_bind_uniform,
                                       int* base_uniform_index) {
  for (size_t uniform_index = 0; uniform_index < count; uniform_index ++) {
    DCHECK(uniform_index < max_locations);

    if (using_bind_uniform) {
      locations[uniform_index] = (*base_uniform_index)++;
      context->bindUniformLocationCHROMIUM(program,
                                           locations[uniform_index],
                                           shader_uniforms[uniform_index]);
    } else {
      locations[uniform_index] =
          context->getUniformLocation(program, shader_uniforms[uniform_index]);
    }
  }
}

}

VertexShaderPosTex::VertexShaderPosTex()
      : matrix_location_(-1) {}

void VertexShaderPosTex::Init(WebGraphicsContext3D* context,
                              unsigned program,
                              bool using_bind_uniform,
                              int* base_uniform_index) {
  static const char* shader_uniforms[] = {
      "matrix",
  };
  int locations[1];

  GetProgramUniformLocations(context,
                             program,
                             shader_uniforms,
                             arraysize(shader_uniforms),
                             arraysize(locations),
                             locations,
                             using_bind_uniform,
                             base_uniform_index);

  matrix_location_ = locations[0];
  DCHECK(matrix_location_ != -1);
}

std::string VertexShaderPosTex::GetShaderString() const {
  return SHADER(
    attribute vec4 a_position;
    attribute vec2 a_texCoord;
    uniform mat4 matrix;
    varying vec2 v_texCoord;
    void main() {
      gl_Position = matrix * a_position;
      v_texCoord = a_texCoord;
    }
  );
}

VertexShaderPosTexYUVStretch::VertexShaderPosTexYUVStretch()
    : matrix_location_(-1),
      tex_scale_location_(-1) {}

void VertexShaderPosTexYUVStretch::Init(WebGraphicsContext3D* context,
                                        unsigned program,
                                        bool using_bind_uniform,
                                        int* base_uniform_index) {
  static const char* shader_uniforms[] = {
    "matrix",
    "texScale",
  };
  int locations[2];

  GetProgramUniformLocations(context,
                             program,
                             shader_uniforms,
                             arraysize(shader_uniforms),
                             arraysize(locations),
                             locations,
                             using_bind_uniform,
                             base_uniform_index);

  matrix_location_ = locations[0];
  tex_scale_location_ = locations[1];
  DCHECK(matrix_location_ != -1 && tex_scale_location_ != -1);
}

std::string VertexShaderPosTexYUVStretch::GetShaderString() const {
  return SHADER(
    precision mediump float;
    attribute vec4 a_position;
    attribute vec2 a_texCoord;
    uniform mat4 matrix;
    varying vec2 v_texCoord;
    uniform vec2 texScale;
    void main() {
        gl_Position = matrix * a_position;
        v_texCoord = a_texCoord * texScale;
    }
  );
}

VertexShaderPos::VertexShaderPos()
    : matrix_location_(-1) {}

void VertexShaderPos::Init(WebGraphicsContext3D* context,
                           unsigned program,
                           bool using_bind_uniform,
                           int* base_uniform_index) {
  static const char* shader_uniforms[] = {
      "matrix",
  };
  int locations[1];

  GetProgramUniformLocations(context,
                             program,
                             shader_uniforms,
                             arraysize(shader_uniforms),
                             arraysize(locations),
                             locations,
                             using_bind_uniform,
                             base_uniform_index);

  matrix_location_ = locations[0];
  DCHECK(matrix_location_ != -1);
}

std::string VertexShaderPos::GetShaderString() const {
  return SHADER(
    attribute vec4 a_position;
    uniform mat4 matrix;
    void main() {
        gl_Position = matrix * a_position;
    }
  );
}

VertexShaderPosTexTransform::VertexShaderPosTexTransform()
    : matrix_location_(-1),
      tex_transform_location_(-1),
      vertex_opacity_location_(-1) {}

void VertexShaderPosTexTransform::Init(WebGraphicsContext3D* context,
                                       unsigned program,
                                       bool using_bind_uniform,
                                       int* base_uniform_index) {
  static const char* shader_uniforms[] = {
    "matrix",
    "texTransform",
    "opacity",
  };
  int locations[3];

  GetProgramUniformLocations(context,
                             program,
                             shader_uniforms,
                             arraysize(shader_uniforms),
                             arraysize(locations),
                             locations,
                             using_bind_uniform,
                             base_uniform_index);

  matrix_location_ = locations[0];
  tex_transform_location_ = locations[1];
  vertex_opacity_location_ = locations[2];
  DCHECK(matrix_location_ != -1 && tex_transform_location_ != -1 &&
         vertex_opacity_location_ != -1);
}

std::string VertexShaderPosTexTransform::GetShaderString() const {
  return SHADER(
    attribute vec4 a_position;
    attribute vec2 a_texCoord;
    attribute float a_index;
    uniform mat4 matrix[8];
    uniform vec4 texTransform[8];
    uniform float opacity[32];
    varying vec2 v_texCoord;
    varying float v_alpha;
    void main() {
      gl_Position = matrix[int(a_index * 0.25)] * a_position;
      vec4 texTrans = texTransform[int(a_index * 0.25)];
      v_texCoord = a_texCoord * texTrans.zw + texTrans.xy;
      v_alpha = opacity[int(a_index)];
    }
  );
}

std::string VertexShaderPosTexTransformFlip::GetShaderString() const {
  return SHADER(
    attribute vec4 a_position;
    attribute vec2 a_texCoord;
    attribute float a_index;
    uniform mat4 matrix[8];
    uniform vec4 texTransform[8];
    uniform float opacity[32];
    varying vec2 v_texCoord;
    varying float v_alpha;
    void main() {
      gl_Position = matrix[int(a_index * 0.25)] * a_position;
      vec4 texTrans = texTransform[int(a_index * 0.25)];
      v_texCoord = a_texCoord * texTrans.zw + texTrans.xy;
      v_texCoord.y = 1.0 - v_texCoord.y;
      v_alpha = opacity[int(a_index)];
    }
  );
}

std::string VertexShaderPosTexIdentity::GetShaderString() const {
  return SHADER(
    attribute vec4 a_position;
    varying vec2 v_texCoord;
    void main() {
      gl_Position = a_position;
      v_texCoord = (a_position.xy + vec2(1.0)) * 0.5;
    }
  );
}

VertexShaderQuad::VertexShaderQuad()
    : matrix_location_(-1),
      point_location_(-1),
      tex_scale_location_(-1) {}

void VertexShaderQuad::Init(WebGraphicsContext3D* context,
                            unsigned program,
                            bool using_bind_uniform,
                            int* base_uniform_index) {
  static const char* shader_uniforms[] = {
    "matrix",
    "point",
    "texScale",
  };
  int locations[3];

  GetProgramUniformLocations(context,
                             program,
                             shader_uniforms,
                             arraysize(shader_uniforms),
                             arraysize(locations),
                             locations,
                             using_bind_uniform,
                             base_uniform_index);

  matrix_location_ = locations[0];
  point_location_ = locations[1];
  tex_scale_location_ = locations[2];
  DCHECK_NE(matrix_location_, -1);
  DCHECK_NE(point_location_, -1);
  DCHECK_NE(tex_scale_location_, -1);
}

std::string VertexShaderQuad::GetShaderString() const {
  return SHADER(
    attribute vec4 a_position;
    attribute vec2 a_texCoord;
    uniform mat4 matrix;
    uniform vec2 point[4];
    uniform vec2 texScale;
    varying vec2 v_texCoord;
    void main() {
      vec2 complement = abs(a_texCoord - 1.0);
      vec4 pos = vec4(0.0, 0.0, a_position.z, a_position.w);
      pos.xy += (complement.x * complement.y) * point[0];
      pos.xy += (a_texCoord.x * complement.y) * point[1];
      pos.xy += (a_texCoord.x * a_texCoord.y) * point[2];
      pos.xy += (complement.x * a_texCoord.y) * point[3];
      gl_Position = matrix * pos;
      v_texCoord = (pos.xy + vec2(0.5)) * texScale;
    }
  );
}

VertexShaderTile::VertexShaderTile()
    : matrix_location_(-1),
      point_location_(-1),
      vertex_tex_transform_location_(-1) {}

void VertexShaderTile::Init(WebGraphicsContext3D* context,
                            unsigned program,
                            bool using_bind_uniform,
                            int* base_uniform_index) {
  static const char* shader_uniforms[] = {
    "matrix",
    "point",
    "vertexTexTransform",
  };
  int locations[3];

  GetProgramUniformLocations(context,
                             program,
                             shader_uniforms,
                             arraysize(shader_uniforms),
                             arraysize(locations),
                             locations,
                             using_bind_uniform,
                             base_uniform_index);

  matrix_location_ = locations[0];
  point_location_ = locations[1];
  vertex_tex_transform_location_ = locations[2];
  DCHECK(matrix_location_ != -1 && point_location_ != -1 &&
         vertex_tex_transform_location_ != -1);
}

std::string VertexShaderTile::GetShaderString() const {
  return SHADER(
    attribute vec4 a_position;
    attribute vec2 a_texCoord;
    uniform mat4 matrix;
    uniform vec2 point[4];
    uniform vec4 vertexTexTransform;
    varying vec2 v_texCoord;
    void main() {
      vec2 complement = abs(a_texCoord - 1.0);
      vec4 pos = vec4(0.0, 0.0, a_position.z, a_position.w);
      pos.xy += (complement.x * complement.y) * point[0];
      pos.xy += (a_texCoord.x * complement.y) * point[1];
      pos.xy += (a_texCoord.x * a_texCoord.y) * point[2];
      pos.xy += (complement.x * a_texCoord.y) * point[3];
      gl_Position = matrix * pos;
      v_texCoord = pos.xy * vertexTexTransform.zw + vertexTexTransform.xy;
    }
  );
}

VertexShaderVideoTransform::VertexShaderVideoTransform()
    : matrix_location_(-1),
      tex_matrix_location_(-1) {}

bool VertexShaderVideoTransform::Init(WebGraphicsContext3D* context,
                                      unsigned program,
                                      bool using_bind_uniform,
                                      int* base_uniform_index) {
  static const char* shader_uniforms[] = {
    "matrix",
    "texMatrix",
  };
  int locations[2];

  GetProgramUniformLocations(context,
                             program,
                             shader_uniforms,
                             arraysize(shader_uniforms),
                             arraysize(locations),
                             locations,
                             using_bind_uniform,
                             base_uniform_index);

  matrix_location_ = locations[0];
  tex_matrix_location_ = locations[1];
  return matrix_location_ != -1 && tex_matrix_location_ != -1;
}

std::string VertexShaderVideoTransform::GetShaderString() const {
  return SHADER(
    attribute vec4 a_position;
    attribute vec2 a_texCoord;
    uniform mat4 matrix;
    uniform mat4 texMatrix;
    varying vec2 v_texCoord;
    void main() {
        gl_Position = matrix * a_position;
        v_texCoord =
            vec2(texMatrix * vec4(a_texCoord.x, 1.0 - a_texCoord.y, 0.0, 1.0));
    }
  );
}

FragmentTexAlphaBinding::FragmentTexAlphaBinding()
    : sampler_location_(-1),
      alpha_location_(-1) {}

void FragmentTexAlphaBinding::Init(WebGraphicsContext3D* context,
                                   unsigned program,
                                   bool using_bind_uniform,
                                   int* base_uniform_index) {
  static const char* shader_uniforms[] = {
    "s_texture",
    "alpha",
  };
  int locations[2];

  GetProgramUniformLocations(context,
                             program,
                             shader_uniforms,
                             arraysize(shader_uniforms),
                             arraysize(locations),
                             locations,
                             using_bind_uniform,
                             base_uniform_index);

  sampler_location_ = locations[0];
  alpha_location_ = locations[1];
  DCHECK(sampler_location_ != -1 && alpha_location_ != -1);
}

FragmentTexOpaqueBinding::FragmentTexOpaqueBinding()
    : sampler_location_(-1) {}

void FragmentTexOpaqueBinding::Init(WebGraphicsContext3D* context,
                                    unsigned program,
                                    bool using_bind_uniform,
                                    int* base_uniform_index) {
  static const char* shader_uniforms[] = {
    "s_texture",
  };
  int locations[1];

  GetProgramUniformLocations(context,
                             program,
                             shader_uniforms,
                             arraysize(shader_uniforms),
                             arraysize(locations),
                             locations,
                             using_bind_uniform,
                             base_uniform_index);

  sampler_location_ = locations[0];
  DCHECK(sampler_location_ != -1);
}

FragmentShaderOESImageExternal::FragmentShaderOESImageExternal()
    : sampler_location_(-1) {}

bool FragmentShaderOESImageExternal::Init(WebGraphicsContext3D* context,
                                          unsigned program,
                                          bool using_bind_uniform,
                                          int* base_uniform_index) {
  static const char* shader_uniforms[] = {
    "s_texture",
  };
  int locations[1];

  GetProgramUniformLocations(context,
                             program,
                             shader_uniforms,
                             arraysize(shader_uniforms),
                             arraysize(locations),
                             locations,
                             using_bind_uniform,
                             base_uniform_index);

  sampler_location_ = locations[0];
  return sampler_location_ != -1;
}

std::string FragmentShaderOESImageExternal::GetShaderString() const {
  // Cannot use the SHADER() macro because of the '#' char
  return "#extension GL_OES_EGL_image_external : require\n"
      SHADER(
         precision mediump float;
         varying vec2 v_texCoord;
         uniform samplerExternalOES s_texture;
         void main() {
           vec4 texColor = texture2D(s_texture, v_texCoord);
           gl_FragColor = vec4(texColor.x, texColor.y, texColor.z, texColor.w);
         }
      );
}

std::string FragmentShaderRGBATexAlpha::GetShaderString() const {
  return SHADER(
    precision mediump float;
    varying vec2 v_texCoord;
    uniform sampler2D s_texture;
    uniform float alpha;
    void main() {
      vec4 texColor = texture2D(s_texture, v_texCoord);
      gl_FragColor = texColor * alpha;
    }
  );
}

std::string FragmentShaderRGBATexVaryingAlpha::GetShaderString() const {
  return SHADER(
    precision mediump float;
    varying vec2 v_texCoord;
    varying float v_alpha;
    uniform sampler2D s_texture;
    void main() {
      vec4 texColor = texture2D(s_texture, v_texCoord);
      gl_FragColor = texColor * v_alpha;
    }
  );
}

std::string FragmentShaderRGBATexRectVaryingAlpha::GetShaderString() const {
  return "#extension GL_ARB_texture_rectangle : require\n"
      SHADER(
        precision mediump float;
        varying vec2 v_texCoord;
        varying float v_alpha;
        uniform sampler2DRect s_texture;
        void main() {
          vec4 texColor = texture2DRect(s_texture, v_texCoord);
          gl_FragColor = texColor * v_alpha;
        }
      );
}

std::string FragmentShaderRGBATexOpaque::GetShaderString() const {
  return SHADER(
    precision mediump float;
    varying vec2 v_texCoord;
    uniform sampler2D s_texture;
    void main() {
      vec4 texColor = texture2D(s_texture, v_texCoord);
      gl_FragColor = vec4(texColor.rgb, 1.0);
    }
  );
}

std::string FragmentShaderRGBATex::GetShaderString() const {
  return SHADER(
    precision mediump float;
    varying vec2 v_texCoord;
    uniform sampler2D s_texture;
    void main() {
      gl_FragColor = texture2D(s_texture, v_texCoord);
    }
  );
}

std::string FragmentShaderRGBATexSwizzleAlpha::GetShaderString() const {
  return SHADER(
    precision mediump float;
    varying vec2 v_texCoord;
    uniform sampler2D s_texture;
    uniform float alpha;
    void main() {
        vec4 texColor = texture2D(s_texture, v_texCoord);
        gl_FragColor =
            vec4(texColor.z, texColor.y, texColor.x, texColor.w) * alpha;
    }
  );
}

std::string FragmentShaderRGBATexSwizzleOpaque::GetShaderString() const {
  return SHADER(
    precision mediump float;
    varying vec2 v_texCoord;
    uniform sampler2D s_texture;
    void main() {
      vec4 texColor = texture2D(s_texture, v_texCoord);
      gl_FragColor = vec4(texColor.z, texColor.y, texColor.x, 1.0);
    }
  );
}

FragmentShaderRGBATexAlphaAA::FragmentShaderRGBATexAlphaAA()
    : sampler_location_(-1),
      alpha_location_(-1),
      edge_location_(-1) {}

void FragmentShaderRGBATexAlphaAA::Init(WebGraphicsContext3D* context,
                                        unsigned program,
                                        bool using_bind_uniform,
                                        int* base_uniform_index) {
  static const char* shader_uniforms[] = {
    "s_texture",
    "alpha",
    "edge",
  };
  int locations[3];

  GetProgramUniformLocations(context,
                             program,
                             shader_uniforms,
                             arraysize(shader_uniforms),
                             arraysize(locations),
                             locations,
                             using_bind_uniform,
                             base_uniform_index);

  sampler_location_ = locations[0];
  alpha_location_ = locations[1];
  edge_location_ = locations[2];
  DCHECK(sampler_location_ != -1 && alpha_location_ != -1 &&
         edge_location_ != -1);
}

std::string FragmentShaderRGBATexAlphaAA::GetShaderString() const {
  return SHADER(
    precision mediump float;
    varying vec2 v_texCoord;
    uniform sampler2D s_texture;
    uniform float alpha;
    uniform vec3 edge[8];
    void main() {
      vec4 texColor = texture2D(s_texture, v_texCoord);
      vec3 pos = vec3(gl_FragCoord.xy, 1);
      float a0 = clamp(dot(edge[0], pos), 0.0, 1.0);
      float a1 = clamp(dot(edge[1], pos), 0.0, 1.0);
      float a2 = clamp(dot(edge[2], pos), 0.0, 1.0);
      float a3 = clamp(dot(edge[3], pos), 0.0, 1.0);
      float a4 = clamp(dot(edge[4], pos), 0.0, 1.0);
      float a5 = clamp(dot(edge[5], pos), 0.0, 1.0);
      float a6 = clamp(dot(edge[6], pos), 0.0, 1.0);
      float a7 = clamp(dot(edge[7], pos), 0.0, 1.0);
      gl_FragColor = texColor * alpha * min(min(a0, a2) * min(a1, a3),
                                            min(a4, a6) * min(a5, a7));
    }
  );
}

FragmentTexClampAlphaAABinding::FragmentTexClampAlphaAABinding()
    : sampler_location_(-1),
      alpha_location_(-1),
      fragment_tex_transform_location_(-1),
      edge_location_(-1) {}

void FragmentTexClampAlphaAABinding::Init(WebGraphicsContext3D* context,
                                          unsigned program,
                                          bool using_bind_uniform,
                                          int* base_uniform_index) {
  static const char* shader_uniforms[] = {
    "s_texture",
    "alpha",
    "fragmentTexTransform",
    "edge",
  };
  int locations[4];

  GetProgramUniformLocations(context,
                             program,
                             shader_uniforms,
                             arraysize(shader_uniforms),
                             arraysize(locations),
                             locations,
                             using_bind_uniform,
                             base_uniform_index);

  sampler_location_ = locations[0];
  alpha_location_ = locations[1];
  fragment_tex_transform_location_ = locations[2];
  edge_location_ = locations[3];
  DCHECK(sampler_location_ != -1 && alpha_location_ != -1 &&
         fragment_tex_transform_location_ != -1 && edge_location_ != -1);
}

std::string FragmentShaderRGBATexClampAlphaAA::GetShaderString() const {
  return SHADER(
    precision mediump float;
    varying vec2 v_texCoord;
    uniform sampler2D s_texture;
    uniform float alpha;
    uniform vec4 fragmentTexTransform;
    uniform vec3 edge[8];
    void main() {
      vec2 texCoord = clamp(v_texCoord, 0.0, 1.0) * fragmentTexTransform.zw +
          fragmentTexTransform.xy;
      vec4 texColor = texture2D(s_texture, texCoord);
      vec3 pos = vec3(gl_FragCoord.xy, 1);
      float a0 = clamp(dot(edge[0], pos), 0.0, 1.0);
      float a1 = clamp(dot(edge[1], pos), 0.0, 1.0);
      float a2 = clamp(dot(edge[2], pos), 0.0, 1.0);
      float a3 = clamp(dot(edge[3], pos), 0.0, 1.0);
      float a4 = clamp(dot(edge[4], pos), 0.0, 1.0);
      float a5 = clamp(dot(edge[5], pos), 0.0, 1.0);
      float a6 = clamp(dot(edge[6], pos), 0.0, 1.0);
      float a7 = clamp(dot(edge[7], pos), 0.0, 1.0);
      gl_FragColor = texColor * alpha * min(min(a0, a2) * min(a1, a3),
                                            min(a4, a6) * min(a5, a7));
    }
  );
}

std::string FragmentShaderRGBATexClampSwizzleAlphaAA::GetShaderString() const {
  return SHADER(
    precision mediump float;
    varying vec2 v_texCoord;
    uniform sampler2D s_texture;
    uniform float alpha;
    uniform vec4 fragmentTexTransform;
    uniform vec3 edge[8];
    void main() {
      vec2 texCoord = clamp(v_texCoord, 0.0, 1.0) * fragmentTexTransform.zw +
          fragmentTexTransform.xy;
      vec4 texColor = texture2D(s_texture, texCoord);
      vec3 pos = vec3(gl_FragCoord.xy, 1);
      float a0 = clamp(dot(edge[0], pos), 0.0, 1.0);
      float a1 = clamp(dot(edge[1], pos), 0.0, 1.0);
      float a2 = clamp(dot(edge[2], pos), 0.0, 1.0);
      float a3 = clamp(dot(edge[3], pos), 0.0, 1.0);
      float a4 = clamp(dot(edge[4], pos), 0.0, 1.0);
      float a5 = clamp(dot(edge[5], pos), 0.0, 1.0);
      float a6 = clamp(dot(edge[6], pos), 0.0, 1.0);
      float a7 = clamp(dot(edge[7], pos), 0.0, 1.0);
      gl_FragColor = vec4(texColor.z, texColor.y, texColor.x, texColor.w) *
          alpha * min(min(a0, a2) * min(a1, a3), min(a4, a6) * min(a5, a7));
    }
  );
}

FragmentShaderRGBATexAlphaMask::FragmentShaderRGBATexAlphaMask()
    : sampler_location_(-1),
      mask_sampler_location_(-1),
      alpha_location_(-1),
      mask_tex_coord_scale_location_(-1) {}

void FragmentShaderRGBATexAlphaMask::Init(WebGraphicsContext3D* context,
                                          unsigned program,
                                          bool using_bind_uniform,
                                          int* base_uniform_index) {
  static const char* shader_uniforms[] = {
    "s_texture",
    "s_mask",
    "alpha",
    "maskTexCoordScale",
    "maskTexCoordOffset",
  };
  int locations[5];

  GetProgramUniformLocations(context,
                             program,
                             shader_uniforms,
                             arraysize(shader_uniforms),
                             arraysize(locations),
                             locations,
                             using_bind_uniform,
                             base_uniform_index);

  sampler_location_ = locations[0];
  mask_sampler_location_ = locations[1];
  alpha_location_ = locations[2];
  mask_tex_coord_scale_location_ = locations[3];
  mask_tex_coord_offset_location_ = locations[4];
  DCHECK(sampler_location_ != -1 && mask_sampler_location_ != -1 &&
         alpha_location_ != -1);
}

std::string FragmentShaderRGBATexAlphaMask::GetShaderString() const {
  return SHADER(
    precision mediump float;
    varying vec2 v_texCoord;
    uniform sampler2D s_texture;
    uniform sampler2D s_mask;
    uniform vec2 maskTexCoordScale;
    uniform vec2 maskTexCoordOffset;
    uniform float alpha;
    void main() {
      vec4 texColor = texture2D(s_texture, v_texCoord);
      vec2 maskTexCoord = vec2(maskTexCoordOffset.x + v_texCoord.x *
                               maskTexCoordScale.x,
                               maskTexCoordOffset.y + v_texCoord.y *
                               maskTexCoordScale.y);
      vec4 maskColor = texture2D(s_mask, maskTexCoord);
      gl_FragColor = vec4(texColor.x, texColor.y,
                          texColor.z, texColor.w) * alpha * maskColor.w;
    }
  );
}

FragmentShaderRGBATexAlphaMaskAA::FragmentShaderRGBATexAlphaMaskAA()
    : sampler_location_(-1),
      mask_sampler_location_(-1),
      alpha_location_(-1),
      edge_location_(-1),
      mask_tex_coord_scale_location_(-1) {}

void FragmentShaderRGBATexAlphaMaskAA::Init(WebGraphicsContext3D* context,
                                            unsigned program,
                                            bool using_bind_uniform,
                                            int* base_uniform_index) {
  static const char* shader_uniforms[] = {
    "s_texture",
    "s_mask",
    "alpha",
    "edge",
    "maskTexCoordScale",
    "maskTexCoordOffset",
  };
  int locations[6];

  GetProgramUniformLocations(context,
                             program,
                             shader_uniforms,
                             arraysize(shader_uniforms),
                             arraysize(locations),
                             locations,
                             using_bind_uniform,
                             base_uniform_index);

  sampler_location_ = locations[0];
  mask_sampler_location_ = locations[1];
  alpha_location_ = locations[2];
  edge_location_ = locations[3];
  mask_tex_coord_scale_location_ = locations[4];
  mask_tex_coord_offset_location_ = locations[5];
  DCHECK(sampler_location_ != -1 && mask_sampler_location_ != -1 &&
         alpha_location_ != -1 && edge_location_ != -1);
}

std::string FragmentShaderRGBATexAlphaMaskAA::GetShaderString() const {
  return SHADER(
    precision mediump float;
    varying vec2 v_texCoord;
    uniform sampler2D s_texture;
    uniform sampler2D s_mask;
    uniform vec2 maskTexCoordScale;
    uniform vec2 maskTexCoordOffset;
    uniform float alpha;
    uniform vec3 edge[8];
    void main() {
      vec4 texColor = texture2D(s_texture, v_texCoord);
      vec2 maskTexCoord = vec2(maskTexCoordOffset.x + v_texCoord.x *
                               maskTexCoordScale.x,
                               maskTexCoordOffset.y + v_texCoord.y *
                               maskTexCoordScale.y);
      vec4 maskColor = texture2D(s_mask, maskTexCoord);
      vec3 pos = vec3(gl_FragCoord.xy, 1);
      float a0 = clamp(dot(edge[0], pos), 0.0, 1.0);
      float a1 = clamp(dot(edge[1], pos), 0.0, 1.0);
      float a2 = clamp(dot(edge[2], pos), 0.0, 1.0);
      float a3 = clamp(dot(edge[3], pos), 0.0, 1.0);
      float a4 = clamp(dot(edge[4], pos), 0.0, 1.0);
      float a5 = clamp(dot(edge[5], pos), 0.0, 1.0);
      float a6 = clamp(dot(edge[6], pos), 0.0, 1.0);
      float a7 = clamp(dot(edge[7], pos), 0.0, 1.0);
      gl_FragColor = vec4(texColor.x, texColor.y, texColor.z, texColor.w) *
          alpha * maskColor.w * min(min(a0, a2) * min(a1, a3),
                                    min(a4, a6) * min(a5, a7));
    }
  );
}

FragmentShaderYUVVideo::FragmentShaderYUVVideo()
    : y_texture_location_(-1),
      u_texture_location_(-1),
      v_texture_location_(-1),
      alpha_location_(-1),
      yuv_matrix_location_(-1),
      yuv_adj_location_(-1) {}

void FragmentShaderYUVVideo::Init(WebGraphicsContext3D* context,
                                  unsigned program,
                                  bool using_bind_uniform,
                                  int* base_uniform_index) {
  static const char* shader_uniforms[] = {
    "y_texture",
    "u_texture",
    "v_texture",
    "alpha",
    "yuv_matrix",
    "yuv_adj",
  };
  int locations[6];

  GetProgramUniformLocations(context,
                             program,
                             shader_uniforms,
                             arraysize(shader_uniforms),
                             arraysize(locations),
                             locations,
                             using_bind_uniform,
                             base_uniform_index);

  y_texture_location_ = locations[0];
  u_texture_location_ = locations[1];
  v_texture_location_ = locations[2];
  alpha_location_ = locations[3];
  yuv_matrix_location_ = locations[4];
  yuv_adj_location_ = locations[5];

  DCHECK(y_texture_location_ != -1 && u_texture_location_ != -1 &&
         v_texture_location_ != -1 && alpha_location_ != -1 &&
         yuv_matrix_location_ != -1 && yuv_adj_location_ != -1);
}

std::string FragmentShaderYUVVideo::GetShaderString() const {
  return SHADER(
    precision mediump float;
    precision mediump int;
    varying vec2 v_texCoord;
    uniform sampler2D y_texture;
    uniform sampler2D u_texture;
    uniform sampler2D v_texture;
    uniform float alpha;
    uniform vec3 yuv_adj;
    uniform mat3 yuv_matrix;
    void main() {
      float y_raw = texture2D(y_texture, v_texCoord).x;
      float u_unsigned = texture2D(u_texture, v_texCoord).x;
      float v_unsigned = texture2D(v_texture, v_texCoord).x;
      vec3 yuv = vec3(y_raw, u_unsigned, v_unsigned) + yuv_adj;
      vec3 rgb = yuv_matrix * yuv;
      gl_FragColor = vec4(rgb, float(1)) * alpha;
    }
  );
}

FragmentShaderColor::FragmentShaderColor()
    : color_location_(-1) {}

void FragmentShaderColor::Init(WebGraphicsContext3D* context,
                               unsigned program,
                               bool using_bind_uniform,
                               int* base_uniform_index) {
  static const char* shader_uniforms[] = {
    "color",
  };
  int locations[1];

  GetProgramUniformLocations(context,
                             program,
                             shader_uniforms,
                             arraysize(shader_uniforms),
                             arraysize(locations),
                             locations,
                             using_bind_uniform,
                             base_uniform_index);

  color_location_ = locations[0];
  DCHECK(color_location_ != -1);
}

std::string FragmentShaderColor::GetShaderString() const {
  return SHADER(
    precision mediump float;
    uniform vec4 color;
    void main() {
      gl_FragColor = color;
    }
  );
}

FragmentShaderColorAA::FragmentShaderColorAA()
    : edge_location_(-1),
      color_location_(-1) {}

void FragmentShaderColorAA::Init(WebGraphicsContext3D* context,
                                 unsigned program,
                                 bool using_bind_uniform,
                                 int* base_uniform_index) {
  static const char* shader_uniforms[] = {
    "edge",
    "color",
  };
  int locations[2];

  GetProgramUniformLocations(context,
                             program,
                             shader_uniforms,
                             arraysize(shader_uniforms),
                             arraysize(locations),
                             locations,
                             using_bind_uniform,
                             base_uniform_index);

  edge_location_ = locations[0];
  color_location_ = locations[1];
  DCHECK(edge_location_ != -1 && color_location_ != -1);
}

std::string FragmentShaderColorAA::GetShaderString() const {
  return SHADER(
    precision mediump float;
    uniform vec4 color;
    uniform vec3 edge[8];
    void main() {
      vec3 pos = vec3(gl_FragCoord.xy, 1);
      float a0 = clamp(dot(edge[0], pos), 0.0, 1.0);
      float a1 = clamp(dot(edge[1], pos), 0.0, 1.0);
      float a2 = clamp(dot(edge[2], pos), 0.0, 1.0);
      float a3 = clamp(dot(edge[3], pos), 0.0, 1.0);
      float a4 = clamp(dot(edge[4], pos), 0.0, 1.0);
      float a5 = clamp(dot(edge[5], pos), 0.0, 1.0);
      float a6 = clamp(dot(edge[6], pos), 0.0, 1.0);
      float a7 = clamp(dot(edge[7], pos), 0.0, 1.0);
      gl_FragColor = color * min(min(a0, a2) * min(a1, a3),
                                 min(a4, a6) * min(a5, a7));
    }
  );
}

FragmentShaderCheckerboard::FragmentShaderCheckerboard()
    : alpha_location_(-1),
      tex_transform_location_(-1),
      frequency_location_(-1) {}

void FragmentShaderCheckerboard::Init(WebGraphicsContext3D* context,
                                      unsigned program,
                                      bool using_bind_uniform,
                                      int* base_uniform_index) {
  static const char* shader_uniforms[] = {
    "alpha",
    "texTransform",
    "frequency",
    "color",
  };
  int locations[4];

  GetProgramUniformLocations(context,
                             program,
                             shader_uniforms,
                             arraysize(shader_uniforms),
                             arraysize(locations),
                             locations,
                             using_bind_uniform,
                             base_uniform_index);

  alpha_location_ = locations[0];
  tex_transform_location_ = locations[1];
  frequency_location_ = locations[2];
  color_location_ = locations[3];
  DCHECK(alpha_location_ != -1 && tex_transform_location_ != -1 &&
         frequency_location_ != -1 && color_location_ != -1);
}

std::string FragmentShaderCheckerboard::GetShaderString() const {
  // Shader based on Example 13-17 of "OpenGL ES 2.0 Programming Guide"
  // by Munshi, Ginsburg, Shreiner.
  return SHADER(
    precision mediump float;
    precision mediump int;
    varying vec2 v_texCoord;
    uniform float alpha;
    uniform float frequency;
    uniform vec4 texTransform;
    uniform vec4 color;
    void main() {
      vec4 color1 = vec4(1.0, 1.0, 1.0, 1.0);
      vec4 color2 = color;
      vec2 texCoord = clamp(v_texCoord, 0.0, 1.0) * texTransform.zw +
          texTransform.xy;
      vec2 coord = mod(floor(texCoord * frequency * 2.0), 2.0);
      float picker = abs(coord.x - coord.y);
      gl_FragColor = mix(color1, color2, picker) * alpha;
    }
  );
}

}  // namespace cc
