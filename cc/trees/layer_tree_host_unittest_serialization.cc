// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/layer_tree_host.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "cc/layers/empty_content_layer_client.h"
#include "cc/layers/heads_up_display_layer.h"
#include "cc/layers/layer.h"
#include "cc/proto/layer.pb.h"
#include "cc/proto/layer_tree_host.pb.h"
#include "cc/test/fake_image_serialization_processor.h"
#include "cc/test/fake_layer_tree_host.h"
#include "cc/test/fake_layer_tree_host_client.h"
#include "cc/test/fake_picture_layer.h"
#include "cc/test/fake_recording_source.h"
#include "cc/test/layer_tree_test.h"
#include "cc/test/skia_common.h"
#include "cc/test/test_task_graph_runner.h"
#include "cc/trees/layer_tree.h"
#include "cc/trees/layer_tree_host_common.h"
#include "cc/trees/layer_tree_settings.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace cc {

namespace {
std::unique_ptr<FakeRecordingSource> CreateRecordingSource(
    const gfx::Rect& viewport) {
  gfx::Rect layer_rect(viewport.right(), viewport.bottom());
  std::unique_ptr<FakeRecordingSource> recording_source =
      FakeRecordingSource::CreateRecordingSource(viewport, layer_rect.size());
  return recording_source;
}

scoped_refptr<FakePictureLayer> CreatePictureLayer() {
  gfx::Rect recorded_viewport(0, 0, 256, 256);

  std::unique_ptr<FakeRecordingSource> recording_source =
      CreateRecordingSource(recorded_viewport);
  recording_source->SetDisplayListUsesCachedPicture(false);

  SkPaint simple_paint;
  simple_paint.setColor(SkColorSetARGB(255, 12, 23, 34));
  recording_source->add_draw_rect_with_paint(gfx::Rect(0, 0, 256, 256),
                                             simple_paint);
  recording_source->SetGenerateDiscardableImagesMetadata(true);
  recording_source->Rerecord();

  ContentLayerClient* client = EmptyContentLayerClient::GetInstance();
  return FakePictureLayer::CreateWithRecordingSource(
      client, std::move(recording_source));
}
}  // namespace

class LayerTreeHostSerializationTest : public testing::Test {
 public:
  LayerTreeHostSerializationTest()
      : image_serialization_processor_(
            base::MakeUnique<FakeImageSerializationProcessor>()) {}

 protected:
  void SetUp() override {
    LayerTreeSettings settings;
    layer_tree_host_src_ = FakeLayerTreeHost::Create(
        &client_src_, &task_graph_runner_src_, settings,
        CompositorMode::SINGLE_THREADED, image_serialization_processor_.get());
    layer_tree_host_dst_ = FakeLayerTreeHost::Create(
        &client_dst_, &task_graph_runner_dst_, settings,
        CompositorMode::SINGLE_THREADED, image_serialization_processor_.get());
    layer_tree_host_src_->InitializePictureCacheForTesting();
    layer_tree_host_dst_->InitializePictureCacheForTesting();
  }

  void TearDown() override {
    // Need to reset |in_paint_layer_contents_| to tear down.

    layer_tree_host_src_->GetLayerTree()->in_paint_layer_contents_ = false;
    layer_tree_host_dst_->GetLayerTree()->in_paint_layer_contents_ = false;

    // Need to reset LayerTreeHost pointers before tear down.
    layer_tree_host_src_ = nullptr;
    layer_tree_host_dst_ = nullptr;
  }

  void VerifyHostHasAllExpectedLayersInTree(Layer* root_layer) {
    LayerTreeHostCommon::CallFunctionForEveryLayer(
        root_layer->GetLayerTree(), [root_layer](Layer* layer) {
          DCHECK(layer->GetLayerTreeHostForTesting());
          EXPECT_EQ(layer, layer->GetLayerTree()->LayerById(layer->id()));
        });
  }

