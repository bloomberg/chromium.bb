// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/surfaces/surface.h"

#include "cc/output/compositor_frame.h"
#include "cc/surfaces/surface_factory.h"

namespace cc {

Surface::Surface(SurfaceId id, const gfx::Size& size, SurfaceFactory* factory)
    : surface_id_(id), size_(size), factory_(factory) {
}

Surface::~Surface() {
  if (current_frame_) {
    ReturnedResourceArray current_resources;
    TransferableResource::ReturnResources(
        current_frame_->delegated_frame_data->resource_list,
        &current_resources);
    factory_->UnrefResources(current_resources);
  }
}

void Surface::QueueFrame(scoped_ptr<CompositorFrame> frame) {
  scoped_ptr<CompositorFrame> previous_frame = current_frame_.Pass();
  current_frame_ = frame.Pass();
  factory_->ReceiveFromChild(
      current_frame_->delegated_frame_data->resource_list);

  if (previous_frame) {
    ReturnedResourceArray previous_resources;
    TransferableResource::ReturnResources(
        previous_frame->delegated_frame_data->resource_list,
        &previous_resources);
    factory_->UnrefResources(previous_resources);
  }
}

const CompositorFrame* Surface::GetEligibleFrame() {
  return current_frame_.get();
}

}  // namespace cc
