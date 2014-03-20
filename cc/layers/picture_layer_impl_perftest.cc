// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/picture_layer_impl.h"

#include "cc/test/fake_impl_proxy.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "cc/test/fake_output_surface.h"
#include "cc/test/fake_picture_layer_impl.h"
#include "cc/test/fake_picture_pile_impl.h"
#include "cc/test/impl_side_painting_settings.h"
#include "cc/test/test_shared_bitmap_manager.h"
#include "cc/trees/layer_tree_impl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_test.h"

namespace cc {
namespace {

static const int kTimeLimitMillis = 2000;
static const int kWarmupRuns = 5;
static const int kTimeCheckInterval = 10;

class PictureLayerImplPerfTest : public testing::Test {
 public:
  PictureLayerImplPerfTest()
      : num_runs_(0),
        proxy_(base::MessageLoopProxy::current()),
        host_impl_(ImplSidePaintingSettings(),
                   &proxy_,
                   &shared_bitmap_manager_) {}

  virtual void SetUp() OVERRIDE {
    host_impl_.InitializeRenderer(
        FakeOutputSurface::Create3d().PassAs<OutputSurface>());
  }

  void SetupPendingTree(const gfx::Size& layer_bounds,
                        const gfx::Size& tile_size) {
    scoped_refptr<FakePicturePileImpl> pile =
        FakePicturePileImpl::CreateFilledPile(tile_size, layer_bounds);
    host_impl_.CreatePendingTree();
    LayerTreeImpl* pending_tree = host_impl_.pending_tree();
    pending_tree->DetachLayerTree();

    scoped_ptr<FakePictureLayerImpl> pending_layer =
        FakePictureLayerImpl::CreateWithPile(pending_tree, 7, pile);
    pending_layer->SetDrawsContent(true);
    pending_tree->SetRootLayer(pending_layer.PassAs<LayerImpl>());

    pending_layer_ = static_cast<FakePictureLayerImpl*>(
        host_impl_.pending_tree()->LayerById(7));
    pending_layer_->DoPostCommitInitializationIfNeeded();
  }

  void EndTest() { elapsed_ = base::TimeTicks::HighResNow() - start_time_; }

  bool DidRun() {
    ++num_runs_;
    if (num_runs_ == kWarmupRuns)
      start_time_ = base::TimeTicks::HighResNow();

    if (!start_time_.is_null() && (num_runs_ % kTimeCheckInterval) == 0) {
      base::TimeDelta elapsed = base::TimeTicks::HighResNow() - start_time_;
      if (elapsed >= base::TimeDelta::FromMilliseconds(kTimeLimitMillis)) {
        elapsed_ = elapsed;
        return false;
      }
    }
    return true;
  }

  void RunLayerRasterTileIteratorTest(const std::string& test_name,
                                      int num_tiles,
                                      const gfx::Size& viewport_size) {
    start_time_ = base::TimeTicks();
    num_runs_ = 0;

    host_impl_.SetViewportSize(viewport_size);
    host_impl_.pending_tree()->UpdateDrawProperties();

    do {
      int count = num_tiles;
      for (PictureLayerImpl::LayerRasterTileIterator it(pending_layer_, false);
           it && count;
           ++it) {
        --count;
      }
    } while (DidRun());

    perf_test::PrintResult("layer_raster_tile_iterator",
                           "",
                           test_name,
                           num_runs_ / elapsed_.InSecondsF(),
                           "runs/s",
                           true);
  }

 protected:
  base::TimeTicks start_time_;
  base::TimeDelta elapsed_;
  int num_runs_;

  TestSharedBitmapManager shared_bitmap_manager_;
  FakeImplProxy proxy_;
  FakeLayerTreeHostImpl host_impl_;
  FakePictureLayerImpl* pending_layer_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PictureLayerImplPerfTest);
};

TEST_F(PictureLayerImplPerfTest, LayerRasterTileIterator) {
  SetupPendingTree(gfx::Size(10000, 10000), gfx::Size(256, 256));

  float low_res_factor = host_impl_.settings().low_res_contents_scale_factor;

  pending_layer_->AddTiling(low_res_factor);
  pending_layer_->AddTiling(0.3f);
  pending_layer_->AddTiling(0.7f);
  pending_layer_->AddTiling(1.0f);
  pending_layer_->AddTiling(2.0f);

  RunLayerRasterTileIteratorTest("32_100x100", 32, gfx::Size(100, 100));
  RunLayerRasterTileIteratorTest("32_500x500", 32, gfx::Size(500, 500));
  RunLayerRasterTileIteratorTest("64_100x100", 64, gfx::Size(100, 100));
  RunLayerRasterTileIteratorTest("64_500x500", 64, gfx::Size(500, 500));
}

}  // namespace
}  // namespace cc
