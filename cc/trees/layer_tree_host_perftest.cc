// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/layer_tree_host.h"

#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/strings/string_piece.h"
#include "cc/layers/content_layer.h"
#include "cc/layers/nine_patch_layer.h"
#include "cc/layers/solid_color_layer.h"
#include "cc/test/fake_content_layer_client.h"
#include "cc/test/layer_tree_json_parser.h"
#include "cc/test/layer_tree_test.h"
#include "cc/test/paths.h"
#include "cc/trees/layer_tree_impl.h"

namespace cc {
namespace {

static const int kTimeLimitMillis = 2000;
static const int kWarmupRuns = 5;
static const int kTimeCheckInterval = 10;

class LayerTreeHostPerfTest : public LayerTreeTest {
 public:
  LayerTreeHostPerfTest()
      : num_draws_(0),
        num_commits_(0),
        full_damage_each_frame_(false),
        animation_driven_drawing_(false),
        measure_commit_cost_(false) {
    fake_content_layer_client_.set_paint_all_opaque(true);
  }

  virtual void BeginTest() OVERRIDE {
    BuildTree();
    PostSetNeedsCommitToMainThread();
  }

  virtual void Animate(base::TimeTicks monotonic_time) OVERRIDE {
    if (animation_driven_drawing_ && !TestEnded())
      layer_tree_host()->SetNeedsAnimate();
  }

  virtual void BeginCommitOnThread(LayerTreeHostImpl* host_impl) OVERRIDE {
    if (measure_commit_cost_)
      commit_start_time_ = base::TimeTicks::HighResNow();
  }

  virtual void CommitCompleteOnThread(LayerTreeHostImpl* host_impl) OVERRIDE {
    if (measure_commit_cost_ && num_draws_ >= kWarmupRuns) {
      total_commit_time_ += base::TimeTicks::HighResNow() - commit_start_time_;
      ++num_commits_;
    }
  }

  virtual void DrawLayersOnThread(LayerTreeHostImpl* impl) OVERRIDE {
    ++num_draws_;
    if (num_draws_ == kWarmupRuns)
      start_time_ = base::TimeTicks::HighResNow();

    if (!start_time_.is_null() && (num_draws_ % kTimeCheckInterval) == 0) {
      base::TimeDelta elapsed = base::TimeTicks::HighResNow() - start_time_;
      if (elapsed >= base::TimeDelta::FromMilliseconds(kTimeLimitMillis)) {
        elapsed_ = elapsed;
        EndTest();
        return;
      }
    }
    if (!animation_driven_drawing_)
      impl->SetNeedsRedraw();
    if (full_damage_each_frame_)
      impl->SetFullRootLayerDamage();
  }

  virtual void BuildTree() {}

  virtual void AfterTest() OVERRIDE {
    num_draws_ -= kWarmupRuns;

    // Format matches chrome/test/perf/perf_test.h:PrintResult
    printf("*RESULT %s: frames: %d, %.2f ms/frame\n",
           test_name_.c_str(),
           num_draws_,
           elapsed_.InMillisecondsF() / num_draws_);
    if (measure_commit_cost_) {
      printf("*RESULT %s: commits: %d, %.2f ms/commit\n",
             test_name_.c_str(),
             num_commits_,
             total_commit_time_.InMillisecondsF() / num_commits_);
    }
  }

 protected:
  base::TimeTicks start_time_;
  int num_draws_;
  int num_commits_;
  std::string test_name_;
  base::TimeDelta elapsed_;
  FakeContentLayerClient fake_content_layer_client_;
  bool full_damage_each_frame_;
  bool animation_driven_drawing_;

  bool measure_commit_cost_;
  base::TimeTicks commit_start_time_;
  base::TimeDelta total_commit_time_;
};


class LayerTreeHostPerfTestJsonReader : public LayerTreeHostPerfTest {
 public:
  LayerTreeHostPerfTestJsonReader()
      : LayerTreeHostPerfTest() {
  }

  void ReadTestFile(std::string name) {
    test_name_ = name;
    base::FilePath test_data_dir;
    ASSERT_TRUE(PathService::Get(cc::DIR_TEST_DATA, &test_data_dir));
    base::FilePath json_file = test_data_dir.AppendASCII(name + ".json");
    ASSERT_TRUE(file_util::ReadFileToString(json_file, &json_));
  }

  virtual void BuildTree() OVERRIDE {
    gfx::Size viewport = gfx::Size(720, 1038);
    layer_tree_host()->SetViewportSize(viewport);
    scoped_refptr<Layer> root = ParseTreeFromJson(json_,
                                                  &fake_content_layer_client_);
    ASSERT_TRUE(root.get());
    layer_tree_host()->SetRootLayer(root);
  }

