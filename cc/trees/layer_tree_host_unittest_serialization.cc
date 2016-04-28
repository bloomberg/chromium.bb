// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/layer_tree_host.h"

#include "cc/layers/heads_up_display_layer.h"
#include "cc/layers/layer.h"
#include "cc/proto/layer.pb.h"
#include "cc/proto/layer_tree_host.pb.h"
#include "cc/test/fake_layer_tree_host.h"
#include "cc/test/fake_layer_tree_host_client.h"
#include "cc/test/layer_tree_test.h"
#include "cc/test/test_task_graph_runner.h"
#include "cc/trees/layer_tree_settings.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace cc {

class LayerTreeHostSerializationTest : public testing::Test {
 public:
  LayerTreeHostSerializationTest()
      : client_src_(FakeLayerTreeHostClient::DIRECT_3D),
        client_dst_(FakeLayerTreeHostClient::DIRECT_3D) {}

 protected:
  void SetUp() override {
    layer_tree_host_src_ =
        FakeLayerTreeHost::Create(&client_src_, &task_graph_runner_src_);
    layer_tree_host_dst_ =
        FakeLayerTreeHost::Create(&client_dst_, &task_graph_runner_dst_);
  }

  void TearDown() override {
    // Need to reset |in_paint_layer_contents_| to tear down.
    layer_tree_host_src_->in_paint_layer_contents_ = false;
    layer_tree_host_dst_->in_paint_layer_contents_ = false;

    // Need to reset LayerTreeHost pointers before tear down.
    layer_tree_host_src_ = nullptr;
    layer_tree_host_dst_ = nullptr;
  }

  void VerifyHostHasAllExpectedLayersInTree(Layer* root_layer) {
    LayerTreeHostCommon::CallFunctionForEveryLayer(
        root_layer->layer_tree_host(), [root_layer](Layer* layer) {
          DCHECK(layer->layer_tree_host());
          EXPECT_EQ(layer, layer->layer_tree_host()->LayerById(layer->id()));
        });
  }

