// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/layer_position_constraint.h"

#include <vector>

#include "cc/animation/animation_host.h"
#include "cc/layers/layer.h"
#include "cc/layers/layer_impl.h"
#include "cc/test/fake_layer_tree_host.h"
#include "cc/test/fake_proxy.h"
#include "cc/test/geometry_test_utils.h"
#include "cc/test/layer_test_common.h"
#include "cc/test/test_task_graph_runner.h"
#include "cc/trees/layer_tree_host_common.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

class LayerWithForcedDrawsContent : public Layer {
 public:
  LayerWithForcedDrawsContent() = default;

  bool DrawsContent() const override;

 private:
  ~LayerWithForcedDrawsContent() override = default;
};

bool LayerWithForcedDrawsContent::DrawsContent() const {
  return true;
}

void SetLayerPropertiesForTesting(Layer* layer,
                                  const gfx::Transform& transform,
                                  const gfx::Point3F& transform_origin,
                                  const gfx::PointF& position,
                                  const gfx::Size& bounds,
                                  bool flatten_transform) {
  layer->SetTransform(transform);
  layer->SetTransformOrigin(transform_origin);
  layer->SetPosition(position);
  layer->SetBounds(bounds);
  layer->SetShouldFlattenTransform(flatten_transform);
}

void ExecuteCalculateDrawProperties(LayerImpl* root_layer) {
  RenderSurfaceList dummy_render_surface_list;
  LayerTreeHostCommon::CalcDrawPropsImplInputsForTesting inputs(
      root_layer, root_layer->bounds(), &dummy_render_surface_list);
  inputs.inner_viewport_scroll_layer =
      root_layer->layer_tree_impl()->InnerViewportScrollLayer();
  inputs.outer_viewport_scroll_layer =
      root_layer->layer_tree_impl()->OuterViewportScrollLayer();
  EXPECT_FALSE(root_layer->layer_tree_impl()->property_trees()->needs_rebuild);
  LayerTreeHostCommon::CalculateDrawProperties(&inputs);
}

class LayerPositionConstraintTest : public testing::Test {
 public:
  LayerPositionConstraintTest()
      : animation_host_(AnimationHost::CreateForTesting(ThreadInstance::MAIN)),
        layer_tree_host_(FakeLayerTreeHost::Create(&fake_client_,
                                                   &task_graph_runner_,
                                                   animation_host_.get())),
        root_impl_(nullptr),
        inner_viewport_container_layer_impl_(nullptr),
        scroll_layer_impl_(nullptr),
        outer_viewport_container_layer_impl_(nullptr),
        child_transform_layer_impl_(nullptr),
        child_impl_(nullptr),
        grand_child_impl_(nullptr),
        great_grand_child_impl_(nullptr) {
    layer_tree_host_->InitializeForTesting(
        TaskRunnerProvider::Create(nullptr, nullptr),
        std::unique_ptr<Proxy>(new FakeProxy));
    CreateTreeForTest();
    fixed_to_top_left_.set_is_fixed_position(true);
    fixed_to_bottom_right_.set_is_fixed_position(true);
    fixed_to_bottom_right_.set_is_fixed_to_right_edge(true);
    fixed_to_bottom_right_.set_is_fixed_to_bottom_edge(true);
  }

  void CreateTreeForTest() {
    // scroll_layer_ is the inner viewport scroll layer and child_ is the outer
    // viewport scroll layer.
    root_ = Layer::Create();
    inner_viewport_container_layer_ = Layer::Create();
    page_scale_layer_ = Layer::Create();
    scroll_layer_ = Layer::Create();
    outer_viewport_container_layer_ = Layer::Create();
    child_transform_layer_ = Layer::Create();
    child_ = Layer::Create();
    grand_child_ = base::WrapRefCounted(new LayerWithForcedDrawsContent());
    great_grand_child_ =
        base::WrapRefCounted(new LayerWithForcedDrawsContent());

    gfx::Transform IdentityMatrix;
    gfx::Point3F transform_origin;
    gfx::PointF position;
    gfx::Size bounds(200, 200);
    gfx::Size clip_bounds(100, 100);
    SetLayerPropertiesForTesting(inner_viewport_container_layer_.get(),
                                 IdentityMatrix, transform_origin, position,
                                 clip_bounds, true);
    SetLayerPropertiesForTesting(page_scale_layer_.get(), IdentityMatrix,
                                 transform_origin, position, clip_bounds, true);
    SetLayerPropertiesForTesting(scroll_layer_.get(), IdentityMatrix,
                                 transform_origin, position, bounds, true);
    SetLayerPropertiesForTesting(outer_viewport_container_layer_.get(),
                                 IdentityMatrix, transform_origin, position,
                                 clip_bounds, true);
    SetLayerPropertiesForTesting(child_.get(), IdentityMatrix, transform_origin,
                                 position, bounds, true);
    SetLayerPropertiesForTesting(grand_child_.get(), IdentityMatrix,
                                 transform_origin, position, bounds, true);
    SetLayerPropertiesForTesting(great_grand_child_.get(), IdentityMatrix,
                                 transform_origin, position, bounds, true);

    root_->SetBounds(clip_bounds);

    inner_viewport_container_layer_->SetMasksToBounds(true);
    scroll_layer_->SetElementId(
        LayerIdToElementIdForTesting(scroll_layer_->id()));
    scroll_layer_->SetScrollable(clip_bounds);
    scroll_layer_->SetIsContainerForFixedPositionLayers(true);

    outer_viewport_container_layer_->SetMasksToBounds(true);
    child_->SetElementId(LayerIdToElementIdForTesting(child_->id()));
    child_->SetScrollable(clip_bounds);
    grand_child_->SetElementId(
        LayerIdToElementIdForTesting(grand_child_->id()));
    grand_child_->SetScrollable(clip_bounds);
    child_->SetIsContainerForFixedPositionLayers(true);

    grand_child_->AddChild(great_grand_child_);
    child_->AddChild(grand_child_);
    child_transform_layer_->AddChild(child_);
    outer_viewport_container_layer_->AddChild(child_transform_layer_);
    scroll_layer_->AddChild(outer_viewport_container_layer_);
    page_scale_layer_->AddChild(scroll_layer_);
    inner_viewport_container_layer_->AddChild(page_scale_layer_);
    root_->AddChild(inner_viewport_container_layer_);

    child_->SetIsResizedByBrowserControls(true);

    layer_tree_host_->SetRootLayer(root_);
    LayerTreeHost::ViewportLayers viewport_layers;
    viewport_layers.page_scale = page_scale_layer_;
    viewport_layers.inner_viewport_container = inner_viewport_container_layer_;
    viewport_layers.outer_viewport_container = outer_viewport_container_layer_;
    viewport_layers.inner_viewport_scroll = scroll_layer_;
    viewport_layers.outer_viewport_scroll = child_;
    layer_tree_host_->RegisterViewportLayers(viewport_layers);
  }

