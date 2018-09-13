// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/gl_scaler.h"

#include <sstream>
#include <string>
#include <utility>

#include "base/logging.h"
#include "components/viz/common/gpu/context_provider.h"
#include "gpu/GLES2/gl2chromium.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/common/capabilities.h"
#include "ui/gfx/color_transform.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace viz {

GLScaler::GLScaler(scoped_refptr<ContextProvider> context_provider)
    : context_provider_(std::move(context_provider)) {
  if (context_provider_) {
    DCHECK(context_provider_->ContextGL());
    context_provider_->AddObserver(this);
  }
}

GLScaler::~GLScaler() {
  OnContextLost();  // Ensures destruction in dependency order.
}

bool GLScaler::SupportsPreciseColorManagement() const {
  if (!context_provider_) {
    return false;
  }
  const gpu::Capabilities& caps = context_provider_->ContextCapabilities();
  return caps.texture_half_float_linear && caps.color_buffer_half_float_rgba;
}

int GLScaler::GetMaxDrawBuffersSupported() const {
  if (!context_provider_) {
    return 0;
  }

  if (max_draw_buffers_ < 0) {
    // Query the GL context for the multiple draw buffers extension and, if
    // present, the actual platform-supported maximum.
    GLES2Interface* const gl = context_provider_->ContextGL();
    DCHECK(gl);
    if (const auto* extensions = gl->GetString(GL_EXTENSIONS)) {
      const std::string extensions_string =
          " " + std::string(reinterpret_cast<const char*>(extensions)) + " ";
      if (extensions_string.find(" GL_EXT_draw_buffers ") !=
          std::string::npos) {
        gl->GetIntegerv(GL_MAX_DRAW_BUFFERS_EXT, &max_draw_buffers_);
      }
    }

    if (max_draw_buffers_ < 1) {
      max_draw_buffers_ = 1;
    }
  }

  return max_draw_buffers_;
}

bool GLScaler::Configure(const Parameters& new_params) {
  shader_programs_.clear();

  if (!context_provider_) {
    return false;
  }
  GLES2Interface* const gl = context_provider_->ContextGL();
  DCHECK(gl);

  params_ = new_params;

  // Ensure the client has provided valid scaling vectors.
  if (params_.scale_from.x() == 0 || params_.scale_from.y() == 0 ||
      params_.scale_to.x() == 0 || params_.scale_to.y() == 0) {
    // The caller computed invalid scale_from and/or scale_to values.
    DVLOG(1) << __func__ << ": Invalid scaling vectors: scale_from="
             << params_.scale_from.ToString()
             << ", scale_to=" << params_.scale_to.ToString();
    return false;
  }

  // Resolve the color spaces according to the rules described in the header
  // file.
  if (!params_.source_color_space.IsValid()) {
    params_.source_color_space = gfx::ColorSpace::CreateSRGB();
  }
  if (!params_.output_color_space.IsValid()) {
    params_.output_color_space = params_.source_color_space;
  }

  // Check that 16-bit half floats are supported if precise color management is
  // being requested.
  if (params_.enable_precise_color_management) {
    if (!SupportsPreciseColorManagement()) {
      DVLOG(1) << __func__
               << ": GL context does not support the half-floats "
                  "required for precise color management.";
      return false;
    }
  }

  // Check that MRT support is available if certain export formats were
  // specified in the Parameters.
  if (params_.export_format == Parameters::ExportFormat::NV61 ||
      params_.export_format ==
          Parameters::ExportFormat::DEINTERLEAVE_PAIRWISE) {
    if (GetMaxDrawBuffersSupported() < 2) {
      DVLOG(1) << __func__ << ": GL context does not support 2+ draw buffers.";
      return false;
    }
  }

  // Color space transformation is meaningless when using the deinterleaver.
  if (params_.export_format ==
          Parameters::ExportFormat::DEINTERLEAVE_PAIRWISE &&
      params_.source_color_space != params_.output_color_space) {
    NOTIMPLEMENTED();
    return false;
  }

  // Check that one of the two implemented output swizzles has been specified.
  for (GLenum s : params_.swizzle) {
    if (s != GL_RGBA && s != GL_BGRA_EXT) {
      NOTIMPLEMENTED();
      return false;
    }
  }

  return true;
}

