// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/surfaces/surface.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>

#include "cc/base/container_util.h"
#include "cc/output/compositor_frame.h"
#include "cc/output/copy_output_request.h"
#include "cc/surfaces/surface_factory.h"
#include "cc/surfaces/surface_id_allocator.h"
#include "cc/surfaces/surface_manager.h"

namespace cc {

// The frame index starts at 2 so that empty frames will be treated as
// completely damaged the first time they're drawn from.
static const int kFrameIndexStart = 2;

Surface::Surface(const SurfaceId& id, SurfaceFactory* factory)
    : surface_id_(id),
      previous_frame_surface_id_(id),
      factory_(factory->AsWeakPtr()),
      frame_index_(kFrameIndexStart),
      destroyed_(false) {}

Surface::~Surface() {
  ClearCopyRequests();
  if (current_frame_ && factory_) {
    UnrefFrameResources(*current_frame_);
  }
  if (!draw_callback_.is_null())
    draw_callback_.Run();
}

void Surface::SetPreviousFrameSurface(Surface* surface) {
  DCHECK(surface);
  frame_index_ = surface->frame_index() + 1;
  previous_frame_surface_id_ = surface->surface_id();
}

void Surface::QueueFrame(CompositorFrame frame, const DrawCallback& callback) {
  DCHECK(factory_);
  ClearCopyRequests();

  TakeLatencyInfo(&frame.metadata.latency_info);

  base::Optional<CompositorFrame> previous_frame = std::move(current_frame_);
  current_frame_ = std::move(frame);

  if (current_frame_) {
    factory_->ReceiveFromChild(current_frame_->resource_list);
  }

  // Empty frames shouldn't be drawn and shouldn't contribute damage, so don't
  // increment frame index for them.
  if (current_frame_ && !current_frame_->render_pass_list.empty()) {
    ++frame_index_;
  }

  previous_frame_surface_id_ = surface_id();

  std::vector<SurfaceId> new_referenced_surfaces;
  new_referenced_surfaces = current_frame_->metadata.referenced_surfaces;

  if (previous_frame)
    UnrefFrameResources(*previous_frame);

  if (!draw_callback_.is_null())
    draw_callback_.Run();
  draw_callback_ = callback;

  bool referenced_surfaces_changed =
      (referenced_surfaces_ != new_referenced_surfaces);
  referenced_surfaces_ = new_referenced_surfaces;
  std::vector<uint32_t> satisfies_sequences =
      std::move(current_frame_->metadata.satisfies_sequences);

  if (referenced_surfaces_changed || !satisfies_sequences.empty()) {
    // Notify the manager that sequences were satisfied either if some new
    // sequences were satisfied, or if the set of referenced surfaces changed
    // to force a GC to happen.
    factory_->manager()->DidSatisfySequences(surface_id_.frame_sink_id(),
                                             &satisfies_sequences);
  }
}

void Surface::RequestCopyOfOutput(
    std::unique_ptr<CopyOutputRequest> copy_request) {
  if (current_frame_ && !current_frame_->render_pass_list.empty()) {
    std::vector<std::unique_ptr<CopyOutputRequest>>& copy_requests =
        current_frame_->render_pass_list.back()->copy_requests;

    if (void* source = copy_request->source()) {
      // Remove existing CopyOutputRequests made on the Surface by the same
      // source.
      auto to_remove =
          std::remove_if(copy_requests.begin(), copy_requests.end(),
                         [source](const std::unique_ptr<CopyOutputRequest>& x) {
                           return x->source() == source;
                         });
      copy_requests.erase(to_remove, copy_requests.end());
    }
    copy_requests.push_back(std::move(copy_request));
  } else {
    copy_request->SendEmptyResult();
  }
}

void Surface::TakeCopyOutputRequests(
    std::multimap<RenderPassId, std::unique_ptr<CopyOutputRequest>>*
        copy_requests) {
  DCHECK(copy_requests->empty());
  if (current_frame_) {
    for (const auto& render_pass : current_frame_->render_pass_list) {
      for (auto& request : render_pass->copy_requests) {
        copy_requests->insert(
            std::make_pair(render_pass->id, std::move(request)));
      }
      render_pass->copy_requests.clear();
    }
  }
}

const CompositorFrame& Surface::GetEligibleFrame() {
  DCHECK(current_frame_);
  return current_frame_.value();
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
    DrawCallback callback = draw_callback_;
    draw_callback_ = DrawCallback();
    callback.Run();
  }
}

void Surface::AddDestructionDependency(SurfaceSequence sequence) {
  destruction_dependencies_.push_back(sequence);
}

void Surface::SatisfyDestructionDependencies(
    std::unordered_set<SurfaceSequence, SurfaceSequenceHash>* sequences,
    std::unordered_set<FrameSinkId, FrameSinkIdHash>* valid_frame_sink_ids) {
  destruction_dependencies_.erase(
      std::remove_if(destruction_dependencies_.begin(),
                     destruction_dependencies_.end(),
                     [sequences, valid_frame_sink_ids](SurfaceSequence seq) {
                       return (!!sequences->erase(seq) ||
                               !valid_frame_sink_ids->count(seq.frame_sink_id));
                     }),
      destruction_dependencies_.end());
}

void Surface::UnrefFrameResources(const CompositorFrame& frame) {
  ReturnedResourceArray resources;
  TransferableResource::ReturnResources(frame.resource_list, &resources);
  // No point in returning same sync token to sender.
  for (auto& resource : resources)
    resource.sync_token.Clear();
  factory_->UnrefResources(resources);
}

void Surface::ClearCopyRequests() {
  if (current_frame_) {
    for (const auto& render_pass : current_frame_->render_pass_list) {
      for (const auto& copy_request : render_pass->copy_requests)
        copy_request->SendEmptyResult();
    }
  }
}

}  // namespace cc
