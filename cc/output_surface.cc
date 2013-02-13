// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/output_surface.h"

#include "base/logging.h"

namespace cc {

OutputSurface::OutputSurface(
    scoped_ptr<WebKit::WebGraphicsContext3D> context3d)
    : client_(NULL),
      context3d_(context3d.Pass()) {
}

OutputSurface::OutputSurface(
    scoped_ptr<cc::SoftwareOutputDevice> software_device)
    : client_(NULL),
      software_device_(software_device.Pass()) {
}

OutputSurface::OutputSurface(
    scoped_ptr<WebKit::WebGraphicsContext3D> context3d,
    scoped_ptr<cc::SoftwareOutputDevice> software_device)
    : client_(NULL),
      context3d_(context3d.Pass()),
      software_device_(software_device.Pass()) {
}

OutputSurface::~OutputSurface() {
}

bool OutputSurface::BindToClient(
    cc::OutputSurfaceClient* client) {
  DCHECK(client);
  client_ = client;
  if (!context3d_)
    return true;
  return context3d_->makeContextCurrent();
}

void OutputSurface::SendFrameToParentCompositor(CompositorFrame*) {
  NOTIMPLEMENTED();
}

}  // namespace cc
