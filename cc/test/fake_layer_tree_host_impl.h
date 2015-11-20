// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_FAKE_LAYER_TREE_HOST_IMPL_H_
#define CC_TEST_FAKE_LAYER_TREE_HOST_IMPL_H_

#include "cc/test/fake_layer_tree_host_impl_client.h"
#include "cc/test/fake_rendering_stats_instrumentation.h"
#include "cc/trees/layer_tree_host_impl.h"
#include "cc/trees/single_thread_proxy.h"

namespace gpu {
class GpuMemoryBufferManager;
}

namespace cc {

class FakeLayerTreeHostImpl : public LayerTreeHostImpl {
 public:
  FakeLayerTreeHostImpl(TaskRunnerProvider* task_runner_provider,
                        SharedBitmapManager* manager,
                        TaskGraphRunner* task_graph_runner);
  FakeLayerTreeHostImpl(const LayerTreeSettings& settings,
                        TaskRunnerProvider* task_runner_provider,
                        SharedBitmapManager* manager,
                        TaskGraphRunner* task_graph_runner);
  FakeLayerTreeHostImpl(const LayerTreeSettings& settings,
                        TaskRunnerProvider* task_runner_provider,
                        SharedBitmapManager* manager,
                        TaskGraphRunner* task_graph_runner,
                        gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager);
  ~FakeLayerTreeHostImpl() override;

  void ForcePrepareToDraw() {
    LayerTreeHostImpl::FrameData frame_data;
    PrepareToDraw(&frame_data);
    DidDrawAllLayers(frame_data);
  }

  void CreatePendingTree() override;

  void NotifyTileStateChanged(const Tile* tile) override;
  BeginFrameArgs CurrentBeginFrameArgs() const override;
  void AdvanceToNextFrame(base::TimeDelta advance_by);
  void UpdateNumChildrenAndDrawPropertiesForActiveTree();
  static void UpdateNumChildrenAndDrawProperties(LayerTreeImpl* layerTree);
  static int RecursiveUpdateNumChildren(LayerImpl* layer);

  using LayerTreeHostImpl::ActivateSyncTree;
  using LayerTreeHostImpl::prepare_tiles_needed;
  using LayerTreeHostImpl::is_likely_to_require_a_draw;
  using LayerTreeHostImpl::RemoveRenderPasses;

  bool notify_tile_state_changed_called() const {
    return notify_tile_state_changed_called_;
  }
  void set_notify_tile_state_changed_called(bool called) {
    notify_tile_state_changed_called_ = called;
  }

 private:
  FakeLayerTreeHostImplClient client_;
  FakeRenderingStatsInstrumentation stats_instrumentation_;
  bool notify_tile_state_changed_called_;
};

}  // namespace cc

#endif  // CC_TEST_FAKE_LAYER_TREE_HOST_IMPL_H_
