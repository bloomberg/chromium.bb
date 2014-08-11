// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/debug/lap_timer.h"
#include "cc/resources/picture_layer_tiling.h"
#include "cc/resources/resource_provider.h"
#include "cc/resources/scoped_resource.h"
#include "cc/test/fake_output_surface.h"
#include "cc/test/fake_output_surface_client.h"
#include "cc/test/fake_picture_layer_tiling_client.h"
#include "cc/test/test_context_provider.h"
#include "cc/test/test_shared_bitmap_manager.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_test.h"

namespace cc {

namespace {

static const int kTimeLimitMillis = 2000;
static const int kWarmupRuns = 5;
static const int kTimeCheckInterval = 10;

class PictureLayerTilingPerfTest : public testing::Test {
 public:
  PictureLayerTilingPerfTest()
      : timer_(kWarmupRuns,
               base::TimeDelta::FromMilliseconds(kTimeLimitMillis),
               kTimeCheckInterval),
        context_provider_(TestContextProvider::Create()) {
    output_surface_ = FakeOutputSurface::Create3d(context_provider_).Pass();
    CHECK(output_surface_->BindToClient(&output_surface_client_));

    shared_bitmap_manager_.reset(new TestSharedBitmapManager());
    resource_provider_ = ResourceProvider::Create(output_surface_.get(),
                                                  shared_bitmap_manager_.get(),
                                                  0,
                                                  false,
                                                  1,
                                                  false).Pass();
  }

  virtual void SetUp() OVERRIDE {
    picture_layer_tiling_client_.SetTileSize(gfx::Size(256, 256));
    picture_layer_tiling_client_.set_max_tiles_for_interest_area(250);
    picture_layer_tiling_client_.set_tree(PENDING_TREE);
    picture_layer_tiling_ = PictureLayerTiling::Create(
        1, gfx::Size(256 * 50, 256 * 50), &picture_layer_tiling_client_);
    picture_layer_tiling_->CreateAllTilesForTesting();
  }

  virtual void TearDown() OVERRIDE {
    picture_layer_tiling_.reset(NULL);
  }

  void RunInvalidateTest(const std::string& test_name, const Region& region) {
    timer_.Reset();
    do {
      picture_layer_tiling_->UpdateTilesToCurrentPile(
          region, picture_layer_tiling_->tiling_size());
      timer_.NextLap();
    } while (!timer_.HasTimeLimitExpired());

    perf_test::PrintResult(
        "invalidation", "", test_name, timer_.LapsPerSecond(), "runs/s", true);
  }

  void RunUpdateTilePrioritiesStationaryTest(const std::string& test_name,
                                             const gfx::Transform& transform) {
    gfx::Rect viewport_rect(0, 0, 1024, 768);

    timer_.Reset();
    do {
      picture_layer_tiling_->UpdateTilePriorities(PENDING_TREE,
                                                  viewport_rect,
                                                  1.f,
                                                  timer_.NumLaps() + 1,
                                                  NULL,
                                                  NULL,
                                                  gfx::Transform());
      timer_.NextLap();
    } while (!timer_.HasTimeLimitExpired());

    perf_test::PrintResult("update_tile_priorities_stationary",
                           "",
                           test_name,
                           timer_.LapsPerSecond(),
                           "runs/s",
                           true);
  }

  void RunUpdateTilePrioritiesScrollingTest(const std::string& test_name,
                                            const gfx::Transform& transform) {
    gfx::Size viewport_size(1024, 768);
    gfx::Rect viewport_rect(viewport_size);
    int xoffsets[] = {10, 0, -10, 0};
    int yoffsets[] = {0, 10, 0, -10};
    int offsetIndex = 0;
    int offsetCount = 0;
    const int maxOffsetCount = 1000;

    timer_.Reset();
    do {
      picture_layer_tiling_->UpdateTilePriorities(PENDING_TREE,
                                                  viewport_rect,
                                                  1.f,
                                                  timer_.NumLaps() + 1,
                                                  NULL,
                                                  NULL,
                                                  gfx::Transform());

      viewport_rect = gfx::Rect(viewport_rect.x() + xoffsets[offsetIndex],
                                viewport_rect.y() + yoffsets[offsetIndex],
                                viewport_rect.width(),
                                viewport_rect.height());

      if (++offsetCount > maxOffsetCount) {
        offsetCount = 0;
        offsetIndex = (offsetIndex + 1) % 4;
      }
      timer_.NextLap();
    } while (!timer_.HasTimeLimitExpired());

    perf_test::PrintResult("update_tile_priorities_scrolling",
                           "",
                           test_name,
                           timer_.LapsPerSecond(),
                           "runs/s",
                           true);
  }

