// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_FAKE_LAYER_TREE_HOST_H_
#define CC_TEST_FAKE_LAYER_TREE_HOST_H_

#include "cc/test/fake_impl_proxy.h"
#include "cc/test/fake_layer_tree_host_client.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/tree_synchronizer.h"

namespace cc {

class FakeLayerTreeHost : protected LayerTreeHost {
 public:
  static scoped_ptr<FakeLayerTreeHost> Create() {
    static FakeLayerTreeHostClient client(FakeLayerTreeHostClient::DIRECT_3D);
    static LayerTreeSettings settings;
    return make_scoped_ptr(new FakeLayerTreeHost(&client, settings));
  }

  static scoped_ptr<FakeLayerTreeHost> Create(
      const LayerTreeSettings& settings) {
    static FakeLayerTreeHostClient client(FakeLayerTreeHostClient::DIRECT_3D);
    return make_scoped_ptr(new FakeLayerTreeHost(&client, settings));
  }

  virtual ~FakeLayerTreeHost() {}

  virtual void SetNeedsCommit() OVERRIDE {}
  virtual void SetNeedsFullTreeSync() OVERRIDE {}

  using LayerTreeHost::SetRootLayer;
  using LayerTreeHost::root_layer;

  LayerImpl* CommitAndCreateLayerImplTree() {
    scoped_ptr<LayerImpl> old_root_layer_impl =
        active_tree()->DetachLayerTree();

    scoped_ptr<LayerImpl> layer_impl =
        TreeSynchronizer::SynchronizeTrees(
            root_layer(),
            old_root_layer_impl.Pass(),
            active_tree());
    TreeSynchronizer::PushProperties(root_layer(), layer_impl.get());

    active_tree()->SetRootLayer(layer_impl.Pass());
    return active_tree()->root_layer();
  }

  FakeLayerTreeHostImpl* host_impl() { return &host_impl_; }
  LayerTreeImpl* active_tree() { return host_impl_.active_tree(); }

 private:
  FakeLayerTreeHost(LayerTreeHostClient* client,
                    const LayerTreeSettings& settings)
      : LayerTreeHost(client, settings),
        host_impl_(settings, &proxy_) {}

  FakeImplProxy proxy_;
  FakeLayerTreeHostImpl host_impl_;
};

}  // namespace cc

#endif  // CC_TEST_FAKE_LAYER_TREE_HOST_H_
