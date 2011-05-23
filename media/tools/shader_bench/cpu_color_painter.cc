// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/yuv_convert.h"
#include "media/tools/shader_bench/cpu_color_painter.h"

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

// RGB pixel shader.
static const char kFragmentShader[] =
    "precision mediump float;\n"
    "precision mediump int;\n"
    "varying vec2 interp_tc;\n"
    "\n"
    "uniform sampler2D rgba_tex;\n"
    "\n"
    "void main() {\n"
    "  vec4 texColor = texture2D(rgba_tex, interp_tc);"
    "  gl_FragColor = vec4(texColor.z, texColor.y, texColor.x, texColor.w);\n"
    "}\n";

CPUColorPainter::CPUColorPainter()
    : program_id_(-1) {
}

CPUColorPainter::~CPUColorPainter() {
  if (program_id_) {
    glDeleteProgram(program_id_);
    glDeleteTextures(media::VideoFrame::kNumRGBPlanes, textures_);
  }
}

void CPUColorPainter::Initialize(int width, int height) {
  glGenTextures(media::VideoFrame::kNumRGBPlanes, textures_);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, textures_[0]);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0,
               GL_RGBA, GL_UNSIGNED_BYTE, 0);

  GLuint program = CreateShaderProgram(kVertexShader, kFragmentShader);

  // Bind parameters.
  glUniform1i(glGetUniformLocation(program, "rgba_tex"), 0);
  program_id_ = program;
}

void CPUColorPainter::Paint(scoped_refptr<media::VideoFrame> video_frame) {
  // Convert to RGBA frame.
  scoped_refptr<media::VideoFrame> rgba_frame;
  media::VideoFrame::CreateFrame(media::VideoFrame::RGBA,
                             video_frame->width(),
                             video_frame->height(),
                             base::TimeDelta(),
                             base::TimeDelta(),
                             &rgba_frame);

  media::ConvertYUVToRGB32(video_frame->data(media::VideoFrame::kYPlane),
                           video_frame->data(media::VideoFrame::kUPlane),
                           video_frame->data(media::VideoFrame::kVPlane),
                           rgba_frame->data(0),
                           video_frame->width(),
                           video_frame->height(),
                           video_frame->stride(media::VideoFrame::kYPlane),
                           video_frame->stride(media::VideoFrame::kUPlane),
                           rgba_frame->stride(0),
                           media::YV12);

  glBindTexture(GL_TEXTURE_2D, textures_[0]);
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, rgba_frame->width(),
                  rgba_frame->height(), GL_RGBA, GL_UNSIGNED_BYTE,
                  rgba_frame->data(0));

  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  surface()->SwapBuffers();
}
