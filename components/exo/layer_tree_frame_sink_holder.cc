// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/layer_tree_frame_sink_holder.h"

#include "cc/output/layer_tree_frame_sink.h"
#include "cc/resources/returned_resource.h"
#include "components/exo/surface_tree_host.h"

namespace exo {

////////////////////////////////////////////////////////////////////////////////
// LayerTreeFrameSinkHolder, public:

LayerTreeFrameSinkHolder::LayerTreeFrameSinkHolder(
    SurfaceTreeHost* surface_tree_host,
    std::unique_ptr<cc::LayerTreeFrameSink> frame_sink)
    : surface_tree_host_(surface_tree_host),
      frame_sink_(std::move(frame_sink)),
      weak_factory_(this) {
  frame_sink_->BindToClient(this);
}

LayerTreeFrameSinkHolder::~LayerTreeFrameSinkHolder() {
  frame_sink_->DetachFromClient();

  // Release all resources which aren't returned from LayerTreeFrameSink.
  for (auto& callback : release_callbacks_)
    callback.second.Run(gpu::SyncToken(), false);
}

bool LayerTreeFrameSinkHolder::HasReleaseCallbackForResource(
    cc::ResourceId id) {
  return release_callbacks_.find(id) != release_callbacks_.end();
}

void LayerTreeFrameSinkHolder::SetResourceReleaseCallback(
    cc::ResourceId id,
    const cc::ReleaseCallback& callback) {
  DCHECK(!callback.is_null());
  release_callbacks_[id] = callback;
}

int LayerTreeFrameSinkHolder::AllocateResourceId() {
  return next_resource_id_++;
}

base::WeakPtr<LayerTreeFrameSinkHolder> LayerTreeFrameSinkHolder::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

////////////////////////////////////////////////////////////////////////////////
// cc::LayerTreeFrameSinkClient overrides:

void LayerTreeFrameSinkHolder::SetBeginFrameSource(
    cc::BeginFrameSource* source) {
  surface_tree_host_->SetBeginFrameSource(source);
}

void LayerTreeFrameSinkHolder::ReclaimResources(
    const std::vector<cc::ReturnedResource>& resources) {
  for (auto& resource : resources) {
    auto it = release_callbacks_.find(resource.id);
    DCHECK(it != release_callbacks_.end());
    if (it != release_callbacks_.end()) {
      it->second.Run(resource.sync_token, resource.lost);
      release_callbacks_.erase(it);
    }
  }
}

void LayerTreeFrameSinkHolder::DidReceiveCompositorFrameAck() {
  surface_tree_host_->DidReceiveCompositorFrameAck();
}

}  // namespace exo
