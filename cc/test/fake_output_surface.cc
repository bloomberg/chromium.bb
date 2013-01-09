// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_output_surface.h"

namespace cc {

FakeOutputSurface::FakeOutputSurface(
    scoped_ptr<WebKit::WebGraphicsContext3D> context3d, bool has_parent)
    : num_sent_frames_(0) {
  context3d_ = context3d.Pass();
  capabilities_.has_parent_compositor = has_parent;
}

FakeOutputSurface::FakeOutputSurface(
    scoped_ptr<SoftwareOutputDevice> software_device, bool has_parent)
    : num_sent_frames_(0) {
  software_device_ = software_device.Pass();
  capabilities_.has_parent_compositor = has_parent;
}

FakeOutputSurface::~FakeOutputSurface() {}

bool FakeOutputSurface::BindToClient(OutputSurfaceClient* client) {
  if (!context3d_)
    return true;
  DCHECK(client);
  if (!context3d_->makeContextCurrent())
    return false;
  client_ = client;
  return true;
}

const struct OutputSurface::Capabilities& FakeOutputSurface::Capabilities()
    const {
  return capabilities_;
}

WebKit::WebGraphicsContext3D* FakeOutputSurface::Context3D() const {
  return context3d_.get();
}

SoftwareOutputDevice* FakeOutputSurface::SoftwareDevice() const {
  return software_device_.get();
}

void FakeOutputSurface::SendFrameToParentCompositor(
    CompositorFrame* frame) {
  frame->AssignTo(&last_sent_frame_);
  ++num_sent_frames_;
}

}  // namespace cc