  void VerifySerializationAndDeserialization() {
    proto::LayerTreeHost proto;

    std::unordered_set<Layer*> layers_that_should_push_properties_src =
        layer_tree_host_src_->LayersThatShouldPushProperties();
    layer_tree_host_src_->ToProtobufForCommit(&proto);
    layer_tree_host_dst_->FromProtobufForCommit(proto);

    EXPECT_EQ(layer_tree_host_src_->needs_full_tree_sync_,
              layer_tree_host_dst_->needs_full_tree_sync_);
    EXPECT_EQ(layer_tree_host_src_->needs_meta_info_recomputation_,
              layer_tree_host_dst_->needs_meta_info_recomputation_);
    EXPECT_EQ(layer_tree_host_src_->source_frame_number_,
              layer_tree_host_dst_->source_frame_number_);
    EXPECT_EQ(layer_tree_host_src_->root_layer()->id(),
              layer_tree_host_dst_->root_layer()->id());
    EXPECT_EQ(layer_tree_host_dst_.get(),
              layer_tree_host_dst_->root_layer_->layer_tree_host());
    EXPECT_EQ(layer_tree_host_src_->root_layer_->double_sided(),
              layer_tree_host_dst_->root_layer_->double_sided());
    EXPECT_EQ(
        layer_tree_host_src_->debug_state_.show_replica_screen_space_rects,
        layer_tree_host_dst_->debug_state_.show_replica_screen_space_rects);
    EXPECT_EQ(layer_tree_host_src_->device_viewport_size_,
              layer_tree_host_dst_->device_viewport_size_);
    EXPECT_EQ(layer_tree_host_src_->top_controls_shrink_blink_size_,
              layer_tree_host_dst_->top_controls_shrink_blink_size_);
    EXPECT_EQ(layer_tree_host_src_->top_controls_height_,
              layer_tree_host_dst_->top_controls_height_);
    EXPECT_EQ(layer_tree_host_src_->top_controls_shown_ratio_,
              layer_tree_host_dst_->top_controls_shown_ratio_);
    EXPECT_EQ(layer_tree_host_src_->device_scale_factor_,
              layer_tree_host_dst_->device_scale_factor_);
    EXPECT_EQ(layer_tree_host_src_->painted_device_scale_factor_,
              layer_tree_host_dst_->painted_device_scale_factor_);
    EXPECT_EQ(layer_tree_host_src_->page_scale_factor_,
              layer_tree_host_dst_->page_scale_factor_);
    EXPECT_EQ(layer_tree_host_src_->min_page_scale_factor_,
              layer_tree_host_dst_->min_page_scale_factor_);
    EXPECT_EQ(layer_tree_host_src_->max_page_scale_factor_,
              layer_tree_host_dst_->max_page_scale_factor_);
    EXPECT_EQ(layer_tree_host_src_->elastic_overscroll_,
              layer_tree_host_dst_->elastic_overscroll_);
    EXPECT_EQ(layer_tree_host_src_->has_gpu_rasterization_trigger_,
              layer_tree_host_dst_->has_gpu_rasterization_trigger_);
    EXPECT_EQ(layer_tree_host_src_->content_is_suitable_for_gpu_rasterization_,
              layer_tree_host_dst_->content_is_suitable_for_gpu_rasterization_);
    EXPECT_EQ(layer_tree_host_src_->background_color_,
              layer_tree_host_dst_->background_color_);
    EXPECT_EQ(layer_tree_host_src_->has_transparent_background_,
              layer_tree_host_dst_->has_transparent_background_);
    EXPECT_EQ(layer_tree_host_src_->in_paint_layer_contents_,
              layer_tree_host_dst_->in_paint_layer_contents_);
    EXPECT_EQ(layer_tree_host_src_->id_, layer_tree_host_dst_->id_);
    EXPECT_EQ(layer_tree_host_src_->next_commit_forces_redraw_,
              layer_tree_host_dst_->next_commit_forces_redraw_);
    for (auto layer : layers_that_should_push_properties_src) {
      EXPECT_TRUE(layer_tree_host_dst_->LayerNeedsPushPropertiesForTesting(
          layer_tree_host_dst_->LayerById(layer->id())));
    }

    if (layer_tree_host_src_->hud_layer_) {
      EXPECT_EQ(layer_tree_host_src_->hud_layer_->id(),
                layer_tree_host_dst_->hud_layer_->id());
      // The HUD layer member is a HeadsUpDisplayLayer instead of Layer, so
      // inspect the proto to see if it contains the the right layer type.
      bool found_hud_layer_type = false;
      for (int i = 0; i < proto.root_layer().children_size(); ++i) {
        if (proto.root_layer().children(i).id() ==
            layer_tree_host_src_->hud_layer_->id()) {
          EXPECT_EQ(proto::LayerNode::HEADS_UP_DISPLAY_LAYER,
                    proto.root_layer().children(i).type());
          found_hud_layer_type = true;
          break;
        }
      }
      EXPECT_TRUE(found_hud_layer_type);
    } else {
      EXPECT_EQ(nullptr, layer_tree_host_dst_->hud_layer_);
    }
    if (layer_tree_host_src_->overscroll_elasticity_layer_) {
      EXPECT_EQ(layer_tree_host_src_->overscroll_elasticity_layer_->id(),
                layer_tree_host_dst_->overscroll_elasticity_layer_->id());
    } else {
      EXPECT_EQ(nullptr, layer_tree_host_dst_->overscroll_elasticity_layer_);
    }
    if (layer_tree_host_src_->page_scale_layer_) {
      EXPECT_EQ(layer_tree_host_src_->page_scale_layer_->id(),
                layer_tree_host_dst_->page_scale_layer_->id());
    } else {
      EXPECT_EQ(nullptr, layer_tree_host_dst_->page_scale_layer_);
    }
    if (layer_tree_host_src_->inner_viewport_scroll_layer_) {
      EXPECT_EQ(layer_tree_host_src_->inner_viewport_scroll_layer_->id(),
                layer_tree_host_dst_->inner_viewport_scroll_layer_->id());
    } else {
      EXPECT_EQ(nullptr, layer_tree_host_dst_->inner_viewport_scroll_layer_);
    }
    if (layer_tree_host_src_->outer_viewport_scroll_layer_) {
      EXPECT_EQ(layer_tree_host_src_->outer_viewport_scroll_layer_->id(),
                layer_tree_host_dst_->outer_viewport_scroll_layer_->id());
    } else {
      EXPECT_EQ(nullptr, layer_tree_host_dst_->outer_viewport_scroll_layer_);
    }
    EXPECT_EQ(layer_tree_host_src_->selection_,
              layer_tree_host_dst_->selection_);
    EXPECT_EQ(layer_tree_host_src_->property_trees_,
              layer_tree_host_dst_->property_trees_);
    EXPECT_EQ(layer_tree_host_src_->surface_id_namespace_,
              layer_tree_host_dst_->surface_id_namespace_);
    EXPECT_EQ(layer_tree_host_src_->next_surface_sequence_,
              layer_tree_host_dst_->next_surface_sequence_);

    // All layers must have a property tree index that matches PropertyTrees.
    if (layer_tree_host_dst_->property_trees_.sequence_number) {
      int seq_num = layer_tree_host_dst_->property_trees_.sequence_number;
      LayerTreeHostCommon::CallFunctionForEveryLayer(
          layer_tree_host_dst_.get(), [seq_num](Layer* layer) {
            EXPECT_EQ(seq_num, layer->property_tree_sequence_number());
          });
    }
  }

