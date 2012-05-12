// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/tools/shader_bench/gpu_color_painter.h"
#include "ui/gl/gl_context.h"

enum { kNumYUVPlanes = 3 };

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
    "\n"
    "void main() {\n"
    "  float y = texture2D(y_tex, interp_tc).x;\n"
    "  float u = texture2D(u_tex, interp_tc).r - .5;\n"
    "  float v = texture2D(v_tex, interp_tc).r - .5;\n"
    "  vec3 rgb = yuv2rgb * vec3(y, u, v);\n"
    "  gl_FragColor = vec4(rgb, 1);\n"
    "}\n";

GPUColorWithLuminancePainter::GPUColorWithLuminancePainter()
    : program_id_(-1) {
}

GPUColorWithLuminancePainter::~GPUColorWithLuminancePainter() {
  if (program_id_) {
    glDeleteProgram(program_id_);
    glDeleteTextures(kNumYUVPlanes, textures_);
  }
}

void GPUColorWithLuminancePainter::Initialize(int width, int height) {
  // Create 3 textures, one for each plane, and bind them to different
  // texture units.
  glGenTextures(kNumYUVPlanes, textures_);

  for (unsigned int i = 0; i < kNumYUVPlanes; ++i) {
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
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, texture_width, texture_height,
                 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, 0);
  }

  GLuint program = CreateShaderProgram(kVertexShader, kFragmentShader);

  // Bind parameters.
  glUniform1i(glGetUniformLocation(program, "y_tex"), 0);
  glUniform1i(glGetUniformLocation(program, "u_tex"), 1);
  glUniform1i(glGetUniformLocation(program, "v_tex"), 2);
  int yuv2rgb_location = glGetUniformLocation(program, "yuv2rgb");

  // DesktopGL supports transpose matrices.
  if (gfx::GetGLImplementation() == gfx::kGLImplementationDesktopGL)
    glUniformMatrix3fv(yuv2rgb_location, 1, GL_TRUE, kYUV2RGB);
  else
    glUniformMatrix3fv(yuv2rgb_location, 1, GL_FALSE, kYUV2RGB_TRANS);

  program_id_ = program;
}

void GPUColorWithLuminancePainter::Paint(
    scoped_refptr<media::VideoFrame> video_frame) {
  for (unsigned int i = 0; i < kNumYUVPlanes; ++i) {
    unsigned int width = (i == media::VideoFrame::kYPlane) ?
        video_frame->width() : video_frame->width() / 2;
    unsigned int height = (i == media::VideoFrame::kYPlane) ?
        video_frame->height() : video_frame->height() / 2;
    glBindTexture(GL_TEXTURE_2D, textures_[i]);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height,
                    GL_LUMINANCE, GL_UNSIGNED_BYTE, video_frame->data(i));
  }

  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  surface()->SwapBuffers();
}
