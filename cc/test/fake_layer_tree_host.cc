// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_layer_tree_host.h"

namespace cc {
FakeLayerTreeHost::FakeLayerTreeHost(FakeLayerTreeHostClient* client,
                                     const LayerTreeSettings& settings)
    : LayerTreeHost(client, NULL, settings),
      client_(client),
      host_impl_(settings, &proxy_, &manager_),
      needs_commit_(false) {
  client_->SetLayerTreeHost(this);
}

scoped_ptr<FakeLayerTreeHost> FakeLayerTreeHost::Create(
    FakeLayerTreeHostClient* client) {
  LayerTreeSettings settings;
  return make_scoped_ptr(new FakeLayerTreeHost(client, settings));
}

scoped_ptr<FakeLayerTreeHost> FakeLayerTreeHost::Create(
    FakeLayerTreeHostClient* client,
    const LayerTreeSettings& settings) {
  return make_scoped_ptr(new FakeLayerTreeHost(client, settings));
}

FakeLayerTreeHost::~FakeLayerTreeHost() {
  client_->SetLayerTreeHost(NULL);
}

void FakeLayerTreeHost::SetNeedsCommit() { needs_commit_ = true; }

LayerImpl* FakeLayerTreeHost::CommitAndCreateLayerImplTree() {
  scoped_ptr<LayerImpl> old_root_layer_impl = active_tree()->DetachLayerTree();

  scoped_ptr<LayerImpl> layer_impl = TreeSynchronizer::SynchronizeTrees(
      root_layer(), old_root_layer_impl.Pass(), active_tree());
  TreeSynchronizer::PushProperties(root_layer(), layer_impl.get());

  active_tree()->SetRootLayer(layer_impl.Pass());
  return active_tree()->root_layer();
}

}  // namespace cc