  void RunAllMembersChangedTest() {
    layer_tree_host_src_->needs_full_tree_sync_ =
        !layer_tree_host_src_->needs_full_tree_sync_;
    layer_tree_host_src_->needs_meta_info_recomputation_ =
        !layer_tree_host_src_->needs_meta_info_recomputation_;
    layer_tree_host_src_->source_frame_number_ *= 3;

    // Just fake setup a layer for both source and dest.
    scoped_refptr<Layer> root_layer_src = Layer::Create();
    layer_tree_host_src_->SetRootLayer(root_layer_src);
    layer_tree_host_dst_->SetRootLayer(Layer::Create());
    root_layer_src->SetDoubleSided(!root_layer_src->double_sided());

    layer_tree_host_src_->debug_state_.show_replica_screen_space_rects =
        !layer_tree_host_src_->debug_state_.show_replica_screen_space_rects;
    layer_tree_host_src_->device_viewport_size_ = gfx::Size(3, 14);
    layer_tree_host_src_->top_controls_shrink_blink_size_ =
        !layer_tree_host_src_->top_controls_shrink_blink_size_;
    layer_tree_host_src_->top_controls_height_ =
        layer_tree_host_src_->top_controls_height_ * 3 + 1;
    layer_tree_host_src_->top_controls_shown_ratio_ =
        layer_tree_host_src_->top_controls_shown_ratio_ * 3 + 1;
    layer_tree_host_src_->device_scale_factor_ =
        layer_tree_host_src_->device_scale_factor_ * 3 + 1;
    layer_tree_host_src_->painted_device_scale_factor_ =
        layer_tree_host_src_->painted_device_scale_factor_ * 3 + 1;
    layer_tree_host_src_->page_scale_factor_ =
        layer_tree_host_src_->page_scale_factor_ * 3 + 1;
    layer_tree_host_src_->min_page_scale_factor_ =
        layer_tree_host_src_->min_page_scale_factor_ * 3 + 1;
    layer_tree_host_src_->max_page_scale_factor_ =
        layer_tree_host_src_->max_page_scale_factor_ * 3 + 1;
    layer_tree_host_src_->elastic_overscroll_ = gfx::Vector2dF(3, 14);
    layer_tree_host_src_->has_gpu_rasterization_trigger_ =
        !layer_tree_host_src_->has_gpu_rasterization_trigger_;
    layer_tree_host_src_->content_is_suitable_for_gpu_rasterization_ =
        !layer_tree_host_src_->content_is_suitable_for_gpu_rasterization_;
    layer_tree_host_src_->background_color_ = SK_ColorMAGENTA;
    layer_tree_host_src_->has_transparent_background_ =
        !layer_tree_host_src_->has_transparent_background_;
    layer_tree_host_src_->id_ = layer_tree_host_src_->id_ * 3 + 1;
    layer_tree_host_src_->next_commit_forces_redraw_ =
        !layer_tree_host_src_->next_commit_forces_redraw_;

    layer_tree_host_src_->hud_layer_ = HeadsUpDisplayLayer::Create();
    root_layer_src->AddChild(layer_tree_host_src_->hud_layer_);
    layer_tree_host_src_->overscroll_elasticity_layer_ = Layer::Create();
    root_layer_src->AddChild(
        layer_tree_host_src_->overscroll_elasticity_layer_);
    layer_tree_host_src_->page_scale_layer_ = Layer::Create();
    root_layer_src->AddChild(layer_tree_host_src_->page_scale_layer_);
    layer_tree_host_src_->inner_viewport_scroll_layer_ = Layer::Create();
    root_layer_src->AddChild(
        layer_tree_host_src_->inner_viewport_scroll_layer_);
    layer_tree_host_src_->outer_viewport_scroll_layer_ = Layer::Create();
    root_layer_src->AddChild(
        layer_tree_host_src_->outer_viewport_scroll_layer_);

    // Set in_paint_layer_contents_ only after all calls to AddChild() have
    // finished to ensure it's allowed to do so at that time.
    layer_tree_host_src_->in_paint_layer_contents_ =
        !layer_tree_host_src_->in_paint_layer_contents_;

    LayerSelectionBound sel_bound;
    sel_bound.edge_top = gfx::Point(14, 3);
    LayerSelection selection;
    selection.start = sel_bound;
    layer_tree_host_src_->selection_ = selection;

    layer_tree_host_src_->property_trees_.sequence_number =
        layer_tree_host_src_->property_trees_.sequence_number * 3 + 1;

    layer_tree_host_src_->surface_id_namespace_ =
        layer_tree_host_src_->surface_id_namespace_ * 3 + 1;
    layer_tree_host_src_->next_surface_sequence_ =
        layer_tree_host_src_->next_surface_sequence_ * 3 + 1;

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
    layer_tree_host_src_->overscroll_elasticity_layer_ = Layer::Create();
    root_layer_src->AddChild(
        layer_tree_host_src_->overscroll_elasticity_layer_);

    VerifySerializationAndDeserialization();
  }

