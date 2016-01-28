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

Surface::Surface(SurfaceId id, SurfaceFactory* factory)
    : surface_id_(id),
      factory_(factory->AsWeakPtr()),
      frame_index_(kFrameIndexStart),
      destroyed_(false) {}

Surface::~Surface() {
  ClearCopyRequests();
  if (current_frame_ && factory_) {
    ReturnedResourceArray current_resources;
    TransferableResource::ReturnResources(
        current_frame_->delegated_frame_data->resource_list,
        &current_resources);
    factory_->UnrefResources(current_resources);
  }
  if (!draw_callback_.is_null())
    draw_callback_.Run(SurfaceDrawStatus::DRAW_SKIPPED);

  if (factory_)
    factory_->SetBeginFrameSource(surface_id_, NULL);
}

void Surface::QueueFrame(scoped_ptr<CompositorFrame> frame,
                         const DrawCallback& callback) {
  DCHECK(factory_);
  ClearCopyRequests();

  if (frame) {
    TakeLatencyInfo(&frame->metadata.latency_info);
  }

  scoped_ptr<CompositorFrame> previous_frame = std::move(current_frame_);
  current_frame_ = std::move(frame);

  if (current_frame_) {
    factory_->ReceiveFromChild(
        current_frame_->delegated_frame_data->resource_list);
  }

  // Empty frames shouldn't be drawn and shouldn't contribute damage, so don't
  // increment frame index for them.
  if (current_frame_ &&
      !current_frame_->delegated_frame_data->render_pass_list.empty())
    ++frame_index_;

  std::vector<SurfaceId> new_referenced_surfaces;
  if (current_frame_) {
    new_referenced_surfaces = current_frame_->metadata.referenced_surfaces;
  }

  if (previous_frame) {
    ReturnedResourceArray previous_resources;
    TransferableResource::ReturnResources(
        previous_frame->delegated_frame_data->resource_list,
        &previous_resources);
    factory_->UnrefResources(previous_resources);
  }
  if (!draw_callback_.is_null())
    draw_callback_.Run(SurfaceDrawStatus::DRAW_SKIPPED);
  draw_callback_ = callback;

  bool referenced_surfaces_changed =
      (referenced_surfaces_ != new_referenced_surfaces);
  referenced_surfaces_ = new_referenced_surfaces;
  std::vector<uint32_t> satisfies_sequences;
  if (current_frame_)
    current_frame_->metadata.satisfies_sequences.swap(satisfies_sequences);
  if (referenced_surfaces_changed || !satisfies_sequences.empty()) {
    // Notify the manager that sequences were satisfied either if some new
    // sequences were satisfied, or if the set of referenced surfaces changed
    // to force a GC to happen.
    factory_->manager()->DidSatisfySequences(
        SurfaceIdAllocator::NamespaceForId(surface_id_), &satisfies_sequences);
  }
}

void Surface::RequestCopyOfOutput(scoped_ptr<CopyOutputRequest> copy_request) {
  if (current_frame_ &&
      !current_frame_->delegated_frame_data->render_pass_list.empty()) {
    std::vector<scoped_ptr<CopyOutputRequest>>& copy_requests =
        current_frame_->delegated_frame_data->render_pass_list.back()
            ->copy_requests;

    if (void* source = copy_request->source()) {
      // Remove existing CopyOutputRequests made on the Surface by the same
      // source.
      auto to_remove =
          std::remove_if(copy_requests.begin(), copy_requests.end(),
                         [source](const scoped_ptr<CopyOutputRequest>& x) {
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
    std::multimap<RenderPassId, scoped_ptr<CopyOutputRequest>>* copy_requests) {
  DCHECK(copy_requests->empty());
  if (current_frame_) {
    for (const auto& render_pass :
         current_frame_->delegated_frame_data->render_pass_list) {
      for (auto& request : render_pass->copy_requests) {
        copy_requests->insert(
            std::make_pair(render_pass->id, std::move(request)));
      }
      render_pass->copy_requests.clear();
    }
  }
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

void Surface::RunDrawCallbacks(SurfaceDrawStatus drawn) {
  if (!draw_callback_.is_null()) {
    DrawCallback callback = draw_callback_;
    draw_callback_ = DrawCallback();
    callback.Run(drawn);
  }
}

void Surface::AddDestructionDependency(SurfaceSequence sequence) {
  destruction_dependencies_.push_back(sequence);
}

void Surface::SatisfyDestructionDependencies(
    std::unordered_set<SurfaceSequence, SurfaceSequenceHash>* sequences,
    std::unordered_set<uint32_t>* valid_id_namespaces) {
  destruction_dependencies_.erase(
      std::remove_if(destruction_dependencies_.begin(),
                     destruction_dependencies_.end(),
                     [sequences, valid_id_namespaces](SurfaceSequence seq) {
                       return (!!sequences->erase(seq) ||
                               !valid_id_namespaces->count(seq.id_namespace));
                     }),
      destruction_dependencies_.end());
}

void Surface::AddBeginFrameSource(BeginFrameSource* begin_frame_source) {
  DCHECK(base::STLIsSorted(begin_frame_sources_));
  DCHECK(!ContainsValue(begin_frame_sources_, begin_frame_source))
      << begin_frame_source;
  begin_frame_sources_.insert(begin_frame_source);
  UpdatePrimaryBeginFrameSource();
}

void Surface::RemoveBeginFrameSource(BeginFrameSource* begin_frame_source) {
  size_t erase_count = begin_frame_sources_.erase(begin_frame_source);
  DCHECK_EQ(1u, erase_count);
  UpdatePrimaryBeginFrameSource();
}

void Surface::UpdatePrimaryBeginFrameSource() {
  // Ensure the BeginFrameSources are sorted so our we make a stable decision
  // regarding which source is primary.
  // TODO(brianderson): Do something smarter based on coverage instead.
  DCHECK(base::STLIsSorted(begin_frame_sources_));

  BeginFrameSource* primary_source = nullptr;
  if (!begin_frame_sources_.empty())
    primary_source = *begin_frame_sources_.begin();

  if (factory_)
    factory_->SetBeginFrameSource(surface_id_, primary_source);
}

void Surface::ClearCopyRequests() {
  if (current_frame_) {
    for (const auto& render_pass :
         current_frame_->delegated_frame_data->render_pass_list) {
      for (const auto& copy_request : render_pass->copy_requests)
        copy_request->SendEmptyResult();
    }
  }
}

}  // namespace cc
