// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layer_tree_host.h"

#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/string_piece.h"
#include "cc/content_layer.h"
#include "cc/nine_patch_layer.h"
#include "cc/solid_color_layer.h"
#include "cc/test/fake_content_layer_client.h"
#include "cc/test/layer_tree_json_parser.h"
#include "cc/test/layer_tree_test_common.h"
#include "cc/test/paths.h"

namespace cc {
namespace {

static const int kTimeLimitMillis = 2000;
static const int kWarmupRuns = 5;
static const int kTimeCheckInterval = 10;

class LayerTreeHostPerfTest : public ThreadedTest {
 public:
  LayerTreeHostPerfTest()
      : num_draws_(0) {
    fake_content_layer_client_.setPaintAllOpaque(true);
  }

  virtual void beginTest() OVERRIDE {
    buildTree();
    postSetNeedsCommitToMainThread();
  }

  virtual void drawLayersOnThread(LayerTreeHostImpl* impl) OVERRIDE {
    ++num_draws_;
    if (num_draws_ == kWarmupRuns)
      start_time_ = base::TimeTicks::HighResNow();

    if (!start_time_.is_null() && (num_draws_ % kTimeCheckInterval) == 0) {
      base::TimeDelta elapsed = base::TimeTicks::HighResNow() - start_time_;
      if (elapsed >= base::TimeDelta::FromMilliseconds(kTimeLimitMillis)) {
        elapsed_ = elapsed;
        endTest();
        return;
      }
    }
    impl->setNeedsRedraw();
  }

  virtual void buildTree() {}

  virtual void afterTest() OVERRIDE {
    // Format matches chrome/test/perf/perf_test.h:PrintResult
    printf("*RESULT %s: frames= %.2f runs/s\n",
           test_name_.c_str(),
           num_draws_ / elapsed_.InSecondsF());
  }

 protected:
  base::TimeTicks start_time_;
  int num_draws_;
  std::string test_name_;
  base::TimeDelta elapsed_;
  FakeContentLayerClient fake_content_layer_client_;
};


class LayerTreeHostPerfTestJsonReader : public LayerTreeHostPerfTest {
 public:
  LayerTreeHostPerfTestJsonReader()
      : LayerTreeHostPerfTest() {
  }

  void readTestFile(std::string name) {
    test_name_ = name;
    base::FilePath test_data_dir;
    ASSERT_TRUE(PathService::Get(cc::DIR_TEST_DATA, &test_data_dir));
    base::FilePath json_file = test_data_dir.AppendASCII(name + ".json");
    ASSERT_TRUE(file_util::ReadFileToString(json_file, &json_));
  }

  virtual void buildTree() OVERRIDE {
    gfx::Size viewport = gfx::Size(720, 1038);
    m_layerTreeHost->setViewportSize(viewport, viewport);
    scoped_refptr<Layer> root = ParseTreeFromJson(json_,
                                                  &fake_content_layer_client_);
    ASSERT_TRUE(root.get());
    m_layerTreeHost->setRootLayer(root);
  }

 private:
  std::string json_;
};

// Simulates a tab switcher scene with two stacks of 10 tabs each.
TEST_F(LayerTreeHostPerfTestJsonReader, TenTenSingleThread) {
  readTestFile("10_10_layer_tree");
  runTest(false);
}

// Simulates main-thread scrolling on each frame.
class ScrollingLayerTreePerfTest : public LayerTreeHostPerfTestJsonReader {
 public:
  ScrollingLayerTreePerfTest()
      : LayerTreeHostPerfTestJsonReader() {
  }

  virtual void buildTree() OVERRIDE {
    LayerTreeHostPerfTestJsonReader::buildTree();
    m_scrollable = m_layerTreeHost->rootLayer()->children()[1];
    ASSERT_TRUE(m_scrollable);
  }

  virtual void layout() OVERRIDE {
    static const gfx::Vector2d delta = gfx::Vector2d(0, 10);
    m_scrollable->setScrollOffset(m_scrollable->scrollOffset() + delta);
  }

 private:
  scoped_refptr<Layer> m_scrollable;
};

TEST_F(ScrollingLayerTreePerfTest, LongScrollablePage) {
  readTestFile("long_scrollable_page");
  runTest(false);
}

}  // namespace
}  // namespace cc