  void RunLayersChangedMultipleSerializations() {
    // Just fake setup a layer for both source and dest.
    scoped_refptr<Layer> root_layer_src = Layer::Create();
    layer_tree_host_src_->SetRootLayer(root_layer_src);
    layer_tree_host_dst_->SetRootLayer(Layer::Create());
    root_layer_src->SetDoubleSided(!root_layer_src->double_sided());

    // Ensure that all the layers work correctly for multiple rounds of
    // serialization and deserialization.
    layer_tree_host_src_->hud_layer_ = HeadsUpDisplayLayer::Create();
    root_layer_src->AddChild(layer_tree_host_src_->hud_layer_);
    layer_tree_host_src_->overscroll_elasticity_layer_ = Layer::Create();
    root_layer_src->AddChild(
        layer_tree_host_src_->overscroll_elasticity_layer_);
    layer_tree_host_src_->page_scale_layer_ = Layer::Create();
    root_layer_src->AddChild(layer_tree_host_src_->page_scale_layer_);
    layer_tree_host_src_->inner_viewport_scroll_layer_ = Layer::Create();
    root_layer_src->AddChild(
        layer_tree_host_src_->inner_viewport_scroll_layer_);
    layer_tree_host_src_->outer_viewport_scroll_layer_ = Layer::Create();
    root_layer_src->AddChild(
        layer_tree_host_src_->outer_viewport_scroll_layer_);

    VerifySerializationAndDeserialization();
    VerifySerializationAndDeserialization();

    layer_tree_host_src_->hud_layer_ = nullptr;
    VerifySerializationAndDeserialization();
    layer_tree_host_src_->overscroll_elasticity_layer_ = nullptr;
    VerifySerializationAndDeserialization();
    layer_tree_host_src_->page_scale_layer_ = nullptr;
    VerifySerializationAndDeserialization();
    layer_tree_host_src_->inner_viewport_scroll_layer_ = nullptr;
    VerifySerializationAndDeserialization();
    layer_tree_host_src_->outer_viewport_scroll_layer_ = nullptr;
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
  TestTaskGraphRunner task_graph_runner_src_;
  FakeLayerTreeHostClient client_src_;
  std::unique_ptr<LayerTreeHost> layer_tree_host_src_;

  TestTaskGraphRunner task_graph_runner_dst_;
  FakeLayerTreeHostClient client_dst_;
  std::unique_ptr<LayerTreeHost> layer_tree_host_dst_;
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

}  // namespace cc
