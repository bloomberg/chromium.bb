// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_TEST_LAYER_TREE_HOST_BASE_H_
#define CC_TEST_TEST_LAYER_TREE_HOST_BASE_H_

#include <memory>

#include "cc/output/output_surface.h"
#include "cc/test/fake_impl_task_runner_provider.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "cc/test/fake_picture_layer_impl.h"
#include "cc/test/test_gpu_memory_buffer_manager.h"
#include "cc/test/test_shared_bitmap_manager.h"
#include "cc/test/test_task_graph_runner.h"
#include "cc/tiles/tile_priority.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"

namespace cc {

class TestLayerTreeHostBase : public testing::Test {
 protected:
  TestLayerTreeHostBase();
  ~TestLayerTreeHostBase() override;

  void SetUp() override;

  virtual LayerTreeSettings CreateSettings();
  virtual std::unique_ptr<OutputSurface> CreateOutputSurface();
  virtual std::unique_ptr<FakeLayerTreeHostImpl> CreateHostImpl(
      const LayerTreeSettings& settings,
      TaskRunnerProvider* task_runner_provider,
      SharedBitmapManager* shared_bitmap_manager,
      TaskGraphRunner* task_graph_runner,
      gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager_);
  virtual std::unique_ptr<TaskGraphRunner> CreateTaskGraphRunner();
  virtual void InitializeRenderer();

  void ResetOutputSurface(std::unique_ptr<OutputSurface> output_surface);
  std::unique_ptr<FakeLayerTreeHostImpl> TakeHostImpl();

  void SetupDefaultTrees(const gfx::Size& layer_bounds);
  void SetupTrees(scoped_refptr<RasterSource> pending_raster_source,
                  scoped_refptr<RasterSource> active_raster_source);
  void SetupPendingTree(scoped_refptr<RasterSource> raster_source);
  void SetupPendingTree(scoped_refptr<RasterSource> raster_source,
                        const gfx::Size& tile_size,
                        const Region& invalidation);
  void ActivateTree();
  void RebuildPropertyTreesOnPendingTree();

  FakeLayerTreeHostImpl* host_impl() const { return host_impl_.get(); }
  TaskGraphRunner* task_graph_runner() const {
    return task_graph_runner_.get();
  }
  OutputSurface* output_surface() const { return output_surface_.get(); }
  FakePictureLayerImpl* pending_layer() const { return pending_layer_; }
  FakePictureLayerImpl* active_layer() const { return active_layer_; }
  FakePictureLayerImpl* old_pending_layer() const { return old_pending_layer_; }
  int layer_id() const { return id_; }

 private:
  void SetInitialTreePriority();

  FakeImplTaskRunnerProvider task_runner_provider_;
  TestSharedBitmapManager shared_bitmap_manager_;
  std::unique_ptr<TaskGraphRunner> task_graph_runner_;
  TestGpuMemoryBufferManager gpu_memory_buffer_manager_;
  std::unique_ptr<OutputSurface> output_surface_;
  std::unique_ptr<FakeLayerTreeHostImpl> host_impl_;

  FakePictureLayerImpl* pending_layer_;
  FakePictureLayerImpl* active_layer_;
  FakePictureLayerImpl* old_pending_layer_;
  const int root_id_;
  const int id_;
};

}  // namespace cc

#endif  // CC_TEST_TEST_LAYER_TREE_HOST_BASE_H_
