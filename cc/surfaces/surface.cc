// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/surfaces/surface.h"

#include "cc/output/compositor_frame.h"
#include "cc/surfaces/surface_manager.h"

namespace cc {

Surface::Surface(SurfaceManager* manager,
                 SurfaceClient* client,
                 const gfx::Size& size)
    : manager_(manager),
      client_(client),
      size_(size) {
  surface_id_ = manager_->RegisterAndAllocateIDForSurface(this);
}

Surface::~Surface() {
  manager_->DeregisterSurface(surface_id_);
}

void Surface::QueueFrame(scoped_ptr<CompositorFrame> frame) {
  current_frame_ = frame.Pass();
}

CompositorFrame* Surface::GetEligibleFrame() { return current_frame_.get(); }

}  // namespace cc
