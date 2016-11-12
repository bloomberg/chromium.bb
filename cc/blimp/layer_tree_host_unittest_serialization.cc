// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "cc/animation/animation_host.h"
#include "cc/blimp/layer_tree_host_remote.h"
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
#include "cc/test/remote_client_layer_factory.h"
#include "cc/test/remote_compositor_test.h"
#include "cc/test/serialization_test_utils.h"
#include "cc/test/skia_common.h"
#include "cc/trees/layer_tree.h"
#include "cc/trees/layer_tree_host_common.h"
#include "cc/trees/layer_tree_settings.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace cc {

class LayerTreeHostSerializationTest : public RemoteCompositorTest {
 protected:
  void VerifySerializationAndDeserialization() {
    // Synchronize state.
    base::RunLoop().RunUntilIdle();
    VerifySerializedTreesAreIdentical(
        layer_tree_host_remote_->GetLayerTree(),
        layer_tree_host_in_process_->GetLayerTree(),
        compositor_state_deserializer_.get());
  }

  void SetUpViewportLayers(LayerTree* engine_layer_tree) {
    scoped_refptr<Layer> overscroll_elasticity_layer = Layer::Create();
    scoped_refptr<Layer> page_scale_layer = Layer::Create();
    scoped_refptr<Layer> inner_viewport_scroll_layer = Layer::Create();
    scoped_refptr<Layer> outer_viewport_scroll_layer = Layer::Create();

    engine_layer_tree->root_layer()->AddChild(overscroll_elasticity_layer);
    engine_layer_tree->root_layer()->AddChild(page_scale_layer);
    engine_layer_tree->root_layer()->AddChild(inner_viewport_scroll_layer);
    engine_layer_tree->root_layer()->AddChild(outer_viewport_scroll_layer);

    engine_layer_tree->RegisterViewportLayers(
        overscroll_elasticity_layer, page_scale_layer,
        inner_viewport_scroll_layer, outer_viewport_scroll_layer);
  }
};

TEST_F(LayerTreeHostSerializationTest, AllMembersChanged) {
  LayerTree* engine_layer_tree = layer_tree_host_remote_->GetLayerTree();

  engine_layer_tree->SetRootLayer(Layer::Create());
  scoped_refptr<Layer> mask_layer = Layer::Create();
  engine_layer_tree->root_layer()->SetMaskLayer(mask_layer.get());
  SetUpViewportLayers(engine_layer_tree);

  engine_layer_tree->SetViewportSize(gfx::Size(3, 14));
  engine_layer_tree->SetDeviceScaleFactor(4.f);
  engine_layer_tree->SetPaintedDeviceScaleFactor(2.f);
  engine_layer_tree->SetPageScaleFactorAndLimits(2.f, 0.5f, 3.f);
  engine_layer_tree->set_background_color(SK_ColorMAGENTA);
  engine_layer_tree->set_has_transparent_background(true);
  LayerSelectionBound sel_bound;
  sel_bound.edge_top = gfx::Point(14, 3);
  LayerSelection selection;
  selection.start = sel_bound;
  engine_layer_tree->RegisterSelection(selection);
  VerifySerializationAndDeserialization();
}

TEST_F(LayerTreeHostSerializationTest, LayersChangedMultipleSerializations) {
  LayerTree* engine_layer_tree = layer_tree_host_remote_->GetLayerTree();
  engine_layer_tree->SetRootLayer(Layer::Create());
  SetUpViewportLayers(engine_layer_tree);

  VerifySerializationAndDeserialization();

  scoped_refptr<Layer> new_child = Layer::Create();
  engine_layer_tree->root_layer()->AddChild(new_child);
  engine_layer_tree->RegisterViewportLayers(nullptr, nullptr, nullptr, nullptr);
  VerifySerializationAndDeserialization();

  engine_layer_tree->SetRootLayer(nullptr);
  VerifySerializationAndDeserialization();
}

TEST_F(LayerTreeHostSerializationTest, AddAndRemoveNodeFromLayerTree) {
  /* Testing serialization when the tree hierarchy changes like this:
           root                  root
          /    \                /    \
         a      b        =>    a      c
                 \                     \
                  c                     d
      */
  LayerTree* engine_layer_tree = layer_tree_host_remote_->GetLayerTree();
  scoped_refptr<Layer> layer_src_root = Layer::Create();
  engine_layer_tree->SetRootLayer(layer_src_root);

  scoped_refptr<Layer> layer_src_a = Layer::Create();
  scoped_refptr<Layer> layer_src_b = Layer::Create();
  scoped_refptr<Layer> layer_src_c = Layer::Create();
  scoped_refptr<Layer> layer_src_d = Layer::Create();

  layer_src_root->AddChild(layer_src_a);
  layer_src_root->AddChild(layer_src_b);
  layer_src_b->AddChild(layer_src_c);
  VerifySerializationAndDeserialization();

  // Now change the Layer Hierarchy
  layer_src_c->RemoveFromParent();
  layer_src_b->RemoveFromParent();
  layer_src_root->AddChild(layer_src_c);
  layer_src_c->AddChild(layer_src_d);
  VerifySerializationAndDeserialization();
}

TEST_F(LayerTreeHostSerializationTest, PictureLayerSerialization) {
  // Override the layer factor to create FakePictureLayers in the deserializer.
  compositor_state_deserializer_->SetLayerFactoryForTesting(
      base::MakeUnique<RemoteClientLayerFactory>());

  LayerTree* engine_layer_tree = layer_tree_host_remote_->GetLayerTree();
  scoped_refptr<Layer> root_layer_src = Layer::Create();
  engine_layer_tree->SetRootLayer(root_layer_src);

  // Ensure that a PictureLayer work correctly for multiple rounds of
  // serialization and deserialization.
  FakeContentLayerClient content_client;
  gfx::Size bounds(256, 256);
  content_client.set_bounds(bounds);
  SkPaint simple_paint;
  simple_paint.setColor(SkColorSetARGB(255, 12, 23, 34));
  content_client.add_draw_rect(gfx::Rect(bounds), simple_paint);
  scoped_refptr<FakePictureLayer> picture_layer_src =
      FakePictureLayer::Create(&content_client);

  root_layer_src->AddChild(picture_layer_src);
  picture_layer_src->SetNeedsDisplay();
  VerifySerializationAndDeserialization();

  layer_tree_host_in_process_->UpdateLayers();
  PictureLayer* picture_layer_dst = reinterpret_cast<PictureLayer*>(
      compositor_state_deserializer_->GetLayerForEngineId(
          picture_layer_src->id()));
  EXPECT_TRUE(AreDisplayListDrawingResultsSame(
      gfx::Rect(gfx::Rect(picture_layer_src->bounds())),
      picture_layer_src->GetDisplayItemList(),
      picture_layer_dst->GetDisplayItemList()));

  // Another round.
  picture_layer_src->SetNeedsDisplay();
  VerifySerializationAndDeserialization();
  layer_tree_host_in_process_->UpdateLayers();
  EXPECT_TRUE(AreDisplayListDrawingResultsSame(
      gfx::Rect(gfx::Rect(picture_layer_src->bounds())),
      picture_layer_src->GetDisplayItemList(),
      picture_layer_dst->GetDisplayItemList()));
}

}  // namespace cc
