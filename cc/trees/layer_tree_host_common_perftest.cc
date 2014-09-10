// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/layer_tree_host_common.h"

#include <sstream>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "base/strings/string_piece.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "cc/base/scoped_ptr_deque.h"
#include "cc/base/scoped_ptr_vector.h"
#include "cc/debug/lap_timer.h"
#include "cc/layers/layer.h"
#include "cc/output/bsp_tree.h"
#include "cc/quads/draw_polygon.h"
#include "cc/quads/draw_quad.h"
#include "cc/test/fake_content_layer_client.h"
#include "cc/test/fake_layer_tree_host_client.h"
#include "cc/test/layer_tree_json_parser.h"
#include "cc/test/layer_tree_test.h"
#include "cc/test/paths.h"
#include "cc/trees/layer_sorter.h"
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
          &update_list,
          0);
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
      DoCalcDrawPropertiesImpl(can_render_to_separate_surface,
                               max_texture_size,
                               active_tree,
                               host_impl);

      timer_.NextLap();
    } while (!timer_.HasTimeLimitExpired());

    EndTest();
  }

  void DoCalcDrawPropertiesImpl(bool can_render_to_separate_surface,
                                int max_texture_size,
                                LayerTreeImpl* active_tree,
                                LayerTreeHostImpl* host_impl) {
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
        &update_list,
        0);
    LayerTreeHostCommon::CalculateDrawProperties(&inputs);
  }
};

class LayerSorterMainTest : public CalcDrawPropsImplTest {
 public:
  void RunSortLayers() { RunTest(false, false, false); }

  virtual void BeginTest() OVERRIDE { PostSetNeedsCommitToMainThread(); }

  virtual void DrawLayersOnThread(LayerTreeHostImpl* host_impl) OVERRIDE {
    LayerTreeImpl* active_tree = host_impl->active_tree();
    // First build the tree and then we'll start running tests on layersorter
    // itself
    bool can_render_to_separate_surface = true;
    int max_texture_size = 8096;
    DoCalcDrawPropertiesImpl(can_render_to_separate_surface,
                             max_texture_size,
                             active_tree,
                             host_impl);

    // Behaviour of this test is different from that of sorting in practice.
    // In this case, all layers that exist in any 3D context are put into a list
    // and are sorted as one big 3D context instead of several smaller ones.
    BuildLayerImplList(active_tree->root_layer(), &base_list_);
    timer_.Reset();
    do {
      // Here we'll move the layers into a LayerImpl list of their own to be
      // sorted so we don't have a sorted list for every run after the first
      LayerImplList test_list = base_list_;
      layer_sorter_.Sort(test_list.begin(), test_list.end());
      timer_.NextLap();
    } while (!timer_.HasTimeLimitExpired());

    EndTest();
  }

  void BuildLayerImplList(LayerImpl* layer, LayerImplList* list) {
    if (layer->Is3dSorted()) {
      list->push_back(layer);
    }

    for (size_t i = 0; i < layer->children().size(); i++) {
      BuildLayerImplList(layer->children()[i], list);
    }
  }

 private:
  LayerImplList base_list_;
  LayerSorter layer_sorter_;
};

class BspTreePerfTest : public LayerSorterMainTest {
 public:
  void RunSortLayers() { RunTest(false, false, false); }

  void SetNumberOfDuplicates(int num_duplicates) {
    num_duplicates_ = num_duplicates;
  }

  virtual void BeginTest() OVERRIDE { PostSetNeedsCommitToMainThread(); }

  virtual void DrawLayersOnThread(LayerTreeHostImpl* host_impl) OVERRIDE {
    LayerTreeImpl* active_tree = host_impl->active_tree();
    // First build the tree and then we'll start running tests on layersorter
    // itself
    bool can_render_to_separate_surface = true;
    int max_texture_size = 8096;
    DoCalcDrawPropertiesImpl(can_render_to_separate_surface,
                             max_texture_size,
                             active_tree,
                             host_impl);

    LayerImplList base_list;
    BuildLayerImplList(active_tree->root_layer(), &base_list);

    int polygon_counter = 0;
    ScopedPtrVector<DrawPolygon> polygon_list;
    for (LayerImplList::iterator it = base_list.begin(); it != base_list.end();
         ++it) {
      DrawPolygon* draw_polygon =
          new DrawPolygon(NULL,
                          gfx::RectF((*it)->content_bounds()),
                          (*it)->draw_transform(),
                          polygon_counter++);
      polygon_list.push_back(scoped_ptr<DrawPolygon>(draw_polygon));
    }

    timer_.Reset();
    do {
      ScopedPtrDeque<DrawPolygon> test_list;
      for (int i = 0; i < num_duplicates_; i++) {
        for (size_t i = 0; i < polygon_list.size(); i++) {
          test_list.push_back(polygon_list[i]->CreateCopy());
        }
      }
      BspTree bsp_tree(&test_list);
      timer_.NextLap();
    } while (!timer_.HasTimeLimitExpired());

    EndTest();
  }

 private:
  int num_duplicates_;
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

TEST_F(LayerSorterMainTest, LayerSorterCubes) {
  SetTestName("layer_sort_cubes");
  ReadTestFile("layer_sort_cubes");
  RunSortLayers();
}

TEST_F(LayerSorterMainTest, LayerSorterRubik) {
  SetTestName("layer_sort_rubik");
  ReadTestFile("layer_sort_rubik");
  RunSortLayers();
}

TEST_F(BspTreePerfTest, BspTreeCubes) {
  SetTestName("bsp_tree_cubes");
  SetNumberOfDuplicates(1);
  ReadTestFile("layer_sort_cubes");
  RunSortLayers();
}

TEST_F(BspTreePerfTest, BspTreeRubik) {
  SetTestName("bsp_tree_rubik");
  SetNumberOfDuplicates(1);
  ReadTestFile("layer_sort_rubik");
  RunSortLayers();
}

TEST_F(BspTreePerfTest, BspTreeCubes_2) {
  SetTestName("bsp_tree_cubes_2");
  SetNumberOfDuplicates(2);
  ReadTestFile("layer_sort_cubes");
  RunSortLayers();
}

TEST_F(BspTreePerfTest, BspTreeCubes_4) {
  SetTestName("bsp_tree_cubes_4");
  SetNumberOfDuplicates(4);
  ReadTestFile("layer_sort_cubes");
  RunSortLayers();
}

}  // namespace
}  // namespace cc
