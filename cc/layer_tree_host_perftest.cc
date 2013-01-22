// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layer_tree_host.h"

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/json/json_reader.h"
#include "base/path_service.h"
#include "base/string_piece.h"
#include "cc/content_layer.h"
#include "cc/nine_patch_layer.h"
#include "cc/solid_color_layer.h"
#include "cc/test/fake_content_layer_client.h"
#include "cc/test/layer_tree_test_common.h"
#include "cc/test/paths.h"

namespace cc {
namespace {

static const int kTimeLimitMillis = 2000;

class LayerTreeHostPerfTest : public ThreadedTest {
 public:
  LayerTreeHostPerfTest()
      : num_draws_(0) {
    fake_delegate_.setPaintAllOpaque(true);
  }

  virtual void beginTest() OVERRIDE {
    buildTree();
    start_time_ = base::TimeTicks::HighResNow();
    postSetNeedsCommitToMainThread();
  }

  virtual void drawLayersOnThread(LayerTreeHostImpl* impl) OVERRIDE {
    ++num_draws_;
    if ((base::TimeTicks::HighResNow() - start_time_) >=
        base::TimeDelta::FromMilliseconds(kTimeLimitMillis))
      endTest();
    else
      impl->setNeedsRedraw();
  }

  virtual void buildTree() {}

  virtual void afterTest() OVERRIDE {
    base::TimeDelta elapsed = base::TimeTicks::HighResNow() - start_time_;
    // Format matches chrome/test/perf/perf_test.h:PrintResult
    printf("*RESULT %s: frames= %.2f runs/s\n",
           test_name_.c_str(),
           num_draws_ / elapsed.InSecondsF());
  }

  scoped_refptr<Layer> CreateLayer(float x, float y, int width, int height) {
    scoped_refptr<Layer> layer = Layer::create();
    layer->setAnchorPoint(gfx::Point());
    layer->setPosition(gfx::PointF(x, y));
    layer->setBounds(gfx::Size(width, height));
    return layer;
  }

  scoped_refptr<ContentLayer> CreateContentLayer(float x, float y, int width, int height, bool drawable=true) {
    scoped_refptr<ContentLayer> layer = ContentLayer::create(&fake_delegate_);
    layer->setAnchorPoint(gfx::Point());
    layer->setPosition(gfx::PointF(x, y));
    layer->setBounds(gfx::Size(width, height));
    layer->setIsDrawable(drawable);
    return layer;
  }

  scoped_refptr<SolidColorLayer> CreateColorLayer(float x, float y, int width, int height, bool drawable=true) {
    scoped_refptr<SolidColorLayer> layer = SolidColorLayer::create();
    layer->setAnchorPoint(gfx::Point());
    layer->setPosition(gfx::PointF(x, y));
    layer->setBounds(gfx::Size(width, height));
    layer->setIsDrawable(drawable);
    return layer;
  }

  scoped_refptr<NinePatchLayer> CreateDecorationLayer(float x, float y, int width, int height) {
    return CreateDecorationLayer(x, y, width, height, gfx::Size(width + 1, height + 1), gfx::Rect(0, 0, width, height));
  }

  scoped_refptr<NinePatchLayer> CreateDecorationLayer(float x, float y, int width, int height, gfx::Size image_size, gfx::Rect aperture, bool drawable=true) {
    scoped_refptr<NinePatchLayer> layer = NinePatchLayer::create();
    layer->setAnchorPoint(gfx::Point());
    layer->setPosition(gfx::PointF(x, y));
    layer->setBounds(gfx::Size(width, height));
    layer->setIsDrawable(drawable);

    SkBitmap bitmap;
    bitmap.setConfig(
        SkBitmap::kARGB_8888_Config, image_size.width(), image_size.height());
    bitmap.allocPixels(NULL, NULL);
    layer->setBitmap(bitmap, aperture);

    return layer;
  }

  scoped_refptr<Layer> addChild(scoped_refptr<Layer> parent, scoped_refptr<Layer> child) {
    parent->addChild(child);
    return child;
  }