  void VerifySerializationAndDeserialization() {
    proto::LayerTreeHost proto;
    LayerTree* layer_tree_src = layer_tree_host_src_->GetLayerTree();
    LayerTree* layer_tree_dst = layer_tree_host_dst_->GetLayerTree();

    std::unordered_set<Layer*> layers_that_should_push_properties_src =
        layer_tree_src->LayersThatShouldPushProperties();
    std::vector<std::unique_ptr<SwapPromise>> swap_promises;
    layer_tree_host_src_->ToProtobufForCommit(&proto, &swap_promises);
    layer_tree_host_dst_->FromProtobufForCommit(proto);

    EXPECT_EQ(layer_tree_src->needs_full_tree_sync_,
              layer_tree_dst->needs_full_tree_sync_);
    EXPECT_EQ(layer_tree_src->needs_meta_info_recomputation_,
              layer_tree_dst->needs_meta_info_recomputation_);
    EXPECT_EQ(layer_tree_host_src_->source_frame_number_,
              layer_tree_host_dst_->source_frame_number_);
    EXPECT_EQ(layer_tree_host_src_->root_layer()->id(),
              layer_tree_host_dst_->root_layer()->id());
    EXPECT_EQ(layer_tree_host_dst_.get(),
              layer_tree_dst->inputs_.root_layer->GetLayerTreeHostForTesting());
    EXPECT_EQ(layer_tree_src->inputs_.root_layer->double_sided(),
              layer_tree_dst->inputs_.root_layer->double_sided());
    EXPECT_EQ(layer_tree_src->inputs_.device_viewport_size,
              layer_tree_dst->inputs_.device_viewport_size);
    EXPECT_EQ(layer_tree_src->inputs_.device_scale_factor,
              layer_tree_dst->inputs_.device_scale_factor);
    EXPECT_EQ(layer_tree_src->inputs_.painted_device_scale_factor,
              layer_tree_dst->inputs_.painted_device_scale_factor);
    EXPECT_EQ(layer_tree_src->inputs_.page_scale_factor,
              layer_tree_dst->inputs_.page_scale_factor);
    EXPECT_EQ(layer_tree_src->inputs_.min_page_scale_factor,
              layer_tree_dst->inputs_.min_page_scale_factor);
    EXPECT_EQ(layer_tree_src->inputs_.max_page_scale_factor,
              layer_tree_dst->inputs_.max_page_scale_factor);
    EXPECT_EQ(layer_tree_src->elastic_overscroll_,
              layer_tree_dst->elastic_overscroll_);
    EXPECT_EQ(layer_tree_host_src_->has_gpu_rasterization_trigger_,
              layer_tree_host_dst_->has_gpu_rasterization_trigger_);
    EXPECT_EQ(layer_tree_host_src_->content_is_suitable_for_gpu_rasterization_,
              layer_tree_host_dst_->content_is_suitable_for_gpu_rasterization_);
    EXPECT_EQ(layer_tree_src->inputs_.background_color,
              layer_tree_dst->inputs_.background_color);
    EXPECT_EQ(layer_tree_src->inputs_.has_transparent_background,
              layer_tree_dst->inputs_.has_transparent_background);
    EXPECT_EQ(layer_tree_src->in_paint_layer_contents(),
              layer_tree_dst->in_paint_layer_contents());
    EXPECT_EQ(layer_tree_host_src_->id_, layer_tree_host_dst_->id_);
    EXPECT_EQ(layer_tree_host_src_->next_commit_forces_redraw_,
              layer_tree_host_dst_->next_commit_forces_redraw_);
    for (auto* layer : layers_that_should_push_properties_src) {
      EXPECT_TRUE(layer_tree_dst->LayerNeedsPushPropertiesForTesting(
          layer_tree_dst->LayerById(layer->id())));
    }

    if (layer_tree_src->hud_layer_) {
      EXPECT_EQ(layer_tree_src->hud_layer_->id(),
                layer_tree_dst->hud_layer_->id());
      // The HUD layer member is a HeadsUpDisplayLayer instead of Layer, so
      // inspect the proto to see if it contains the the right layer type.
      bool found_hud_layer_type = false;
      for (int i = 0; i < proto.layer_tree().root_layer().children_size();
           ++i) {
        if (proto.layer_tree().root_layer().children(i).id() ==
            layer_tree_src->hud_layer_->id()) {
          EXPECT_EQ(proto::LayerNode::HEADS_UP_DISPLAY_LAYER,
                    proto.layer_tree().root_layer().children(i).type());
          found_hud_layer_type = true;
          break;
        }
      }
      EXPECT_TRUE(found_hud_layer_type);
    } else {
      EXPECT_FALSE(layer_tree_dst->hud_layer_);
    }
    if (layer_tree_src->overscroll_elasticity_layer()) {
      EXPECT_EQ(layer_tree_src->overscroll_elasticity_layer()->id(),
                layer_tree_dst->overscroll_elasticity_layer()->id());
    } else {
      EXPECT_FALSE(layer_tree_dst->overscroll_elasticity_layer());
    }
    if (layer_tree_src->page_scale_layer()) {
      EXPECT_EQ(layer_tree_src->page_scale_layer()->id(),
                layer_tree_dst->page_scale_layer()->id());
    } else {
      EXPECT_FALSE(layer_tree_dst->page_scale_layer());
    }
    if (layer_tree_src->inner_viewport_scroll_layer()) {
      EXPECT_EQ(layer_tree_src->inner_viewport_scroll_layer()->id(),
                layer_tree_dst->inner_viewport_scroll_layer()->id());
    } else {
      EXPECT_FALSE(layer_tree_dst->inner_viewport_scroll_layer());
    }
    if (layer_tree_src->outer_viewport_scroll_layer()) {
      EXPECT_EQ(layer_tree_src->outer_viewport_scroll_layer()->id(),
                layer_tree_dst->outer_viewport_scroll_layer()->id());
    } else {
      EXPECT_FALSE(layer_tree_dst->outer_viewport_scroll_layer());
    }
    EXPECT_EQ(layer_tree_src->inputs_.selection,
              layer_tree_dst->inputs_.selection);
    EXPECT_EQ(layer_tree_src->property_trees_, layer_tree_dst->property_trees_);

    // All layers must have a property tree index that matches PropertyTrees.
    if (layer_tree_dst->property_trees_.sequence_number) {
      int seq_num = layer_tree_dst->property_trees_.sequence_number;
      LayerTreeHostCommon::CallFunctionForEveryLayer(
          layer_tree_host_dst_->GetLayerTree(), [seq_num](Layer* layer) {
            EXPECT_EQ(seq_num, layer->property_tree_sequence_number());
          });
    }
  }

