// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/picture_layer_impl.h"

#include "base/thread_task_runner_handle.h"
#include "cc/debug/lap_timer.h"
#include "cc/test/fake_impl_proxy.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "cc/test/fake_output_surface.h"
#include "cc/test/fake_picture_layer_impl.h"
#include "cc/test/fake_picture_pile_impl.h"
#include "cc/test/test_shared_bitmap_manager.h"
#include "cc/test/test_task_graph_runner.h"
#include "cc/tiles/tiling_set_raster_queue_all.h"
#include "cc/trees/layer_tree_impl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_test.h"

namespace cc {
namespace {

static const int kTimeLimitMillis = 2000;
static const int kWarmupRuns = 5;
static const int kTimeCheckInterval = 10;

void AddTiling(float scale,
               FakePictureLayerImpl* layer,
               std::vector<Tile*>* all_tiles) {
  PictureLayerTiling* tiling = layer->AddTiling(scale);

  tiling->set_resolution(HIGH_RESOLUTION);
  tiling->CreateAllTilesForTesting();
  std::vector<Tile*> tiling_tiles = tiling->AllTilesForTesting();
  std::copy(
      tiling_tiles.begin(), tiling_tiles.end(), std::back_inserter(*all_tiles));
}

class PictureLayerImplPerfTest : public testing::Test {
 public:
  PictureLayerImplPerfTest()
      : proxy_(base::ThreadTaskRunnerHandle::Get()),
        output_surface_(FakeOutputSurface::Create3d()),
        host_impl_(LayerTreeSettings(),
                   &proxy_,
                   &shared_bitmap_manager_,
                   &task_graph_runner_),
        timer_(kWarmupRuns,
               base::TimeDelta::FromMilliseconds(kTimeLimitMillis),
               kTimeCheckInterval) {}

  void SetUp() override {
    host_impl_.InitializeRenderer(output_surface_.get());
  }

  void SetupActiveTree(const gfx::Size& layer_bounds,
                       const gfx::Size& tile_size) {
    scoped_refptr<FakePicturePileImpl> pile =
        FakePicturePileImpl::CreateFilledPile(tile_size, layer_bounds);
    LayerTreeImpl* active_tree = host_impl_.active_tree();
    active_tree->DetachLayerTree();

    scoped_ptr<FakePictureLayerImpl> active_layer =
        FakePictureLayerImpl::CreateWithRasterSource(active_tree, 7, pile);
    active_layer->SetDrawsContent(true);
    active_layer->SetHasRenderSurface(true);
    active_tree->SetRootLayer(active_layer.Pass());

    active_layer_ = static_cast<FakePictureLayerImpl*>(
        host_impl_.active_tree()->LayerById(7));
  }

  void RunRasterQueueConstructAndIterateTest(const std::string& test_name,
                                             int num_tiles,
                                             const gfx::Size& viewport_size) {
    host_impl_.SetViewportSize(viewport_size);
    bool update_lcd_text = false;
    host_impl_.active_tree()->UpdateDrawProperties(update_lcd_text);

    timer_.Reset();
    do {
      int count = num_tiles;
      scoped_ptr<TilingSetRasterQueueAll> queue(new TilingSetRasterQueueAll(
          active_layer_->picture_layer_tiling_set(), false));
      while (count--) {
        ASSERT_TRUE(!queue->IsEmpty()) << "count: " << count;
        ASSERT_TRUE(queue->Top().tile()) << "count: " << count;
        queue->Pop();
      }
      timer_.NextLap();
    } while (!timer_.HasTimeLimitExpired());

    perf_test::PrintResult("tiling_set_raster_queue_construct_and_iterate", "",
                           test_name, timer_.LapsPerSecond(), "runs/s", true);
  }

  void RunRasterQueueConstructTest(const std::string& test_name,
                                   const gfx::Rect& viewport) {
    host_impl_.SetViewportSize(viewport.size());
    active_layer_->PushScrollOffsetFromMainThreadAndClobberActiveValue(
        gfx::ScrollOffset(viewport.x(), viewport.y()));
    bool update_lcd_text = false;
    host_impl_.active_tree()->UpdateDrawProperties(update_lcd_text);

    timer_.Reset();
    do {
      scoped_ptr<TilingSetRasterQueueAll> queue(new TilingSetRasterQueueAll(
          active_layer_->picture_layer_tiling_set(), false));
      timer_.NextLap();
    } while (!timer_.HasTimeLimitExpired());

    perf_test::PrintResult("tiling_set_raster_queue_construct", "", test_name,
                           timer_.LapsPerSecond(), "runs/s", true);
  }

  void RunEvictionQueueConstructAndIterateTest(
      const std::string& test_name,
      int num_tiles,
      const gfx::Size& viewport_size) {
    host_impl_.SetViewportSize(viewport_size);
    active_layer_->MarkAllTilingsUsed();
    bool update_lcd_text = false;
    host_impl_.active_tree()->UpdateDrawProperties(update_lcd_text);

    timer_.Reset();
    do {
      int count = num_tiles;
      scoped_ptr<TilingSetEvictionQueue> queue(new TilingSetEvictionQueue(
          active_layer_->picture_layer_tiling_set()));
      while (count--) {
        ASSERT_TRUE(!queue->IsEmpty()) << "count: " << count;
        ASSERT_TRUE(queue->Top().tile()) << "count: " << count;
        queue->Pop();
      }
      timer_.NextLap();
    } while (!timer_.HasTimeLimitExpired());

    perf_test::PrintResult("tiling_set_eviction_queue_construct_and_iterate",
                           "", test_name, timer_.LapsPerSecond(), "runs/s",
                           true);
  }