  void CommitAndUpdateImplPointers() {
    LayerTreeHostCommon::CalcDrawPropsMainInputsForTesting inputs(
        root_.get(), root_->bounds());
    inputs.inner_viewport_scroll_layer =
        layer_tree_host_->inner_viewport_scroll_layer();
    inputs.outer_viewport_scroll_layer =
        layer_tree_host_->outer_viewport_scroll_layer();
    LayerTreeHostCommon::CalculateDrawPropertiesForTesting(&inputs);

    // Since scroll deltas aren't sent back to the main thread in this test
    // setup, clear them to maintain consistent state.
    if (root_impl_) {
      SetScrollOffsetDelta(scroll_layer_impl_, gfx::Vector2dF());
      SetScrollOffsetDelta(child_impl_, gfx::Vector2dF());
      SetScrollOffsetDelta(grand_child_impl_, gfx::Vector2dF());
    }
    root_impl_ = layer_tree_host_->CommitAndCreateLayerImplTree();
    layer_tree_impl_ = root_impl_->layer_tree_impl();
    inner_viewport_container_layer_impl_ =
        layer_tree_impl_->LayerById(inner_viewport_container_layer_->id());
    scroll_layer_impl_ = layer_tree_impl_->LayerById(scroll_layer_->id());
    outer_viewport_container_layer_impl_ =
        layer_tree_impl_->LayerById(outer_viewport_container_layer_->id());
    child_transform_layer_impl_ =
        layer_tree_impl_->LayerById(child_transform_layer_->id());
    child_impl_ = layer_tree_impl_->LayerById(child_->id());
    grand_child_impl_ = layer_tree_impl_->LayerById(grand_child_->id());
    great_grand_child_impl_ =
        layer_tree_impl_->LayerById(great_grand_child_->id());
  }

 protected:
  FakeLayerTreeHostClient fake_client_;
  TestTaskGraphRunner task_graph_runner_;
  std::unique_ptr<AnimationHost> animation_host_;
  std::unique_ptr<FakeLayerTreeHost> layer_tree_host_;
  scoped_refptr<Layer> root_;
  scoped_refptr<Layer> page_scale_layer_;
  scoped_refptr<Layer> inner_viewport_container_layer_;
  scoped_refptr<Layer> scroll_layer_;
  scoped_refptr<Layer> outer_viewport_container_layer_;
  scoped_refptr<Layer> child_transform_layer_;
  scoped_refptr<Layer> child_;
  scoped_refptr<Layer> grand_child_;
  scoped_refptr<Layer> great_grand_child_;
  LayerTreeImpl* layer_tree_impl_;
  LayerImpl* root_impl_;
  LayerImpl* inner_viewport_container_layer_impl_;
  LayerImpl* scroll_layer_impl_;
  LayerImpl* outer_viewport_container_layer_impl_;
  LayerImpl* child_transform_layer_impl_;
  LayerImpl* child_impl_;
  LayerImpl* grand_child_impl_;
  LayerImpl* great_grand_child_impl_;

  LayerPositionConstraint fixed_to_top_left_;
  LayerPositionConstraint fixed_to_bottom_right_;

  // LayerImpl should not be aware of synced property logics, this function is
  // a hack for the test to arbitrarily set the scroll delta for setting up.
  static void SetScrollOffsetDelta(LayerImpl* layer_impl,
                                   const gfx::Vector2dF& delta) {
    if (layer_impl->layer_tree_impl()
            ->property_trees()
            ->scroll_tree.SetScrollOffsetDeltaForTesting(
                layer_impl->element_id(), delta))
      layer_impl->layer_tree_impl()->DidUpdateScrollOffset(
          layer_impl->element_id());
  }
};

TEST_F(LayerPositionConstraintTest,
     ScrollCompensationForFixedPositionLayerWithDirectContainer) {
  // This test checks for correct scroll compensation when the fixed-position
  // container is the direct parent of the fixed-position layer.
  child_->SetIsContainerForFixedPositionLayers(true);
  grand_child_->SetPositionConstraint(fixed_to_top_left_);

  CommitAndUpdateImplPointers();

  // Case 1: scroll delta of 0, 0
  SetScrollOffsetDelta(child_impl_, gfx::Vector2d(0, 0));
  ExecuteCalculateDrawProperties(root_impl_);

  gfx::Transform expected_child_transform;
  gfx::Transform expected_grand_child_transform = expected_child_transform;

  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_child_transform,
                                  child_impl_->DrawTransform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_grand_child_transform,
                                  grand_child_impl_->DrawTransform());

  // Case 2: scroll delta of 10, 10
  SetScrollOffsetDelta(child_impl_, gfx::Vector2d(10, 10));
  child_impl_->SetDrawsContent(true);
  ExecuteCalculateDrawProperties(root_impl_);

  // Here the child is affected by scroll delta, but the fixed position
  // grand_child should not be affected.
  expected_child_transform.MakeIdentity();
  expected_child_transform.Translate(-10.0, -10.0);

  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_child_transform,
                                  child_impl_->DrawTransform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_grand_child_transform,
                                  grand_child_impl_->DrawTransform());

  // Case 3: fixed-container size delta of 20, 20
  outer_viewport_container_layer_impl_->SetViewportBoundsDelta(
      gfx::Vector2d(20, 20));
  ExecuteCalculateDrawProperties(root_impl_);

  // Top-left fixed-position layer should not be affected by container size.
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_child_transform,
                                  child_impl_->DrawTransform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_grand_child_transform,
                                  grand_child_impl_->DrawTransform());

  // Case 4: Bottom-right fixed-position layer.
  grand_child_->SetPositionConstraint(fixed_to_bottom_right_);
  CommitAndUpdateImplPointers();

  SetScrollOffsetDelta(child_impl_, gfx::Vector2d(10, 10));
  outer_viewport_container_layer_impl_->SetViewportBoundsDelta(
      gfx::Vector2d(20, 20));
  ExecuteCalculateDrawProperties(root_impl_);

  // Bottom-right fixed-position layer moves as container resizes.
  expected_grand_child_transform.MakeIdentity();
  // Apply size delta from the child(container) layer.
  expected_grand_child_transform.Translate(20.0, 20.0);

  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_child_transform,
                                  child_impl_->DrawTransform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_grand_child_transform,
                                  grand_child_impl_->DrawTransform());
}

