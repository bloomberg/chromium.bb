// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/layer_tree_host_common.h"

#include <sstream>

#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/strings/string_piece.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "cc/layers/layer.h"
#include "cc/test/fake_content_layer_client.h"
#include "cc/test/fake_layer_tree_host_client.h"
#include "cc/test/lap_timer.h"
#include "cc/test/layer_tree_json_parser.h"
#include "cc/test/layer_tree_test.h"
#include "cc/test/paths.h"
#include "cc/trees/layer_tree_impl.h"
#include "testing/perf/perf_test.h"

namespace cc {
namespace {

static const int kTimeLimitMillis = 2000;
static const int kWarmupRuns = 5;
static const int kTimeCheckInterval = 10;

class LayerTreeHostCommonPerfTest : public LayerTreeTest {
 public:
  LayerTreeHostCommonPerfTest()
      : timer_(kWarmupRuns,
               base::TimeDelta::FromMilliseconds(kTimeLimitMillis),
               kTimeCheckInterval) {}

  void ReadTestFile(const std::string& name) {
    base::FilePath test_data_dir;
    ASSERT_TRUE(PathService::Get(CCPaths::DIR_TEST_DATA, &test_data_dir));
    base::FilePath json_file = test_data_dir.AppendASCII(name + ".json");
    ASSERT_TRUE(base::ReadFileToString(json_file, &json_));
  }

  virtual void SetupTree() OVERRIDE {
    gfx::Size viewport = gfx::Size(720, 1038);
    layer_tree_host()->SetViewportSize(viewport);
    scoped_refptr<Layer> root =
        ParseTreeFromJson(json_, &content_layer_client_);
    ASSERT_TRUE(root.get());
    layer_tree_host()->SetRootLayer(root);
  }

  void SetTestName(const std::string& name) { test_name_ = name; }

  virtual void AfterTest() OVERRIDE {
    CHECK(!test_name_.empty()) << "Must SetTestName() before TearDown().";
    perf_test::PrintResult("calc_draw_props_time",
                           "",
                           test_name_,
                           1000 * timer_.MsPerLap(),
                           "us",
                           true);
  }

 protected:
  FakeContentLayerClient content_layer_client_;
  LapTimer timer_;
  std::string test_name_;
  std::string json_;
};

class CalcDrawPropsMainTest : public LayerTreeHostCommonPerfTest {
 public:
  void RunCalcDrawProps() {
    RunTest(false, false, false);
  }

  virtual void BeginTest() OVERRIDE {
    timer_.Reset();

    do {
      bool can_render_to_separate_surface = true;
      int max_texture_size = 8096;
      RenderSurfaceLayerList update_list;
      LayerTreeHostCommon::CalcDrawPropsMainInputs inputs(
          layer_tree_host()->root_layer(),
          layer_tree_host()->device_viewport_size(),
          gfx::Transform(),
          layer_tree_host()->device_scale_factor(),
          layer_tree_host()->page_scale_factor(),
          layer_tree_host()->page_scale_layer(),
          max_texture_size,
          layer_tree_host()->settings().can_use_lcd_text,
          can_render_to_separate_surface,
          layer_tree_host()
              ->settings()
              .layer_transforms_should_scale_layer_contents,
          &update_list);
      LayerTreeHostCommon::CalculateDrawProperties(&inputs);

      timer_.NextLap();
    } while (!timer_.HasTimeLimitExpired());

    EndTest();
  }
};

class CalcDrawPropsImplTest : public LayerTreeHostCommonPerfTest {
 public:
  void RunCalcDrawProps() {
    RunTestWithImplSidePainting();
  }

  virtual void BeginTest() OVERRIDE {
    PostSetNeedsCommitToMainThread();
  }

  virtual void DrawLayersOnThread(LayerTreeHostImpl* host_impl) OVERRIDE {
    timer_.Reset();
    LayerTreeImpl* active_tree = host_impl->active_tree();

    do {
      bool can_render_to_separate_surface = true;
      int max_texture_size = 8096;
      LayerImplList update_list;
      LayerTreeHostCommon::CalcDrawPropsImplInputs inputs(
          active_tree->root_layer(),
          active_tree->DrawViewportSize(),
          host_impl->DrawTransform(),
          active_tree->device_scale_factor(),
          active_tree->total_page_scale_factor(),
          active_tree->InnerViewportContainerLayer(),
          max_texture_size,
          host_impl->settings().can_use_lcd_text,
          can_render_to_separate_surface,
          host_impl->settings().layer_transforms_should_scale_layer_contents,
          &update_list);
      LayerTreeHostCommon::CalculateDrawProperties(&inputs);

      timer_.NextLap();
    } while (!timer_.HasTimeLimitExpired());

    EndTest();
  }
};

TEST_F(CalcDrawPropsMainTest, TenTen) {
  SetTestName("10_10_main_thread");
  ReadTestFile("10_10_layer_tree");
  RunCalcDrawProps();
}

TEST_F(CalcDrawPropsMainTest, HeavyPage) {
  SetTestName("heavy_page_main_thread");
  ReadTestFile("heavy_layer_tree");
  RunCalcDrawProps();
}

TEST_F(CalcDrawPropsMainTest, TouchRegionLight) {
  SetTestName("touch_region_light_main_thread");
  ReadTestFile("touch_region_light");
  RunCalcDrawProps();
}

TEST_F(CalcDrawPropsMainTest, TouchRegionHeavy) {
  SetTestName("touch_region_heavy_main_thread");
  ReadTestFile("touch_region_heavy");
  RunCalcDrawProps();
}

TEST_F(CalcDrawPropsImplTest, TenTen) {
  SetTestName("10_10");
  ReadTestFile("10_10_layer_tree");
  RunCalcDrawProps();
}

TEST_F(CalcDrawPropsImplTest, HeavyPage) {
  SetTestName("heavy_page");
  ReadTestFile("heavy_layer_tree");
  RunCalcDrawProps();
}

TEST_F(CalcDrawPropsImplTest, TouchRegionLight) {
  SetTestName("touch_region_light");
  ReadTestFile("touch_region_light");
  RunCalcDrawProps();
}

TEST_F(CalcDrawPropsImplTest, TouchRegionHeavy) {
  SetTestName("touch_region_heavy");
  ReadTestFile("touch_region_heavy");
  RunCalcDrawProps();
}

}  // namespace
}  // namespace cc
