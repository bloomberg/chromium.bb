// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_FAKE_LAYER_TREE_HOST_H_
#define CC_TEST_FAKE_LAYER_TREE_HOST_H_

#include "cc/debug/micro_benchmark_controller.h"
#include "cc/test/fake_impl_task_runner_provider.h"
#include "cc/test/fake_layer_tree_host_client.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "cc/test/test_shared_bitmap_manager.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/tree_synchronizer.h"

namespace cc {
class ImageSerializationProcessor;
class TestTaskGraphRunner;

class FakeLayerTreeHost : public LayerTreeHost {
 public:
  static scoped_ptr<FakeLayerTreeHost> Create(
      FakeLayerTreeHostClient* client,
      TestTaskGraphRunner* task_graph_runner);
  static scoped_ptr<FakeLayerTreeHost> Create(
      FakeLayerTreeHostClient* client,
      TestTaskGraphRunner* task_graph_runner,
      const LayerTreeSettings& settings);
  static scoped_ptr<FakeLayerTreeHost> Create(
      FakeLayerTreeHostClient* client,
      TestTaskGraphRunner* task_graph_runner,
      const LayerTreeSettings& settings,
      CompositorMode mode);
  static scoped_ptr<FakeLayerTreeHost> Create(
      FakeLayerTreeHostClient* client,
      TestTaskGraphRunner* task_graph_runner,
      const LayerTreeSettings& settings,
      CompositorMode mode,
      InitParams params);
  static scoped_ptr<FakeLayerTreeHost> Create(
      FakeLayerTreeHostClient* client,
      TestTaskGraphRunner* task_graph_runner,
      const LayerTreeSettings& settings,
      CompositorMode mode,
      ImageSerializationProcessor* image_serialization_processor);
  ~FakeLayerTreeHost() override;

  const RendererCapabilities& GetRendererCapabilities() const override;
  void SetNeedsCommit() override;
  void SetNeedsUpdateLayers() override {}
  void SetNeedsFullTreeSync() override {}

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
  using LayerTreeHost::RecordGpuRasterizationHistogram;

  void UpdateLayers() { LayerTreeHost::UpdateLayers(); }

  MicroBenchmarkController* GetMicroBenchmarkController() {
    return &micro_benchmark_controller_;
  }

  bool needs_commit() { return needs_commit_; }

  void set_renderer_capabilities(const RendererCapabilities& capabilities) {
    renderer_capabilities_set = true;
    renderer_capabilities = capabilities;
  }

 protected:
  FakeLayerTreeHost(FakeLayerTreeHostClient* client,
                    LayerTreeHost::InitParams* params,
                    CompositorMode mode);

 private:
  FakeImplTaskRunnerProvider task_runner_provider_;
  FakeLayerTreeHostClient* client_;
  TestSharedBitmapManager manager_;
  FakeLayerTreeHostImpl host_impl_;
  bool needs_commit_;

  bool renderer_capabilities_set;
  RendererCapabilities renderer_capabilities;
};

}  // namespace cc

#endif  // CC_TEST_FAKE_LAYER_TREE_HOST_H_