TEST_F(LayerPositionConstraintTest,
     ScrollCompensationForFixedPositionLayerWithDistantContainer) {
  // This test checks for correct scroll compensation when the fixed-position
  // container is NOT the direct parent of the fixed-position layer.
  child_->SetIsContainerForFixedPositionLayers(true);
  grand_child_->SetPosition(gfx::PointF(8.f, 6.f));
  great_grand_child_->SetPositionConstraint(fixed_to_top_left_);

  CommitAndUpdateImplPointers();

  // Case 1: scroll delta of 0, 0
  SetScrollOffsetDelta(child_impl_, gfx::Vector2d(0, 0));
  child_impl_->SetDrawsContent(true);
  ExecuteCalculateDrawProperties(root_impl_);

  gfx::Transform expected_child_transform;
  gfx::Transform expected_grand_child_transform;
  expected_grand_child_transform.Translate(8.0, 6.0);

  gfx::Transform expected_great_grand_child_transform =
      expected_grand_child_transform;

  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_child_transform,
                                  child_impl_->DrawTransform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_grand_child_transform,
                                  grand_child_impl_->DrawTransform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_great_grand_child_transform,
                                  great_grand_child_impl_->DrawTransform());

  // Case 2: scroll delta of 10, 10
  SetScrollOffsetDelta(child_impl_, gfx::Vector2d(10, 10));
  ExecuteCalculateDrawProperties(root_impl_);

  // Here the child and grand_child are affected by scroll delta, but the fixed
  // position great_grand_child should not be affected.
  expected_child_transform.MakeIdentity();
  expected_child_transform.Translate(-10.0, -10.0);
  expected_grand_child_transform.MakeIdentity();
  expected_grand_child_transform.Translate(-2.0, -4.0);
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_child_transform,
                                  child_impl_->DrawTransform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_grand_child_transform,
                                  grand_child_impl_->DrawTransform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_great_grand_child_transform,
                                  great_grand_child_impl_->DrawTransform());

  // Case 3: fixed-container size delta of 20, 20
  outer_viewport_container_layer_impl_->SetViewportBoundsDelta(
      gfx::Vector2d(20, 20));
  ExecuteCalculateDrawProperties(root_impl_);

  // Top-left fixed-position layer should not be affected by container size.
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_child_transform,
                                  child_impl_->DrawTransform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_grand_child_transform,
                                  grand_child_impl_->DrawTransform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_great_grand_child_transform,
                                  great_grand_child_impl_->DrawTransform());

  // Case 4: Bottom-right fixed-position layer.
  great_grand_child_->SetPositionConstraint(fixed_to_bottom_right_);
  CommitAndUpdateImplPointers();
  SetScrollOffsetDelta(child_impl_, gfx::Vector2d(10, 10));
  outer_viewport_container_layer_impl_->SetViewportBoundsDelta(
      gfx::Vector2d(20, 20));
  ExecuteCalculateDrawProperties(root_impl_);

  // Bottom-right fixed-position layer moves as container resizes.
  expected_great_grand_child_transform.MakeIdentity();
  // Apply size delta from the child(container) layer.
  expected_great_grand_child_transform.Translate(20.0, 20.0);
  // Apply layer position from the grand child layer.
  expected_great_grand_child_transform.Translate(8.0, 6.0);

  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_child_transform,
                                  child_impl_->DrawTransform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_grand_child_transform,
                                  grand_child_impl_->DrawTransform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_great_grand_child_transform,
                                  great_grand_child_impl_->DrawTransform());
}

TEST_F(LayerPositionConstraintTest,
     ScrollCompensationForFixedPositionLayerWithMultipleScrollDeltas) {
  // This test checks for correct scroll compensation when the fixed-position
  // container has multiple ancestors that have nonzero scroll delta before
  // reaching the space where the layer is fixed.
  gfx::Transform rotation_about_z;
  rotation_about_z.RotateAboutZAxis(90.0);

  child_transform_layer_->SetIsContainerForFixedPositionLayers(true);
  child_transform_layer_->SetTransform(rotation_about_z);
  grand_child_->SetPosition(gfx::PointF(8.f, 6.f));
  great_grand_child_->SetPositionConstraint(fixed_to_top_left_);

  CommitAndUpdateImplPointers();

  // Case 1: scroll delta of 0, 0
  SetScrollOffsetDelta(child_impl_, gfx::Vector2d(0, 0));
  child_impl_->SetDrawsContent(true);
  ExecuteCalculateDrawProperties(root_impl_);

  gfx::Transform expected_child_transform;
  expected_child_transform.PreconcatTransform(rotation_about_z);

  gfx::Transform expected_grand_child_transform;
  expected_grand_child_transform.PreconcatTransform(
      rotation_about_z);  // child's local transform is inherited
  // translation because of position occurs before layer's local transform.
  expected_grand_child_transform.Translate(8.0, 6.0);

  gfx::Transform expected_great_grand_child_transform =
      expected_grand_child_transform;

  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_child_transform,
                                  child_impl_->DrawTransform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_grand_child_transform,
                                  grand_child_impl_->DrawTransform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_great_grand_child_transform,
                                  great_grand_child_impl_->DrawTransform());

  // Case 2: scroll delta of 10, 20
  SetScrollOffsetDelta(child_impl_, gfx::Vector2d(10, 0));
  SetScrollOffsetDelta(grand_child_impl_, gfx::Vector2d(5, 0));
  ExecuteCalculateDrawProperties(root_impl_);

  // Here the child and grand_child are affected by scroll delta, but the fixed
  // position great_grand_child should not be affected.
  expected_child_transform.MakeIdentity();
  expected_child_transform.PreconcatTransform(rotation_about_z);
  expected_child_transform.Translate(-10.0, 0.0);  // scroll delta

  expected_grand_child_transform.MakeIdentity();
  expected_grand_child_transform.PreconcatTransform(
      rotation_about_z);  // child's local transform is inherited
  expected_grand_child_transform.Translate(
      -10.0, 0.0);  // child's scroll delta is inherited
  expected_grand_child_transform.Translate(-5.0,
                                           0.0);  // grand_child's scroll delta
  // translation because of position
  expected_grand_child_transform.Translate(8.0, 6.0);

  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_child_transform,
                                  child_impl_->DrawTransform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_grand_child_transform,
                                  grand_child_impl_->DrawTransform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_great_grand_child_transform,
                                  great_grand_child_impl_->DrawTransform());
}

