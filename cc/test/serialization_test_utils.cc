// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/serialization_test_utils.h"

#include "cc/blimp/compositor_state_deserializer.h"
#include "cc/layers/layer.h"
#include "cc/layers/solid_color_scrollbar_layer.h"
#include "cc/test/fake_picture_layer.h"
#include "cc/test/skia_common.h"
#include "cc/trees/layer_tree.h"
#include "cc/trees/layer_tree_host_common.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {

#define EXPECT_LAYERS_EQ(engine_layer_id, client_layer)                    \
  EXPECT_EQ(                                                               \
      compositor_state_deserializer->GetLayerForEngineId(engine_layer_id), \
      client_layer);

void VerifySerializedTreesAreIdentical(
    LayerTree* engine_layer_tree,
    LayerTree* client_layer_tree,
    CompositorStateDeserializer* compositor_state_deserializer) {
  if (engine_layer_tree->root_layer()) {
    for (auto* engine_layer : *engine_layer_tree) {
      Layer* client_layer = compositor_state_deserializer->GetLayerForEngineId(
          engine_layer->id());
      EXPECT_EQ(client_layer->GetLayerTree(), client_layer_tree);
      VerifySerializedLayersAreIdentical(engine_layer,
                                         compositor_state_deserializer);
    }
  } else {
    EXPECT_EQ(client_layer_tree->root_layer(), nullptr);
  }

  // Viewport layers.
  // Overscroll Elasticty Layer.
  Layer* client_overscroll_elasticity_layer =
      client_layer_tree->overscroll_elasticity_layer();
  if (engine_layer_tree->overscroll_elasticity_layer()) {
    int engine_overscroll_elasticity_layer_id =
        engine_layer_tree->overscroll_elasticity_layer()->id();

    EXPECT_LAYERS_EQ(engine_overscroll_elasticity_layer_id,
                     client_overscroll_elasticity_layer);
  } else {
    EXPECT_EQ(client_overscroll_elasticity_layer, nullptr);
  }

  // PageScale Layer.
  Layer* client_page_scale_layer = client_layer_tree->page_scale_layer();
  if (engine_layer_tree->page_scale_layer()) {
    int engine_page_scale_layer_id =
        engine_layer_tree->page_scale_layer()->id();

    EXPECT_LAYERS_EQ(engine_page_scale_layer_id, client_page_scale_layer);
  } else {
    EXPECT_EQ(client_page_scale_layer, nullptr);
  }

  // InnerViewportScroll Layer.
  Layer* client_inner_viewport_layer =
      client_layer_tree->inner_viewport_scroll_layer();
  if (engine_layer_tree->inner_viewport_scroll_layer()) {
    int engine_inner_viewport_layer_id =
        engine_layer_tree->inner_viewport_scroll_layer()->id();

    EXPECT_LAYERS_EQ(engine_inner_viewport_layer_id,
                     client_inner_viewport_layer);
  } else {
    EXPECT_EQ(client_inner_viewport_layer, nullptr);
  }

  // OuterViewportScroll Layer.
  Layer* client_outer_viewport_layer =
      client_layer_tree->outer_viewport_scroll_layer();
  if (engine_layer_tree->outer_viewport_scroll_layer()) {
    int engine_outer_viewport_layer_id =
        engine_layer_tree->outer_viewport_scroll_layer()->id();

    EXPECT_LAYERS_EQ(engine_outer_viewport_layer_id,
                     client_outer_viewport_layer);
  } else {
    EXPECT_EQ(client_outer_viewport_layer, nullptr);
  }

  // Other LayerTree inputs.
  EXPECT_EQ(engine_layer_tree->device_scale_factor(),
            client_layer_tree->device_scale_factor());
  EXPECT_EQ(engine_layer_tree->painted_device_scale_factor(),
            client_layer_tree->painted_device_scale_factor());
  EXPECT_EQ(engine_layer_tree->device_viewport_size(),
            client_layer_tree->device_viewport_size());

  EXPECT_EQ(engine_layer_tree->page_scale_factor(),
            client_layer_tree->page_scale_factor());
  EXPECT_EQ(engine_layer_tree->min_page_scale_factor(),
            client_layer_tree->min_page_scale_factor());
  EXPECT_EQ(engine_layer_tree->max_page_scale_factor(),
            client_layer_tree->max_page_scale_factor());

  EXPECT_EQ(engine_layer_tree->background_color(),
            client_layer_tree->background_color());
  EXPECT_EQ(engine_layer_tree->has_transparent_background(),
            client_layer_tree->has_transparent_background());

  EXPECT_EQ(engine_layer_tree->selection(), client_layer_tree->selection());

  EXPECT_EQ(engine_layer_tree->have_scroll_event_handlers(),
            client_layer_tree->have_scroll_event_handlers());

  for (int i = 0; i < static_cast<int>(EventListenerClass::kNumClasses); ++i) {
    EXPECT_EQ(engine_layer_tree->event_listener_properties(
                  static_cast<EventListenerClass>(i)),
              client_layer_tree->event_listener_properties(
                  static_cast<EventListenerClass>(i)));
  }
}

