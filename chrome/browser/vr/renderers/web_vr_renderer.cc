// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/renderers/web_vr_renderer.h"

#include "chrome/browser/vr/vr_gl_util.h"

namespace vr {

namespace {

// clang-format off
static constexpr char const* kWebVrVertexShader = SHADER(
  precision mediump float;
  attribute vec4 a_Position;
  varying vec2 v_TexCoordinate;

  void main() {
    v_TexCoordinate = vec2(0.5 + a_Position[0], 0.5 - a_Position[1]);
    gl_Position = vec4(a_Position.xyz * 2.0, 1.0);
  }
);

static constexpr char const* kWebVrFragmentShader = OEIE_SHADER(
  precision highp float;
  uniform samplerExternalOES u_Texture;
  varying vec2 v_TexCoordinate;

  void main() {
    gl_FragColor = texture2D(u_Texture, v_TexCoordinate);
  }
);
// clang-format on

}  // namespace

WebVrRenderer::WebVrRenderer()
    : BaseQuadRenderer(kWebVrVertexShader, kWebVrFragmentShader) {
  texture_handle_ = glGetUniformLocation(program_handle_, "u_Texture");
}

// Draw the stereo WebVR frame
void WebVrRenderer::Draw(int texture_handle) {
  glUseProgram(program_handle_);

  // Bind vertex attributes
  glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer_);

  // Set up position attribute.
  glVertexAttribPointer(position_handle_, 2, GL_FLOAT, false, 0, 0);
  glEnableVertexAttribArray(position_handle_);

  // Bind texture. This is a 1:1 pixel copy since the source surface
  // and renderbuffer destination size are resized to match, so use
  // GL_NEAREST.
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_EXTERNAL_OES, texture_handle);
  SetTexParameters(GL_TEXTURE_EXTERNAL_OES);
  glUniform1i(texture_handle_, 0);

  // Blit texture to buffer
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer_);
  glDrawElements(GL_TRIANGLES, BaseQuadRenderer::NumQuadIndices(),
                 GL_UNSIGNED_SHORT, 0);

  glDisableVertexAttribArray(position_handle_);
}

// Note that we don't explicitly delete gl objects here, they're deleted
// automatically when we call ShutdownGL, and deleting them here leads to
// segfaults.
WebVrRenderer::~WebVrRenderer() = default;

}  // namespace vr