TEST_F(LayerPositionConstraintTest,
     ScrollCompensationForFixedPositionWithIntermediateSurfaceAndTransforms) {
  // This test checks for correct scroll compensation when the fixed-position
  // container contributes to a different render surface than the fixed-position
  // layer. In this case, the surface draw transforms also have to be accounted
  // for when checking the scroll delta.
  child_->SetIsContainerForFixedPositionLayers(true);
  grand_child_->SetPosition(gfx::PointF(8.f, 6.f));
  grand_child_->SetForceRenderSurfaceForTesting(true);
  great_grand_child_->SetPositionConstraint(fixed_to_top_left_);

  gfx::Transform rotation_about_z;
  rotation_about_z.RotateAboutZAxis(90.0);
  great_grand_child_->SetTransform(rotation_about_z);

  CommitAndUpdateImplPointers();

  // Case 1: scroll delta of 0, 0
  SetScrollOffsetDelta(child_impl_, gfx::Vector2d(0, 0));
  ExecuteCalculateDrawProperties(root_impl_);

  gfx::Transform expected_child_transform;
  gfx::Transform expected_surface_draw_transform;
  expected_surface_draw_transform.Translate(8.0, 6.0);
  gfx::Transform expected_grand_child_transform;
  gfx::Transform expected_great_grand_child_transform;
  expected_great_grand_child_transform.PreconcatTransform(rotation_about_z);
  EXPECT_TRUE(GetRenderSurface(grand_child_impl_));
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_child_transform,
                                  child_impl_->DrawTransform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      expected_surface_draw_transform,
      GetRenderSurface(grand_child_impl_)->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_grand_child_transform,
                                  grand_child_impl_->DrawTransform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_great_grand_child_transform,
                                  great_grand_child_impl_->DrawTransform());

  // Case 2: scroll delta of 10, 30
  SetScrollOffsetDelta(child_impl_, gfx::Vector2d(10, 30));
  child_impl_->SetDrawsContent(true);
  ExecuteCalculateDrawProperties(root_impl_);

  // Here the grand_child remains unchanged, because it scrolls along with the
  // render surface, and the translation is actually in the render surface. But,
  // the fixed position great_grand_child is more awkward: its actually being
  // drawn with respect to the render surface, but it needs to remain fixed with
  // resepct to a container beyond that surface. So, the net result is that,
  // unlike previous tests where the fixed position layer's transform remains
  // unchanged, here the fixed position layer's transform explicitly contains
  // the translation that cancels out the scroll.
  expected_child_transform.MakeIdentity();
  expected_child_transform.Translate(-10.0, -30.0);  // scroll delta

  expected_surface_draw_transform.MakeIdentity();
  expected_surface_draw_transform.Translate(-10.0, -30.0);  // scroll delta
  expected_surface_draw_transform.Translate(8.0, 6.0);

  expected_great_grand_child_transform.MakeIdentity();
  // explicit canceling out the scroll delta that gets embedded in the fixed
  // position layer's surface.
  expected_great_grand_child_transform.Translate(10.0, 30.0);
  expected_great_grand_child_transform.PreconcatTransform(rotation_about_z);

  EXPECT_TRUE(GetRenderSurface(grand_child_impl_));
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_child_transform,
                                  child_impl_->DrawTransform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      expected_surface_draw_transform,
      GetRenderSurface(grand_child_impl_)->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_grand_child_transform,
                                  grand_child_impl_->DrawTransform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_great_grand_child_transform,
                                  great_grand_child_impl_->DrawTransform());

  // Case 3: fixed-container size delta of 20, 20
  outer_viewport_container_layer_impl_->SetViewportBoundsDelta(
      gfx::Vector2d(20, 20));
  ExecuteCalculateDrawProperties(root_impl_);

  // Top-left fixed-position layer should not be affected by container size.
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_child_transform,
                                  child_impl_->DrawTransform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_grand_child_transform,
                                  grand_child_impl_->DrawTransform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_great_grand_child_transform,
                                  great_grand_child_impl_->DrawTransform());

  // Case 4: Bottom-right fixed-position layer.
  great_grand_child_->SetPositionConstraint(fixed_to_bottom_right_);

  CommitAndUpdateImplPointers();
  SetScrollOffsetDelta(child_impl_, gfx::Vector2d(10, 30));
  outer_viewport_container_layer_impl_->SetViewportBoundsDelta(
      gfx::Vector2d(20, 20));

  ExecuteCalculateDrawProperties(root_impl_);

  // Bottom-right fixed-position layer moves as container resizes.
  expected_great_grand_child_transform.MakeIdentity();
  // explicit canceling out the scroll delta that gets embedded in the fixed
  // position layer's surface.
  expected_great_grand_child_transform.Translate(10.0, 30.0);
  // Also apply size delta in the child(container) layer space.
  expected_great_grand_child_transform.Translate(20.0, 20.0);
  expected_great_grand_child_transform.PreconcatTransform(rotation_about_z);

  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_child_transform,
                                  child_impl_->DrawTransform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_grand_child_transform,
                                  grand_child_impl_->DrawTransform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_great_grand_child_transform,
                                  great_grand_child_impl_->DrawTransform());
}

