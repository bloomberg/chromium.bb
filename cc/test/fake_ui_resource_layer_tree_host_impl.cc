// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_ui_resource_layer_tree_host_impl.h"

#include "cc/test/fake_layer_tree_host_impl.h"

namespace cc {

FakeUIResourceLayerTreeHostImpl::FakeUIResourceLayerTreeHostImpl(Proxy* proxy)
    : FakeLayerTreeHostImpl(proxy), fake_next_resource_id_(1) {}

FakeUIResourceLayerTreeHostImpl::~FakeUIResourceLayerTreeHostImpl() {}

void FakeUIResourceLayerTreeHostImpl::CreateUIResource(
    UIResourceId uid,
    const UIResourceBitmap& bitmap) {
  if (ResourceIdForUIResource(uid))
    DeleteUIResource(uid);
  fake_ui_resource_map_[uid] = fake_next_resource_id_;
}

void FakeUIResourceLayerTreeHostImpl::DeleteUIResource(UIResourceId uid) {
  ResourceProvider::ResourceId id = ResourceIdForUIResource(uid);
  if (id)
    fake_ui_resource_map_.erase(uid);
}

ResourceProvider::ResourceId
    FakeUIResourceLayerTreeHostImpl::ResourceIdForUIResource(
        UIResourceId uid) const {
  UIResourceMap::const_iterator iter = fake_ui_resource_map_.find(uid);
  if (iter != fake_ui_resource_map_.end())
    return iter->second;
  return 0;
}

}  // namespace cc