  void RunRasterIteratorConstructTest(const std::string& test_name,
                                      const gfx::Rect& viewport) {
    gfx::Size bounds(viewport.size());
    picture_layer_tiling_ =
        PictureLayerTiling::Create(1, bounds, &picture_layer_tiling_client_);
    picture_layer_tiling_client_.set_tree(ACTIVE_TREE);
    picture_layer_tiling_->UpdateTilePriorities(
        ACTIVE_TREE, viewport, 1.0f, 1.0, NULL, NULL, gfx::Transform());

    timer_.Reset();
    do {
      PictureLayerTiling::TilingRasterTileIterator it(
          picture_layer_tiling_.get(), ACTIVE_TREE);
      timer_.NextLap();
    } while (!timer_.HasTimeLimitExpired());

    perf_test::PrintResult("tiling_raster_tile_iterator_construct",
                           "",
                           test_name,
                           timer_.LapsPerSecond(),
                           "runs/s",
                           true);
  }

  void RunRasterIteratorConstructAndIterateTest(const std::string& test_name,
                                                int num_tiles,
                                                const gfx::Rect& viewport) {
    gfx::Size bounds(10000, 10000);
    picture_layer_tiling_ =
        PictureLayerTiling::Create(1, bounds, &picture_layer_tiling_client_);
    picture_layer_tiling_client_.set_tree(ACTIVE_TREE);
    picture_layer_tiling_->UpdateTilePriorities(
        ACTIVE_TREE, viewport, 1.0f, 1.0, NULL, NULL, gfx::Transform());

    timer_.Reset();
    do {
      int count = num_tiles;
      PictureLayerTiling::TilingRasterTileIterator it(
          picture_layer_tiling_.get(), ACTIVE_TREE);
      while (count--) {
        ASSERT_TRUE(it) << "count: " << count;
        ASSERT_TRUE(*it != NULL) << "count: " << count;
        ++it;
      }
      timer_.NextLap();
    } while (!timer_.HasTimeLimitExpired());

    perf_test::PrintResult("tiling_raster_tile_iterator_construct_and_iterate",
                           "",
                           test_name,
                           timer_.LapsPerSecond(),
                           "runs/s",
                           true);
  }

  void RunEvictionIteratorConstructTest(const std::string& test_name,
                                        const gfx::Rect& viewport) {
    gfx::Size bounds(viewport.size());
    picture_layer_tiling_ =
        PictureLayerTiling::Create(1, bounds, &picture_layer_tiling_client_);
    picture_layer_tiling_client_.set_tree(ACTIVE_TREE);
    picture_layer_tiling_->UpdateTilePriorities(
        ACTIVE_TREE, viewport, 1.0f, 1.0, NULL, NULL, gfx::Transform());

    timer_.Reset();
    TreePriority priorities[] = {SAME_PRIORITY_FOR_BOTH_TREES,
                                 SMOOTHNESS_TAKES_PRIORITY,
                                 NEW_CONTENT_TAKES_PRIORITY};
    int priority_count = 0;
    do {
      PictureLayerTiling::TilingEvictionTileIterator it(
          picture_layer_tiling_.get(),
          priorities[priority_count],
          PictureLayerTiling::NOW);
      priority_count = (priority_count + 1) % arraysize(priorities);
      timer_.NextLap();
    } while (!timer_.HasTimeLimitExpired());

    perf_test::PrintResult("tiling_eviction_tile_iterator_construct",
                           "",
                           test_name,
                           timer_.LapsPerSecond(),
                           "runs/s",
                           true);
  }

  void RunEvictionIteratorConstructAndIterateTest(const std::string& test_name,
                                                  int num_tiles,
                                                  const gfx::Rect& viewport) {
    gfx::Size bounds(10000, 10000);
    picture_layer_tiling_ =
        PictureLayerTiling::Create(1, bounds, &picture_layer_tiling_client_);
    picture_layer_tiling_client_.set_tree(ACTIVE_TREE);
    picture_layer_tiling_->UpdateTilePriorities(
        ACTIVE_TREE, viewport, 1.0f, 1.0, NULL, NULL, gfx::Transform());

    TreePriority priorities[] = {SAME_PRIORITY_FOR_BOTH_TREES,
                                 SMOOTHNESS_TAKES_PRIORITY,
                                 NEW_CONTENT_TAKES_PRIORITY};

    // Ensure all tiles have resources.
    std::vector<Tile*> all_tiles = picture_layer_tiling_->AllTilesForTesting();
    for (std::vector<Tile*>::iterator tile_it = all_tiles.begin();
         tile_it != all_tiles.end();
         ++tile_it) {
      Tile* tile = *tile_it;
      ManagedTileState::TileVersion& tile_version =
          tile->GetTileVersionForTesting(tile->GetRasterModeForTesting());
      tile_version.SetResourceForTesting(
          ScopedResource::Create(resource_provider_.get()).Pass());
    }

    int priority_count = 0;
    timer_.Reset();
    do {
      int count = num_tiles;
      PictureLayerTiling::TilingEvictionTileIterator it(
          picture_layer_tiling_.get(),
          priorities[priority_count],
          PictureLayerTiling::EVENTUALLY);
      while (count--) {
        ASSERT_TRUE(it) << "count: " << count;
        ASSERT_TRUE(*it != NULL) << "count: " << count;
        ++it;
      }
      priority_count = (priority_count + 1) % arraysize(priorities);
      timer_.NextLap();
    } while (!timer_.HasTimeLimitExpired());

    // Remove all resources from tiles to make sure the tile version destructor
    // doesn't complain.
    for (std::vector<Tile*>::iterator tile_it = all_tiles.begin();
         tile_it != all_tiles.end();
         ++tile_it) {
      Tile* tile = *tile_it;
      ManagedTileState::TileVersion& tile_version =
          tile->GetTileVersionForTesting(tile->GetRasterModeForTesting());
      tile_version.SetResourceForTesting(scoped_ptr<ScopedResource>());
    }

    perf_test::PrintResult(
        "tiling_eviction_tile_iterator_construct_and_iterate",
        "",
        test_name,
        timer_.LapsPerSecond(),
        "runs/s",
        true);
  }