void VerifySerializedLayersAreIdentical(
    Layer* engine_layer,
    CompositorStateDeserializer* compositor_state_deserializer) {
  Layer* client_layer =
      compositor_state_deserializer->GetLayerForEngineId(engine_layer->id());
  if (engine_layer == nullptr) {
    EXPECT_EQ(client_layer, nullptr);
    return;
  }

  // Parent.
  if (engine_layer->parent()) {
    int engine_parent_id = engine_layer->parent()->id();
    EXPECT_LAYERS_EQ(engine_parent_id, client_layer->parent());
  } else {
    EXPECT_EQ(client_layer->parent(), nullptr);
  }

  // Mask Layers.
  if (engine_layer->mask_layer()) {
    int engine_mask_layer_id = engine_layer->mask_layer()->id();
    EXPECT_LAYERS_EQ(engine_mask_layer_id, client_layer->mask_layer());
  } else {
    EXPECT_EQ(client_layer->mask_layer(), nullptr);
  }

  // Scroll parent.
  if (engine_layer->scroll_parent()) {
    int engine_scroll_parent_id = engine_layer->scroll_parent()->id();
    EXPECT_LAYERS_EQ(engine_scroll_parent_id, client_layer->scroll_parent());
  } else {
    EXPECT_EQ(client_layer->scroll_parent(), nullptr);
  }

  // Clip parent.
  if (engine_layer->clip_parent()) {
    int engine_clip_parent_id = engine_layer->clip_parent()->id();
    EXPECT_LAYERS_EQ(engine_clip_parent_id, client_layer->clip_parent());
  } else {
    EXPECT_EQ(client_layer->clip_parent(), nullptr);
  }

  // Scroll-clip layer.
  if (engine_layer->scroll_clip_layer()) {
    int scroll_clip_id = engine_layer->scroll_clip_layer()->id();
    EXPECT_LAYERS_EQ(scroll_clip_id, client_layer->scroll_clip_layer());
  } else {
    EXPECT_EQ(client_layer->scroll_clip_layer(), nullptr);
  }

  // Other Layer properties.
  // Note: The update rect is ignored here since it is cleared during
  // serialization.
  EXPECT_EQ(engine_layer->transform_origin(), client_layer->transform_origin());
  EXPECT_EQ(engine_layer->background_color(), client_layer->background_color());
  EXPECT_EQ(engine_layer->bounds(), client_layer->bounds());
  EXPECT_EQ(engine_layer->double_sided(), client_layer->double_sided());
  EXPECT_EQ(engine_layer->hide_layer_and_subtree(),
            client_layer->hide_layer_and_subtree());
  EXPECT_EQ(engine_layer->masks_to_bounds(), client_layer->masks_to_bounds());
  EXPECT_EQ(engine_layer->main_thread_scrolling_reasons(),
            client_layer->main_thread_scrolling_reasons());
  EXPECT_EQ(engine_layer->non_fast_scrollable_region(),
            client_layer->non_fast_scrollable_region());
  EXPECT_EQ(engine_layer->touch_event_handler_region(),
            client_layer->touch_event_handler_region());
  EXPECT_EQ(engine_layer->contents_opaque(), client_layer->contents_opaque());
  EXPECT_EQ(engine_layer->opacity(), client_layer->opacity());
  EXPECT_EQ(engine_layer->blend_mode(), client_layer->blend_mode());
  EXPECT_EQ(engine_layer->is_root_for_isolated_group(),
            client_layer->is_root_for_isolated_group());
  EXPECT_EQ(engine_layer->position(), client_layer->position());
  EXPECT_EQ(engine_layer->IsContainerForFixedPositionLayers(),
            client_layer->IsContainerForFixedPositionLayers());
  EXPECT_EQ(engine_layer->position_constraint(),
            client_layer->position_constraint());
  EXPECT_EQ(engine_layer->should_flatten_transform(),
            client_layer->should_flatten_transform());
  EXPECT_EQ(engine_layer->use_parent_backface_visibility(),
            client_layer->use_parent_backface_visibility());
  EXPECT_EQ(engine_layer->transform(), client_layer->transform());
  EXPECT_EQ(engine_layer->sorting_context_id(),
            client_layer->sorting_context_id());
  EXPECT_EQ(engine_layer->user_scrollable_horizontal(),
            client_layer->user_scrollable_horizontal());
  EXPECT_EQ(engine_layer->user_scrollable_vertical(),
            client_layer->user_scrollable_vertical());
  EXPECT_EQ(engine_layer->scroll_offset(), client_layer->scroll_offset());
}