  void RunEvictionQueueConstructTest(const std::string& test_name,
                                     const gfx::Rect& viewport) {
    host_impl_.SetViewportSize(viewport.size());
    active_layer_->PushScrollOffsetFromMainThreadAndClobberActiveValue(
        gfx::ScrollOffset(viewport.x(), viewport.y()));
    active_layer_->MarkAllTilingsUsed();
    bool update_lcd_text = false;
    host_impl_.active_tree()->UpdateDrawProperties(update_lcd_text);

    timer_.Reset();
    do {
      scoped_ptr<TilingSetEvictionQueue> queue(new TilingSetEvictionQueue(
          active_layer_->picture_layer_tiling_set()));
      timer_.NextLap();
    } while (!timer_.HasTimeLimitExpired());

    perf_test::PrintResult("tiling_set_eviction_queue_construct", "", test_name,
                           timer_.LapsPerSecond(), "runs/s", true);
  }

 protected:
  TestSharedBitmapManager shared_bitmap_manager_;
  TestTaskGraphRunner task_graph_runner_;
  FakeImplProxy proxy_;
  scoped_ptr<OutputSurface> output_surface_;
  FakeLayerTreeHostImpl host_impl_;
  FakePictureLayerImpl* active_layer_;
  LapTimer timer_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PictureLayerImplPerfTest);
};

TEST_F(PictureLayerImplPerfTest, TilingSetRasterQueueConstructAndIterate) {
  SetupActiveTree(gfx::Size(10000, 10000), gfx::Size(256, 256));

  float low_res_factor = host_impl_.settings().low_res_contents_scale_factor;

  active_layer_->AddTiling(low_res_factor);
  active_layer_->AddTiling(0.3f);
  active_layer_->AddTiling(0.7f);
  active_layer_->AddTiling(1.0f);
  active_layer_->AddTiling(2.0f);

  RunRasterQueueConstructAndIterateTest("32_100x100", 32, gfx::Size(100, 100));
  RunRasterQueueConstructAndIterateTest("32_500x500", 32, gfx::Size(500, 500));
  RunRasterQueueConstructAndIterateTest("64_100x100", 64, gfx::Size(100, 100));
  RunRasterQueueConstructAndIterateTest("64_500x500", 64, gfx::Size(500, 500));
}

TEST_F(PictureLayerImplPerfTest, TilingSetRasterQueueConstruct) {
  SetupActiveTree(gfx::Size(10000, 10000), gfx::Size(256, 256));

  float low_res_factor = host_impl_.settings().low_res_contents_scale_factor;

  active_layer_->AddTiling(low_res_factor);
  active_layer_->AddTiling(0.3f);
  active_layer_->AddTiling(0.7f);
  active_layer_->AddTiling(1.0f);
  active_layer_->AddTiling(2.0f);

  RunRasterQueueConstructTest("0_0_100x100", gfx::Rect(0, 0, 100, 100));
  RunRasterQueueConstructTest("5000_0_100x100", gfx::Rect(5000, 0, 100, 100));
  RunRasterQueueConstructTest("9999_0_100x100", gfx::Rect(9999, 0, 100, 100));
}

TEST_F(PictureLayerImplPerfTest, TilingSetEvictionQueueConstructAndIterate) {
  SetupActiveTree(gfx::Size(10000, 10000), gfx::Size(256, 256));

  float low_res_factor = host_impl_.settings().low_res_contents_scale_factor;

  std::vector<Tile*> all_tiles;
  AddTiling(low_res_factor, active_layer_, &all_tiles);
  AddTiling(0.3f, active_layer_, &all_tiles);
  AddTiling(0.7f, active_layer_, &all_tiles);
  AddTiling(1.0f, active_layer_, &all_tiles);
  AddTiling(2.0f, active_layer_, &all_tiles);

  ASSERT_TRUE(host_impl_.tile_manager() != nullptr);
  host_impl_.tile_manager()->InitializeTilesWithResourcesForTesting(all_tiles);

  RunEvictionQueueConstructAndIterateTest(
      "32_100x100", 32, gfx::Size(100, 100));
  RunEvictionQueueConstructAndIterateTest(
      "32_500x500", 32, gfx::Size(500, 500));
  RunEvictionQueueConstructAndIterateTest(
      "64_100x100", 64, gfx::Size(100, 100));
  RunEvictionQueueConstructAndIterateTest(
      "64_500x500", 64, gfx::Size(500, 500));
}

TEST_F(PictureLayerImplPerfTest, TilingSetEvictionQueueConstruct) {
  SetupActiveTree(gfx::Size(10000, 10000), gfx::Size(256, 256));

  float low_res_factor = host_impl_.settings().low_res_contents_scale_factor;

  std::vector<Tile*> all_tiles;
  AddTiling(low_res_factor, active_layer_, &all_tiles);
  AddTiling(0.3f, active_layer_, &all_tiles);
  AddTiling(0.7f, active_layer_, &all_tiles);
  AddTiling(1.0f, active_layer_, &all_tiles);
  AddTiling(2.0f, active_layer_, &all_tiles);

  ASSERT_TRUE(host_impl_.tile_manager() != nullptr);
  host_impl_.tile_manager()->InitializeTilesWithResourcesForTesting(all_tiles);

  RunEvictionQueueConstructTest("0_0_100x100", gfx::Rect(0, 0, 100, 100));
  RunEvictionQueueConstructTest("5000_0_100x100", gfx::Rect(5000, 0, 100, 100));
  RunEvictionQueueConstructTest("9999_0_100x100", gfx::Rect(9999, 0, 100, 100));
}

}  // namespace
}  // namespace cc