TEST_F(LayerPositionConstraintTest,
     ScrollCompensationForFixedPositionLayerWithMultipleIntermediateSurfaces) {
  // This test checks for correct scroll compensation when the fixed-position
  // container contributes to a different render surface than the fixed-position
  // layer, with additional render surfaces in-between. This checks that the
  // conversion to ancestor surfaces is accumulated properly in the final matrix
  // transform.

  // Add one more layer to the test tree for this scenario.
  scoped_refptr<Layer> fixed_position_child =
      base::WrapRefCounted(new LayerWithForcedDrawsContent());
  SetLayerPropertiesForTesting(fixed_position_child.get(), gfx::Transform(),
                               gfx::Point3F(), gfx::PointF(),
                               gfx::Size(100, 100), true);
  great_grand_child_->AddChild(fixed_position_child);

  // Actually set up the scenario here.
  child_->SetIsContainerForFixedPositionLayers(true);
  grand_child_->SetPosition(gfx::PointF(8.f, 6.f));
  grand_child_->SetForceRenderSurfaceForTesting(true);
  great_grand_child_->SetPosition(gfx::PointF(40.f, 60.f));
  great_grand_child_->SetForceRenderSurfaceForTesting(true);
  fixed_position_child->SetPositionConstraint(fixed_to_top_left_);

  // The additional rotation, which is non-commutative with translations, helps
  // to verify that we have correct order-of-operations in the final scroll
  // compensation.  Note that rotating about the center of the layer ensures we
  // do not accidentally clip away layers that we want to test.
  gfx::Transform rotation_about_z;
  rotation_about_z.Translate(50.0, 50.0);
  rotation_about_z.RotateAboutZAxis(90.0);
  rotation_about_z.Translate(-50.0, -50.0);
  fixed_position_child->SetTransform(rotation_about_z);

  CommitAndUpdateImplPointers();
  LayerImpl* fixed_position_child_impl =
      layer_tree_impl_->LayerById(fixed_position_child->id());

  // Case 1: scroll delta of 0, 0
  SetScrollOffsetDelta(child_impl_, gfx::Vector2d(0, 0));
  child_impl_->SetDrawsContent(true);
  ExecuteCalculateDrawProperties(root_impl_);

  gfx::Transform expected_child_transform;

  gfx::Transform expected_grand_child_surface_draw_transform;
  expected_grand_child_surface_draw_transform.Translate(8.0, 6.0);

  gfx::Transform expected_grand_child_transform;

  gfx::Transform expected_great_grand_child_surface_draw_transform;
  expected_great_grand_child_surface_draw_transform.Translate(40.0, 60.0);

  gfx::Transform expected_great_grand_child_transform;

  gfx::Transform expected_fixed_position_child_transform;
  expected_fixed_position_child_transform.PreconcatTransform(rotation_about_z);

  EXPECT_TRUE(GetRenderSurface(grand_child_impl_));
  EXPECT_TRUE(GetRenderSurface(great_grand_child_impl_));
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_child_transform,
                                  child_impl_->DrawTransform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      expected_grand_child_surface_draw_transform,
      GetRenderSurface(grand_child_impl_)->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_grand_child_transform,
                                  grand_child_impl_->DrawTransform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      expected_great_grand_child_surface_draw_transform,
      GetRenderSurface(great_grand_child_impl_)->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_great_grand_child_transform,
                                  great_grand_child_impl_->DrawTransform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_fixed_position_child_transform,
                                  fixed_position_child_impl->DrawTransform());

  // Case 2: scroll delta of 10, 30
  SetScrollOffsetDelta(child_impl_, gfx::Vector2d(10, 30));
  ExecuteCalculateDrawProperties(root_impl_);

  expected_child_transform.MakeIdentity();
  expected_child_transform.Translate(-10.0, -30.0);  // scroll delta

  expected_grand_child_surface_draw_transform.MakeIdentity();
  expected_grand_child_surface_draw_transform.Translate(-10.0,
                                                        -30.0);  // scroll delta
  expected_grand_child_surface_draw_transform.Translate(8.0, 6.0);

  // grand_child, great_grand_child, and great_grand_child's surface are not
  // expected to change, since they are all not fixed, and they are all drawn
  // with respect to grand_child's surface that already has the scroll delta
  // accounted for.

  // But the great-great grandchild, "fixed_position_child", should have a
  // transform that explicitly cancels out the scroll delta.
  expected_fixed_position_child_transform.MakeIdentity();
  expected_fixed_position_child_transform.Translate(10.0, 30.0);
  expected_fixed_position_child_transform.PreconcatTransform(rotation_about_z);

  EXPECT_TRUE(GetRenderSurface(grand_child_impl_));
  EXPECT_TRUE(GetRenderSurface(great_grand_child_impl_));
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_child_transform,
                                  child_impl_->DrawTransform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      expected_grand_child_surface_draw_transform,
      GetRenderSurface(grand_child_impl_)->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_grand_child_transform,
                                  grand_child_impl_->DrawTransform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      expected_great_grand_child_surface_draw_transform,
      GetRenderSurface(great_grand_child_impl_)->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_great_grand_child_transform,
                                  great_grand_child_impl_->DrawTransform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_fixed_position_child_transform,
                                  fixed_position_child_impl->DrawTransform());

  // Case 3: fixed-container size delta of 20, 20
  outer_viewport_container_layer_impl_->SetViewportBoundsDelta(
      gfx::Vector2d(20, 20));
  ExecuteCalculateDrawProperties(root_impl_);

  // Top-left fixed-position layer should not be affected by container size.
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_child_transform,
                                  child_impl_->DrawTransform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_grand_child_transform,
                                  grand_child_impl_->DrawTransform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_great_grand_child_transform,
                                  great_grand_child_impl_->DrawTransform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_fixed_position_child_transform,
                                  fixed_position_child_impl->DrawTransform());

  // Case 4: Bottom-right fixed-position layer.
  fixed_position_child->SetPositionConstraint(fixed_to_bottom_right_);
  CommitAndUpdateImplPointers();
  fixed_position_child_impl =
      layer_tree_impl_->LayerById(fixed_position_child->id());
  SetScrollOffsetDelta(child_impl_, gfx::Vector2d(10, 30));
  outer_viewport_container_layer_impl_->SetViewportBoundsDelta(
      gfx::Vector2d(20, 20));
  ExecuteCalculateDrawProperties(root_impl_);

  // Bottom-right fixed-position layer moves as container resizes.
  expected_fixed_position_child_transform.MakeIdentity();
  // explicit canceling out the scroll delta that gets embedded in the fixed
  // position layer's surface.
  expected_fixed_position_child_transform.Translate(10.0, 30.0);
  // Also apply size delta in the child(container) layer space.
  expected_fixed_position_child_transform.Translate(20.0, 20.0);
  expected_fixed_position_child_transform.PreconcatTransform(rotation_about_z);

  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_child_transform,
                                  child_impl_->DrawTransform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_grand_child_transform,
                                  grand_child_impl_->DrawTransform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_great_grand_child_transform,
                                  great_grand_child_impl_->DrawTransform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_fixed_position_child_transform,
                                  fixed_position_child_impl->DrawTransform());
}