  void RunAllMembersChangedTest() {
    LayerTree* layer_tree_src = layer_tree_host_src_->GetLayerTree();

    layer_tree_src->needs_full_tree_sync_ =
        !layer_tree_src->needs_full_tree_sync_;
    layer_tree_src->needs_meta_info_recomputation_ =
        !layer_tree_src->needs_meta_info_recomputation_;
    layer_tree_host_src_->source_frame_number_ *= 3;

    // Just fake setup a layer for both source and dest.
    scoped_refptr<Layer> root_layer_src = Layer::Create();
    layer_tree_host_src_->SetRootLayer(root_layer_src);
    layer_tree_host_dst_->SetRootLayer(Layer::Create());
    root_layer_src->SetDoubleSided(!root_layer_src->double_sided());

    layer_tree_src->inputs_.device_viewport_size = gfx::Size(3, 14);
    layer_tree_src->inputs_.device_scale_factor =
        layer_tree_src->inputs_.device_scale_factor * 3 + 1;
    layer_tree_src->inputs_.painted_device_scale_factor =
        layer_tree_src->inputs_.painted_device_scale_factor * 3 + 1;
    layer_tree_src->inputs_.page_scale_factor =
        layer_tree_src->inputs_.page_scale_factor * 3 + 1;
    layer_tree_src->inputs_.min_page_scale_factor =
        layer_tree_src->inputs_.min_page_scale_factor * 3 + 1;
    layer_tree_src->inputs_.max_page_scale_factor =
        layer_tree_src->inputs_.max_page_scale_factor * 3 + 1;
    layer_tree_src->elastic_overscroll_ = gfx::Vector2dF(3, 14);
    layer_tree_host_src_->has_gpu_rasterization_trigger_ =
        !layer_tree_host_src_->has_gpu_rasterization_trigger_;
    layer_tree_host_src_->content_is_suitable_for_gpu_rasterization_ =
        !layer_tree_host_src_->content_is_suitable_for_gpu_rasterization_;
    layer_tree_src->inputs_.background_color = SK_ColorMAGENTA;
    layer_tree_src->inputs_.has_transparent_background =
        !layer_tree_src->inputs_.has_transparent_background;
    layer_tree_host_src_->id_ = layer_tree_host_src_->id_ * 3 + 1;
    layer_tree_host_src_->next_commit_forces_redraw_ =
        !layer_tree_host_src_->next_commit_forces_redraw_;

    layer_tree_src->hud_layer_ = HeadsUpDisplayLayer::Create();
    root_layer_src->AddChild(layer_tree_src->hud_layer_);

    scoped_refptr<Layer> overscroll_elasticity_layer = Layer::Create();
    scoped_refptr<Layer> page_scale_layer = Layer::Create();
    scoped_refptr<Layer> inner_viewport_scroll_layer = Layer::Create();
    scoped_refptr<Layer> outer_viewport_scroll_layer = Layer::Create();

    root_layer_src->AddChild(overscroll_elasticity_layer);
    root_layer_src->AddChild(page_scale_layer);
    root_layer_src->AddChild(inner_viewport_scroll_layer);
    root_layer_src->AddChild(outer_viewport_scroll_layer);

    layer_tree_src->RegisterViewportLayers(
        overscroll_elasticity_layer, page_scale_layer,
        inner_viewport_scroll_layer, outer_viewport_scroll_layer);

    // Set in_paint_layer_contents_ only after all calls to AddChild() have
    // finished to ensure it's allowed to do so at that time.
    layer_tree_src->in_paint_layer_contents_ =
        !layer_tree_src->in_paint_layer_contents();

    LayerSelectionBound sel_bound;
    sel_bound.edge_top = gfx::Point(14, 3);
    LayerSelection selection;
    selection.start = sel_bound;
    layer_tree_src->inputs_.selection = selection;

    layer_tree_src->property_trees_.sequence_number =
        layer_tree_src->property_trees_.sequence_number * 3 + 1;

    VerifySerializationAndDeserialization();
  }