bool GLScaler::ScaleToMultipleOutputs(GLuint src_texture,
                                      const gfx::Size& src_texture_size,
                                      const gfx::Vector2dF& src_offset,
                                      GLuint dest_texture_0,
                                      GLuint dest_texture_1,
                                      const gfx::Rect& output_rect) {
  NOTIMPLEMENTED();
  return false;
}

bool GLScaler::ComputeRegionOfInfluence(const gfx::Size& src_texture_size,
                                        const gfx::Vector2dF& src_offset,
                                        const gfx::Rect& output_rect,
                                        gfx::Rect* sampling_rect,
                                        gfx::Vector2dF* offset) const {
  NOTIMPLEMENTED();
  return false;
}

// static
bool GLScaler::ParametersHasSameScaleRatio(const GLScaler::Parameters& params,
                                           const gfx::Vector2d& from,
                                           const gfx::Vector2d& to) {
  // Returns true iff a_num/a_denom == b_num/b_denom.
  const auto AreRatiosEqual = [](int32_t a_num, int32_t a_denom, int32_t b_num,
                                 int32_t b_denom) -> bool {
    // The math (for each dimension):
    //   If: a_num/a_denom == b_num/b_denom
    //   Then: a_num*b_denom == b_num*a_denom
    //
    // ...and cast to int64_t to guarantee no overflow from the multiplications.
    return (static_cast<int64_t>(a_num) * b_denom) ==
           (static_cast<int64_t>(b_num) * a_denom);
  };

  return AreRatiosEqual(params.scale_from.x(), params.scale_to.x(), from.x(),
                        to.x()) &&
         AreRatiosEqual(params.scale_from.y(), params.scale_to.y(), from.y(),
                        to.y());
}

void GLScaler::OnContextLost() {
  // The destruction order here is important due to data dependencies.
  shader_programs_.clear();
  if (context_provider_) {
    context_provider_->RemoveObserver(this);
    context_provider_ = nullptr;
  }
}

GLScaler::ShaderProgram* GLScaler::GetShaderProgram(
    Shader shader,
    GLint texture_format,
    const gfx::ColorTransform* color_transform,
    const GLenum swizzle[2]) {
  const ShaderCacheKey key{
      shader,
      texture_format,
      color_transform ? color_transform->GetSrcColorSpace() : gfx::ColorSpace(),
      color_transform ? color_transform->GetDstColorSpace() : gfx::ColorSpace(),
      swizzle[0],
      swizzle[1]};
  auto it = shader_programs_.find(key);
  if (it == shader_programs_.end()) {
    GLES2Interface* const gl = context_provider_->ContextGL();
    DCHECK(gl);
    it = shader_programs_
             .emplace(std::piecewise_construct, std::forward_as_tuple(key),
                      std::forward_as_tuple(gl, shader, texture_format,
                                            color_transform, swizzle))
             .first;
  }
  return &it->second;
}

GLScaler::Parameters::Parameters() = default;
GLScaler::Parameters::~Parameters() = default;

// static
const GLfloat GLScaler::ShaderProgram::kVertexAttributes[16] = {
    -1.0f, -1.0f, 0.0f, 0.0f,  // vertex 0
    1.0f,  -1.0f, 1.0f, 0.0f,  // vertex 1
    -1.0f, 1.0f,  0.0f, 1.0f,  // vertex 2
    1.0f,  1.0f,  1.0f, 1.0f,  // vertex 3
};