 protected:
  base::TimeTicks start_time_;
  int num_draws_;
  std::string test_name_;
  FakeContentLayerClient fake_delegate_;
};

class LayerTreeHostPerfTestSevenTabSwitcher : public LayerTreeHostPerfTest {
 public:
  LayerTreeHostPerfTestSevenTabSwitcher()
      : LayerTreeHostPerfTest() {
    test_name_ = "SevenTabSwitcher";
  }

  virtual void buildTree() OVERRIDE {
    scoped_refptr<Layer> root = CreateLayer(0, 0, 720, 1038); // 1
    scoped_refptr<Layer> layer;

    gfx::Transform down_scale_matrix;
    down_scale_matrix.Scale(0.747, 0.747);

    layer = addChild(root, CreateLayer(0, 0, 0, 0)); // 2

    layer = addChild(root, CreateLayer(628, 15, 0, 0)); // 5
    layer = addChild(root, CreateDecorationLayer(564, -49, 665, 274)); // 13
    layer = addChild(root, CreateDecorationLayer(-16, -16, 569, 807)); // 12
    layer = addChild(root, CreateColorLayer(628, 15, 720, 1038)); // 11
    layer->setTransform(down_scale_matrix);
    layer = addChild(root, CreateContentLayer(628, 16, 720, 1038)); // 34
    layer->setTransform(down_scale_matrix);

    layer = addChild(root, CreateLayer(628, 15, 0, 0)); // 6
    layer = addChild(root, CreateDecorationLayer(612, -1, 569, 807)); // 10
    layer = addChild(root, CreateDecorationLayer(827.135986f, -1, 354, 96)); // 9
    layer = addChild(root, CreateContentLayer(628, 15, 0, 0)); // 8
    layer = addChild(root, CreateContentLayer(627.418f, 15, 0, 0)); // 7

    layer = addChild(root, CreateLayer(628, 161, 0, 0)); // 74
    layer = addChild(root, CreateDecorationLayer(564, 97, 665, 383)); // 82
    layer = addChild(root, CreateDecorationLayer(0, 0, 569, 807)); // 81
    layer = addChild(root, CreateColorLayer(628, 161, 720, 1038)); // 80
    layer->setTransform(down_scale_matrix);
    layer = addChild(root, CreateContentLayer(628, 161, 720, 1038)); // 44
    layer->setTransform(down_scale_matrix);

    layer = addChild(root, CreateLayer(628, 161, 0, 0)); // 75
    layer = addChild(root, CreateDecorationLayer(612, 145, 569, 807)); // 79
    layer = addChild(root, CreateDecorationLayer(827.135986f, 145, 354, 96)); // 78
    layer = addChild(root, CreateContentLayer(628, 161, 0, 0)); // 77
    layer = addChild(root, CreateContentLayer(627.418f, 161, 0, 0)); // 76

    layer = addChild(root, CreateLayer(628, 417, 0, 0)); // 83
    layer = addChild(root, CreateDecorationLayer(564, 353, 665, 445)); // 91
    layer = addChild(root, CreateDecorationLayer(0, 0, 569, 807)); // 90
    layer = addChild(root, CreateColorLayer(628, 417, 720, 1038)); // 89
    layer->setTransform(down_scale_matrix);
    layer = addChild(root, CreateContentLayer(628, 417, 720, 1038)); // 54
    layer->setTransform(down_scale_matrix);

    layer = addChild(root, CreateLayer(628, 417, 0, 0)); // 84
    layer = addChild(root, CreateDecorationLayer(612, 401, 569, 807)); // 88
    layer = addChild(root, CreateDecorationLayer(827.135986f, 401, 354, 96)); // 87
    layer = addChild(root, CreateContentLayer(628, 417, 0, 0)); // 86
    layer = addChild(root, CreateContentLayer(627.418f, 417, 0, 0)); // 85

    layer = addChild(root, CreateLayer(628, 735, 0, 0)); // 92
    layer = addChild(root, CreateDecorationLayer(564, 671, 665, 439)); // 100
    layer = addChild(root, CreateDecorationLayer(0, 0, 569, 807)); // 99
    layer = addChild(root, CreateColorLayer(628, 735, 720, 1038)); // 98
    layer->setTransform(down_scale_matrix);
    layer = addChild(root, CreateContentLayer(628, 735, 720, 1038)); // 64
    layer->setTransform(down_scale_matrix);

    layer = addChild(root, CreateLayer(628, 735, 0, 0)); // 93
    layer = addChild(root, CreateDecorationLayer(612, 719, 569, 807)); // 97
    layer = addChild(root, CreateDecorationLayer(827.135986f, 719, 354, 96)); // 96
    layer = addChild(root, CreateContentLayer(628, 735, 0, 0)); // 95
    layer = addChild(root, CreateContentLayer(627.418f, 735, 0, 0)); // 94

    layer = addChild(root, CreateLayer(30, 15, 0, 0)); // 101
    layer = addChild(root, CreateDecorationLayer(-34, -49, 665, 337)); // 109
    layer = addChild(root, CreateDecorationLayer(0, 0, 569, 807)); // 108
    layer = addChild(root, CreateColorLayer(30, 15, 720, 1038)); // 107
    layer->setTransform(down_scale_matrix);
    layer = addChild(root, CreateContentLayer(30, 15, 0, 0)); // 3
    layer->setTransform(down_scale_matrix);

    layer = addChild(root, CreateLayer(30, 15, 0, 0)); // 102
    layer = addChild(root, CreateDecorationLayer(14, -1, 569, 807)); // 106
    layer = addChild(root, CreateDecorationLayer(229.135986f, -1, 354, 96)); // 105
    layer = addChild(root, CreateContentLayer(30, 15, 0, 0)); // 104
    layer = addChild(root, CreateContentLayer(30, 15, 0, 0)); // 103

    layer = addChild(root, CreateLayer(30, 227, 0, 0)); // 110
    layer = addChild(root, CreateDecorationLayer(-34, 163, 665, 517)); // 118
    layer = addChild(root, CreateDecorationLayer(0, 0, 569, 807)); // 117
    layer = addChild(root, CreateColorLayer(30, 227, 720, 1038)); // 116
    layer->setTransform(down_scale_matrix);
    layer = addChild(root, CreateContentLayer(30, 227, 720, 1038)); // 4
    layer->setTransform(down_scale_matrix);

    layer = addChild(root, CreateLayer(30, 227, 0, 0)); // 111
    layer = addChild(root, CreateDecorationLayer(14, 211, 569, 807)); // 115
    layer = addChild(root, CreateDecorationLayer(229.135986f, 211, 354, 96)); // 114
    layer = addChild(root, CreateContentLayer(30, 227, 0, 0)); // 113
    layer = addChild(root, CreateContentLayer(30, 227, 0, 0)); // 112

    layer = addChild(root, CreateLayer(30, 617, 0, 0)); // 119
    layer = addChild(root, CreateDecorationLayer(-34, 553, 665, 559)); // 127
    layer = addChild(root, CreateDecorationLayer(136.349190f, 566.524940f, 569, 807)); // 126
    layer = addChild(root, CreateColorLayer(30, 617, 720, 1038)); // 125
    layer->setTransform(down_scale_matrix);
    layer = addChild(root, CreateContentLayer(30, 617, 720, 1038)); // 14
    layer->setTransform(down_scale_matrix);

    layer = addChild(root, CreateLayer(30, 617, 0, 0)); // 120
    layer = addChild(root, CreateDecorationLayer(14, 601, 569, 807)); // 124
    layer = addChild(root, CreateDecorationLayer(229.135986f, 601, 354, 96)); // 123
    layer = addChild(root, CreateContentLayer(30, 617, 0, 0)); // 122
    layer = addChild(root, CreateContentLayer(30, 617, 0, 0)); // 121

    m_layerTreeHost->setViewportSize(gfx::Size(720, 1038), gfx::Size(720, 1038));
    m_layerTreeHost->setRootLayer(root);
  }
};

TEST_F(LayerTreeHostPerfTestSevenTabSwitcher, runSingleThread) {
  runTest(false);
}

class LayerTreeHostPerfTestJsonReader : public LayerTreeHostPerfTest {
 public:
  LayerTreeHostPerfTestJsonReader()
      : LayerTreeHostPerfTest() {
  }