void VerifySerializedScrollbarLayersAreIdentical(
    SolidColorScrollbarLayer* engine_layer,
    CompositorStateDeserializer* compositor_state_deserializer) {
  SolidColorScrollbarLayer* client_layer =
      static_cast<SolidColorScrollbarLayer*>(
          compositor_state_deserializer->GetLayerForEngineId(
              engine_layer->id()));

  if (engine_layer == nullptr) {
    EXPECT_EQ(client_layer, nullptr);
    return;
  }

  EXPECT_LAYERS_EQ(
      engine_layer->ScrollLayerId(),
      client_layer->GetLayerTree()->LayerById(client_layer->ScrollLayerId()));
  EXPECT_EQ(engine_layer->orientation(), client_layer->orientation());
  EXPECT_EQ(engine_layer->thumb_thickness(), client_layer->thumb_thickness());
  EXPECT_EQ(engine_layer->track_start(), client_layer->track_start());
  EXPECT_EQ(engine_layer->is_left_side_vertical_scrollbar(),
            client_layer->is_left_side_vertical_scrollbar());
}

void VerifySerializedPictureLayersAreIdentical(
    FakePictureLayer* engine_layer,
    CompositorStateDeserializer* compositor_state_deserializer) {
  FakePictureLayer* client_layer = static_cast<FakePictureLayer*>(
      compositor_state_deserializer->GetLayerForEngineId(engine_layer->id()));
  EXPECT_EQ(engine_layer->nearest_neighbor(), client_layer->nearest_neighbor());

  RecordingSource* engine_source = engine_layer->GetRecordingSourceForTesting();
  RecordingSource* client_source = client_layer->GetRecordingSourceForTesting();
  EXPECT_EQ(engine_source->recorded_viewport(),
            client_source->recorded_viewport());
  if (engine_source->GetDisplayItemList()) {
    EXPECT_TRUE(AreDisplayListDrawingResultsSame(
        engine_source->recorded_viewport(), engine_source->GetDisplayItemList(),
        client_source->GetDisplayItemList()));
  } else {
    EXPECT_EQ(client_source->GetDisplayItemList(), nullptr);
  }
}

}  // namespace cc