GLScaler::ShaderProgram::ShaderProgram(
    gpu::gles2::GLES2Interface* gl,
    GLScaler::Shader shader,
    GLint texture_format,
    const gfx::ColorTransform* color_transform,
    const GLenum swizzle[2])
    : gl_(gl),
      shader_(shader),
      texture_format_(texture_format),
      program_(gl_->CreateProgram()) {
  DCHECK(program_);

  std::basic_ostringstream<GLchar> vertex_header;
  std::basic_ostringstream<GLchar> fragment_directives;
  std::basic_ostringstream<GLchar> fragment_header;
  std::basic_ostringstream<GLchar> shared_variables;
  std::basic_ostringstream<GLchar> vertex_main;
  std::basic_ostringstream<GLchar> fragment_main;

  vertex_header
      << ("precision highp float;\n"
          "attribute vec2 a_position;\n"
          "attribute vec2 a_texcoord;\n"
          "uniform vec4 src_rect;\n");

  fragment_header << "precision mediump float;\n";
  if (texture_format_ == GL_RGBA16F_EXT) {
    fragment_header << "precision mediump sampler2D;\n";
  } else if (texture_format_ == GL_RGBA) {
    fragment_header << "precision lowp sampler2D;\n";
  } else {
    NOTIMPLEMENTED();
  }
  fragment_header << "uniform sampler2D s_texture;\n";

  if (color_transform && shader_ != Shader::PLANAR_CHANNEL_3) {
    const std::string& source = color_transform->GetShaderSource();
    // Assumption: gfx::ColorTransform::GetShaderSource() should provide a
    // function named DoColorConversion() that takes a vec3 argument and returns
    // a vec3.
    DCHECK_NE(source.find("DoColorConversion"), std::string::npos);
    fragment_header << source;
  }

  vertex_main
      << ("  gl_Position = vec4(a_position, 0.0, 1.0);\n"
          "  vec2 texcoord = src_rect.xy + a_texcoord * src_rect.zw;\n");

  switch (shader_) {
    case Shader::BILINEAR:
      shared_variables << "varying highp vec2 v_texcoord;\n";
      vertex_main << "  v_texcoord = texcoord;\n";
      fragment_main << "  vec4 sample = texture2D(s_texture, v_texcoord);\n";
      if (color_transform) {
        fragment_main << "  sample.rgb = DoColorConversion(sample.rgb);\n";
      }
      if (swizzle[0] == GL_BGRA_EXT) {
        fragment_main << "  sample.rb = sample.br;\n";
      }
      fragment_main << "  gl_FragColor = sample;\n";
      break;

    case Shader::BILINEAR2:
      // This is equivialent to two passes of the BILINEAR shader above. It can
      // be used to scale an image down 1.0x-2.0x in either dimension, or
      // exactly 4x.
      shared_variables << "varying highp vec4 v_texcoords;\n";
      vertex_header << "uniform vec2 scaling_vector;\n";
      vertex_main
          << ("  vec2 step = scaling_vector / 4.0;\n"
              "  v_texcoords.xy = texcoord + step;\n"
              "  v_texcoords.zw = texcoord - step;\n");
      fragment_main
          << ("  vec4 blended = (texture2D(s_texture, v_texcoords.xy) +\n"
              "                  texture2D(s_texture, v_texcoords.zw)) /\n"
              "                 2.0;\n");
      if (color_transform) {
        fragment_main << "  blended.rgb = DoColorConversion(blended.rgb);\n";
      }
      if (swizzle[0] == GL_BGRA_EXT) {
        fragment_main << "  blended.rb = blended.br;\n";
      }
      fragment_main << "  gl_FragColor = blended;\n";
      break;

    case Shader::BILINEAR3:
      // This is kind of like doing 1.5 passes of the BILINEAR shader. It can be
      // used to scale an image down 1.5x-3.0x, or exactly 6x.
      shared_variables
          << ("varying highp vec4 v_texcoords0;\n"
              "varying highp vec2 v_texcoords1;\n");
      vertex_header << "uniform vec2 scaling_vector;\n";
      vertex_main
          << ("  vec2 step = scaling_vector / 3.0;\n"
              "  v_texcoords0.xy = texcoord + step;\n"
              "  v_texcoords0.zw = texcoord;\n"
              "  v_texcoords1 = texcoord - step;\n");
      fragment_main
          << ("  vec4 blended = (texture2D(s_texture, v_texcoords0.xy) +\n"
              "                  texture2D(s_texture, v_texcoords0.zw) +\n"
              "                  texture2D(s_texture, v_texcoords1)) / 3.0;\n");
      if (color_transform) {
        fragment_main << "  blended.rgb = DoColorConversion(blended.rgb);\n";
      }
      if (swizzle[0] == GL_BGRA_EXT) {
        fragment_main << "  blended.rb = blended.br;\n";
      }
      fragment_main << "  gl_FragColor = blended;\n";
      break;

    case Shader::BILINEAR4:
      // This is equivialent to three passes of the BILINEAR shader above. It
      // can be used to scale an image down 2.0x-4.0x or exactly 8x.
      shared_variables << "varying highp vec4 v_texcoords[2];\n";
      vertex_header << "uniform vec2 scaling_vector;\n";
      vertex_main
          << ("  vec2 step = scaling_vector / 8.0;\n"
              "  v_texcoords[0].xy = texcoord - step * 3.0;\n"
              "  v_texcoords[0].zw = texcoord - step;\n"
              "  v_texcoords[1].xy = texcoord + step;\n"
              "  v_texcoords[1].zw = texcoord + step * 3.0;\n");
      fragment_main
          << ("  vec4 blended = (\n"
              "      texture2D(s_texture, v_texcoords[0].xy) +\n"
              "      texture2D(s_texture, v_texcoords[0].zw) +\n"
              "      texture2D(s_texture, v_texcoords[1].xy) +\n"
              "      texture2D(s_texture, v_texcoords[1].zw)) / 4.0;\n");
      if (color_transform) {
        fragment_main << "  blended.rgb = DoColorConversion(blended.rgb);\n";
      }
      if (swizzle[0] == GL_BGRA_EXT) {
        fragment_main << "  blended.rb = blended.br;\n";
      }
      fragment_main << "  gl_FragColor = blended;\n";
      break;

    case Shader::BILINEAR2X2:
      // This is equivialent to four passes of the BILINEAR shader above, two in
      // each dimension. It can be used to scale an image down 1.0x-2.0x in both
      // X and Y directions. Or, it could be used to scale an image down by
      // exactly 4x in both dimensions.
      shared_variables << "varying highp vec4 v_texcoords[2];\n";
      vertex_header << "uniform vec2 scaling_vector;\n";
      vertex_main
          << ("  vec2 step = scaling_vector / 4.0;\n"
              "  v_texcoords[0].xy = texcoord + vec2(step.x, step.y);\n"
              "  v_texcoords[0].zw = texcoord + vec2(step.x, -step.y);\n"
              "  v_texcoords[1].xy = texcoord + vec2(-step.x, step.y);\n"
              "  v_texcoords[1].zw = texcoord + vec2(-step.x, -step.y);\n");
      fragment_main
          << ("  vec4 blended = (\n"
              "      texture2D(s_texture, v_texcoords[0].xy) +\n"
              "      texture2D(s_texture, v_texcoords[0].zw) +\n"
              "      texture2D(s_texture, v_texcoords[1].xy) +\n"
              "      texture2D(s_texture, v_texcoords[1].zw)) / 4.0;\n");
      if (color_transform) {
        fragment_main << "  blended.rgb = DoColorConversion(blended.rgb);\n";
      }
      if (swizzle[0] == GL_BGRA_EXT) {
        fragment_main << "  blended.rb = blended.br;\n";
      }
      fragment_main << "  gl_FragColor = blended;\n";
      break;

    case Shader::BICUBIC_UPSCALE:
      // When scaling up, 4 texture reads are necessary. However, some
      // instructions can be saved because the parameter passed to the bicubic
      // function will be in a known range. Also, when sampling the bicubic
      // function like this, the sum is always exactly one, so normalization can
      // be skipped as well.
      shared_variables << "varying highp vec2 v_texcoord;\n";
      vertex_main << "  v_texcoord = texcoord;\n";
      fragment_header
          << ("uniform highp vec2 src_pixelsize;\n"
              "uniform highp vec2 scaling_vector;\n"
              "const float a = -0.5;\n"
              // This function is equivialent to calling the bicubic
              // function with x-1, x, 1-x and 2-x (assuming
              // 0 <= x < 1)
              "vec4 filt4(float x) {\n"
              "  return vec4(x * x * x, x * x, x, 1) *\n"
              "         mat4(       a,      -2.0 * a,   a, 0.0,\n"
              "               a + 2.0,      -a - 3.0, 0.0, 1.0,\n"
              "              -a - 2.0, 3.0 + 2.0 * a,  -a, 0.0,\n"
              "                    -a,             a, 0.0, 0.0);\n"
              "}\n"
              "mat4 pixels_x(highp vec2 pos, highp vec2 step) {\n"
              "  return mat4(texture2D(s_texture, pos - step),\n"
              "              texture2D(s_texture, pos),\n"
              "              texture2D(s_texture, pos + step),\n"
              "              texture2D(s_texture, pos + step * 2.0));\n"
              "}\n");
      fragment_main
          << ("  highp vec2 pixel_pos = v_texcoord * src_pixelsize - \n"
              "      scaling_vector / 2.0;\n"
              "  highp float frac = fract(dot(pixel_pos, scaling_vector));\n"
              "  highp vec2 base = \n"
              "      (floor(pixel_pos) + vec2(0.5)) / src_pixelsize;\n"
              "  highp vec2 step = scaling_vector / src_pixelsize;\n"
              "  vec4 blended = pixels_x(base, step) * filt4(frac);\n");
      if (color_transform) {
        fragment_main << "  blended.rgb = DoColorConversion(blended.rgb);\n";
      }
      if (swizzle[0] == GL_BGRA_EXT) {
        fragment_main << "  blended.rb = blended.br;\n";
      }
      fragment_main << "  gl_FragColor = blended;\n";
      break;

    case Shader::BICUBIC_HALF_1D:
      // This scales down an image by exactly half in one dimension. The
      // bilinear lookup reduces the number of texture reads from 8 to 4.
      shared_variables << "varying highp vec4 v_texcoords[2];\n";
      vertex_header
          << ("uniform vec2 scaling_vector;\n"
              "const float CenterDist = 99.0 / 140.0;\n"
              "const float LobeDist = 11.0 / 4.0;\n");
      vertex_main
          << ("  vec2 step = scaling_vector / 2.0;\n"
              "  v_texcoords[0].xy = texcoord - LobeDist * step;\n"
              "  v_texcoords[0].zw = texcoord - CenterDist * step;\n"
              "  v_texcoords[1].xy = texcoord + CenterDist * step;\n"
              "  v_texcoords[1].zw = texcoord + LobeDist * step;\n");
      fragment_header
          << ("const float CenterWeight = 35.0 / 64.0;\n"
              "const float LobeWeight = -3.0 / 64.0;\n");
      fragment_main
          << ("  vec4 blended = \n"
              // Lobe pixels
              "      (texture2D(s_texture, v_texcoords[0].xy) +\n"
              "       texture2D(s_texture, v_texcoords[1].zw)) *\n"
              "          LobeWeight +\n"
              // Center pixels
              "      (texture2D(s_texture, v_texcoords[0].zw) +\n"
              "       texture2D(s_texture, v_texcoords[1].xy)) *\n"
              "          CenterWeight;\n");
      if (color_transform) {
        fragment_main << "  blended.rgb = DoColorConversion(blended.rgb);\n";
      }
      if (swizzle[0] == GL_BGRA_EXT) {
        fragment_main << "  blended.rb = blended.br;\n";
      }
      fragment_main << "  gl_FragColor = blended;\n";
      break;

    case Shader::PLANAR_CHANNEL_0:
    case Shader::PLANAR_CHANNEL_1:
    case Shader::PLANAR_CHANNEL_2:
    case Shader::PLANAR_CHANNEL_3: {
      // Select one color channel, and pack 4 pixels into one output quad. See
      // header file for diagram.
      shared_variables << "varying highp vec4 v_texcoords[2];\n";
      vertex_header << "uniform vec2 scaling_vector;\n";
      vertex_main
          << ("  vec2 step = scaling_vector / 4.0;\n"
              "  v_texcoords[0].xy = texcoord - step * 1.5;\n"
              "  v_texcoords[0].zw = texcoord - step * 0.5;\n"
              "  v_texcoords[1].xy = texcoord + step * 0.5;\n"
              "  v_texcoords[1].zw = texcoord + step * 1.5;\n");
      std::basic_string<GLchar> convert_open;
      std::basic_string<GLchar> convert_close;
      if (color_transform && shader_ != Shader::PLANAR_CHANNEL_3) {
        convert_open = "DoColorConversion(";
        convert_close = ".rgb)";
      }
      const char selector = "rgba"[static_cast<int>(shader_) -
                                   static_cast<int>(Shader::PLANAR_CHANNEL_0)];
      fragment_main << "  vec4 packed_quad = vec4(\n"
                    << "      " << convert_open
                    << "texture2D(s_texture, v_texcoords[0].xy)"
                    << convert_close << '.' << selector << ",\n"
                    << "      " << convert_open
                    << "texture2D(s_texture, v_texcoords[0].zw)"
                    << convert_close << '.' << selector << ",\n"
                    << "      " << convert_open
                    << "texture2D(s_texture, v_texcoords[1].xy)"
                    << convert_close << '.' << selector << ",\n"
                    << "      " << convert_open
                    << "texture2D(s_texture, v_texcoords[1].zw)"
                    << convert_close << '.' << selector << ");\n";
      if (swizzle[0] == GL_BGRA_EXT) {
        fragment_main << "  packed_quad.rb = packed_quad.br;\n";
      }
      fragment_main << "  gl_FragColor = packed_quad;\n";
      break;
    }

    case Shader::I422_NV61_MRT:
      // I422 sampling, delivered via two output textures (NV61 format). See
      // header file for diagram.
      shared_variables << "varying highp vec4 v_texcoords[2];\n";
      vertex_header << "uniform vec2 scaling_vector;\n";
      vertex_main
          << ("  vec2 step = scaling_vector / 4.0;\n"
              "  v_texcoords[0].xy = texcoord - step * 1.5;\n"
              "  v_texcoords[0].zw = texcoord - step * 0.5;\n"
              "  v_texcoords[1].xy = texcoord + step * 0.5;\n"
              "  v_texcoords[1].zw = texcoord + step * 1.5;\n");
      fragment_directives << "#extension GL_EXT_draw_buffers : enable\n";
      fragment_main
          << ("  vec3 pixel0 = texture2D(s_texture, v_texcoords[0].xy).rgb;\n"
              "  vec3 pixel1 = texture2D(s_texture, v_texcoords[0].zw).rgb;\n"
              "  vec3 pixel01 = (pixel0 + pixel1) / 2.0;\n"
              "  vec3 pixel2 = texture2D(s_texture, v_texcoords[1].xy).rgb;\n"
              "  vec3 pixel3 = texture2D(s_texture, v_texcoords[1].zw).rgb;\n"
              "  vec3 pixel23 = (pixel2 + pixel3) / 2.0;\n");
      if (color_transform) {
        fragment_main
            << ("  pixel0 = DoColorConversion(pixel0);\n"
                "  pixel1 = DoColorConversion(pixel1);\n"
                "  pixel01 = DoColorConversion(pixel01);\n"
                "  pixel2 = DoColorConversion(pixel2);\n"
                "  pixel3 = DoColorConversion(pixel3);\n"
                "  pixel23 = DoColorConversion(pixel23);\n");
      }
      if (swizzle[0] == GL_BGRA_EXT) {
        fragment_main
            << ("  gl_FragData[0] = \n"
                "      vec4(pixel2.r, pixel1.r, pixel0.r, pixel3.r);\n");
      } else {
        fragment_main
            << ("  gl_FragData[0] = \n"
                "      vec4(pixel0.r, pixel1.r, pixel2.r, pixel3.r);\n");
      }
      if (swizzle[1] == GL_BGRA_EXT) {
        fragment_main
            << ("  gl_FragData[1] = \n"
                "      vec4(pixel23.g, pixel01.b, pixel01.g, pixel23.b);\n");
      } else {
        fragment_main
            << ("  gl_FragData[1] = \n"
                "      vec4(pixel01.g, pixel01.b, pixel23.g, pixel23.b);\n");
      }
      break;

    case Shader::DEINTERLEAVE_PAIRWISE_MRT:
      // Sample two pixels and unswizzle them. There's no need to do vertical
      // scaling with math, since the bilinear interpolation in the sampler
      // takes care of that.
      shared_variables << "varying highp vec4 v_texcoords;\n";
      vertex_header << "uniform vec2 scaling_vector;\n";
      vertex_main
          << ("  vec2 step = scaling_vector / 2.0;\n"
              "  v_texcoords.xy = texcoord - step * 0.5;\n"
              "  v_texcoords.zw = texcoord + step * 0.5;\n");
      fragment_directives << "#extension GL_EXT_draw_buffers : enable\n";
      DCHECK(!color_transform);
      fragment_main
          << ("  vec4 lo_uvuv = texture2D(s_texture, v_texcoords.xy);\n"
              "  vec4 hi_uvuv = texture2D(s_texture, v_texcoords.zw);\n"
              "  vec4 uuuu = vec4(lo_uvuv.rb, hi_uvuv.rb);\n"
              "  vec4 vvvv = vec4(lo_uvuv.ga, hi_uvuv.ga);\n");
      if (swizzle[0] == GL_BGRA_EXT) {
        fragment_main << "  uuuu.rb = uuuu.br;\n";
      }
      fragment_main << "  gl_FragData[0] = uuuu;\n";
      if (swizzle[1] == GL_BGRA_EXT) {
        fragment_main << "  vvvv.rb = vvvv.br;\n";
      }
      fragment_main << "  gl_FragData[1] = vvvv;\n";
      break;
  }

  // Helper function to compile the shader source and log the GLSL compiler's
  // results.
  const auto CompileShaderFromSource =
      [](GLES2Interface* gl, const std::basic_string<GLchar>& source,
         GLenum type) -> GLuint {
    VLOG(2) << __func__ << ": Compiling shader " << type
            << " with source:" << std::endl
            << source;
    const GLuint shader = gl->CreateShader(type);
    const GLchar* source_data = source.data();
    const GLint length = base::checked_cast<GLint>(source.size());
    gl->ShaderSource(shader, 1, &source_data, &length);
    gl->CompileShader(shader);
    GLint compile_status = GL_FALSE;
    gl->GetShaderiv(shader, GL_COMPILE_STATUS, &compile_status);

    // Fetch logs and forward them to the system logger. If compilation failed,
    // clean-up and return 0 for error.
    if (compile_status != GL_TRUE || VLOG_IS_ON(2)) {
      GLint log_length = 0;
      gl->GetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_length);
      std::string log;
      if (log_length > 0) {
        std::unique_ptr<GLchar[]> tmp(new GLchar[log_length]);
        GLsizei returned_log_length = 0;
        gl->GetShaderInfoLog(shader, log_length, &returned_log_length,
                             tmp.get());
        log.assign(tmp.get(), returned_log_length);
      }
      if (log.empty()) {
        log = "<<NO LOG>>";
      }
      if (compile_status != GL_TRUE) {
        LOG(ERROR) << __func__ << ": Compilation of shader " << type
                   << " failed:" << std::endl
                   << log;
        gl->DeleteShader(shader);
        return 0;
      }
      VLOG(2) << __func__ << ": Compilation of shader " << type
              << " succeeded:" << std::endl
              << log;
    }
    return shader;
  };

  // Compile the vertex shader and attach it to the program.
  const std::string shared_variables_str = shared_variables.str();
  const GLuint vertex_shader =
      CompileShaderFromSource(gl_,
                              vertex_header.str() + shared_variables_str +
                                  "void main() {\n" + vertex_main.str() + "}\n",
                              GL_VERTEX_SHADER);
  if (vertex_shader == 0) {
    return;
  }
  gl_->AttachShader(program_, vertex_shader);
  gl_->DeleteShader(vertex_shader);

  // Compile the fragment shader and attach to |program_|.
  const GLuint fragment_shader = CompileShaderFromSource(
      gl_,
      fragment_directives.str() + fragment_header.str() + shared_variables_str +
          "void main() {\n" + fragment_main.str() + "}\n",
      GL_FRAGMENT_SHADER);
  if (fragment_shader == 0) {
    return;
  }
  gl_->AttachShader(program_, fragment_shader);
  gl_->DeleteShader(fragment_shader);

  // Link the program.
  gl_->LinkProgram(program_);
  GLint link_status = GL_FALSE;
  gl_->GetProgramiv(program_, GL_LINK_STATUS, &link_status);
  if (link_status != GL_TRUE) {
    LOG(ERROR) << "Failed to link shader program.";
    return;
  }