  void readTestFile(std::string name) {
    test_name_ = name;
    FilePath test_data_dir;
    ASSERT_TRUE(PathService::Get(cc::DIR_TEST_DATA, &test_data_dir));
    FilePath json_file = test_data_dir.AppendASCII(name + ".json");
    std::string json;
    ASSERT_TRUE(file_util::ReadFileToString(json_file, &json));
    tree_.reset(base::JSONReader::Read(json));
    ASSERT_TRUE(tree_);
  }

  scoped_refptr<Layer> parseLayer(base::Value* val) {
    DictionaryValue* dict;
    bool success = true;
    success &= val->GetAsDictionary(&dict);
    std::string layer_type;
    success &= dict->GetString("LayerType", &layer_type);
    ListValue* list;
    success &= dict->GetList("Bounds", &list);
    int width, height;
    success &= list->GetInteger(0, &width);
    success &= list->GetInteger(1, &height);
    success &= dict->GetList("Position", &list);
    double position_x, position_y;
    success &= list->GetDouble(0, &position_x);
    success &= list->GetDouble(1, &position_y);

    bool draws_content;
    success &= dict->GetBoolean("DrawsContent", &draws_content);

    scoped_refptr<Layer> new_layer;
    if (layer_type == "SolidColorLayer") {
      new_layer = CreateColorLayer(position_x, position_y, width, height, draws_content);
    } else if (layer_type == "ContentLayer") {
      new_layer = CreateContentLayer(position_x, position_y, width, height, draws_content);
    } else if (layer_type == "NinePatchLayer") {
      success &= dict->GetList("ImageAperture", &list);
      int aperture_x, aperture_y, aperture_width, aperture_height;
      success &= list->GetInteger(0, &aperture_x);
      success &= list->GetInteger(1, &aperture_y);
      success &= list->GetInteger(2, &aperture_width);
      success &= list->GetInteger(3, &aperture_height);

      success &= dict->GetList("ImageBounds", &list);
      int image_width, image_height;
      success &= list->GetInteger(0, &image_width);
      success &= list->GetInteger(1, &image_height);

      new_layer = CreateDecorationLayer(
          position_x, position_y, width, height,
          gfx::Size(image_width, image_height),
          gfx::Rect(aperture_x, aperture_y, aperture_width, aperture_height),
          draws_content);

    } else { // Type "Layer" or "unknown"
      new_layer = CreateLayer(position_x, position_y, width, height);
    }

    double opacity;
    if (dict->GetDouble("Opacity", &opacity))
      new_layer->setOpacity(opacity);

    success &= dict->GetList("DrawTransform", &list);
    double transform[16];
    for (int i = 0; i < 16; ++i)
      success &= list->GetDouble(i, &transform[i]);

    gfx::Transform gfxTransform;
    gfxTransform.matrix().setColMajord(transform);
    new_layer->setTransform(gfxTransform);

    success &= dict->GetList("Children", &list);
    for (ListValue::const_iterator it = list->begin();
         it != list->end(); ++it) {
      new_layer->addChild(parseLayer(*it));
    }

    if (!success)
      ADD_FAILURE() << "Could not parse json data";

    return new_layer;
  }

  virtual void buildTree() OVERRIDE {
    gfx::Size viewport = gfx::Size(720, 1038);
    m_layerTreeHost->setViewportSize(viewport, viewport);
    m_layerTreeHost->setRootLayer(parseLayer(tree_.get()));
  }

 private:
  scoped_ptr<base::Value> tree_;
};

TEST_F(LayerTreeHostPerfTestJsonReader, tenTenSingleThread) {
  readTestFile("10_10_layer_tree");
  runTest(false);
}

}  // namespace
}  // namespace cc