TEST_F(
    LayerPositionConstraintTest,
    ScrollCompensationForFixedPositionLayerWithMultipleSurfacesAndTransforms) {
  // This test checks for correct scroll compensation when the fixed-position
  // container contributes to a different render surface than the fixed-position
  // layer, with additional render surfaces in-between, and the fixed-position
  // container is transformed. This checks that the conversion to ancestor
  // surfaces is accumulated properly in the final matrix transform.

  // Add one more layer to the test tree for this scenario.
  scoped_refptr<Layer> fixed_position_child =
      base::WrapRefCounted(new LayerWithForcedDrawsContent());
  SetLayerPropertiesForTesting(fixed_position_child.get(), gfx::Transform(),
                               gfx::Point3F(), gfx::PointF(),
                               gfx::Size(100, 100), true);
  great_grand_child_->AddChild(fixed_position_child);

  // Actually set up the scenario here.
  child_transform_layer_->SetIsContainerForFixedPositionLayers(true);
  grand_child_->SetPosition(gfx::PointF(8.f, 6.f));
  grand_child_->SetForceRenderSurfaceForTesting(true);
  great_grand_child_->SetPosition(gfx::PointF(40.f, 60.f));
  great_grand_child_->SetForceRenderSurfaceForTesting(true);
  fixed_position_child->SetPositionConstraint(fixed_to_top_left_);

  // The additional rotations, which are non-commutative with translations, help
  // to verify that we have correct order-of-operations in the final scroll
  // compensation.  Note that rotating about the center of the layer ensures we
  // do not accidentally clip away layers that we want to test.
  gfx::Transform rotation_about_z;
  rotation_about_z.Translate(50.0, 50.0);
  rotation_about_z.RotateAboutZAxis(30.0);
  rotation_about_z.Translate(-50.0, -50.0);
  child_transform_layer_->SetTransform(rotation_about_z);
  fixed_position_child->SetTransform(rotation_about_z);

  CommitAndUpdateImplPointers();
  LayerImpl* fixed_position_child_impl =
      layer_tree_impl_->LayerById(fixed_position_child->id());

  // Case 1: scroll delta of 0, 0
  SetScrollOffsetDelta(child_impl_, gfx::Vector2d(0, 0));
  child_impl_->SetDrawsContent(true);
  ExecuteCalculateDrawProperties(root_impl_);

  gfx::Transform expected_child_transform;
  expected_child_transform.PreconcatTransform(rotation_about_z);

  gfx::Transform expected_grand_child_surface_draw_transform;
  expected_grand_child_surface_draw_transform.PreconcatTransform(
      rotation_about_z);
  expected_grand_child_surface_draw_transform.Translate(8.0, 6.0);

  gfx::Transform expected_grand_child_transform;

  gfx::Transform expected_great_grand_child_surface_draw_transform;
  expected_great_grand_child_surface_draw_transform.Translate(40.0, 60.0);

  gfx::Transform expected_great_grand_child_transform;

  gfx::Transform expected_fixed_position_child_transform;
  expected_fixed_position_child_transform.PreconcatTransform(rotation_about_z);

  EXPECT_TRUE(GetRenderSurface(grand_child_impl_));
  EXPECT_TRUE(GetRenderSurface(great_grand_child_impl_));
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_child_transform,
                                  child_impl_->DrawTransform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      expected_grand_child_surface_draw_transform,
      GetRenderSurface(grand_child_impl_)->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_grand_child_transform,
                                  grand_child_impl_->DrawTransform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      expected_great_grand_child_surface_draw_transform,
      GetRenderSurface(great_grand_child_impl_)->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_great_grand_child_transform,
                                  great_grand_child_impl_->DrawTransform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_fixed_position_child_transform,
                                  fixed_position_child_impl->DrawTransform());

  // Case 2: scroll delta of 10, 30
  SetScrollOffsetDelta(child_impl_, gfx::Vector2d(10, 30));
  ExecuteCalculateDrawProperties(root_impl_);

  expected_child_transform.MakeIdentity();
  expected_child_transform.PreconcatTransform(rotation_about_z);
  expected_child_transform.Translate(-10.0, -30.0);  // scroll delta

  expected_grand_child_surface_draw_transform.MakeIdentity();
  expected_grand_child_surface_draw_transform.PreconcatTransform(
      rotation_about_z);
  expected_grand_child_surface_draw_transform.Translate(-10.0,
                                                        -30.0);  // scroll delta
  expected_grand_child_surface_draw_transform.Translate(8.0, 6.0);

  // grand_child, great_grand_child, and great_grand_child's surface are not
  // expected to change, since they are all not fixed, and they are all drawn
  // with respect to grand_child's surface that already has the scroll delta
  // accounted for.

  // But the great-great grandchild, "fixed_position_child", should have a
  // transform that explicitly cancels out the scroll delta.
  expected_fixed_position_child_transform.MakeIdentity();
  // explicit canceling out the scroll delta that gets embedded in the fixed
  // position layer's surface.
  expected_fixed_position_child_transform.Translate(10.0, 30.0);
  expected_fixed_position_child_transform.PreconcatTransform(rotation_about_z);

  EXPECT_TRUE(GetRenderSurface(grand_child_impl_));
  EXPECT_TRUE(GetRenderSurface(great_grand_child_impl_));
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_child_transform,
                                  child_impl_->DrawTransform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      expected_grand_child_surface_draw_transform,
      GetRenderSurface(grand_child_impl_)->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_grand_child_transform,
                                  grand_child_impl_->DrawTransform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      expected_great_grand_child_surface_draw_transform,
      GetRenderSurface(great_grand_child_impl_)->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_great_grand_child_transform,
                                  great_grand_child_impl_->DrawTransform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_fixed_position_child_transform,
                                  fixed_position_child_impl->DrawTransform());
}