#define DCHECK_RESOLVED_LOCATION(member)                                  \
  DCHECK(member != -1 || gl_->GetGraphicsResetStatusKHR() != GL_NO_ERROR) \
      << "Failed to get " #member " in program, or GPU process crashed."

  // Resolve the locations of the global variables.
  position_location_ = gl_->GetAttribLocation(program_, "a_position");
  DCHECK_RESOLVED_LOCATION(position_location_);
  texcoord_location_ = gl_->GetAttribLocation(program_, "a_texcoord");
  DCHECK_RESOLVED_LOCATION(texcoord_location_);
  texture_location_ = gl_->GetUniformLocation(program_, "s_texture");
  DCHECK_RESOLVED_LOCATION(texture_location_);
  src_rect_location_ = gl_->GetUniformLocation(program_, "src_rect");
  DCHECK_RESOLVED_LOCATION(src_rect_location_);
  switch (shader_) {
    case Shader::BILINEAR:
      break;

    case Shader::BILINEAR2:
    case Shader::BILINEAR3:
    case Shader::BILINEAR4:
    case Shader::BILINEAR2X2:
    case Shader::BICUBIC_HALF_1D:
    case Shader::PLANAR_CHANNEL_0:
    case Shader::PLANAR_CHANNEL_1:
    case Shader::PLANAR_CHANNEL_2:
    case Shader::PLANAR_CHANNEL_3:
    case Shader::I422_NV61_MRT:
    case Shader::DEINTERLEAVE_PAIRWISE_MRT:
      scaling_vector_location_ =
          gl_->GetUniformLocation(program_, "scaling_vector");
      DCHECK_RESOLVED_LOCATION(scaling_vector_location_);
      break;

    case Shader::BICUBIC_UPSCALE:
      src_pixelsize_location_ =
          gl_->GetUniformLocation(program_, "src_pixelsize");
      DCHECK_RESOLVED_LOCATION(src_pixelsize_location_);
      scaling_vector_location_ =
          gl_->GetUniformLocation(program_, "scaling_vector");
      DCHECK_RESOLVED_LOCATION(scaling_vector_location_);
      break;
  }

