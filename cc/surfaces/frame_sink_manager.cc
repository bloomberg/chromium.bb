// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/surfaces/frame_sink_manager.h"

#include <stddef.h>
#include <stdint.h>

#include "base/logging.h"
#include "cc/surfaces/frame_sink_manager_client.h"
#include "cc/surfaces/primary_begin_frame_source.h"

#if DCHECK_IS_ON()
#include <sstream>
#endif

namespace cc {

FrameSinkManager::FrameSinkSourceMapping::FrameSinkSourceMapping() = default;

FrameSinkManager::FrameSinkSourceMapping::FrameSinkSourceMapping(
    const FrameSinkSourceMapping& other) = default;

FrameSinkManager::FrameSinkSourceMapping::~FrameSinkSourceMapping() = default;

FrameSinkManager::FrameSinkManager() = default;

FrameSinkManager::~FrameSinkManager() {
  // All FrameSinks should be unregistered prior to SurfaceManager destruction.
  DCHECK_EQ(clients_.size(), 0u);
  DCHECK_EQ(registered_sources_.size(), 0u);
}

void FrameSinkManager::RegisterFrameSinkId(const FrameSinkId& frame_sink_id) {
  bool inserted = valid_frame_sink_ids_.insert(frame_sink_id).second;
  DCHECK(inserted);
}

void FrameSinkManager::InvalidateFrameSinkId(const FrameSinkId& frame_sink_id) {
  valid_frame_sink_ids_.erase(frame_sink_id);
}

void FrameSinkManager::RegisterFrameSinkManagerClient(
    const FrameSinkId& frame_sink_id,
    FrameSinkManagerClient* client) {
  DCHECK(client);
  DCHECK_EQ(valid_frame_sink_ids_.count(frame_sink_id), 1u);

  clients_[frame_sink_id] = client;

  auto it = frame_sink_source_map_.find(frame_sink_id);
  if (it != frame_sink_source_map_.end()) {
    if (it->second.source)
      client->SetBeginFrameSource(it->second.source);
  }
}

void FrameSinkManager::UnregisterFrameSinkManagerClient(
    const FrameSinkId& frame_sink_id) {
  DCHECK_EQ(valid_frame_sink_ids_.count(frame_sink_id), 1u);
  auto client_iter = clients_.find(frame_sink_id);
  DCHECK(client_iter != clients_.end());

  auto source_iter = frame_sink_source_map_.find(frame_sink_id);
  if (source_iter != frame_sink_source_map_.end()) {
    if (source_iter->second.source)
      client_iter->second->SetBeginFrameSource(nullptr);
    if (!source_iter->second.has_children())
      frame_sink_source_map_.erase(source_iter);
  }
  clients_.erase(client_iter);
}

void FrameSinkManager::RegisterBeginFrameSource(
    BeginFrameSource* source,
    const FrameSinkId& frame_sink_id) {
  DCHECK(source);
  DCHECK_EQ(registered_sources_.count(source), 0u);
  DCHECK_EQ(valid_frame_sink_ids_.count(frame_sink_id), 1u);

  registered_sources_[source] = frame_sink_id;
  RecursivelyAttachBeginFrameSource(frame_sink_id, source);

  primary_source_.OnBeginFrameSourceAdded(source);
}

void FrameSinkManager::UnregisterBeginFrameSource(BeginFrameSource* source) {
  DCHECK(source);
  DCHECK_EQ(registered_sources_.count(source), 1u);

  FrameSinkId frame_sink_id = registered_sources_[source];
  registered_sources_.erase(source);

  primary_source_.OnBeginFrameSourceRemoved(source);

  if (frame_sink_source_map_.count(frame_sink_id) == 0u)
    return;

  // TODO(enne): these walks could be done in one step.
  // Remove this begin frame source from its subtree.
  RecursivelyDetachBeginFrameSource(frame_sink_id, source);
  // Then flush every remaining registered source to fix any sources that
  // became null because of the previous step but that have an alternative.
  for (auto source_iter : registered_sources_)
    RecursivelyAttachBeginFrameSource(source_iter.second, source_iter.first);
}

BeginFrameSource* FrameSinkManager::GetPrimaryBeginFrameSource() {
  return &primary_source_;
}

void FrameSinkManager::RecursivelyAttachBeginFrameSource(
    const FrameSinkId& frame_sink_id,
    BeginFrameSource* source) {
  FrameSinkSourceMapping& mapping = frame_sink_source_map_[frame_sink_id];
  if (!mapping.source) {
    mapping.source = source;
    auto client_iter = clients_.find(frame_sink_id);
    if (client_iter != clients_.end())
      client_iter->second->SetBeginFrameSource(source);
  }
  for (size_t i = 0; i < mapping.children.size(); ++i)
    RecursivelyAttachBeginFrameSource(mapping.children[i], source);
}

void FrameSinkManager::RecursivelyDetachBeginFrameSource(
    const FrameSinkId& frame_sink_id,
    BeginFrameSource* source) {
  auto iter = frame_sink_source_map_.find(frame_sink_id);
  if (iter == frame_sink_source_map_.end())
    return;
  if (iter->second.source == source) {
    iter->second.source = nullptr;
    auto client_iter = clients_.find(frame_sink_id);
    if (client_iter != clients_.end())
      client_iter->second->SetBeginFrameSource(nullptr);
  }

  if (!iter->second.has_children() && !clients_.count(frame_sink_id)) {
    frame_sink_source_map_.erase(iter);
    return;
  }

  std::vector<FrameSinkId>& children = iter->second.children;
  for (size_t i = 0; i < children.size(); ++i) {
    RecursivelyDetachBeginFrameSource(children[i], source);
  }
}

bool FrameSinkManager::ChildContains(
    const FrameSinkId& child_frame_sink_id,
    const FrameSinkId& search_frame_sink_id) const {
  auto iter = frame_sink_source_map_.find(child_frame_sink_id);
  if (iter == frame_sink_source_map_.end())
    return false;

  const std::vector<FrameSinkId>& children = iter->second.children;
  for (size_t i = 0; i < children.size(); ++i) {
    if (children[i] == search_frame_sink_id)
      return true;
    if (ChildContains(children[i], search_frame_sink_id))
      return true;
  }
  return false;
}

void FrameSinkManager::RegisterFrameSinkHierarchy(
    const FrameSinkId& parent_frame_sink_id,
    const FrameSinkId& child_frame_sink_id) {
  // If it's possible to reach the parent through the child's descendant chain,
  // then this will create an infinite loop.  Might as well just crash here.
  CHECK(!ChildContains(child_frame_sink_id, parent_frame_sink_id));

  std::vector<FrameSinkId>& children =
      frame_sink_source_map_[parent_frame_sink_id].children;
  for (size_t i = 0; i < children.size(); ++i)
    DCHECK(children[i] != child_frame_sink_id);
  children.push_back(child_frame_sink_id);

  // If the parent has no source, then attaching it to this child will
  // not change any downstream sources.
  BeginFrameSource* parent_source =
      frame_sink_source_map_[parent_frame_sink_id].source;
  if (!parent_source)
    return;

  DCHECK_EQ(registered_sources_.count(parent_source), 1u);
  RecursivelyAttachBeginFrameSource(child_frame_sink_id, parent_source);
}

void FrameSinkManager::UnregisterFrameSinkHierarchy(
    const FrameSinkId& parent_frame_sink_id,
    const FrameSinkId& child_frame_sink_id) {
  // Deliberately do not check validity of either parent or child FrameSinkId
  // here.  They were valid during the registration, so were valid at some
  // point in time.  This makes it possible to invalidate parent and child
  // FrameSinkIds independently of each other and not have an ordering
  // dependency  of unregistering the hierarchy first before either of them.
  DCHECK_EQ(frame_sink_source_map_.count(parent_frame_sink_id), 1u);

  auto iter = frame_sink_source_map_.find(parent_frame_sink_id);

  std::vector<FrameSinkId>& children = iter->second.children;
  bool found_child = false;
  for (size_t i = 0; i < children.size(); ++i) {
    if (children[i] == child_frame_sink_id) {
      found_child = true;
      children[i] = children.back();
      children.resize(children.size() - 1);
      break;
    }
  }
  DCHECK(found_child);

  // The CompositorFrameSinkSupport and hierarchy can be registered/unregistered
  // in either order, so empty frame_sink_source_map entries need to be
  // checked when removing either clients or relationships.
  if (!iter->second.has_children() && !clients_.count(parent_frame_sink_id) &&
      !iter->second.source) {
    frame_sink_source_map_.erase(iter);
    return;
  }

  // If the parent does not have a begin frame source, then disconnecting it
  // will not change any of its children.
  BeginFrameSource* parent_source = iter->second.source;
  if (!parent_source)
    return;

  // TODO(enne): these walks could be done in one step.
  RecursivelyDetachBeginFrameSource(child_frame_sink_id, parent_source);
  for (auto source_iter : registered_sources_)
    RecursivelyAttachBeginFrameSource(source_iter.second, source_iter.first);
}

}  // namespace cc
