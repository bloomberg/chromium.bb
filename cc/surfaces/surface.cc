// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/surfaces/surface.h"

#include "cc/output/compositor_frame.h"
#include "cc/output/copy_output_request.h"
#include "cc/surfaces/surface_factory.h"

namespace cc {

// The frame index starts at 2 so that empty frames will be treated as
// completely damaged the first time they're drawn from.
static const int kFrameIndexStart = 2;

Surface::Surface(SurfaceId id, const gfx::Size& size, SurfaceFactory* factory)
    : surface_id_(id),
      size_(size),
      factory_(factory),
      frame_index_(kFrameIndexStart) {
}

Surface::~Surface() {
  for (ScopedPtrVector<CopyOutputRequest>::iterator it = copy_requests_.begin();
       it != copy_requests_.end();
       ++it) {
    (*it)->SendEmptyResult();
  }
  copy_requests_.clear();
  if (current_frame_) {
    ReturnedResourceArray current_resources;
    TransferableResource::ReturnResources(
        current_frame_->delegated_frame_data->resource_list,
        &current_resources);
    factory_->UnrefResources(current_resources);
  }
}

void Surface::QueueFrame(scoped_ptr<CompositorFrame> frame,
                         const base::Closure& callback) {
  for (ScopedPtrVector<CopyOutputRequest>::iterator it = copy_requests_.begin();
       it != copy_requests_.end();
       ++it) {
    (*it)->SendEmptyResult();
  }
  copy_requests_.clear();

  TakeLatencyInfo(&frame->metadata.latency_info);
  scoped_ptr<CompositorFrame> previous_frame = current_frame_.Pass();
  current_frame_ = frame.Pass();
  factory_->ReceiveFromChild(
      current_frame_->delegated_frame_data->resource_list);
  ++frame_index_;

  if (previous_frame) {
    ReturnedResourceArray previous_resources;
    TransferableResource::ReturnResources(
        previous_frame->delegated_frame_data->resource_list,
        &previous_resources);
    factory_->UnrefResources(previous_resources);
  }
  if (!draw_callback_.is_null())
    draw_callback_.Run();
  draw_callback_ = callback;
}

void Surface::RequestCopyOfOutput(scoped_ptr<CopyOutputRequest> copy_request) {
  copy_requests_.push_back(copy_request.Pass());
}

void Surface::TakeCopyOutputRequests(
    ScopedPtrVector<CopyOutputRequest>* copy_requests) {
  DCHECK(copy_requests->empty());
  copy_requests->swap(copy_requests_);
}

const CompositorFrame* Surface::GetEligibleFrame() {
  return current_frame_.get();
}

void Surface::TakeLatencyInfo(std::vector<ui::LatencyInfo>* latency_info) {
  if (!current_frame_)
    return;
  if (latency_info->empty()) {
    current_frame_->metadata.latency_info.swap(*latency_info);
    return;
  }
  std::copy(current_frame_->metadata.latency_info.begin(),
            current_frame_->metadata.latency_info.end(),
            std::back_inserter(*latency_info));
  current_frame_->metadata.latency_info.clear();
}

void Surface::RunDrawCallbacks() {
  if (!draw_callback_.is_null()) {
    base::Closure callback = draw_callback_;
    draw_callback_ = base::Closure();
    callback.Run();
  }
}

}  // namespace cc
