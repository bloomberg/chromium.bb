// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/compositor_fake_web_graphics_context_3d.h"

namespace cc {

bool CompositorFakeWebGraphicsContext3D::makeContextCurrent() {
  return true;
}

WebKit::WebGLId CompositorFakeWebGraphicsContext3D::createProgram() {
  return 1;
}

WebKit::WebGLId CompositorFakeWebGraphicsContext3D::createShader(
    WebKit::WGC3Denum) {
  return 1;
}

void CompositorFakeWebGraphicsContext3D::getShaderiv(
    WebKit::WebGLId,
    WebKit::WGC3Denum,
    WebKit::WGC3Dint* value) {
  *value = 1;
}

void CompositorFakeWebGraphicsContext3D::getProgramiv(
    WebKit::WebGLId,
    WebKit::WGC3Denum,
    WebKit::WGC3Dint* value) {
  *value = 1;
}

}  // namespace cc
