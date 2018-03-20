// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/renderers/external_textured_quad_renderer.h"

#include "chrome/browser/vr/vr_gl_util.h"
#include "ui/gfx/transform.h"

namespace vr {

namespace {

// clang-format off
static constexpr char const* kFragmentShader = OEIE_SHADER(
  precision highp float;
  uniform samplerExternalOES u_Texture;
  uniform samplerExternalOES u_OverlayTexture;
  uniform vec4 u_CopyRect;
  varying vec2 v_TexCoordinate;
  varying vec2 v_CornerPosition;
  uniform mediump float u_Opacity;
  uniform mediump float u_OverlayOpacity;
  void main() {
    vec2 scaledTex =
        vec2(u_CopyRect[0] + v_TexCoordinate.x * u_CopyRect[2],
        u_CopyRect[1] + v_TexCoordinate.y * u_CopyRect[3]);
    lowp vec4 color = texture2D(u_Texture, scaledTex);
    lowp vec4 overlay_color = texture2D(u_OverlayTexture, scaledTex);
    overlay_color = overlay_color * u_OverlayOpacity;
    color = mix(color, overlay_color, overlay_color.a);
    float mask = 1.0 - step(1.0, length(v_CornerPosition));
    gl_FragColor = color * u_Opacity * mask;
  }
);
// clang-format on

}  // namespace

ExternalTexturedQuadRenderer::ExternalTexturedQuadRenderer()
    : TexturedQuadRenderer(TexturedQuadRenderer::VertexShader(),
                           kFragmentShader) {}

ExternalTexturedQuadRenderer::~ExternalTexturedQuadRenderer() = default;

GLenum ExternalTexturedQuadRenderer::TextureType() const {
  return GL_TEXTURE_EXTERNAL_OES;
}

}  // namespace vr