 private:
  std::string json_;
};

// Simulates a tab switcher scene with two stacks of 10 tabs each.
TEST_F(LayerTreeHostPerfTestJsonReader, TenTenSingleThread) {
  ReadTestFile("10_10_layer_tree");
  RunTest(false, false, false);
}

// Simulates a tab switcher scene with two stacks of 10 tabs each.
TEST_F(LayerTreeHostPerfTestJsonReader,
       TenTenSingleThread_FullDamageEachFrame) {
  full_damage_each_frame_ = true;
  ReadTestFile("10_10_layer_tree");
  RunTest(false, false, false);
}

// Invalidates a leaf layer in the tree on the main thread after every commit.
class LayerTreeHostPerfTestLeafInvalidates
    : public LayerTreeHostPerfTestJsonReader {
 public:
  virtual void BuildTree() OVERRIDE {
    LayerTreeHostPerfTestJsonReader::BuildTree();

    // Find a leaf layer.
    for (layer_to_invalidate_ = layer_tree_host()->root_layer();
         layer_to_invalidate_->children().size();
         layer_to_invalidate_ = layer_to_invalidate_->children()[0]) {}
  }

  virtual void DidCommitAndDrawFrame() OVERRIDE {
    if (TestEnded())
      return;

    static bool flip = true;
    layer_to_invalidate_->SetOpacity(flip ? 1.f : 0.5f);
    flip = !flip;
  }

 protected:
  Layer* layer_to_invalidate_;
};

// Simulates a tab switcher scene with two stacks of 10 tabs each. Invalidate a
// property on a leaf layer in the tree every commit.
TEST_F(LayerTreeHostPerfTestLeafInvalidates, TenTenSingleThread) {
  ReadTestFile("10_10_layer_tree");
  RunTest(false, false, false);
}

// Simulates main-thread scrolling on each frame.
class ScrollingLayerTreePerfTest : public LayerTreeHostPerfTestJsonReader {
 public:
  ScrollingLayerTreePerfTest()
      : LayerTreeHostPerfTestJsonReader() {
  }

  virtual void BuildTree() OVERRIDE {
    LayerTreeHostPerfTestJsonReader::BuildTree();
    scrollable_ = layer_tree_host()->root_layer()->children()[1];
    ASSERT_TRUE(scrollable_.get());
  }

  virtual void Layout() OVERRIDE {
    static const gfx::Vector2d delta = gfx::Vector2d(0, 10);
    scrollable_->SetScrollOffset(scrollable_->scroll_offset() + delta);
  }

 private:
  scoped_refptr<Layer> scrollable_;
};

TEST_F(ScrollingLayerTreePerfTest, LongScrollablePage) {
  ReadTestFile("long_scrollable_page");
  RunTest(false, false, false);
}

class ImplSidePaintingPerfTest : public LayerTreeHostPerfTestJsonReader {
 protected:
  // Run test with impl-side painting.
  void RunTestWithImplSidePainting() { RunTest(true, false, true); }
};

// Simulates a page with several large, transformed and animated layers.
TEST_F(ImplSidePaintingPerfTest, HeavyPage) {
  animation_driven_drawing_ = true;
  measure_commit_cost_ = true;
  ReadTestFile("heavy_layer_tree");
  RunTestWithImplSidePainting();
}

class PageScaleImplSidePaintingPerfTest : public ImplSidePaintingPerfTest {
 public:
  PageScaleImplSidePaintingPerfTest()
      : max_scale_(16.f), min_scale_(1.f / max_scale_) {}

  virtual void SetupTree() OVERRIDE {
    layer_tree_host()->SetPageScaleFactorAndLimits(1.f, min_scale_, max_scale_);
  }

  virtual void ApplyScrollAndScale(gfx::Vector2d scroll_delta,
                                   float scale_delta) OVERRIDE {
    float page_scale_factor = layer_tree_host()->page_scale_factor();
    page_scale_factor *= scale_delta;
    layer_tree_host()->SetPageScaleFactorAndLimits(
        page_scale_factor, min_scale_, max_scale_);
  }

  virtual void AnimateLayers(LayerTreeHostImpl* host_impl,
                             base::TimeTicks monotonic_time) OVERRIDE {
    if (!host_impl->pinch_gesture_active()) {
      host_impl->PinchGestureBegin();
      start_time_ = monotonic_time;
    }
    gfx::Point anchor(200, 200);

    float seconds = (monotonic_time - start_time_).InSecondsF();

    // Every half second, zoom from min scale to max scale.
    float interval = 0.5f;

    // Start time in the middle of the interval when zoom = 1.
    seconds += interval / 2.f;

    // Stack two ranges together to go up from min to max and down from
    // max to min in the next so as not to have a zoom discrepancy.
    float time_in_two_intervals = fmod(seconds, 2.f * interval) / interval;

    // Map everything to go from min to max between 0 and 1.
    float time_in_one_interval =
        time_in_two_intervals > 1.f ? 2.f - time_in_two_intervals
                                    : time_in_two_intervals;
    // Normalize time to -1..1.
    float normalized = 2.f * time_in_one_interval - 1.f;
    float scale_factor = std::abs(normalized) * (max_scale_ - 1.f) + 1.f;
    float total_scale = normalized < 0.f ? 1.f / scale_factor : scale_factor;

    float desired_delta =
        total_scale / host_impl->active_tree()->total_page_scale_factor();
    host_impl->PinchGestureUpdate(desired_delta, anchor);
  }

 private:
  float max_scale_;
  float min_scale_;
  base::TimeTicks start_time_;
};

TEST_F(PageScaleImplSidePaintingPerfTest, HeavyPage) {
  measure_commit_cost_ = true;
  ReadTestFile("heavy_layer_tree");
  RunTestWithImplSidePainting();
}

}  // namespace
}  // namespace cc
