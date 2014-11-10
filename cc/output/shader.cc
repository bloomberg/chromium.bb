// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/output/shader.h"

#include <algorithm>

#include "base/basictypes.h"
#include "base/logging.h"
#include "cc/output/gl_renderer.h"  // For the GLC() macro.
#include "gpu/command_buffer/client/gles2_interface.h"

#define SHADER0(Src) #Src
#define VERTEX_SHADER(Src) SetVertexTexCoordPrecision(SHADER0(Src))
#define FRAGMENT_SHADER(Src)    \
  SetFragmentTexCoordPrecision( \
      precision,                \
      SetFragmentSamplerType(sampler, SetBlendModeFunctions(SHADER0(Src))))

using gpu::gles2::GLES2Interface;

namespace cc {

namespace {

static void GetProgramUniformLocations(GLES2Interface* context,
                                       unsigned program,
                                       size_t count,
                                       const char** uniforms,
                                       int* locations,
                                       int* base_uniform_index) {
  for (size_t i = 0; i < count; i++) {
    locations[i] = (*base_uniform_index)++;
    context->BindUniformLocationCHROMIUM(program, locations[i], uniforms[i]);
  }
}

static std::string SetFragmentTexCoordPrecision(
    TexCoordPrecision requested_precision,
    std::string shader_string) {
  switch (requested_precision) {
    case TexCoordPrecisionHigh:
      DCHECK_NE(shader_string.find("TexCoordPrecision"), std::string::npos);
      return "#ifdef GL_FRAGMENT_PRECISION_HIGH\n"
             "  #define TexCoordPrecision highp\n"
             "#else\n"
             "  #define TexCoordPrecision mediump\n"
             "#endif\n" +
             shader_string;
    case TexCoordPrecisionMedium:
      DCHECK_NE(shader_string.find("TexCoordPrecision"), std::string::npos);
      return "#define TexCoordPrecision mediump\n" + shader_string;
    case TexCoordPrecisionNA:
      DCHECK_EQ(shader_string.find("TexCoordPrecision"), std::string::npos);
      DCHECK_EQ(shader_string.find("texture2D"), std::string::npos);
      DCHECK_EQ(shader_string.find("texture2DRect"), std::string::npos);
      return shader_string;
    default:
      NOTREACHED();
      break;
  }
  return shader_string;
}

static std::string SetVertexTexCoordPrecision(const char* shader_string) {
  // We unconditionally use highp in the vertex shader since
  // we are unlikely to be vertex shader bound when drawing large quads.
  // Also, some vertex shaders mutate the texture coordinate in such a
  // way that the effective precision might be lower than expected.
  return "#define TexCoordPrecision highp\n" + std::string(shader_string);
}

TexCoordPrecision TexCoordPrecisionRequired(GLES2Interface* context,
                                            int* highp_threshold_cache,
                                            int highp_threshold_min,
                                            int x,
                                            int y) {
  if (*highp_threshold_cache == 0) {
    // Initialize range and precision with minimum spec values for when
    // GetShaderPrecisionFormat is a test stub.
    // TODO(brianderson): Implement better stubs of GetShaderPrecisionFormat
    // everywhere.
    GLint range[2] = {14, 14};
    GLint precision = 10;
    GLC(context,
        context->GetShaderPrecisionFormat(
            GL_FRAGMENT_SHADER, GL_MEDIUM_FLOAT, range, &precision));
    *highp_threshold_cache = 1 << precision;
  }

  int highp_threshold = std::max(*highp_threshold_cache, highp_threshold_min);
  if (x > highp_threshold || y > highp_threshold)
    return TexCoordPrecisionHigh;
  return TexCoordPrecisionMedium;
}

static std::string SetFragmentSamplerType(SamplerType requested_type,
                                          std::string shader_string) {
  switch (requested_type) {
    case SamplerType2D:
      DCHECK_NE(shader_string.find("SamplerType"), std::string::npos);
      DCHECK_NE(shader_string.find("TextureLookup"), std::string::npos);
      return "#define SamplerType sampler2D\n"
             "#define TextureLookup texture2D\n" +
             shader_string;
    case SamplerType2DRect:
      DCHECK_NE(shader_string.find("SamplerType"), std::string::npos);
      DCHECK_NE(shader_string.find("TextureLookup"), std::string::npos);
      return "#extension GL_ARB_texture_rectangle : require\n"
             "#define SamplerType sampler2DRect\n"
             "#define TextureLookup texture2DRect\n" +
             shader_string;
    case SamplerTypeExternalOES:
      DCHECK_NE(shader_string.find("SamplerType"), std::string::npos);
      DCHECK_NE(shader_string.find("TextureLookup"), std::string::npos);
      return "#extension GL_OES_EGL_image_external : require\n"
             "#define SamplerType samplerExternalOES\n"
             "#define TextureLookup texture2D\n" +
             shader_string;
    case SamplerTypeNA:
      DCHECK_EQ(shader_string.find("SamplerType"), std::string::npos);
      DCHECK_EQ(shader_string.find("TextureLookup"), std::string::npos);
      return shader_string;
    default:
      NOTREACHED();
      break;
  }
  return shader_string;
}

}  // namespace

TexCoordPrecision TexCoordPrecisionRequired(GLES2Interface* context,
                                            int* highp_threshold_cache,
                                            int highp_threshold_min,
                                            const gfx::Point& max_coordinate) {
  return TexCoordPrecisionRequired(context,
                                   highp_threshold_cache,
                                   highp_threshold_min,
                                   max_coordinate.x(),
                                   max_coordinate.y());
}

TexCoordPrecision TexCoordPrecisionRequired(GLES2Interface* context,
                                            int* highp_threshold_cache,
                                            int highp_threshold_min,
                                            const gfx::Size& max_size) {
  return TexCoordPrecisionRequired(context,
                                   highp_threshold_cache,
                                   highp_threshold_min,
                                   max_size.width(),
                                   max_size.height());
}

VertexShaderPosTex::VertexShaderPosTex() : matrix_location_(-1) {
}

void VertexShaderPosTex::Init(GLES2Interface* context,
                              unsigned program,
                              int* base_uniform_index) {
  static const char* uniforms[] = {
      "matrix",
  };
  int locations[arraysize(uniforms)];

  GetProgramUniformLocations(context,
                             program,
                             arraysize(uniforms),
                             uniforms,
                             locations,
                             base_uniform_index);
  matrix_location_ = locations[0];
}

std::string VertexShaderPosTex::GetShaderString() const {
  // clang-format off
  return VERTEX_SHADER(
      // clang-format on
      attribute vec4 a_position;
      attribute TexCoordPrecision vec2 a_texCoord;
      uniform mat4 matrix;
      varying TexCoordPrecision vec2 v_texCoord;
      void main() {
        gl_Position = matrix * a_position;
        v_texCoord = a_texCoord;
      }
      // clang-format off
  );  // NOLINT(whitespace/parens)
  // clang-format on
}

VertexShaderPosTexYUVStretchOffset::VertexShaderPosTexYUVStretchOffset()
    : matrix_location_(-1), tex_scale_location_(-1), tex_offset_location_(-1) {
}

void VertexShaderPosTexYUVStretchOffset::Init(GLES2Interface* context,
                                              unsigned program,
                                              int* base_uniform_index) {
  static const char* uniforms[] = {
      "matrix", "texScale", "texOffset",
  };
  int locations[arraysize(uniforms)];

  GetProgramUniformLocations(context,
                             program,
                             arraysize(uniforms),
                             uniforms,
                             locations,
                             base_uniform_index);
  matrix_location_ = locations[0];
  tex_scale_location_ = locations[1];
  tex_offset_location_ = locations[2];
}

std::string VertexShaderPosTexYUVStretchOffset::GetShaderString() const {
  // clang-format off
  return VERTEX_SHADER(
      // clang-format on
      precision mediump float;
      attribute vec4 a_position;
      attribute TexCoordPrecision vec2 a_texCoord;
      uniform mat4 matrix;
      varying TexCoordPrecision vec2 v_texCoord;
      uniform TexCoordPrecision vec2 texScale;
      uniform TexCoordPrecision vec2 texOffset;
      void main() {
        gl_Position = matrix * a_position;
        v_texCoord = a_texCoord * texScale + texOffset;
      }
      // clang-format off
  );  // NOLINT(whitespace/parens)
  // clang-format on
}

VertexShaderPos::VertexShaderPos() : matrix_location_(-1) {
}

void VertexShaderPos::Init(GLES2Interface* context,
                           unsigned program,
                           int* base_uniform_index) {
  static const char* uniforms[] = {
      "matrix",
  };
  int locations[arraysize(uniforms)];

  GetProgramUniformLocations(context,
                             program,
                             arraysize(uniforms),
                             uniforms,
                             locations,
                             base_uniform_index);
  matrix_location_ = locations[0];
}

std::string VertexShaderPos::GetShaderString() const {
  // clang-format off
  return VERTEX_SHADER(
      // clang-format on
      attribute vec4 a_position;
      uniform mat4 matrix;
      void main() { gl_Position = matrix * a_position; }
      // clang-format off
  );  // NOLINT(whitespace/parens)
  // clang-format on
}

VertexShaderPosTexTransform::VertexShaderPosTexTransform()
    : matrix_location_(-1),
      tex_transform_location_(-1),
      vertex_opacity_location_(-1) {
}

void VertexShaderPosTexTransform::Init(GLES2Interface* context,
                                       unsigned program,
                                       int* base_uniform_index) {
  static const char* uniforms[] = {
      "matrix", "texTransform", "opacity",
  };
  int locations[arraysize(uniforms)];

  GetProgramUniformLocations(context,
                             program,
                             arraysize(uniforms),
                             uniforms,
                             locations,
                             base_uniform_index);
  matrix_location_ = locations[0];
  tex_transform_location_ = locations[1];
  vertex_opacity_location_ = locations[2];
}

std::string VertexShaderPosTexTransform::GetShaderString() const {
  // clang-format off
  return VERTEX_SHADER(
      // clang-format on
      attribute vec4 a_position;
      attribute TexCoordPrecision vec2 a_texCoord;
      attribute float a_index;
      uniform mat4 matrix[8];
      uniform TexCoordPrecision vec4 texTransform[8];
      uniform float opacity[32];
      varying TexCoordPrecision vec2 v_texCoord;
      varying float v_alpha;
      void main() {
        int quad_index = int(a_index * 0.25);  // NOLINT
        gl_Position = matrix[quad_index] * a_position;
        TexCoordPrecision vec4 texTrans = texTransform[quad_index];
        v_texCoord = a_texCoord * texTrans.zw + texTrans.xy;
        v_alpha = opacity[int(a_index)];  // NOLINT
      }
      // clang-format off
  );  // NOLINT(whitespace/parens)
  // clang-format on
}

std::string VertexShaderPosTexIdentity::GetShaderString() const {
  // clang-format off
  return VERTEX_SHADER(
      // clang-format on
      attribute vec4 a_position;
      varying TexCoordPrecision vec2 v_texCoord;
      void main() {
        gl_Position = a_position;
        v_texCoord = (a_position.xy + vec2(1.0)) * 0.5;
      }
      // clang-format off
  );  // NOLINT(whitespace/parens)
  // clang-format on
}

VertexShaderQuad::VertexShaderQuad()
    : matrix_location_(-1), quad_location_(-1) {
}

void VertexShaderQuad::Init(GLES2Interface* context,
                            unsigned program,
                            int* base_uniform_index) {
  static const char* uniforms[] = {
      "matrix", "quad",
  };
  int locations[arraysize(uniforms)];

  GetProgramUniformLocations(context,
                             program,
                             arraysize(uniforms),
                             uniforms,
                             locations,
                             base_uniform_index);
  matrix_location_ = locations[0];
  quad_location_ = locations[1];
}

std::string VertexShaderQuad::GetShaderString() const {
#if defined(OS_ANDROID)
  // TODO(epenner): Find the cause of this 'quad' uniform
  // being missing if we don't add dummy variables.
  // http://crbug.com/240602
  // clang-format off
  return VERTEX_SHADER(
      // clang-format on
      attribute TexCoordPrecision vec4 a_position;
      attribute float a_index;
      uniform mat4 matrix;
      uniform TexCoordPrecision vec2 quad[4];
      uniform TexCoordPrecision vec2 dummy_uniform;
      varying TexCoordPrecision vec2 dummy_varying;
      void main() {
        vec2 pos = quad[int(a_index)];  // NOLINT
        gl_Position = matrix * vec4(pos, a_position.z, a_position.w);
        dummy_varying = dummy_uniform;
      }
      // clang-format off
  );  // NOLINT(whitespace/parens)
// clang-format on
#else
  // clang-format off
  return VERTEX_SHADER(
      // clang-format on
      attribute TexCoordPrecision vec4 a_position;
      attribute float a_index;
      uniform mat4 matrix;
      uniform TexCoordPrecision vec2 quad[4];
      void main() {
        vec2 pos = quad[int(a_index)];  // NOLINT
        gl_Position = matrix * vec4(pos, a_position.z, a_position.w);
      }
      // clang-format off
  );  // NOLINT(whitespace/parens)
// clang-format on
#endif
}

VertexShaderQuadAA::VertexShaderQuadAA()
    : matrix_location_(-1),
      viewport_location_(-1),
      quad_location_(-1),
      edge_location_(-1) {
}

void VertexShaderQuadAA::Init(GLES2Interface* context,
                              unsigned program,
                              int* base_uniform_index) {
  static const char* uniforms[] = {
      "matrix", "viewport", "quad", "edge",
  };
  int locations[arraysize(uniforms)];

  GetProgramUniformLocations(context,
                             program,
                             arraysize(uniforms),
                             uniforms,
                             locations,
                             base_uniform_index);
  matrix_location_ = locations[0];
  viewport_location_ = locations[1];
  quad_location_ = locations[2];
  edge_location_ = locations[3];
}

std::string VertexShaderQuadAA::GetShaderString() const {
  // clang-format off
  return VERTEX_SHADER(
      // clang-format on
      attribute TexCoordPrecision vec4 a_position;
      attribute float a_index;
      uniform mat4 matrix;
      uniform vec4 viewport;
      uniform TexCoordPrecision vec2 quad[4];
      uniform TexCoordPrecision vec3 edge[8];
      varying TexCoordPrecision vec4 edge_dist[2];  // 8 edge distances.

      void main() {
        vec2 pos = quad[int(a_index)];  // NOLINT
        gl_Position = matrix * vec4(pos, a_position.z, a_position.w);
        vec2 ndc_pos = 0.5 * (1.0 + gl_Position.xy / gl_Position.w);
        vec3 screen_pos = vec3(viewport.xy + viewport.zw * ndc_pos, 1.0);
        edge_dist[0] = vec4(dot(edge[0], screen_pos),
                            dot(edge[1], screen_pos),
                            dot(edge[2], screen_pos),
                            dot(edge[3], screen_pos)) *
                       gl_Position.w;
        edge_dist[1] = vec4(dot(edge[4], screen_pos),
                            dot(edge[5], screen_pos),
                            dot(edge[6], screen_pos),
                            dot(edge[7], screen_pos)) *
                       gl_Position.w;
      }
      // clang-format off
  );  // NOLINT(whitespace/parens)
  // clang-format on
}

VertexShaderQuadTexTransformAA::VertexShaderQuadTexTransformAA()
    : matrix_location_(-1),
      viewport_location_(-1),
      quad_location_(-1),
      edge_location_(-1),
      tex_transform_location_(-1) {
}

void VertexShaderQuadTexTransformAA::Init(GLES2Interface* context,
                                          unsigned program,
                                          int* base_uniform_index) {
  static const char* uniforms[] = {
      "matrix", "viewport", "quad", "edge", "texTrans",
  };
  int locations[arraysize(uniforms)];

  GetProgramUniformLocations(context,
                             program,
                             arraysize(uniforms),
                             uniforms,
                             locations,
                             base_uniform_index);
  matrix_location_ = locations[0];
  viewport_location_ = locations[1];
  quad_location_ = locations[2];
  edge_location_ = locations[3];
  tex_transform_location_ = locations[4];
}

std::string VertexShaderQuadTexTransformAA::GetShaderString() const {
  // clang-format off
  return VERTEX_SHADER(
      // clang-format on
      attribute TexCoordPrecision vec4 a_position;
      attribute float a_index;
      uniform mat4 matrix;
      uniform vec4 viewport;
      uniform TexCoordPrecision vec2 quad[4];
      uniform TexCoordPrecision vec3 edge[8];
      uniform TexCoordPrecision vec4 texTrans;
      varying TexCoordPrecision vec2 v_texCoord;
      varying TexCoordPrecision vec4 edge_dist[2];  // 8 edge distances.

      void main() {
        vec2 pos = quad[int(a_index)];  // NOLINT
        gl_Position = matrix * vec4(pos, a_position.z, a_position.w);
        vec2 ndc_pos = 0.5 * (1.0 + gl_Position.xy / gl_Position.w);
        vec3 screen_pos = vec3(viewport.xy + viewport.zw * ndc_pos, 1.0);
        edge_dist[0] = vec4(dot(edge[0], screen_pos),
                            dot(edge[1], screen_pos),
                            dot(edge[2], screen_pos),
                            dot(edge[3], screen_pos)) *
                       gl_Position.w;
        edge_dist[1] = vec4(dot(edge[4], screen_pos),
                            dot(edge[5], screen_pos),
                            dot(edge[6], screen_pos),
                            dot(edge[7], screen_pos)) *
                       gl_Position.w;
        v_texCoord = (pos.xy + vec2(0.5)) * texTrans.zw + texTrans.xy;
      }
      // clang-format off
  );  // NOLINT(whitespace/parens)
  // clang-format on
}

VertexShaderTile::VertexShaderTile()
    : matrix_location_(-1),
      quad_location_(-1),
      vertex_tex_transform_location_(-1) {
}

void VertexShaderTile::Init(GLES2Interface* context,
                            unsigned program,
                            int* base_uniform_index) {
  static const char* uniforms[] = {
      "matrix", "quad", "vertexTexTransform",
  };
  int locations[arraysize(uniforms)];

  GetProgramUniformLocations(context,
                             program,
                             arraysize(uniforms),
                             uniforms,
                             locations,
                             base_uniform_index);
  matrix_location_ = locations[0];
  quad_location_ = locations[1];
  vertex_tex_transform_location_ = locations[2];
}

std::string VertexShaderTile::GetShaderString() const {
  // clang-format off
  return VERTEX_SHADER(
      // clang-format on
      attribute TexCoordPrecision vec4 a_position;
      attribute TexCoordPrecision vec2 a_texCoord;
      attribute float a_index;
      uniform mat4 matrix;
      uniform TexCoordPrecision vec2 quad[4];
      uniform TexCoordPrecision vec4 vertexTexTransform;
      varying TexCoordPrecision vec2 v_texCoord;
      void main() {
        vec2 pos = quad[int(a_index)];  // NOLINT
        gl_Position = matrix * vec4(pos, a_position.z, a_position.w);
        v_texCoord = a_texCoord * vertexTexTransform.zw + vertexTexTransform.xy;
      }
      // clang-format off
  );  // NOLINT(whitespace/parens)
  // clang-format on
}

VertexShaderTileAA::VertexShaderTileAA()
    : matrix_location_(-1),
      viewport_location_(-1),
      quad_location_(-1),
      edge_location_(-1),
      vertex_tex_transform_location_(-1) {
}

void VertexShaderTileAA::Init(GLES2Interface* context,
                              unsigned program,
                              int* base_uniform_index) {
  static const char* uniforms[] = {
      "matrix", "viewport", "quad", "edge", "vertexTexTransform",
  };
  int locations[arraysize(uniforms)];

  GetProgramUniformLocations(context,
                             program,
                             arraysize(uniforms),
                             uniforms,
                             locations,
                             base_uniform_index);
  matrix_location_ = locations[0];
  viewport_location_ = locations[1];
  quad_location_ = locations[2];
  edge_location_ = locations[3];
  vertex_tex_transform_location_ = locations[4];
}

std::string VertexShaderTileAA::GetShaderString() const {
  // clang-format off
  return VERTEX_SHADER(
      // clang-format on
      attribute TexCoordPrecision vec4 a_position;
      attribute float a_index;
      uniform mat4 matrix;
      uniform vec4 viewport;
      uniform TexCoordPrecision vec2 quad[4];
      uniform TexCoordPrecision vec3 edge[8];
      uniform TexCoordPrecision vec4 vertexTexTransform;
      varying TexCoordPrecision vec2 v_texCoord;
      varying TexCoordPrecision vec4 edge_dist[2];  // 8 edge distances.

      void main() {
        vec2 pos = quad[int(a_index)];  // NOLINT
        gl_Position = matrix * vec4(pos, a_position.z, a_position.w);
        vec2 ndc_pos = 0.5 * (1.0 + gl_Position.xy / gl_Position.w);
        vec3 screen_pos = vec3(viewport.xy + viewport.zw * ndc_pos, 1.0);
        edge_dist[0] = vec4(dot(edge[0], screen_pos),
                            dot(edge[1], screen_pos),
                            dot(edge[2], screen_pos),
                            dot(edge[3], screen_pos)) *
                       gl_Position.w;
        edge_dist[1] = vec4(dot(edge[4], screen_pos),
                            dot(edge[5], screen_pos),
                            dot(edge[6], screen_pos),
                            dot(edge[7], screen_pos)) *
                       gl_Position.w;
        v_texCoord = pos.xy * vertexTexTransform.zw + vertexTexTransform.xy;
      }
      // clang-format off
  );  // NOLINT(whitespace/parens)
  // clang-format on
}

VertexShaderVideoTransform::VertexShaderVideoTransform()
    : matrix_location_(-1), tex_matrix_location_(-1) {
}

void VertexShaderVideoTransform::Init(GLES2Interface* context,
                                      unsigned program,
                                      int* base_uniform_index) {
  static const char* uniforms[] = {
      "matrix", "texMatrix",
  };
  int locations[arraysize(uniforms)];

  GetProgramUniformLocations(context,
                             program,
                             arraysize(uniforms),
                             uniforms,
                             locations,
                             base_uniform_index);
  matrix_location_ = locations[0];
  tex_matrix_location_ = locations[1];
}

std::string VertexShaderVideoTransform::GetShaderString() const {
  // clang-format off
  return VERTEX_SHADER(
      // clang-format on
      attribute vec4 a_position;
      attribute TexCoordPrecision vec2 a_texCoord;
      uniform mat4 matrix;
      uniform TexCoordPrecision mat4 texMatrix;
      varying TexCoordPrecision vec2 v_texCoord;
      void main() {
        gl_Position = matrix * a_position;
        v_texCoord =
            vec2(texMatrix * vec4(a_texCoord.x, 1.0 - a_texCoord.y, 0.0, 1.0));
      }
      // clang-format off
  );  // NOLINT(whitespace/parens)
  // clang-format on
}

#define BLEND_MODE_UNIFORMS "s_backdropTexture", "backdropRect"
#define UNUSED_BLEND_MODE_UNIFORMS (!has_blend_mode() ? 2 : 0)
#define BLEND_MODE_SET_LOCATIONS(X, POS)                   \
  if (has_blend_mode()) {                                  \
    DCHECK_LT(static_cast<size_t>(POS) + 1, arraysize(X)); \
    backdrop_location_ = locations[POS];                   \
    backdrop_rect_location_ = locations[POS + 1];          \
  }

FragmentTexBlendMode::FragmentTexBlendMode()
    : backdrop_location_(-1),
      backdrop_rect_location_(-1),
      blend_mode_(BlendModeNone) {
}

std::string FragmentTexBlendMode::SetBlendModeFunctions(
    std::string shader_string) const {
  if (shader_string.find("ApplyBlendMode") == std::string::npos)
    return shader_string;

  if (!has_blend_mode()) {
    return "#define ApplyBlendMode(X) (X)\n" + shader_string;
  }

  // clang-format off
  static const std::string kFunctionApplyBlendMode = SHADER0(
      // clang-format on
      uniform SamplerType s_backdropTexture;
      uniform TexCoordPrecision vec4 backdropRect;

      vec4 GetBackdropColor() {
        TexCoordPrecision vec2 bgTexCoord = gl_FragCoord.xy - backdropRect.xy;
        bgTexCoord.x /= backdropRect.z;
        bgTexCoord.y /= backdropRect.w;
        return TextureLookup(s_backdropTexture, bgTexCoord);
      }

      vec4 ApplyBlendMode(vec4 src) {
        vec4 dst = GetBackdropColor();
        return Blend(src, dst);
      }
      // clang-format off
  );  // NOLINT(whitespace/parens)
  // clang-format on

  return "precision mediump float;" + GetHelperFunctions() +
         GetBlendFunction() + kFunctionApplyBlendMode + shader_string;
}

std::string FragmentTexBlendMode::GetHelperFunctions() const {
  // clang-format off
  static const std::string kFunctionHardLight = SHADER0(
      // clang-format on
      vec3 hardLight(vec4 src, vec4 dst) {
        vec3 result;
        result.r =
            (2.0 * src.r <= src.a)
                ? (2.0 * src.r * dst.r)
                : (src.a * dst.a - 2.0 * (dst.a - dst.r) * (src.a - src.r));
        result.g =
            (2.0 * src.g <= src.a)
                ? (2.0 * src.g * dst.g)
                : (src.a * dst.a - 2.0 * (dst.a - dst.g) * (src.a - src.g));
        result.b =
            (2.0 * src.b <= src.a)
                ? (2.0 * src.b * dst.b)
                : (src.a * dst.a - 2.0 * (dst.a - dst.b) * (src.a - src.b));
        result.rgb += src.rgb * (1.0 - dst.a) + dst.rgb * (1.0 - src.a);
        return result;
      }
      // clang-format off
  );  // NOLINT(whitespace/parens)

  static const std::string kFunctionColorDodgeComponent = SHADER0(
      // clang-format on
      float getColorDodgeComponent(
          float srcc, float srca, float dstc, float dsta) {
        if (0.0 == dstc)
          return srcc * (1.0 - dsta);
        float d = srca - srcc;
        if (0.0 == d)
          return srca * dsta + srcc * (1.0 - dsta) + dstc * (1.0 - srca);
        d = min(dsta, dstc * srca / d);
        return d * srca + srcc * (1.0 - dsta) + dstc * (1.0 - srca);
      }
      // clang-format off
  );  // NOLINT(whitespace/parens)

  static const std::string kFunctionColorBurnComponent = SHADER0(
      // clang-format on
      float getColorBurnComponent(
          float srcc, float srca, float dstc, float dsta) {
        if (dsta == dstc)
          return srca * dsta + srcc * (1.0 - dsta) + dstc * (1.0 - srca);
        if (0.0 == srcc)
          return dstc * (1.0 - srca);
        float d = max(0.0, dsta - (dsta - dstc) * srca / srcc);
        return srca * d + srcc * (1.0 - dsta) + dstc * (1.0 - srca);
      }
      // clang-format off
  );  // NOLINT(whitespace/parens)

  static const std::string kFunctionSoftLightComponentPosDstAlpha = SHADER0(
      // clang-format on
      float getSoftLightComponent(
          float srcc, float srca, float dstc, float dsta) {
        if (2.0 * srcc <= srca) {
          return (dstc * dstc * (srca - 2.0 * srcc)) / dsta +
                 (1.0 - dsta) * srcc + dstc * (-srca + 2.0 * srcc + 1.0);
        } else if (4.0 * dstc <= dsta) {
          float DSqd = dstc * dstc;
          float DCub = DSqd * dstc;
          float DaSqd = dsta * dsta;
          float DaCub = DaSqd * dsta;
          return (-DaCub * srcc +
                  DaSqd * (srcc - dstc * (3.0 * srca - 6.0 * srcc - 1.0)) +
                  12.0 * dsta * DSqd * (srca - 2.0 * srcc) -
                  16.0 * DCub * (srca - 2.0 * srcc)) /
                 DaSqd;
        } else {
          return -sqrt(dsta * dstc) * (srca - 2.0 * srcc) - dsta * srcc +
                 dstc * (srca - 2.0 * srcc + 1.0) + srcc;
        }
      }
      // clang-format off
  );  // NOLINT(whitespace/parens)

  static const std::string kFunctionLum = SHADER0(
      // clang-format on
      float luminance(vec3 color) { return dot(vec3(0.3, 0.59, 0.11), color); }

      vec3 set_luminance(vec3 hueSat, float alpha, vec3 lumColor) {
        float diff = luminance(lumColor - hueSat);
        vec3 outColor = hueSat + diff;
        float outLum = luminance(outColor);
        float minComp = min(min(outColor.r, outColor.g), outColor.b);
        float maxComp = max(max(outColor.r, outColor.g), outColor.b);
        if (minComp < 0.0 && outLum != minComp) {
          outColor = outLum +
                     ((outColor - vec3(outLum, outLum, outLum)) * outLum) /
                         (outLum - minComp);
        }
        if (maxComp > alpha && maxComp != outLum) {
          outColor =
              outLum +
              ((outColor - vec3(outLum, outLum, outLum)) * (alpha - outLum)) /
                  (maxComp - outLum);
        }
        return outColor;
      }
      // clang-format off
  );  // NOLINT(whitespace/parens)

  static const std::string kFunctionSat = SHADER0(
      // clang-format on
      float saturation(vec3 color) {
        return max(max(color.r, color.g), color.b) -
               min(min(color.r, color.g), color.b);
      }

      vec3 set_saturation_helper(
          float minComp, float midComp, float maxComp, float sat) {
        if (minComp < maxComp) {
          vec3 result;
          result.r = 0.0;
          result.g = sat * (midComp - minComp) / (maxComp - minComp);
          result.b = sat;
          return result;
        } else {
          return vec3(0, 0, 0);
        }
      }

      vec3 set_saturation(vec3 hueLumColor, vec3 satColor) {
        float sat = saturation(satColor);
        if (hueLumColor.r <= hueLumColor.g) {
          if (hueLumColor.g <= hueLumColor.b) {
            hueLumColor.rgb = set_saturation_helper(
                hueLumColor.r, hueLumColor.g, hueLumColor.b, sat);
          } else if (hueLumColor.r <= hueLumColor.b) {
            hueLumColor.rbg = set_saturation_helper(
                hueLumColor.r, hueLumColor.b, hueLumColor.g, sat);
          } else {
            hueLumColor.brg = set_saturation_helper(
                hueLumColor.b, hueLumColor.r, hueLumColor.g, sat);
          }
        } else if (hueLumColor.r <= hueLumColor.b) {
          hueLumColor.grb = set_saturation_helper(
              hueLumColor.g, hueLumColor.r, hueLumColor.b, sat);
        } else if (hueLumColor.g <= hueLumColor.b) {
          hueLumColor.gbr = set_saturation_helper(
              hueLumColor.g, hueLumColor.b, hueLumColor.r, sat);
        } else {
          hueLumColor.bgr = set_saturation_helper(
              hueLumColor.b, hueLumColor.g, hueLumColor.r, sat);
        }
        return hueLumColor;
      }
      // clang-format off
  );  // NOLINT(whitespace/parens)
  // clang-format on

  switch (blend_mode_) {
    case BlendModeOverlay:
    case BlendModeHardLight:
      return kFunctionHardLight;
    case BlendModeColorDodge:
      return kFunctionColorDodgeComponent;
    case BlendModeColorBurn:
      return kFunctionColorBurnComponent;
    case BlendModeSoftLight:
      return kFunctionSoftLightComponentPosDstAlpha;
    case BlendModeHue:
    case BlendModeSaturation:
      return kFunctionLum + kFunctionSat;
    case BlendModeColor:
    case BlendModeLuminosity:
      return kFunctionLum;
    default:
      return std::string();
  }
}

std::string FragmentTexBlendMode::GetBlendFunction() const {
  return "vec4 Blend(vec4 src, vec4 dst) {"
         "    vec4 result;"
         "    result.a = src.a + (1.0 - src.a) * dst.a;" +
         GetBlendFunctionBodyForRGB() +
         "    return result;"
         "}";
}

std::string FragmentTexBlendMode::GetBlendFunctionBodyForRGB() const {
  switch (blend_mode_) {
    case BlendModeNormal:
      return "result.rgb = src.rgb + dst.rgb * (1.0 - src.a);";
    case BlendModeScreen:
      return "result.rgb = src.rgb + (1.0 - src.rgb) * dst.rgb;";
    case BlendModeLighten:
      return "result.rgb = max((1.0 - src.a) * dst.rgb + src.rgb,"
             "                 (1.0 - dst.a) * src.rgb + dst.rgb);";
    case BlendModeOverlay:
      return "result.rgb = hardLight(dst, src);";
    case BlendModeDarken:
      return "result.rgb = min((1.0 - src.a) * dst.rgb + src.rgb,"
             "                 (1.0 - dst.a) * src.rgb + dst.rgb);";
    case BlendModeColorDodge:
      return "result.r = getColorDodgeComponent(src.r, src.a, dst.r, dst.a);"
             "result.g = getColorDodgeComponent(src.g, src.a, dst.g, dst.a);"
             "result.b = getColorDodgeComponent(src.b, src.a, dst.b, dst.a);";
    case BlendModeColorBurn:
      return "result.r = getColorBurnComponent(src.r, src.a, dst.r, dst.a);"
             "result.g = getColorBurnComponent(src.g, src.a, dst.g, dst.a);"
             "result.b = getColorBurnComponent(src.b, src.a, dst.b, dst.a);";
    case BlendModeHardLight:
      return "result.rgb = hardLight(src, dst);";
    case BlendModeSoftLight:
      return "if (0.0 == dst.a) {"
             "  result.rgb = src.rgb;"
             "} else {"
             "  result.r = getSoftLightComponent(src.r, src.a, dst.r, dst.a);"
             "  result.g = getSoftLightComponent(src.g, src.a, dst.g, dst.a);"
             "  result.b = getSoftLightComponent(src.b, src.a, dst.b, dst.a);"
             "}";
    case BlendModeDifference:
      return "result.rgb = src.rgb + dst.rgb -"
             "    2.0 * min(src.rgb * dst.a, dst.rgb * src.a);";
    case BlendModeExclusion:
      return "result.rgb = dst.rgb + src.rgb - 2.0 * dst.rgb * src.rgb;";
    case BlendModeMultiply:
      return "result.rgb = (1.0 - src.a) * dst.rgb +"
             "    (1.0 - dst.a) * src.rgb + src.rgb * dst.rgb;";
    case BlendModeHue:
      return "vec4 dstSrcAlpha = dst * src.a;"
             "result.rgb ="
             "    set_luminance(set_saturation(src.rgb * dst.a,"
             "                                 dstSrcAlpha.rgb),"
             "                  dstSrcAlpha.a,"
             "                  dstSrcAlpha.rgb);"
             "result.rgb += (1.0 - src.a) * dst.rgb + (1.0 - dst.a) * src.rgb;";
    case BlendModeSaturation:
      return "vec4 dstSrcAlpha = dst * src.a;"
             "result.rgb = set_luminance(set_saturation(dstSrcAlpha.rgb,"
             "                                          src.rgb * dst.a),"
             "                           dstSrcAlpha.a,"
             "                           dstSrcAlpha.rgb);"
             "result.rgb += (1.0 - src.a) * dst.rgb + (1.0 - dst.a) * src.rgb;";
    case BlendModeColor:
      return "vec4 srcDstAlpha = src * dst.a;"
             "result.rgb = set_luminance(srcDstAlpha.rgb,"
             "                           srcDstAlpha.a,"
             "                           dst.rgb * src.a);"
             "result.rgb += (1.0 - src.a) * dst.rgb + (1.0 - dst.a) * src.rgb;";
    case BlendModeLuminosity:
      return "vec4 srcDstAlpha = src * dst.a;"
             "result.rgb = set_luminance(dst.rgb * src.a,"
             "                           srcDstAlpha.a,"
             "                           srcDstAlpha.rgb);"
             "result.rgb += (1.0 - src.a) * dst.rgb + (1.0 - dst.a) * src.rgb;";
    case BlendModeNone:
    case NumBlendModes:
      NOTREACHED();
  }
  return "result = vec4(1.0, 0.0, 0.0, 1.0);";
}

FragmentTexAlphaBinding::FragmentTexAlphaBinding()
    : sampler_location_(-1), alpha_location_(-1) {
}

void FragmentTexAlphaBinding::Init(GLES2Interface* context,
                                   unsigned program,
                                   int* base_uniform_index) {
  static const char* uniforms[] = {
      "s_texture", "alpha", BLEND_MODE_UNIFORMS,
  };
  int locations[arraysize(uniforms)];

  GetProgramUniformLocations(context,
                             program,
                             arraysize(uniforms) - UNUSED_BLEND_MODE_UNIFORMS,
                             uniforms,
                             locations,
                             base_uniform_index);
  sampler_location_ = locations[0];
  alpha_location_ = locations[1];
  BLEND_MODE_SET_LOCATIONS(locations, 2);
}

FragmentTexColorMatrixAlphaBinding::FragmentTexColorMatrixAlphaBinding()
    : sampler_location_(-1),
      alpha_location_(-1),
      color_matrix_location_(-1),
      color_offset_location_(-1) {
}

void FragmentTexColorMatrixAlphaBinding::Init(GLES2Interface* context,
                                              unsigned program,
                                              int* base_uniform_index) {
  static const char* uniforms[] = {
      "s_texture", "alpha", "colorMatrix", "colorOffset", BLEND_MODE_UNIFORMS,
  };
  int locations[arraysize(uniforms)];

  GetProgramUniformLocations(context,
                             program,
                             arraysize(uniforms) - UNUSED_BLEND_MODE_UNIFORMS,
                             uniforms,
                             locations,
                             base_uniform_index);
  sampler_location_ = locations[0];
  alpha_location_ = locations[1];
  color_matrix_location_ = locations[2];
  color_offset_location_ = locations[3];
  BLEND_MODE_SET_LOCATIONS(locations, 4);
}

FragmentTexOpaqueBinding::FragmentTexOpaqueBinding() : sampler_location_(-1) {
}

void FragmentTexOpaqueBinding::Init(GLES2Interface* context,
                                    unsigned program,
                                    int* base_uniform_index) {
  static const char* uniforms[] = {
      "s_texture",
  };
  int locations[arraysize(uniforms)];

  GetProgramUniformLocations(context,
                             program,
                             arraysize(uniforms),
                             uniforms,
                             locations,
                             base_uniform_index);
  sampler_location_ = locations[0];
}

std::string FragmentShaderRGBATexAlpha::GetShaderString(
    TexCoordPrecision precision,
    SamplerType sampler) const {
  // clang-format off
  return FRAGMENT_SHADER(
      // clang-format on
      precision mediump float;
      varying TexCoordPrecision vec2 v_texCoord;
      uniform SamplerType s_texture;
      uniform float alpha;
      void main() {
        vec4 texColor = TextureLookup(s_texture, v_texCoord);
        gl_FragColor = ApplyBlendMode(texColor * alpha);
      }
      // clang-format off
  );  // NOLINT(whitespace/parens)
  // clang-format on
}

std::string FragmentShaderRGBATexColorMatrixAlpha::GetShaderString(
    TexCoordPrecision precision,
    SamplerType sampler) const {
  // clang-format off
  return FRAGMENT_SHADER(
      // clang-format on
      precision mediump float;
      varying TexCoordPrecision vec2 v_texCoord;
      uniform SamplerType s_texture;
      uniform float alpha;
      uniform mat4 colorMatrix;
      uniform vec4 colorOffset;
      void main() {
        vec4 texColor = TextureLookup(s_texture, v_texCoord);
        float nonZeroAlpha = max(texColor.a, 0.00001);
        texColor = vec4(texColor.rgb / nonZeroAlpha, nonZeroAlpha);
        texColor = colorMatrix * texColor + colorOffset;
        texColor.rgb *= texColor.a;
        texColor = clamp(texColor, 0.0, 1.0);
        gl_FragColor = ApplyBlendMode(texColor * alpha);
      }
      // clang-format off
  );  // NOLINT(whitespace/parens)
  // clang-format on
}

std::string FragmentShaderRGBATexVaryingAlpha::GetShaderString(
    TexCoordPrecision precision,
    SamplerType sampler) const {
  // clang-format off
  return FRAGMENT_SHADER(
      // clang-format on
      precision mediump float;
      varying TexCoordPrecision vec2 v_texCoord;
      varying float v_alpha;
      uniform SamplerType s_texture;
      void main() {
        vec4 texColor = TextureLookup(s_texture, v_texCoord);
        gl_FragColor = texColor * v_alpha;
      }
      // clang-format off
  );  // NOLINT(whitespace/parens)
  // clang-format on
}

std::string FragmentShaderRGBATexPremultiplyAlpha::GetShaderString(
    TexCoordPrecision precision,
    SamplerType sampler) const {
  // clang-format off
  return FRAGMENT_SHADER(
      // clang-format on
      precision mediump float;
      varying TexCoordPrecision vec2 v_texCoord;
      varying float v_alpha;
      uniform SamplerType s_texture;
      void main() {
        vec4 texColor = TextureLookup(s_texture, v_texCoord);
        texColor.rgb *= texColor.a;
        gl_FragColor = texColor * v_alpha;
      }
      // clang-format off
  );  // NOLINT(whitespace/parens)
  // clang-format on
}

FragmentTexBackgroundBinding::FragmentTexBackgroundBinding()
    : background_color_location_(-1), sampler_location_(-1) {
}

void FragmentTexBackgroundBinding::Init(GLES2Interface* context,
                                        unsigned program,
                                        int* base_uniform_index) {
  static const char* uniforms[] = {
      "s_texture", "background_color",
  };
  int locations[arraysize(uniforms)];

  GetProgramUniformLocations(context,
                             program,
                             arraysize(uniforms),
                             uniforms,
                             locations,
                             base_uniform_index);

  sampler_location_ = locations[0];
  DCHECK_NE(sampler_location_, -1);

  background_color_location_ = locations[1];
  DCHECK_NE(background_color_location_, -1);
}

std::string FragmentShaderTexBackgroundVaryingAlpha::GetShaderString(
    TexCoordPrecision precision,
    SamplerType sampler) const {
  // clang-format off
  return FRAGMENT_SHADER(
      // clang-format on
      precision mediump float;
      varying TexCoordPrecision vec2 v_texCoord;
      varying float v_alpha;
      uniform vec4 background_color;
      uniform SamplerType s_texture;
      void main() {
        vec4 texColor = TextureLookup(s_texture, v_texCoord);
        texColor += background_color * (1.0 - texColor.a);
        gl_FragColor = texColor * v_alpha;
      }
      // clang-format off
  );  // NOLINT(whitespace/parens)
  // clang-format on
}

std::string FragmentShaderTexBackgroundPremultiplyAlpha::GetShaderString(
    TexCoordPrecision precision,
    SamplerType sampler) const {
  // clang-format off
  return FRAGMENT_SHADER(
      // clang-format on
      precision mediump float;
      varying TexCoordPrecision vec2 v_texCoord;
      varying float v_alpha;
      uniform vec4 background_color;
      uniform SamplerType s_texture;
      void main() {
        vec4 texColor = TextureLookup(s_texture, v_texCoord);
        texColor.rgb *= texColor.a;
        texColor += background_color * (1.0 - texColor.a);
        gl_FragColor = texColor * v_alpha;
      }
      // clang-format off
  );  // NOLINT(whitespace/parens)
  // clang-format on
}

std::string FragmentShaderRGBATexOpaque::GetShaderString(
    TexCoordPrecision precision,
    SamplerType sampler) const {
  // clang-format off
  return FRAGMENT_SHADER(
      // clang-format on
      precision mediump float;
      varying TexCoordPrecision vec2 v_texCoord;
      uniform SamplerType s_texture;
      void main() {
        vec4 texColor = TextureLookup(s_texture, v_texCoord);
        gl_FragColor = vec4(texColor.rgb, 1.0);
      }
      // clang-format off
  );  // NOLINT(whitespace/parens)
  // clang-format on
}

std::string FragmentShaderRGBATex::GetShaderString(TexCoordPrecision precision,
                                                   SamplerType sampler) const {
  // clang-format off
  return FRAGMENT_SHADER(
      // clang-format on
      precision mediump float;
      varying TexCoordPrecision vec2 v_texCoord;
      uniform SamplerType s_texture;
      void main() { gl_FragColor = TextureLookup(s_texture, v_texCoord); }
      // clang-format off
  );  // NOLINT(whitespace/parens)
  // clang-format on
}

std::string FragmentShaderRGBATexSwizzleAlpha::GetShaderString(
    TexCoordPrecision precision,
    SamplerType sampler) const {
  // clang-format off
  return FRAGMENT_SHADER(
      // clang-format on
      precision mediump float;
      varying TexCoordPrecision vec2 v_texCoord;
      uniform SamplerType s_texture;
      uniform float alpha;
      void main() {
        vec4 texColor = TextureLookup(s_texture, v_texCoord);
        gl_FragColor =
            vec4(texColor.z, texColor.y, texColor.x, texColor.w) * alpha;
      }
      // clang-format off
  );  // NOLINT(whitespace/parens)
  // clang-format on
}

std::string FragmentShaderRGBATexSwizzleOpaque::GetShaderString(
    TexCoordPrecision precision,
    SamplerType sampler) const {
  // clang-format off
  return FRAGMENT_SHADER(
      // clang-format on
      precision mediump float;
      varying TexCoordPrecision vec2 v_texCoord;
      uniform SamplerType s_texture;
      void main() {
        vec4 texColor = TextureLookup(s_texture, v_texCoord);
        gl_FragColor = vec4(texColor.z, texColor.y, texColor.x, 1.0);
      }
      // clang-format off
  );  // NOLINT(whitespace/parens)
  // clang-format on
}

FragmentShaderRGBATexAlphaAA::FragmentShaderRGBATexAlphaAA()
    : sampler_location_(-1), alpha_location_(-1) {
}

void FragmentShaderRGBATexAlphaAA::Init(GLES2Interface* context,
                                        unsigned program,
                                        int* base_uniform_index) {
  static const char* uniforms[] = {
      "s_texture", "alpha", BLEND_MODE_UNIFORMS,
  };
  int locations[arraysize(uniforms)];

  GetProgramUniformLocations(context,
                             program,
                             arraysize(uniforms) - UNUSED_BLEND_MODE_UNIFORMS,
                             uniforms,
                             locations,
                             base_uniform_index);
  sampler_location_ = locations[0];
  alpha_location_ = locations[1];
  BLEND_MODE_SET_LOCATIONS(locations, 2);
}

std::string FragmentShaderRGBATexAlphaAA::GetShaderString(
    TexCoordPrecision precision,
    SamplerType sampler) const {
  // clang-format off
  return FRAGMENT_SHADER(
      // clang-format on
      precision mediump float;
      uniform SamplerType s_texture;
      uniform float alpha;
      varying TexCoordPrecision vec2 v_texCoord;
      varying TexCoordPrecision vec4 edge_dist[2];  // 8 edge distances.

      void main() {
        vec4 texColor = TextureLookup(s_texture, v_texCoord);
        vec4 d4 = min(edge_dist[0], edge_dist[1]);
        vec2 d2 = min(d4.xz, d4.yw);
        float aa = clamp(gl_FragCoord.w * min(d2.x, d2.y), 0.0, 1.0);
        gl_FragColor = ApplyBlendMode(texColor * alpha * aa);
      }
      // clang-format off
  );  // NOLINT(whitespace/parens)
  // clang-format on
}

FragmentTexClampAlphaAABinding::FragmentTexClampAlphaAABinding()
    : sampler_location_(-1),
      alpha_location_(-1),
      fragment_tex_transform_location_(-1) {
}

void FragmentTexClampAlphaAABinding::Init(GLES2Interface* context,
                                          unsigned program,
                                          int* base_uniform_index) {
  static const char* uniforms[] = {
      "s_texture", "alpha", "fragmentTexTransform",
  };
  int locations[arraysize(uniforms)];

  GetProgramUniformLocations(context,
                             program,
                             arraysize(uniforms),
                             uniforms,
                             locations,
                             base_uniform_index);
  sampler_location_ = locations[0];
  alpha_location_ = locations[1];
  fragment_tex_transform_location_ = locations[2];
}

std::string FragmentShaderRGBATexClampAlphaAA::GetShaderString(
    TexCoordPrecision precision,
    SamplerType sampler) const {
  // clang-format off
  return FRAGMENT_SHADER(
      // clang-format on
      precision mediump float;
      uniform SamplerType s_texture;
      uniform float alpha;
      uniform TexCoordPrecision vec4 fragmentTexTransform;
      varying TexCoordPrecision vec2 v_texCoord;
      varying TexCoordPrecision vec4 edge_dist[2];  // 8 edge distances.

      void main() {
        TexCoordPrecision vec2 texCoord =
            clamp(v_texCoord, 0.0, 1.0) * fragmentTexTransform.zw +
            fragmentTexTransform.xy;
        vec4 texColor = TextureLookup(s_texture, texCoord);
        vec4 d4 = min(edge_dist[0], edge_dist[1]);
        vec2 d2 = min(d4.xz, d4.yw);
        float aa = clamp(gl_FragCoord.w * min(d2.x, d2.y), 0.0, 1.0);
        gl_FragColor = texColor * alpha * aa;
      }
      // clang-format off
  );  // NOLINT(whitespace/parens)
  // clang-format on
}

std::string FragmentShaderRGBATexClampSwizzleAlphaAA::GetShaderString(
    TexCoordPrecision precision,
    SamplerType sampler) const {
  // clang-format off
  return FRAGMENT_SHADER(
      // clang-format on
      precision mediump float;
      uniform SamplerType s_texture;
      uniform float alpha;
      uniform TexCoordPrecision vec4 fragmentTexTransform;
      varying TexCoordPrecision vec2 v_texCoord;
      varying TexCoordPrecision vec4 edge_dist[2];  // 8 edge distances.

      void main() {
        TexCoordPrecision vec2 texCoord =
            clamp(v_texCoord, 0.0, 1.0) * fragmentTexTransform.zw +
            fragmentTexTransform.xy;
        vec4 texColor = TextureLookup(s_texture, texCoord);
        vec4 d4 = min(edge_dist[0], edge_dist[1]);
        vec2 d2 = min(d4.xz, d4.yw);
        float aa = clamp(gl_FragCoord.w * min(d2.x, d2.y), 0.0, 1.0);
        gl_FragColor =
            vec4(texColor.z, texColor.y, texColor.x, texColor.w) * alpha * aa;
      }
      // clang-format off
  );  // NOLINT(whitespace/parens)
  // clang-format on
}

FragmentShaderRGBATexAlphaMask::FragmentShaderRGBATexAlphaMask()
    : sampler_location_(-1),
      mask_sampler_location_(-1),
      alpha_location_(-1),
      mask_tex_coord_scale_location_(-1) {
}

void FragmentShaderRGBATexAlphaMask::Init(GLES2Interface* context,
                                          unsigned program,
                                          int* base_uniform_index) {
  static const char* uniforms[] = {
      "s_texture",
      "s_mask",
      "alpha",
      "maskTexCoordScale",
      "maskTexCoordOffset",
      BLEND_MODE_UNIFORMS,
  };
  int locations[arraysize(uniforms)];

  GetProgramUniformLocations(context,
                             program,
                             arraysize(uniforms) - UNUSED_BLEND_MODE_UNIFORMS,
                             uniforms,
                             locations,
                             base_uniform_index);
  sampler_location_ = locations[0];
  mask_sampler_location_ = locations[1];
  alpha_location_ = locations[2];
  mask_tex_coord_scale_location_ = locations[3];
  mask_tex_coord_offset_location_ = locations[4];
  BLEND_MODE_SET_LOCATIONS(locations, 5);
}

std::string FragmentShaderRGBATexAlphaMask::GetShaderString(
    TexCoordPrecision precision,
    SamplerType sampler) const {
  // clang-format off
  return FRAGMENT_SHADER(
      // clang-format on
      precision mediump float;
      varying TexCoordPrecision vec2 v_texCoord;
      uniform sampler2D s_texture;
      uniform SamplerType s_mask;
      uniform TexCoordPrecision vec2 maskTexCoordScale;
      uniform TexCoordPrecision vec2 maskTexCoordOffset;
      uniform float alpha;
      void main() {
        vec4 texColor = texture2D(s_texture, v_texCoord);
        TexCoordPrecision vec2 maskTexCoord =
            vec2(maskTexCoordOffset.x + v_texCoord.x * maskTexCoordScale.x,
                 maskTexCoordOffset.y + v_texCoord.y * maskTexCoordScale.y);
        vec4 maskColor = TextureLookup(s_mask, maskTexCoord);
        gl_FragColor = ApplyBlendMode(texColor * alpha * maskColor.w);
      }
      // clang-format off
  );  // NOLINT(whitespace/parens)
  // clang-format on
}

FragmentShaderRGBATexAlphaMaskAA::FragmentShaderRGBATexAlphaMaskAA()
    : sampler_location_(-1),
      mask_sampler_location_(-1),
      alpha_location_(-1),
      mask_tex_coord_scale_location_(-1),
      mask_tex_coord_offset_location_(-1) {
}

void FragmentShaderRGBATexAlphaMaskAA::Init(GLES2Interface* context,
                                            unsigned program,
                                            int* base_uniform_index) {
  static const char* uniforms[] = {
      "s_texture",
      "s_mask",
      "alpha",
      "maskTexCoordScale",
      "maskTexCoordOffset",
      BLEND_MODE_UNIFORMS,
  };
  int locations[arraysize(uniforms)];

  GetProgramUniformLocations(context,
                             program,
                             arraysize(uniforms) - UNUSED_BLEND_MODE_UNIFORMS,
                             uniforms,
                             locations,
                             base_uniform_index);
  sampler_location_ = locations[0];
  mask_sampler_location_ = locations[1];
  alpha_location_ = locations[2];
  mask_tex_coord_scale_location_ = locations[3];
  mask_tex_coord_offset_location_ = locations[4];
  BLEND_MODE_SET_LOCATIONS(locations, 5);
}

std::string FragmentShaderRGBATexAlphaMaskAA::GetShaderString(
    TexCoordPrecision precision,
    SamplerType sampler) const {
  // clang-format off
  return FRAGMENT_SHADER(
      // clang-format on
      precision mediump float;
      uniform sampler2D s_texture;
      uniform SamplerType s_mask;
      uniform TexCoordPrecision vec2 maskTexCoordScale;
      uniform TexCoordPrecision vec2 maskTexCoordOffset;
      uniform float alpha;
      varying TexCoordPrecision vec2 v_texCoord;
      varying TexCoordPrecision vec4 edge_dist[2];  // 8 edge distances.

      void main() {
        vec4 texColor = texture2D(s_texture, v_texCoord);
        TexCoordPrecision vec2 maskTexCoord =
            vec2(maskTexCoordOffset.x + v_texCoord.x * maskTexCoordScale.x,
                 maskTexCoordOffset.y + v_texCoord.y * maskTexCoordScale.y);
        vec4 maskColor = TextureLookup(s_mask, maskTexCoord);
        vec4 d4 = min(edge_dist[0], edge_dist[1]);
        vec2 d2 = min(d4.xz, d4.yw);
        float aa = clamp(gl_FragCoord.w * min(d2.x, d2.y), 0.0, 1.0);
        gl_FragColor = ApplyBlendMode(texColor * alpha * maskColor.w * aa);
      }
      // clang-format off
  );  // NOLINT(whitespace/parens)
  // clang-format on
}

FragmentShaderRGBATexAlphaMaskColorMatrixAA::
    FragmentShaderRGBATexAlphaMaskColorMatrixAA()
    : sampler_location_(-1),
      mask_sampler_location_(-1),
      alpha_location_(-1),
      mask_tex_coord_scale_location_(-1),
      color_matrix_location_(-1),
      color_offset_location_(-1) {
}

void FragmentShaderRGBATexAlphaMaskColorMatrixAA::Init(
    GLES2Interface* context,
    unsigned program,
    int* base_uniform_index) {
  static const char* uniforms[] = {
      "s_texture",
      "s_mask",
      "alpha",
      "maskTexCoordScale",
      "maskTexCoordOffset",
      "colorMatrix",
      "colorOffset",
      BLEND_MODE_UNIFORMS,
  };
  int locations[arraysize(uniforms)];

  GetProgramUniformLocations(context,
                             program,
                             arraysize(uniforms) - UNUSED_BLEND_MODE_UNIFORMS,
                             uniforms,
                             locations,
                             base_uniform_index);
  sampler_location_ = locations[0];
  mask_sampler_location_ = locations[1];
  alpha_location_ = locations[2];
  mask_tex_coord_scale_location_ = locations[3];
  mask_tex_coord_offset_location_ = locations[4];
  color_matrix_location_ = locations[5];
  color_offset_location_ = locations[6];
  BLEND_MODE_SET_LOCATIONS(locations, 7);
}

std::string FragmentShaderRGBATexAlphaMaskColorMatrixAA::GetShaderString(
    TexCoordPrecision precision,
    SamplerType sampler) const {
  // clang-format off
  return FRAGMENT_SHADER(
      // clang-format on
      precision mediump float;
      uniform sampler2D s_texture;
      uniform SamplerType s_mask;
      uniform vec2 maskTexCoordScale;
      uniform vec2 maskTexCoordOffset;
      uniform mat4 colorMatrix;
      uniform vec4 colorOffset;
      uniform float alpha;
      varying TexCoordPrecision vec2 v_texCoord;
      varying TexCoordPrecision vec4 edge_dist[2];  // 8 edge distances.

      void main() {
        vec4 texColor = texture2D(s_texture, v_texCoord);
        float nonZeroAlpha = max(texColor.a, 0.00001);
        texColor = vec4(texColor.rgb / nonZeroAlpha, nonZeroAlpha);
        texColor = colorMatrix * texColor + colorOffset;
        texColor.rgb *= texColor.a;
        texColor = clamp(texColor, 0.0, 1.0);
        TexCoordPrecision vec2 maskTexCoord =
            vec2(maskTexCoordOffset.x + v_texCoord.x * maskTexCoordScale.x,
                 maskTexCoordOffset.y + v_texCoord.y * maskTexCoordScale.y);
        vec4 maskColor = TextureLookup(s_mask, maskTexCoord);
        vec4 d4 = min(edge_dist[0], edge_dist[1]);
        vec2 d2 = min(d4.xz, d4.yw);
        float aa = clamp(gl_FragCoord.w * min(d2.x, d2.y), 0.0, 1.0);
        gl_FragColor = ApplyBlendMode(texColor * alpha * maskColor.w * aa);
      }
      // clang-format off
  );  // NOLINT(whitespace/parens)
  // clang-format on
}

FragmentShaderRGBATexAlphaColorMatrixAA::
    FragmentShaderRGBATexAlphaColorMatrixAA()
    : sampler_location_(-1),
      alpha_location_(-1),
      color_matrix_location_(-1),
      color_offset_location_(-1) {
}

void FragmentShaderRGBATexAlphaColorMatrixAA::Init(GLES2Interface* context,
                                                   unsigned program,
                                                   int* base_uniform_index) {
  static const char* uniforms[] = {
      "s_texture", "alpha", "colorMatrix", "colorOffset", BLEND_MODE_UNIFORMS,
  };
  int locations[arraysize(uniforms)];

  GetProgramUniformLocations(context,
                             program,
                             arraysize(uniforms) - UNUSED_BLEND_MODE_UNIFORMS,
                             uniforms,
                             locations,
                             base_uniform_index);
  sampler_location_ = locations[0];
  alpha_location_ = locations[1];
  color_matrix_location_ = locations[2];
  color_offset_location_ = locations[3];
  BLEND_MODE_SET_LOCATIONS(locations, 4);
}

std::string FragmentShaderRGBATexAlphaColorMatrixAA::GetShaderString(
    TexCoordPrecision precision,
    SamplerType sampler) const {
  // clang-format off
  return FRAGMENT_SHADER(
      // clang-format on
      precision mediump float;
      uniform SamplerType s_texture;
      uniform float alpha;
      uniform mat4 colorMatrix;
      uniform vec4 colorOffset;
      varying TexCoordPrecision vec2 v_texCoord;
      varying TexCoordPrecision vec4 edge_dist[2];  // 8 edge distances.

      void main() {
        vec4 texColor = TextureLookup(s_texture, v_texCoord);
        float nonZeroAlpha = max(texColor.a, 0.00001);
        texColor = vec4(texColor.rgb / nonZeroAlpha, nonZeroAlpha);
        texColor = colorMatrix * texColor + colorOffset;
        texColor.rgb *= texColor.a;
        texColor = clamp(texColor, 0.0, 1.0);
        vec4 d4 = min(edge_dist[0], edge_dist[1]);
        vec2 d2 = min(d4.xz, d4.yw);
        float aa = clamp(gl_FragCoord.w * min(d2.x, d2.y), 0.0, 1.0);
        gl_FragColor = ApplyBlendMode(texColor * alpha * aa);
      }
      // clang-format off
  );  // NOLINT(whitespace/parens)
  // clang-format on
}

FragmentShaderRGBATexAlphaMaskColorMatrix::
    FragmentShaderRGBATexAlphaMaskColorMatrix()
    : sampler_location_(-1),
      mask_sampler_location_(-1),
      alpha_location_(-1),
      mask_tex_coord_scale_location_(-1) {
}

void FragmentShaderRGBATexAlphaMaskColorMatrix::Init(GLES2Interface* context,
                                                     unsigned program,
                                                     int* base_uniform_index) {
  static const char* uniforms[] = {
      "s_texture",
      "s_mask",
      "alpha",
      "maskTexCoordScale",
      "maskTexCoordOffset",
      "colorMatrix",
      "colorOffset",
      BLEND_MODE_UNIFORMS,
  };
  int locations[arraysize(uniforms)];

  GetProgramUniformLocations(context,
                             program,
                             arraysize(uniforms) - UNUSED_BLEND_MODE_UNIFORMS,
                             uniforms,
                             locations,
                             base_uniform_index);
  sampler_location_ = locations[0];
  mask_sampler_location_ = locations[1];
  alpha_location_ = locations[2];
  mask_tex_coord_scale_location_ = locations[3];
  mask_tex_coord_offset_location_ = locations[4];
  color_matrix_location_ = locations[5];
  color_offset_location_ = locations[6];
  BLEND_MODE_SET_LOCATIONS(locations, 7);
}

std::string FragmentShaderRGBATexAlphaMaskColorMatrix::GetShaderString(
    TexCoordPrecision precision,
    SamplerType sampler) const {
  // clang-format off
  return FRAGMENT_SHADER(
      // clang-format on
      precision mediump float;
      varying TexCoordPrecision vec2 v_texCoord;
      uniform sampler2D s_texture;
      uniform SamplerType s_mask;
      uniform vec2 maskTexCoordScale;
      uniform vec2 maskTexCoordOffset;
      uniform mat4 colorMatrix;
      uniform vec4 colorOffset;
      uniform float alpha;
      void main() {
        vec4 texColor = texture2D(s_texture, v_texCoord);
        float nonZeroAlpha = max(texColor.a, 0.00001);
        texColor = vec4(texColor.rgb / nonZeroAlpha, nonZeroAlpha);
        texColor = colorMatrix * texColor + colorOffset;
        texColor.rgb *= texColor.a;
        texColor = clamp(texColor, 0.0, 1.0);
        TexCoordPrecision vec2 maskTexCoord =
            vec2(maskTexCoordOffset.x + v_texCoord.x * maskTexCoordScale.x,
                 maskTexCoordOffset.y + v_texCoord.y * maskTexCoordScale.y);
        vec4 maskColor = TextureLookup(s_mask, maskTexCoord);
        gl_FragColor = ApplyBlendMode(texColor * alpha * maskColor.w);
      }
      // clang-format off
  );  // NOLINT(whitespace/parens)
  // clang-format on
}

FragmentShaderYUVVideo::FragmentShaderYUVVideo()
    : y_texture_location_(-1),
      u_texture_location_(-1),
      v_texture_location_(-1),
      alpha_location_(-1),
      yuv_matrix_location_(-1),
      yuv_adj_location_(-1) {
}

void FragmentShaderYUVVideo::Init(GLES2Interface* context,
                                  unsigned program,
                                  int* base_uniform_index) {
  static const char* uniforms[] = {
      "y_texture", "u_texture", "v_texture", "alpha", "yuv_matrix", "yuv_adj",
  };
  int locations[arraysize(uniforms)];

  GetProgramUniformLocations(context,
                             program,
                             arraysize(uniforms),
                             uniforms,
                             locations,
                             base_uniform_index);
  y_texture_location_ = locations[0];
  u_texture_location_ = locations[1];
  v_texture_location_ = locations[2];
  alpha_location_ = locations[3];
  yuv_matrix_location_ = locations[4];
  yuv_adj_location_ = locations[5];
}

std::string FragmentShaderYUVVideo::GetShaderString(TexCoordPrecision precision,
                                                    SamplerType sampler) const {
  // clang-format off
  return FRAGMENT_SHADER(
      // clang-format on
      precision mediump float;
      precision mediump int;
      varying TexCoordPrecision vec2 v_texCoord;
      uniform SamplerType y_texture;
      uniform SamplerType u_texture;
      uniform SamplerType v_texture;
      uniform float alpha;
      uniform vec3 yuv_adj;
      uniform mat3 yuv_matrix;
      void main() {
        float y_raw = TextureLookup(y_texture, v_texCoord).x;
        float u_unsigned = TextureLookup(u_texture, v_texCoord).x;
        float v_unsigned = TextureLookup(v_texture, v_texCoord).x;
        vec3 yuv = vec3(y_raw, u_unsigned, v_unsigned) + yuv_adj;
        vec3 rgb = yuv_matrix * yuv;
        gl_FragColor = vec4(rgb, 1.0) * alpha;
      }
      // clang-format off
  );  // NOLINT(whitespace/parens)
  // clang-format on
}

FragmentShaderYUVAVideo::FragmentShaderYUVAVideo()
    : y_texture_location_(-1),
      u_texture_location_(-1),
      v_texture_location_(-1),
      a_texture_location_(-1),
      alpha_location_(-1),
      yuv_matrix_location_(-1),
      yuv_adj_location_(-1) {
}

void FragmentShaderYUVAVideo::Init(GLES2Interface* context,
                                   unsigned program,
                                   int* base_uniform_index) {
  static const char* uniforms[] = {
      "y_texture",
      "u_texture",
      "v_texture",
      "a_texture",
      "alpha",
      "cc_matrix",
      "yuv_adj",
  };
  int locations[arraysize(uniforms)];

  GetProgramUniformLocations(context,
                             program,
                             arraysize(uniforms),
                             uniforms,
                             locations,
                             base_uniform_index);
  y_texture_location_ = locations[0];
  u_texture_location_ = locations[1];
  v_texture_location_ = locations[2];
  a_texture_location_ = locations[3];
  alpha_location_ = locations[4];
  yuv_matrix_location_ = locations[5];
  yuv_adj_location_ = locations[6];
}

std::string FragmentShaderYUVAVideo::GetShaderString(
    TexCoordPrecision precision,
    SamplerType sampler) const {
  // clang-format off
  return FRAGMENT_SHADER(
      // clang-format on
      precision mediump float;
      precision mediump int;
      varying TexCoordPrecision vec2 v_texCoord;
      uniform SamplerType y_texture;
      uniform SamplerType u_texture;
      uniform SamplerType v_texture;
      uniform SamplerType a_texture;
      uniform float alpha;
      uniform vec3 yuv_adj;
      uniform mat3 yuv_matrix;
      void main() {
        float y_raw = TextureLookup(y_texture, v_texCoord).x;
        float u_unsigned = TextureLookup(u_texture, v_texCoord).x;
        float v_unsigned = TextureLookup(v_texture, v_texCoord).x;
        float a_raw = TextureLookup(a_texture, v_texCoord).x;
        vec3 yuv = vec3(y_raw, u_unsigned, v_unsigned) + yuv_adj;
        vec3 rgb = yuv_matrix * yuv;
        gl_FragColor = vec4(rgb, 1.0) * (alpha * a_raw);
      }
      // clang-format off
  );  // NOLINT(whitespace/parens)
  // clang-format on
}

FragmentShaderColor::FragmentShaderColor() : color_location_(-1) {
}

void FragmentShaderColor::Init(GLES2Interface* context,
                               unsigned program,
                               int* base_uniform_index) {
  static const char* uniforms[] = {
      "color",
  };
  int locations[arraysize(uniforms)];

  GetProgramUniformLocations(context,
                             program,
                             arraysize(uniforms),
                             uniforms,
                             locations,
                             base_uniform_index);
  color_location_ = locations[0];
}

std::string FragmentShaderColor::GetShaderString(TexCoordPrecision precision,
                                                 SamplerType sampler) const {
  // clang-format off
  return FRAGMENT_SHADER(
      // clang-format on
      precision mediump float;
      uniform vec4 color;
      void main() { gl_FragColor = color; }
      // clang-format off
  );  // NOLINT(whitespace/parens)
  // clang-format on
}

FragmentShaderColorAA::FragmentShaderColorAA() : color_location_(-1) {
}

void FragmentShaderColorAA::Init(GLES2Interface* context,
                                 unsigned program,
                                 int* base_uniform_index) {
  static const char* uniforms[] = {
      "color",
  };
  int locations[arraysize(uniforms)];

  GetProgramUniformLocations(context,
                             program,
                             arraysize(uniforms),
                             uniforms,
                             locations,
                             base_uniform_index);
  color_location_ = locations[0];
}

std::string FragmentShaderColorAA::GetShaderString(TexCoordPrecision precision,
                                                   SamplerType sampler) const {
  // clang-format off
  return FRAGMENT_SHADER(
      // clang-format on
      precision mediump float;
      uniform vec4 color;
      varying vec4 edge_dist[2];  // 8 edge distances.

      void main() {
        vec4 d4 = min(edge_dist[0], edge_dist[1]);
        vec2 d2 = min(d4.xz, d4.yw);
        float aa = clamp(gl_FragCoord.w * min(d2.x, d2.y), 0.0, 1.0);
        gl_FragColor = color * aa;
      }
      // clang-format off
  );  // NOLINT(whitespace/parens)
  // clang-format on
}

FragmentShaderCheckerboard::FragmentShaderCheckerboard()
    : alpha_location_(-1),
      tex_transform_location_(-1),
      frequency_location_(-1) {
}

void FragmentShaderCheckerboard::Init(GLES2Interface* context,
                                      unsigned program,
                                      int* base_uniform_index) {
  static const char* uniforms[] = {
      "alpha", "texTransform", "frequency", "color",
  };
  int locations[arraysize(uniforms)];

  GetProgramUniformLocations(context,
                             program,
                             arraysize(uniforms),
                             uniforms,
                             locations,
                             base_uniform_index);
  alpha_location_ = locations[0];
  tex_transform_location_ = locations[1];
  frequency_location_ = locations[2];
  color_location_ = locations[3];
}

std::string FragmentShaderCheckerboard::GetShaderString(
    TexCoordPrecision precision,
    SamplerType sampler) const {
  // Shader based on Example 13-17 of "OpenGL ES 2.0 Programming Guide"
  // by Munshi, Ginsburg, Shreiner.
  // clang-format off
  return FRAGMENT_SHADER(
      // clang-format on
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
        vec2 texCoord =
            clamp(v_texCoord, 0.0, 1.0) * texTransform.zw + texTransform.xy;
        vec2 coord = mod(floor(texCoord * frequency * 2.0), 2.0);
        float picker = abs(coord.x - coord.y);  // NOLINT
        gl_FragColor = mix(color1, color2, picker) * alpha;
      }
      // clang-format off
  );  // NOLINT(whitespace/parens)
  // clang-format on
}

}  // namespace cc
