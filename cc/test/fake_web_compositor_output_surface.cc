// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_web_compositor_output_surface.h"

namespace cc {

FakeWebCompositorOutputSurface::FakeWebCompositorOutputSurface(
    scoped_ptr<WebKit::WebGraphicsContext3D> context3D) {
  m_context3D = context3D.Pass();
}

FakeWebCompositorOutputSurface::FakeWebCompositorOutputSurface(
    scoped_ptr<WebKit::WebCompositorSoftwareOutputDevice> softwareDevice) {
  m_softwareDevice = softwareDevice.Pass();
}

FakeWebCompositorOutputSurface::~FakeWebCompositorOutputSurface() {
}

bool FakeWebCompositorOutputSurface::bindToClient(
    WebKit::WebCompositorOutputSurfaceClient* client) {
  if (!m_context3D)
    return true;
  DCHECK(client);
  if (!m_context3D->makeContextCurrent())
    return false;
  m_client = client;
  return true;
}

const FakeWebCompositorOutputSurface::Capabilities&
    FakeWebCompositorOutputSurface::capabilities() const {
  return m_capabilities;
}

WebKit::WebGraphicsContext3D*
    FakeWebCompositorOutputSurface::context3D() const {
  return m_context3D.get();
}

WebKit::WebCompositorSoftwareOutputDevice*
    FakeWebCompositorOutputSurface::softwareDevice() const {
  return m_softwareDevice.get();
}

}  // namespace cc