TEST_F(LayerPositionConstraintTest,
     ScrollCompensationForFixedPositionLayerWithContainerLayerThatHasSurface) {
  // This test checks for correct scroll compensation when the fixed-position
  // container itself has a render surface. In this case, the container layer
  // should be treated like a layer that contributes to a render target, and
  // that render target is completely irrelevant; it should not affect the
  // scroll compensation.
  child_->SetIsContainerForFixedPositionLayers(true);
  child_->SetForceRenderSurfaceForTesting(true);
  grand_child_->SetPositionConstraint(fixed_to_top_left_);

  CommitAndUpdateImplPointers();

  // Case 1: scroll delta of 0, 0
  SetScrollOffsetDelta(child_impl_, gfx::Vector2d(0, 0));
  ExecuteCalculateDrawProperties(root_impl_);

  gfx::Transform expected_surface_draw_transform;
  gfx::Transform expected_child_transform;
  gfx::Transform expected_grand_child_transform;
  EXPECT_TRUE(GetRenderSurface(child_impl_));
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      expected_surface_draw_transform,
      GetRenderSurface(child_impl_)->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_child_transform,
                                  child_impl_->DrawTransform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_grand_child_transform,
                                  grand_child_impl_->DrawTransform());

  // Case 2: scroll delta of 10, 10
  SetScrollOffsetDelta(child_impl_, gfx::Vector2d(10, 10));
  ExecuteCalculateDrawProperties(root_impl_);

  // The surface is translated by scroll delta, the child transform doesn't
  // change because it scrolls along with the surface, but the fixed position
  // grand_child needs to compensate for the scroll translation.
  expected_surface_draw_transform.MakeIdentity();
  expected_surface_draw_transform.Translate(-10.0, -10.0);
  expected_grand_child_transform.MakeIdentity();
  expected_grand_child_transform.Translate(10.0, 10.0);

  EXPECT_TRUE(GetRenderSurface(child_impl_));
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      expected_surface_draw_transform,
      GetRenderSurface(child_impl_)->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_child_transform,
                                  child_impl_->DrawTransform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_grand_child_transform,
                                  grand_child_impl_->DrawTransform());

  // Case 3: fixed-container size delta of 20, 20
  outer_viewport_container_layer_impl_->SetViewportBoundsDelta(
      gfx::Vector2d(20, 20));
  ExecuteCalculateDrawProperties(root_impl_);

  // Top-left fixed-position layer should not be affected by container size.
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_child_transform,
                                  child_impl_->DrawTransform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_grand_child_transform,
                                  grand_child_impl_->DrawTransform());

  // Case 4: Bottom-right fixed-position layer.
  grand_child_->SetPositionConstraint(fixed_to_bottom_right_);
  CommitAndUpdateImplPointers();
  SetScrollOffsetDelta(child_impl_, gfx::Vector2d(10, 10));
  outer_viewport_container_layer_impl_->SetViewportBoundsDelta(
      gfx::Vector2d(20, 20));
  ExecuteCalculateDrawProperties(root_impl_);

  // Bottom-right fixed-position layer moves as container resizes.
  expected_grand_child_transform.MakeIdentity();
  // The surface is translated by scroll delta, the child transform doesn't
  // change because it scrolls along with the surface, but the fixed position
  // grand_child needs to compensate for the scroll translation.
  expected_grand_child_transform.Translate(10.0, 10.0);
  // Apply size delta from the child(container) layer.
  expected_grand_child_transform.Translate(20.0, 20.0);

  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_child_transform,
                                  child_impl_->DrawTransform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_grand_child_transform,
                                  grand_child_impl_->DrawTransform());
}

TEST_F(LayerPositionConstraintTest,
     ScrollCompensationForFixedPositionLayerThatIsAlsoFixedPositionContainer) {
  // This test checks the scenario where a fixed-position layer also happens to
  // be a container itself for a descendant fixed position layer. In particular,
  // the layer should not accidentally be fixed to itself.
  child_->SetIsContainerForFixedPositionLayers(true);
  grand_child_->SetPositionConstraint(fixed_to_top_left_);

  // This should not confuse the grand_child. If correct, the grand_child would
  // still be considered fixed to its container (i.e. "child").
  grand_child_->SetIsContainerForFixedPositionLayers(true);

  CommitAndUpdateImplPointers();

  // Case 1: scroll delta of 0, 0
  SetScrollOffsetDelta(child_impl_, gfx::Vector2d(0, 0));
  child_impl_->SetDrawsContent(true);
  ExecuteCalculateDrawProperties(root_impl_);

  gfx::Transform expected_child_transform;
  gfx::Transform expected_grand_child_transform;
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_child_transform,
                                  child_impl_->DrawTransform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_grand_child_transform,
                                  grand_child_impl_->DrawTransform());

  // Case 2: scroll delta of 10, 10
  SetScrollOffsetDelta(child_impl_, gfx::Vector2d(10, 10));
  ExecuteCalculateDrawProperties(root_impl_);

  // Here the child is affected by scroll delta, but the fixed position
  // grand_child should not be affected.
  expected_child_transform.MakeIdentity();
  expected_child_transform.Translate(-10.0, -10.0);
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_child_transform,
                                  child_impl_->DrawTransform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_grand_child_transform,
                                  grand_child_impl_->DrawTransform());

  // Case 3: fixed-container size delta of 20, 20
  outer_viewport_container_layer_impl_->SetViewportBoundsDelta(
      gfx::Vector2d(20, 20));
  ExecuteCalculateDrawProperties(root_impl_);

  // Top-left fixed-position layer should not be affected by container size.
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_child_transform,
                                  child_impl_->DrawTransform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_grand_child_transform,
                                  grand_child_impl_->DrawTransform());

  // Case 4: Bottom-right fixed-position layer.
  grand_child_->SetPositionConstraint(fixed_to_bottom_right_);
  CommitAndUpdateImplPointers();
  SetScrollOffsetDelta(child_impl_, gfx::Vector2d(10, 10));
  outer_viewport_container_layer_impl_->SetViewportBoundsDelta(
      gfx::Vector2d(20, 20));

  ExecuteCalculateDrawProperties(root_impl_);

  // Bottom-right fixed-position layer moves as container resizes.
  expected_grand_child_transform.MakeIdentity();
  // Apply size delta from the child(container) layer.
  expected_grand_child_transform.Translate(20.0, 20.0);

  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_child_transform,
                                  child_impl_->DrawTransform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_grand_child_transform,
                                  grand_child_impl_->DrawTransform());
}

