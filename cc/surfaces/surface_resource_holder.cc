// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/surfaces/surface_resource_holder.h"

#include "cc/surfaces/surface_resource_holder_client.h"
namespace cc {

SurfaceResourceHolder::SurfaceResourceHolder(
    SurfaceResourceHolderClient* client)
    : client_(client) {}

SurfaceResourceHolder::~SurfaceResourceHolder() = default;

SurfaceResourceHolder::ResourceRefs::ResourceRefs()
    : refs_received_from_child(0), refs_holding_resource_alive(0) {}

void SurfaceResourceHolder::Reset() {
  resource_id_info_map_.clear();
}

void SurfaceResourceHolder::ReceiveFromChild(
    const TransferableResourceArray& resources) {
  for (const auto& resource : resources) {
    ResourceRefs& ref = resource_id_info_map_[resource.id];
    ref.refs_holding_resource_alive++;
    ref.refs_received_from_child++;
  }
}

void SurfaceResourceHolder::RefResources(
    const TransferableResourceArray& resources) {
  for (TransferableResourceArray::const_iterator it = resources.begin();
       it != resources.end();
       ++it) {
    ResourceIdInfoMap::iterator count_it = resource_id_info_map_.find(it->id);
    DCHECK(count_it != resource_id_info_map_.end());
    count_it->second.refs_holding_resource_alive++;
  }
}

void SurfaceResourceHolder::UnrefResources(
    const ReturnedResourceArray& resources) {
  ReturnedResourceArray resources_available_to_return;

  for (ReturnedResourceArray::const_iterator it = resources.begin();
       it != resources.end();
       ++it) {
    unsigned id = it->id;
    ResourceIdInfoMap::iterator count_it = resource_id_info_map_.find(id);
    if (count_it == resource_id_info_map_.end())
      continue;
    ResourceRefs& ref = count_it->second;
    ref.refs_holding_resource_alive -= it->count;
    // Keep the newest return sync token that has data.
    // TODO(jbauman): Handle the case with two valid sync tokens.
    if (it->sync_token.HasData())
      ref.sync_token = it->sync_token;
    if (ref.refs_holding_resource_alive == 0) {
      ReturnedResource returned = *it;
      returned.sync_token = ref.sync_token;
      returned.count = ref.refs_received_from_child;
      resources_available_to_return.push_back(returned);
      resource_id_info_map_.erase(count_it);
    }
  }

  client_->ReturnResources(resources_available_to_return);
}

}  // namespace cc
