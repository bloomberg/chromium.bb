// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/tools/shader_bench/gpu_color_painter_exp.h"

// Matrix used for the YUV to RGB conversion.
static const float kYUV2RGB[9] = {
  1.f, 0.f, 1.403f,
  1.f, -.344f, -.714f,
  1.f, 1.772f, 0.f,
};

static const float kYUV2RGB_TRANS[9] = {
  1.f, 1.f, 1.f,
  0.f, -.344f, 1.772f,
  1.403f, -.714f, 0.f,
};

const float attributeSelector[4] = {
  1.f, 10.f, 100.f, 1000.f,
};

// Pass-through vertex shader.
static const char kVertexShader[] =
    "precision highp float;\n"
    "precision highp int;\n"
    "varying vec2 interp_tc;\n"
    "\n"
    "attribute vec4 in_pos;\n"
    "attribute vec2 in_tc;\n"
    "\n"
    "void main() {\n"
    "  interp_tc = in_tc;\n"
    "  gl_Position = in_pos;\n"
    "}\n";

// YUV to RGB pixel shader. Loads a pixel from each plane and pass through the
// matrix.
static const char kFragmentShader[] =
    "precision mediump float;\n"
    "precision mediump int;\n"
    "varying vec2 interp_tc;\n"
    "\n"
    "uniform sampler2D y_tex;\n"
    "uniform sampler2D u_tex;\n"
    "uniform sampler2D v_tex;\n"
    "uniform mat3 yuv2rgb;\n"
    "uniform vec4 attrib_vector;                          \n"
    "uniform int texture_width;                          \n"
    "\n"
    "void main() {\n"
    "  float x_pos = interp_tc.x * float(texture_width); \n"
    "  float y_index = floor(mod(x_pos, 4.0));            \n"
    "  float y_denom = pow(10.0, y_index);                \n"
    "  vec4 attrib_y = mod(floor(attrib_vector / y_denom), 10.0); \n"
    "  float uv_index = floor(mod(floor(x_pos / 4.0), 4.0)); \n"
    "  float uv_denom = pow(10.0, uv_index);              \n"
    "  vec4 attrib_uv = mod(floor(attrib_vector / uv_denom), 10.0); \n"
    "  vec4 texel_y = texture2D(y_tex, interp_tc);   \n"
    "  vec4 texel_u = texture2D(u_tex, interp_tc);   \n"
    "  vec4 texel_v = texture2D(v_tex, interp_tc);   \n"
    "  float y = dot(attrib_y, texel_y);                  \n"
    "  float u = dot(attrib_uv, texel_u) - 0.5;           \n"
    "  float v = dot(attrib_uv, texel_v) - 0.5;           \n"
    "  vec3 rgb = yuv2rgb * vec3(y, u, v);\n"
    "  gl_FragColor = vec4(rgb, 1);\n"
    "}\n";

GPUColorRGBALumHackPainter::GPUColorRGBALumHackPainter()
    : program_id_(-1) {
}

GPUColorRGBALumHackPainter::~GPUColorRGBALumHackPainter() {
  if (program_id_) {
    glDeleteProgram(program_id_);
    glDeleteTextures(media::VideoFrame::kNumYUVPlanes, textures_);
  }
}

void GPUColorRGBALumHackPainter::Initialize(int width, int height) {
  glGenTextures(3, textures_);
  for (unsigned int i = 0; i < media::VideoFrame::kNumYUVPlanes; ++i) {
    unsigned int texture_width = (i == media::VideoFrame::kYPlane) ?
        width : width / 2;
    unsigned int texture_height = (i == media::VideoFrame::kYPlane) ?
        height : height / 2;
    glActiveTexture(GL_TEXTURE0 + i);
    glBindTexture(GL_TEXTURE_2D, textures_[i]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texture_width / 4, texture_height,
                 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
  }

  GLuint program = CreateShaderProgram(kVertexShader, kFragmentShader);

  // Bind parameters.
  glUniform1i(glGetUniformLocation(program, "y_tex"), 0);
  glUniform1i(glGetUniformLocation(program, "u_tex"), 1);
  glUniform1i(glGetUniformLocation(program, "v_tex"), 2);
  glUniform1i(glGetUniformLocation(program, "texture_width"), width);
  int yuv2rgb_location = glGetUniformLocation(program, "yuv2rgb");
  if (gfx::GetGLImplementation() == gfx::kGLImplementationDesktopGL)
    glUniformMatrix3fv(yuv2rgb_location, 1, GL_TRUE, kYUV2RGB);
  else
    glUniformMatrix3fv(yuv2rgb_location, 1, GL_FALSE, kYUV2RGB_TRANS);
  int attrib_location = glGetUniformLocation(program, "attrib_vector");
  glUniform4fv(attrib_location, 1, attributeSelector);

  program_id_ = program;
}

void GPUColorRGBALumHackPainter::Paint(
    scoped_refptr<media::VideoFrame> video_frame) {
  for (unsigned int i = 0; i < media::VideoFrame::kNumYUVPlanes; ++i) {
    unsigned int width = (i == media::VideoFrame::kYPlane) ?
        video_frame->width() : video_frame->width() / 2;
    unsigned int height = (i == media::VideoFrame::kYPlane) ?
        video_frame->height() : video_frame->height() / 2;
    glBindTexture(GL_TEXTURE_2D, textures_[i]);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width / 4, height,
                    GL_RGBA, GL_UNSIGNED_BYTE, video_frame->data(i));
  }

  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  surface()->SwapBuffers();
}