TEST_F(LayerPositionConstraintTest,
     ScrollCompensationForFixedWithinFixedWithSameContainer) {
  // This test checks scroll compensation for a fixed-position layer that is
  // inside of another fixed-position layer and both share the same container.
  // In this situation, the parent fixed-position layer will receive
  // the scroll compensation, and the child fixed-position layer does not
  // need to compensate further.
  child_->SetIsContainerForFixedPositionLayers(true);
  grand_child_->SetPositionConstraint(fixed_to_top_left_);

  // Note carefully - great_grand_child is fixed to bottom right, to test
  // sizeDelta being applied correctly; the compensation skips the grand_child
  // because it is fixed to top left.
  great_grand_child_->SetPositionConstraint(fixed_to_bottom_right_);

  CommitAndUpdateImplPointers();

  // Case 1: scrollDelta
  SetScrollOffsetDelta(child_impl_, gfx::Vector2d(10, 10));
  child_impl_->SetDrawsContent(true);
  ExecuteCalculateDrawProperties(root_impl_);

  // Here the child is affected by scroll delta, but the fixed position
  // grand_child should not be affected.
  gfx::Transform expected_child_transform;
  expected_child_transform.Translate(-10.0, -10.0);

  gfx::Transform expected_grand_child_transform;
  gfx::Transform expected_great_grand_child_transform;

  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_child_transform,
                                  child_impl_->DrawTransform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_grand_child_transform,
                                  grand_child_impl_->DrawTransform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_great_grand_child_transform,
                                  great_grand_child_impl_->DrawTransform());

  // Case 2: sizeDelta
  SetScrollOffsetDelta(child_impl_, gfx::Vector2d(0, 0));
  outer_viewport_container_layer_impl_->SetViewportBoundsDelta(
      gfx::Vector2d(20, 20));
  ExecuteCalculateDrawProperties(root_impl_);

  expected_child_transform.MakeIdentity();

  expected_grand_child_transform.MakeIdentity();

  // Fixed to bottom-right, size-delta compensation is applied.
  expected_great_grand_child_transform.MakeIdentity();
  expected_great_grand_child_transform.Translate(20.0, 20.0);

  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_child_transform,
                                  child_impl_->DrawTransform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_grand_child_transform,
                                  grand_child_impl_->DrawTransform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_great_grand_child_transform,
                                  great_grand_child_impl_->DrawTransform());
}

TEST_F(LayerPositionConstraintTest,
     ScrollCompensationForFixedWithinFixedWithInterveningContainer) {
  // This test checks scroll compensation for a fixed-position layer that is
  // inside of another fixed-position layer, but they have different fixed
  // position containers. In this situation, the child fixed-position element
  // would still have to compensate with respect to its container.

  // Add one more layer to the hierarchy for this test.
  scoped_refptr<Layer> great_great_grand_child =
      base::WrapRefCounted(new LayerWithForcedDrawsContent());
  great_grand_child_->AddChild(great_great_grand_child);

  child_->SetIsContainerForFixedPositionLayers(true);
  grand_child_->SetPositionConstraint(fixed_to_top_left_);
  great_grand_child_->SetIsContainerForFixedPositionLayers(true);
  great_grand_child_->SetElementId(
      LayerIdToElementIdForTesting(great_grand_child_->id()));
  great_grand_child_->SetScrollable(gfx::Size(100, 100));
  great_great_grand_child->SetPositionConstraint(fixed_to_top_left_);

  CommitAndUpdateImplPointers();

  LayerImpl* container1 = child_impl_;
  LayerImpl* fixed_to_container1 = grand_child_impl_;
  LayerImpl* container2 = great_grand_child_impl_;
  LayerImpl* fixed_to_container2 =
      layer_tree_impl_->LayerById(great_great_grand_child->id());

  SetScrollOffsetDelta(container1, gfx::Vector2d(0, 15));
  container1->SetDrawsContent(true);
  SetScrollOffsetDelta(container2, gfx::Vector2d(30, 0));
  container2->SetDrawsContent(true);
  ExecuteCalculateDrawProperties(root_impl_);

  gfx::Transform expected_container1_transform;
  expected_container1_transform.Translate(0.0, -15.0);

  gfx::Transform expected_fixed_to_container1_transform;

  // Since the container is a descendant of the fixed layer above,
  // the expected draw transform for container2 would not
  // include the scrollDelta that was applied to container1.
  gfx::Transform expected_container2_transform;
  expected_container2_transform.Translate(-30.0, 0.0);

  gfx::Transform expected_fixed_to_container2_transform;

  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_container1_transform,
                                  container1->DrawTransform());

  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_fixed_to_container1_transform,
                                  fixed_to_container1->DrawTransform());

  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_container2_transform,
                                  container2->DrawTransform());

  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_fixed_to_container2_transform,
                                  fixed_to_container2->DrawTransform());
}

TEST_F(LayerPositionConstraintTest,
       ScrollCompensationForOuterViewportBoundsDelta) {
  // This test checks for correct scroll compensation when the fixed-position
  // container is the outer viewport scroll layer and has non-zero bounds delta.
  scoped_refptr<Layer> fixed_child =
      base::WrapRefCounted(new LayerWithForcedDrawsContent());
  fixed_child->SetBounds(gfx::Size(300, 300));
  child_->AddChild(fixed_child);
  fixed_child->SetPositionConstraint(fixed_to_top_left_);

  CommitAndUpdateImplPointers();

  LayerImpl* fixed_child_impl =
      root_impl_->layer_tree_impl()->FindActiveTreeLayerById(fixed_child->id());

  // Case 1: fixed-container size delta of 20, 20
  SetScrollOffsetDelta(child_impl_, gfx::Vector2d(10, 10));
  child_impl_->SetDrawsContent(true);
  outer_viewport_container_layer_impl_->SetViewportBoundsDelta(
      gfx::Vector2d(20, 20));
  gfx::Transform expected_scroll_layer_transform;
  expected_scroll_layer_transform.Translate(-10.0, -10.0);
  gfx::Transform expected_fixed_child_transform;

  ExecuteCalculateDrawProperties(root_impl_);

  // Top-left fixed-position layer should not be affected by container size.
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_scroll_layer_transform,
                                  child_impl_->DrawTransform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_fixed_child_transform,
                                  fixed_child_impl->DrawTransform());

  // Case 2: Bottom-right fixed-position layer.
  fixed_child->SetPositionConstraint(fixed_to_bottom_right_);
  CommitAndUpdateImplPointers();
  fixed_child_impl =
      root_impl_->layer_tree_impl()->FindActiveTreeLayerById(fixed_child->id());

  SetScrollOffsetDelta(child_impl_, gfx::Vector2d(10, 10));
  outer_viewport_container_layer_impl_->SetViewportBoundsDelta(
      gfx::Vector2d(20, 20));
  ExecuteCalculateDrawProperties(root_impl_);

  // Bottom-right fixed-position layer moves as container resizes.
  expected_fixed_child_transform.MakeIdentity();
  // Apply size delta.
  expected_fixed_child_transform.Translate(20.0, 20.0);

  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_scroll_layer_transform,
                                  child_impl_->DrawTransform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_fixed_child_transform,
                                  fixed_child_impl->DrawTransform());
}

}  // namespace
}  // namespace cc
