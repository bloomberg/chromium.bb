// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/picture_layer_tiling.h"
#include "cc/test/fake_picture_layer_tiling_client.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace cc {

namespace {

static const int kTimeLimitMillis = 2000;
static const int kWarmupRuns = 5;
static const int kTimeCheckInterval = 10;

class PictureLayerTilingPerfTest : public testing::Test {
 public:
  PictureLayerTilingPerfTest() : num_runs_(0) {}

  virtual void SetUp() OVERRIDE {
    picture_layer_tiling_client_.SetTileSize(gfx::Size(256, 256));
    picture_layer_tiling_ = PictureLayerTiling::Create(
        1, gfx::Size(256 * 50, 256 * 50), &picture_layer_tiling_client_);
    picture_layer_tiling_->CreateAllTilesForTesting();
  }

  virtual void TearDown() OVERRIDE {
    picture_layer_tiling_.reset(NULL);
  }

  void EndTest() {
    elapsed_ = base::TimeTicks::HighResNow() - start_time_;
  }

  void AfterTest(const std::string& test_name) {
    printf("*RESULT %s: %.2f runs/s\n",
           test_name.c_str(),
           num_runs_ / elapsed_.InSecondsF());
  }

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

  void RunInvalidateTest(const std::string& test_name, const Region& region) {
    start_time_ = base::TimeTicks();
    num_runs_ = 0;
    do {
      picture_layer_tiling_->Invalidate(region);
    } while (DidRun());

    AfterTest(test_name);
  }

  void RunUpdateTilePrioritiesStationaryTest(
      const std::string& test_name,
      const gfx::Transform& transform) {
    start_time_ = base::TimeTicks();
    num_runs_ = 0;

    gfx::Size layer_bounds(50 * 256, 50 * 256);
    do {
      picture_layer_tiling_->UpdateTilePriorities(
        ACTIVE_TREE,
        layer_bounds,
        gfx::Rect(layer_bounds),
        gfx::Rect(layer_bounds),
        layer_bounds,
        layer_bounds,
        1.f,
        1.f,
        transform,
        transform,
        num_runs_ + 1,
        250);
    } while (DidRun());

    AfterTest(test_name);
  }

  void RunUpdateTilePrioritiesScrollingTest(
      const std::string& test_name,
      const gfx::Transform& transform) {
    start_time_ = base::TimeTicks();
    num_runs_ = 0;

    gfx::Size layer_bounds(50 * 256, 50 * 256);
    gfx::Size viewport_size(1024, 768);
    gfx::Rect viewport_rect(viewport_size);
    int xoffsets[] = {10, 0, -10, 0};
    int yoffsets[] = {0, 10, 0, -10};
    int offsetIndex = 0;
    int offsetCount = 0;
    const int maxOffsetCount = 1000;
    do {
      picture_layer_tiling_->UpdateTilePriorities(
        ACTIVE_TREE,
        viewport_size,
        viewport_rect,
        gfx::Rect(layer_bounds),
        layer_bounds,
        layer_bounds,
        1.f,
        1.f,
        transform,
        transform,
        num_runs_ + 1,
        250);

      viewport_rect = gfx::Rect(
        viewport_rect.x() + xoffsets[offsetIndex],
        viewport_rect.y() + yoffsets[offsetIndex],
        viewport_rect.width(),
        viewport_rect.height());

      if (++offsetCount > maxOffsetCount) {
        offsetCount = 0;
        offsetIndex = (offsetIndex + 1) % 4;
      }
    } while (DidRun());

    AfterTest(test_name);
  }

 private:
  FakePictureLayerTilingClient picture_layer_tiling_client_;
  scoped_ptr<PictureLayerTiling> picture_layer_tiling_;

  base::TimeTicks start_time_;
  base::TimeDelta elapsed_;
  int num_runs_;
};

TEST_F(PictureLayerTilingPerfTest, Invalidate) {
  Region one_tile(gfx::Rect(256, 256));
  RunInvalidateTest("invalidation_1x1", one_tile);

  Region half_region(gfx::Rect(25 * 256, 50 * 256));
  RunInvalidateTest("invalidation_25x50", half_region);

  Region full_region(gfx::Rect(50 * 256, 50 * 256));
  RunInvalidateTest("invalidation_50x50", full_region);
}

TEST_F(PictureLayerTilingPerfTest, UpdateTilePriorities) {
  gfx::Transform transform;
  RunUpdateTilePrioritiesStationaryTest(
      "update_tile_priorities_stationary_no_transform",
      transform);
  RunUpdateTilePrioritiesScrollingTest(
      "update_tile_priorities_scrolling_no_transform",
      transform);

  transform.Rotate(10);
  RunUpdateTilePrioritiesStationaryTest(
      "update_tile_priorities_stationary_rotation",
      transform);
  RunUpdateTilePrioritiesScrollingTest(
      "update_tile_priorities_scrolling_rotation",
      transform);

  transform.ApplyPerspectiveDepth(10);
  RunUpdateTilePrioritiesStationaryTest(
      "update_tile_priorities_stationary_perspective",
      transform);
  RunUpdateTilePrioritiesScrollingTest(
      "update_tile_priorities_scrolling_perspective",
      transform);
}

}  // namespace

}  // namespace cc