 private:
  FakePictureLayerTilingClient picture_layer_tiling_client_;
  scoped_ptr<PictureLayerTiling> picture_layer_tiling_;

  LapTimer timer_;

  scoped_refptr<ContextProvider> context_provider_;
  FakeOutputSurfaceClient output_surface_client_;
  scoped_ptr<FakeOutputSurface> output_surface_;
  scoped_ptr<SharedBitmapManager> shared_bitmap_manager_;
  scoped_ptr<ResourceProvider> resource_provider_;
};

TEST_F(PictureLayerTilingPerfTest, Invalidate) {
  Region one_tile(gfx::Rect(256, 256));
  RunInvalidateTest("1x1", one_tile);

  Region half_region(gfx::Rect(25 * 256, 50 * 256));
  RunInvalidateTest("25x50", half_region);

  Region full_region(gfx::Rect(50 * 256, 50 * 256));
  RunInvalidateTest("50x50", full_region);
}

#if defined(OS_ANDROID)
// TODO(vmpstr): Investigate why this is noisy (crbug.com/310220).
TEST_F(PictureLayerTilingPerfTest, DISABLED_UpdateTilePriorities) {
#else
TEST_F(PictureLayerTilingPerfTest, UpdateTilePriorities) {
#endif  // defined(OS_ANDROID)
  gfx::Transform transform;

  RunUpdateTilePrioritiesStationaryTest("no_transform", transform);
  RunUpdateTilePrioritiesScrollingTest("no_transform", transform);

  transform.Rotate(10);
  RunUpdateTilePrioritiesStationaryTest("rotation", transform);
  RunUpdateTilePrioritiesScrollingTest("rotation", transform);

  transform.ApplyPerspectiveDepth(10);
  RunUpdateTilePrioritiesStationaryTest("perspective", transform);
  RunUpdateTilePrioritiesScrollingTest("perspective", transform);
}

TEST_F(PictureLayerTilingPerfTest, TilingRasterTileIteratorConstruct) {
  RunRasterIteratorConstructTest("0_0_100x100", gfx::Rect(0, 0, 100, 100));
  RunRasterIteratorConstructTest("50_0_100x100", gfx::Rect(50, 0, 100, 100));
  RunRasterIteratorConstructTest("100_0_100x100", gfx::Rect(100, 0, 100, 100));
  RunRasterIteratorConstructTest("150_0_100x100", gfx::Rect(150, 0, 100, 100));
}

TEST_F(PictureLayerTilingPerfTest,
       TilingRasterTileIteratorConstructAndIterate) {
  RunRasterIteratorConstructAndIterateTest(
      "32_100x100", 32, gfx::Rect(0, 0, 100, 100));
  RunRasterIteratorConstructAndIterateTest(
      "32_500x500", 32, gfx::Rect(0, 0, 500, 500));
  RunRasterIteratorConstructAndIterateTest(
      "64_100x100", 64, gfx::Rect(0, 0, 100, 100));
  RunRasterIteratorConstructAndIterateTest(
      "64_500x500", 64, gfx::Rect(0, 0, 500, 500));
}

TEST_F(PictureLayerTilingPerfTest, TilingEvictionTileIteratorConstruct) {
  RunEvictionIteratorConstructTest("0_0_100x100", gfx::Rect(0, 0, 100, 100));
  RunEvictionIteratorConstructTest("50_0_100x100", gfx::Rect(50, 0, 100, 100));
  RunEvictionIteratorConstructTest("100_0_100x100",
                                   gfx::Rect(100, 0, 100, 100));
  RunEvictionIteratorConstructTest("150_0_100x100",
                                   gfx::Rect(150, 0, 100, 100));
}

TEST_F(PictureLayerTilingPerfTest,
       TilingEvictionTileIteratorConstructAndIterate) {
  RunEvictionIteratorConstructAndIterateTest(
      "32_100x100", 32, gfx::Rect(0, 0, 100, 100));
  RunEvictionIteratorConstructAndIterateTest(
      "32_500x500", 32, gfx::Rect(0, 0, 500, 500));
  RunEvictionIteratorConstructAndIterateTest(
      "64_100x100", 64, gfx::Rect(0, 0, 100, 100));
  RunEvictionIteratorConstructAndIterateTest(
      "64_500x500", 64, gfx::Rect(0, 0, 500, 500));
}

}  // namespace

}  // namespace cc
