// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_output_surface.h"

#include "cc/output/output_surface_client.h"

namespace cc {

FakeOutputSurface::FakeOutputSurface(
    scoped_ptr<WebKit::WebGraphicsContext3D> context3d, bool has_parent)
    : OutputSurface(context3d.Pass()),
      num_sent_frames_(0),
      vsync_notification_enabled_(false) {
  capabilities_.has_parent_compositor = has_parent;
}

FakeOutputSurface::FakeOutputSurface(
    scoped_ptr<SoftwareOutputDevice> software_device, bool has_parent)
    : OutputSurface(software_device.Pass()),
      num_sent_frames_(0) {
  capabilities_.has_parent_compositor = has_parent;
}

FakeOutputSurface::~FakeOutputSurface() {}

bool FakeOutputSurface::BindToClient(
    cc::OutputSurfaceClient* client) {
  DCHECK(client);
  client_ = client;
  if (!context3d_)
    return true;
  return context3d_->makeContextCurrent();
}

void FakeOutputSurface::SendFrameToParentCompositor(
    CompositorFrame* frame) {
  frame->AssignTo(&last_sent_frame_);
  ++num_sent_frames_;
}

void FakeOutputSurface::EnableVSyncNotification(bool enable) {
  vsync_notification_enabled_ = enable;
}

void FakeOutputSurface::DidVSync(base::TimeTicks frame_time) {
  client_->DidVSync(frame_time);
}

}  // namespace cc