  void RunLayersChangedTest() {
    // Just fake setup a layer for both source and dest.
    scoped_refptr<Layer> root_layer_src = Layer::Create();
    layer_tree_host_src_->SetRootLayer(root_layer_src);
    layer_tree_host_dst_->SetRootLayer(Layer::Create());
    root_layer_src->SetDoubleSided(!root_layer_src->double_sided());

    // No HUD layer or |overscroll_elasticity_layer_|, or the inner/outer
    // viewport scroll layers.
    scoped_refptr<Layer> overscroll_elasticity_layer = Layer::Create();
    layer_tree_host_src_->GetLayerTree()->inputs_.overscroll_elasticity_layer =
        overscroll_elasticity_layer;
    root_layer_src->AddChild(overscroll_elasticity_layer);

    VerifySerializationAndDeserialization();
  }

  void RunLayersChangedMultipleSerializations() {
    // Just fake setup a layer for both source and dest.
    scoped_refptr<Layer> root_layer_src = Layer::Create();
    layer_tree_host_src_->SetRootLayer(root_layer_src);
    layer_tree_host_dst_->SetRootLayer(Layer::Create());
    root_layer_src->SetDoubleSided(!root_layer_src->double_sided());

    LayerTree* layer_tree_src = layer_tree_host_src_->GetLayerTree();

    // Ensure that all the layers work correctly for multiple rounds of
    // serialization and deserialization.
    layer_tree_src->hud_layer_ = HeadsUpDisplayLayer::Create();
    root_layer_src->AddChild(layer_tree_src->hud_layer_);

    scoped_refptr<Layer> overscroll_elasticity_layer = Layer::Create();
    scoped_refptr<Layer> page_scale_layer = Layer::Create();
    scoped_refptr<Layer> inner_viewport_scroll_layer = Layer::Create();
    scoped_refptr<Layer> outer_viewport_scroll_layer = Layer::Create();

    root_layer_src->AddChild(overscroll_elasticity_layer);
    root_layer_src->AddChild(page_scale_layer);
    root_layer_src->AddChild(inner_viewport_scroll_layer);
    root_layer_src->AddChild(outer_viewport_scroll_layer);
    layer_tree_host_src_->GetLayerTree()->RegisterViewportLayers(
        overscroll_elasticity_layer, page_scale_layer,
        inner_viewport_scroll_layer, outer_viewport_scroll_layer);

    VerifySerializationAndDeserialization();
    VerifySerializationAndDeserialization();

    layer_tree_src->hud_layer_ = nullptr;
    VerifySerializationAndDeserialization();
    layer_tree_host_src_->GetLayerTree()->inputs_.overscroll_elasticity_layer =
        nullptr;
    VerifySerializationAndDeserialization();
    layer_tree_host_src_->GetLayerTree()->inputs_.page_scale_layer = nullptr;
    VerifySerializationAndDeserialization();
    layer_tree_host_src_->GetLayerTree()->inputs_.inner_viewport_scroll_layer =
        nullptr;
    VerifySerializationAndDeserialization();
    layer_tree_host_src_->GetLayerTree()->inputs_.outer_viewport_scroll_layer =
        nullptr;
    VerifySerializationAndDeserialization();
  }