#undef DCHECK_RESOLVED_LOCATION
}

GLScaler::ShaderProgram::~ShaderProgram() {
  gl_->DeleteProgram(program_);
}

void GLScaler::ShaderProgram::UseProgram(const gfx::Size& src_texture_size,
                                         const gfx::RectF& src_rect,
                                         const gfx::Size& dst_size,
                                         GLScaler::Axis primary_axis,
                                         bool flip_y) {
  gl_->UseProgram(program_);

  // OpenGL defines the last parameter to VertexAttribPointer as type
  // "const GLvoid*" even though it is actually an offset into the buffer
  // object's data store and not a pointer to the client's address space.
  const void* offsets[2] = {nullptr,
                            reinterpret_cast<const void*>(2 * sizeof(GLfloat))};

  gl_->VertexAttribPointer(position_location_, 2, GL_FLOAT, GL_FALSE,
                           4 * sizeof(GLfloat), offsets[0]);
  gl_->EnableVertexAttribArray(position_location_);

  gl_->VertexAttribPointer(texcoord_location_, 2, GL_FLOAT, GL_FALSE,
                           4 * sizeof(GLfloat), offsets[1]);
  gl_->EnableVertexAttribArray(texcoord_location_);

  // Always sample from the first texture unit.
  gl_->Uniform1i(texture_location_, 0);

  // Convert |src_rect| from pixel coordinates to texture coordinates. The
  // source texture coordinates are in the range [0.0,1.0] for each dimension,
  // but the sampling rect may slightly "spill" outside that range (e.g., for
  // scaler overscan).
  GLfloat src_rect_texcoord[4] = {
      src_rect.x() / src_texture_size.width(),
      src_rect.y() / src_texture_size.height(),
      src_rect.width() / src_texture_size.width(),
      src_rect.height() / src_texture_size.height(),
  };
  if (flip_y) {
    src_rect_texcoord[1] += src_rect_texcoord[3];
    src_rect_texcoord[3] *= -1.0f;
  }
  gl_->Uniform4fv(src_rect_location_, 1, src_rect_texcoord);

  // Set shader-specific uniform inputs. The |scaling_vector| is the ratio of
  // the number of source pixels sampled per dest pixels output. It is used by
  // the shader programs to locate distinct texels from the source texture, and
  // sample them at the appropriate offset to produce each output texel.
  switch (shader_) {
    case Shader::BILINEAR:
      break;

    case Shader::BILINEAR2:
    case Shader::BILINEAR3:
    case Shader::BILINEAR4:
    case Shader::BICUBIC_HALF_1D:
    case Shader::PLANAR_CHANNEL_0:
    case Shader::PLANAR_CHANNEL_1:
    case Shader::PLANAR_CHANNEL_2:
    case Shader::PLANAR_CHANNEL_3:
    case Shader::I422_NV61_MRT:
    case Shader::DEINTERLEAVE_PAIRWISE_MRT:
      switch (primary_axis) {
        case HORIZONTAL:
          gl_->Uniform2f(scaling_vector_location_,
                         src_rect_texcoord[2] / dst_size.width(), 0.0);
          break;
        case VERTICAL:
          gl_->Uniform2f(scaling_vector_location_, 0.0,
                         src_rect_texcoord[3] / dst_size.height());
          break;
      }
      break;

    case Shader::BILINEAR2X2:
      gl_->Uniform2f(scaling_vector_location_,
                     src_rect_texcoord[2] / dst_size.width(),
                     src_rect_texcoord[3] / dst_size.height());
      break;

    case Shader::BICUBIC_UPSCALE:
      gl_->Uniform2f(src_pixelsize_location_, src_texture_size.width(),
                     src_texture_size.height());
      // For this shader program, the |scaling_vector| has an alternate meaning:
      // It is only being used to select whether bicubic sampling is stepped in
      // the X or the Y direction.
      gl_->Uniform2f(scaling_vector_location_,
                     primary_axis == HORIZONTAL ? 1.0 : 0.0,
                     primary_axis == VERTICAL ? 1.0 : 0.0);
      break;
  }
}

}  // namespace viz
