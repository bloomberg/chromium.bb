// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_FAKE_LAYER_TREE_HOST_H_
#define CC_TEST_FAKE_LAYER_TREE_HOST_H_

#include "cc/debug/micro_benchmark_controller.h"
#include "cc/test/fake_impl_proxy.h"
#include "cc/test/fake_layer_tree_host_client.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "cc/test/test_shared_bitmap_manager.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/tree_synchronizer.h"

namespace cc {

class FakeLayerTreeHost : public LayerTreeHost {
 public:
  static scoped_ptr<FakeLayerTreeHost> Create(FakeLayerTreeHostClient* client);
  static scoped_ptr<FakeLayerTreeHost> Create(
      FakeLayerTreeHostClient* client,
      const LayerTreeSettings& settings);

  virtual ~FakeLayerTreeHost();

  virtual void SetNeedsCommit() OVERRIDE;
  virtual void SetNeedsFullTreeSync() OVERRIDE {}

  using LayerTreeHost::SetRootLayer;
  using LayerTreeHost::root_layer;

  LayerImpl* CommitAndCreateLayerImplTree();

  FakeLayerTreeHostImpl* host_impl() { return &host_impl_; }
  LayerTreeImpl* active_tree() { return host_impl_.active_tree(); }

  using LayerTreeHost::ScheduleMicroBenchmark;
  using LayerTreeHost::SendMessageToMicroBenchmark;
  using LayerTreeHost::SetOutputSurfaceLostForTesting;
  using LayerTreeHost::InitializeSingleThreaded;
  using LayerTreeHost::InitializeForTesting;
  void UpdateLayers(ResourceUpdateQueue* queue) {
    LayerTreeHost::UpdateLayers(queue);
  }

  MicroBenchmarkController* GetMicroBenchmarkController() {
    return &micro_benchmark_controller_;
  }

  bool needs_commit() { return needs_commit_; }

 private:
  FakeLayerTreeHost(FakeLayerTreeHostClient* client,
                    const LayerTreeSettings& settings);

  FakeImplProxy proxy_;
  FakeLayerTreeHostClient* client_;
  TestSharedBitmapManager manager_;
  FakeLayerTreeHostImpl host_impl_;
  bool needs_commit_;
};

}  // namespace cc

#endif  // CC_TEST_FAKE_LAYER_TREE_HOST_H_