  void RunPictureLayerMultipleSerializationsTest() {
    // Just fake setup a layer for both source and dest.
    scoped_refptr<Layer> root_layer_src = Layer::Create();
    layer_tree_host_src_->SetRootLayer(root_layer_src);
    layer_tree_host_dst_->SetRootLayer(Layer::Create());

    // Ensure that a PictureLayer work correctly for multiple rounds of
    // serialization and deserialization.
    scoped_refptr<FakePictureLayer> picture_layer_src = CreatePictureLayer();
    root_layer_src->AddChild(picture_layer_src);
    picture_layer_src->SetBounds(gfx::Size(10, 10));
    picture_layer_src->SetIsDrawable(true);
    picture_layer_src->SavePaintProperties();
    picture_layer_src->Update();
    picture_layer_src->SavePaintProperties();
    VerifySerializationAndDeserialization();
    ASSERT_EQ(1U, layer_tree_host_dst_->root_layer()->children().size());
    PictureLayer* picture_layer_dst = reinterpret_cast<PictureLayer*>(
        layer_tree_host_dst_->root_layer()->child_at(0));

    RecordingSource* recording_source_src =
        picture_layer_src->GetRecordingSourceForTesting();
    RecordingSource* recording_source_dst =
        picture_layer_dst->GetRecordingSourceForTesting();
    EXPECT_EQ(recording_source_src->GetSize(), recording_source_dst->GetSize());
    EXPECT_TRUE(AreDisplayListDrawingResultsSame(
        gfx::Rect(recording_source_src->GetSize()),
        picture_layer_src->GetDisplayItemList(),
        picture_layer_dst->GetDisplayItemList()));

    VerifySerializationAndDeserialization();
  }

  void RunAddAndRemoveNodeFromLayerTree() {
    /* Testing serialization when the tree hierarchy changes like this:
         root                  root
        /    \                /    \
       a      b        =>    a      c
               \                     \
                c                     d
    */
    scoped_refptr<Layer> layer_src_root = Layer::Create();
    layer_tree_host_src_->SetRootLayer(layer_src_root);

    scoped_refptr<Layer> layer_src_a = Layer::Create();
    scoped_refptr<Layer> layer_src_b = Layer::Create();
    scoped_refptr<Layer> layer_src_c = Layer::Create();
    scoped_refptr<Layer> layer_src_d = Layer::Create();

    layer_src_root->AddChild(layer_src_a);
    layer_src_root->AddChild(layer_src_b);
    layer_src_b->AddChild(layer_src_c);

    VerifySerializationAndDeserialization();
    VerifyHostHasAllExpectedLayersInTree(layer_tree_host_dst_->root_layer());

    // Now change the Layer Hierarchy
    layer_src_c->RemoveFromParent();
    layer_src_b->RemoveFromParent();
    layer_src_root->AddChild(layer_src_c);
    layer_src_c->AddChild(layer_src_d);

    VerifySerializationAndDeserialization();
    VerifyHostHasAllExpectedLayersInTree(layer_tree_host_dst_->root_layer());
  }

 private:
  std::unique_ptr<ImageSerializationProcessor> image_serialization_processor_;

  TestTaskGraphRunner task_graph_runner_src_;
  FakeLayerTreeHostClient client_src_;
  std::unique_ptr<FakeLayerTreeHost> layer_tree_host_src_;

  TestTaskGraphRunner task_graph_runner_dst_;
  FakeLayerTreeHostClient client_dst_;
  std::unique_ptr<FakeLayerTreeHost> layer_tree_host_dst_;
};

TEST_F(LayerTreeHostSerializationTest, AllMembersChanged) {
  RunAllMembersChangedTest();
}

TEST_F(LayerTreeHostSerializationTest, LayersChanged) {
  RunLayersChangedTest();
}

TEST_F(LayerTreeHostSerializationTest, LayersChangedMultipleSerializations) {
  RunLayersChangedMultipleSerializations();
}

TEST_F(LayerTreeHostSerializationTest, AddAndRemoveNodeFromLayerTree) {
  RunAddAndRemoveNodeFromLayerTree();
}

TEST_F(LayerTreeHostSerializationTest, PictureLayerMultipleSerializations) {
  RunPictureLayerMultipleSerializationsTest();
}

}  // namespace cc
