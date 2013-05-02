// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/layer_position_constraint.h"

#include <vector>

#include "cc/layers/layer_impl.h"
#include "cc/test/fake_impl_proxy.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "cc/test/geometry_test_utils.h"
#include "cc/trees/layer_tree_host_common.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

void SetLayerPropertiesForTesting(LayerImpl* layer,
                                  const gfx::Transform& transform,
                                  const gfx::Transform& sublayer_transform,
                                  gfx::PointF anchor,
                                  gfx::PointF position,
                                  gfx::Size bounds,
                                  bool preserves3d) {
  layer->SetTransform(transform);
  layer->SetSublayerTransform(sublayer_transform);
  layer->SetAnchorPoint(anchor);
  layer->SetPosition(position);
  layer->SetBounds(bounds);
  layer->SetPreserves3d(preserves3d);
  layer->SetContentBounds(bounds);
}

void ExecuteCalculateDrawProperties(LayerImpl* root_layer,
                                    float device_scale_factor,
                                    float page_scale_factor,
                                    LayerImpl* page_scale_application_layer,
                                    bool can_use_lcd_text) {
  gfx::Transform identity_matrix;
  std::vector<LayerImpl*> dummy_render_surface_layer_list;
  int dummy_max_texture_size = 512;
  gfx::Size device_viewport_size =
      gfx::Size(root_layer->bounds().width() * device_scale_factor,
                root_layer->bounds().height() * device_scale_factor);

  // We are probably not testing what is intended if the root_layer bounds are
  // empty.
  DCHECK(!root_layer->bounds().IsEmpty());
  LayerTreeHostCommon::CalculateDrawProperties(
      root_layer,
      device_viewport_size,
      device_scale_factor,
      page_scale_factor,
      page_scale_application_layer,
      dummy_max_texture_size,
      can_use_lcd_text,
      &dummy_render_surface_layer_list);
}

void ExecuteCalculateDrawProperties(LayerImpl* root_layer) {
  LayerImpl* page_scale_application_layer = NULL;
  ExecuteCalculateDrawProperties(
      root_layer, 1.f, 1.f, page_scale_application_layer, false);
}

class LayerPositionConstraintTest : public testing::Test {
 public:
  LayerPositionConstraintTest()
      : host_impl_(&proxy_) {
    root_ = CreateTreeForTest();
    fixed_to_top_left_.set_is_fixed_position(true);
    fixed_to_bottom_right_.set_is_fixed_position(true);
    fixed_to_bottom_right_.set_is_fixed_to_right_edge(true);
    fixed_to_bottom_right_.set_is_fixed_to_bottom_edge(true);
  }

  scoped_ptr<LayerImpl> CreateTreeForTest() {
    scoped_ptr<LayerImpl> root =
        LayerImpl::Create(host_impl_.active_tree(), 1);
    scoped_ptr<LayerImpl> child =
        LayerImpl::Create(host_impl_.active_tree(), 2);
    scoped_ptr<LayerImpl> grand_child =
        LayerImpl::Create(host_impl_.active_tree(), 3);
    scoped_ptr<LayerImpl> great_grand_child =
        LayerImpl::Create(host_impl_.active_tree(), 4);

    gfx::Transform IdentityMatrix;
    gfx::PointF anchor;
    gfx::PointF position;
    gfx::Size bounds(100, 100);
    SetLayerPropertiesForTesting(root.get(),
                                 IdentityMatrix,
                                 IdentityMatrix,
                                 anchor,
                                 position,
                                 bounds,
                                 false);
    SetLayerPropertiesForTesting(child.get(),
                                 IdentityMatrix,
                                 IdentityMatrix,
                                 anchor,
                                 position,
                                 bounds,
                                 false);
    SetLayerPropertiesForTesting(grand_child.get(),
                                 IdentityMatrix,
                                 IdentityMatrix,
                                 anchor,
                                 position,
                                 bounds,
                                 false);
    SetLayerPropertiesForTesting(great_grand_child.get(),
                                 IdentityMatrix,
                                 IdentityMatrix,
                                 anchor,
                                 position,
                                 bounds,
                                 false);

    grand_child->AddChild(great_grand_child.Pass());
    child->AddChild(grand_child.Pass());
    root->AddChild(child.Pass());

    return root.Pass();
  }

 protected:
  FakeImplProxy proxy_;
  FakeLayerTreeHostImpl host_impl_;
  scoped_ptr<LayerImpl> root_;

  LayerPositionConstraint fixed_to_top_left_;
  LayerPositionConstraint fixed_to_bottom_right_;
};

TEST_F(LayerPositionConstraintTest,
     ScrollCompensationForFixedPositionLayerWithDirectContainer) {
  // This test checks for correct scroll compensation when the fixed-position
  // container is the direct parent of the fixed-position layer.
  LayerImpl* child = root_->children()[0];
  LayerImpl* grand_child = child->children()[0];

  child->SetIsContainerForFixedPositionLayers(true);
  grand_child->SetPositionConstraint(fixed_to_top_left_);

  // Case 1: scroll delta of 0, 0
  child->SetScrollDelta(gfx::Vector2d(0, 0));
  ExecuteCalculateDrawProperties(root_.get());

  gfx::Transform expected_child_transform;
  gfx::Transform expected_grand_child_transform = expected_child_transform;

  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_child_transform,
                                  child->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_grand_child_transform,
                                  grand_child->draw_transform());

  // Case 2: scroll delta of 10, 10
  child->SetScrollDelta(gfx::Vector2d(10, 10));
  ExecuteCalculateDrawProperties(root_.get());

  // Here the child is affected by scroll delta, but the fixed position
  // grand_child should not be affected.
  expected_child_transform.MakeIdentity();
  expected_child_transform.Translate(-10.0, -10.0);

  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_child_transform,
                                  child->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_grand_child_transform,
                                  grand_child->draw_transform());

  // Case 3: fixed-container size delta of 20, 20
  child->SetFixedContainerSizeDelta(gfx::Vector2d(20, 20));
  ExecuteCalculateDrawProperties(root_.get());

  // Top-left fixed-position layer should not be affected by container size.
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_child_transform,
                                  child->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_grand_child_transform,
                                  grand_child->draw_transform());

  // Case 4: Bottom-right fixed-position layer.
  grand_child->SetPositionConstraint(fixed_to_bottom_right_);
  ExecuteCalculateDrawProperties(root_.get());

  // Bottom-right fixed-position layer moves as container resizes.
  expected_grand_child_transform.MakeIdentity();
  // Apply size delta from the child(container) layer.
  expected_grand_child_transform.Translate(20.0, 20.0);

  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_child_transform,
                                  child->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_grand_child_transform,
                                  grand_child->draw_transform());
}

TEST_F(LayerPositionConstraintTest,
     ScrollCompensationForFixedPositionLayerWithTransformedDirectContainer) {
  // This test checks for correct scroll compensation when the fixed-position
  // container is the direct parent of the fixed-position layer, but that
  // container is transformed.  In this case, the fixed position element
  // inherits the container's transform, but the scroll delta that has to be
  // undone should not be affected by that transform.
  //
  // Transforms are in general non-commutative; using something like a
  // non-uniform scale helps to verify that translations and non-uniform scales
  // are applied in the correct order.
  LayerImpl* child = root_->children()[0];
  LayerImpl* grand_child = child->children()[0];

  // This scale will cause child and grand_child to be effectively 200 x 800
  // with respect to the render target.
  gfx::Transform non_uniform_scale;
  non_uniform_scale.Scale(2.0, 8.0);
  child->SetTransform(non_uniform_scale);

  child->SetIsContainerForFixedPositionLayers(true);
  grand_child->SetPositionConstraint(fixed_to_top_left_);

  // Case 1: scroll delta of 0, 0
  child->SetScrollDelta(gfx::Vector2d(0, 0));
  ExecuteCalculateDrawProperties(root_.get());

  gfx::Transform expected_child_transform;
  expected_child_transform.PreconcatTransform(non_uniform_scale);

  gfx::Transform expected_grand_child_transform = expected_child_transform;

  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_child_transform,
                                  child->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_grand_child_transform,
                                  grand_child->draw_transform());

  // Case 2: scroll delta of 10, 20
  child->SetScrollDelta(gfx::Vector2d(10, 20));
  ExecuteCalculateDrawProperties(root_.get());

  // The child should be affected by scroll delta, but the fixed position
  // grand_child should not be affected.
  expected_child_transform.MakeIdentity();
  expected_child_transform.Translate(-10.0, -20.0);  // scroll delta
  expected_child_transform.PreconcatTransform(non_uniform_scale);

  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_child_transform,
                                  child->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_grand_child_transform,
                                  grand_child->draw_transform());

  // Case 3: fixed-container size delta of 20, 20
  child->SetFixedContainerSizeDelta(gfx::Vector2d(20, 20));
  ExecuteCalculateDrawProperties(root_.get());

  // Top-left fixed-position layer should not be affected by container size.
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_child_transform,
                                  child->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_grand_child_transform,
                                  grand_child->draw_transform());

  // Case 4: Bottom-right fixed-position layer.
  grand_child->SetPositionConstraint(fixed_to_bottom_right_);
  ExecuteCalculateDrawProperties(root_.get());

  // Bottom-right fixed-position layer moves as container resizes.
  expected_grand_child_transform.MakeIdentity();
  // Apply child layer transform.
  expected_grand_child_transform.PreconcatTransform(non_uniform_scale);
  // Apply size delta from the child(container) layer.
  expected_grand_child_transform.Translate(20.0, 20.0);

  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_child_transform,
                                  child->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_grand_child_transform,
                                  grand_child->draw_transform());
}

TEST_F(LayerPositionConstraintTest,
     ScrollCompensationForFixedPositionLayerWithDistantContainer) {
  // This test checks for correct scroll compensation when the fixed-position
  // container is NOT the direct parent of the fixed-position layer.
  LayerImpl* child = root_->children()[0];
  LayerImpl* grand_child = child->children()[0];
  LayerImpl* great_grand_child = grand_child->children()[0];

  child->SetIsContainerForFixedPositionLayers(true);
  grand_child->SetPosition(gfx::PointF(8.f, 6.f));
  great_grand_child->SetPositionConstraint(fixed_to_top_left_);

  // Case 1: scroll delta of 0, 0
  child->SetScrollDelta(gfx::Vector2d(0, 0));
  ExecuteCalculateDrawProperties(root_.get());

  gfx::Transform expected_child_transform;
  gfx::Transform expected_grand_child_transform;
  expected_grand_child_transform.Translate(8.0, 6.0);

  gfx::Transform expected_great_grand_child_transform =
      expected_grand_child_transform;

  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_child_transform,
                                  child->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_grand_child_transform,
                                  grand_child->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_great_grand_child_transform,
                                  great_grand_child->draw_transform());

  // Case 2: scroll delta of 10, 10
  child->SetScrollDelta(gfx::Vector2d(10, 10));
  ExecuteCalculateDrawProperties(root_.get());

  // Here the child and grand_child are affected by scroll delta, but the fixed
  // position great_grand_child should not be affected.
  expected_child_transform.MakeIdentity();
  expected_child_transform.Translate(-10.0, -10.0);
  expected_grand_child_transform.MakeIdentity();
  expected_grand_child_transform.Translate(-2.0, -4.0);
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_child_transform,
                                  child->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_grand_child_transform,
                                  grand_child->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_great_grand_child_transform,
                                  great_grand_child->draw_transform());

  // Case 3: fixed-container size delta of 20, 20
  child->SetFixedContainerSizeDelta(gfx::Vector2d(20, 20));
  ExecuteCalculateDrawProperties(root_.get());

  // Top-left fixed-position layer should not be affected by container size.
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_child_transform,
                                  child->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_grand_child_transform,
                                  grand_child->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_great_grand_child_transform,
                                  great_grand_child->draw_transform());

  // Case 4: Bottom-right fixed-position layer.
  great_grand_child->SetPositionConstraint(fixed_to_bottom_right_);
  ExecuteCalculateDrawProperties(root_.get());

  // Bottom-right fixed-position layer moves as container resizes.
  expected_great_grand_child_transform.MakeIdentity();
  // Apply size delta from the child(container) layer.
  expected_great_grand_child_transform.Translate(20.0, 20.0);
  // Apply layer position from the grand child layer.
  expected_great_grand_child_transform.Translate(8.0, 6.0);

  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_child_transform,
                                  child->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_grand_child_transform,
                                  grand_child->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_great_grand_child_transform,
                                  great_grand_child->draw_transform());
}

TEST_F(LayerPositionConstraintTest,
     ScrollCompensationForFixedPositionLayerWithDistantContainerAndTransforms) {
  // This test checks for correct scroll compensation when the fixed-position
  // container is NOT the direct parent of the fixed-position layer, and the
  // hierarchy has various transforms that have to be processed in the correct
  // order.
  LayerImpl* child = root_->children()[0];
  LayerImpl* grand_child = child->children()[0];
  LayerImpl* great_grand_child = grand_child->children()[0];

  gfx::Transform rotation_about_z;
  rotation_about_z.RotateAboutZAxis(90.0);

  child->SetIsContainerForFixedPositionLayers(true);
  child->SetTransform(rotation_about_z);
  grand_child->SetPosition(gfx::PointF(8.f, 6.f));
  grand_child->SetTransform(rotation_about_z);
  // great_grand_child is positioned upside-down with respect to the render
  // target.
  great_grand_child->SetPositionConstraint(fixed_to_top_left_);

  // Case 1: scroll delta of 0, 0
  child->SetScrollDelta(gfx::Vector2d(0, 0));
  ExecuteCalculateDrawProperties(root_.get());

  gfx::Transform expected_child_transform;
  expected_child_transform.PreconcatTransform(rotation_about_z);

  gfx::Transform expected_grand_child_transform;
  expected_grand_child_transform.PreconcatTransform(
      rotation_about_z);  // child's local transform is inherited
  // translation because of position occurs before layer's local transform.
  expected_grand_child_transform.Translate(8.0, 6.0);
  expected_grand_child_transform.PreconcatTransform(
      rotation_about_z);  // grand_child's local transform

  gfx::Transform expected_great_grand_child_transform =
      expected_grand_child_transform;

  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_child_transform,
                                  child->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_grand_child_transform,
                                  grand_child->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_great_grand_child_transform,
                                  great_grand_child->draw_transform());

  // Case 2: scroll delta of 10, 20
  child->SetScrollDelta(gfx::Vector2d(10, 20));
  ExecuteCalculateDrawProperties(root_.get());

  // Here the child and grand_child are affected by scroll delta, but the fixed
  // position great_grand_child should not be affected.
  expected_child_transform.MakeIdentity();
  expected_child_transform.Translate(-10.0, -20.0);  // scroll delta
  expected_child_transform.PreconcatTransform(rotation_about_z);

  expected_grand_child_transform.MakeIdentity();
  expected_grand_child_transform.Translate(
      -10.0, -20.0);      // child's scroll delta is inherited
  expected_grand_child_transform.PreconcatTransform(
      rotation_about_z);  // child's local transform is inherited
  // translation because of position occurs before layer's local transform.
  expected_grand_child_transform.Translate(8.0, 6.0);
  expected_grand_child_transform.PreconcatTransform(
      rotation_about_z);  // grand_child's local transform

  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_child_transform,
                                  child->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_grand_child_transform,
                                  grand_child->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_great_grand_child_transform,
                                  great_grand_child->draw_transform());

  // Case 3: fixed-container size delta of 20, 20
  child->SetFixedContainerSizeDelta(gfx::Vector2d(20, 20));
  ExecuteCalculateDrawProperties(root_.get());

  // Top-left fixed-position layer should not be affected by container size.
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_child_transform,
                                  child->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_grand_child_transform,
                                  grand_child->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_great_grand_child_transform,
                                  great_grand_child->draw_transform());

  // Case 4: Bottom-right fixed-position layer.
  great_grand_child->SetPositionConstraint(fixed_to_bottom_right_);
  ExecuteCalculateDrawProperties(root_.get());

  // Bottom-right fixed-position layer moves as container resizes.
  expected_great_grand_child_transform.MakeIdentity();
  // Apply child layer transform.
  expected_great_grand_child_transform.PreconcatTransform(rotation_about_z);
  // Apply size delta from the child(container) layer.
  expected_great_grand_child_transform.Translate(20.0, 20.0);
  // Apply layer position from the grand child layer.
  expected_great_grand_child_transform.Translate(8.0, 6.0);
  // Apply grand child layer transform.
  expected_great_grand_child_transform.PreconcatTransform(rotation_about_z);

  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_child_transform,
                                  child->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_grand_child_transform,
                                  grand_child->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_great_grand_child_transform,
                                  great_grand_child->draw_transform());
}

TEST_F(LayerPositionConstraintTest,
     ScrollCompensationForFixedPositionLayerWithMultipleScrollDeltas) {
  // This test checks for correct scroll compensation when the fixed-position
  // container has multiple ancestors that have nonzero scroll delta before
  // reaching the space where the layer is fixed.  In this test, each scroll
  // delta occurs in a different space because of each layer's local transform.
  // This test checks for correct scroll compensation when the fixed-position
  // container is NOT the direct parent of the fixed-position layer, and the
  // hierarchy has various transforms that have to be processed in the correct
  // order.
  LayerImpl* child = root_->children()[0];
  LayerImpl* grand_child = child->children()[0];
  LayerImpl* great_grand_child = grand_child->children()[0];

  gfx::Transform rotation_about_z;
  rotation_about_z.RotateAboutZAxis(90.0);

  child->SetIsContainerForFixedPositionLayers(true);
  child->SetTransform(rotation_about_z);
  grand_child->SetPosition(gfx::PointF(8.f, 6.f));
  grand_child->SetTransform(rotation_about_z);
  // great_grand_child is positioned upside-down with respect to the render
  // target.
  great_grand_child->SetPositionConstraint(fixed_to_top_left_);

  // Case 1: scroll delta of 0, 0
  child->SetScrollDelta(gfx::Vector2d(0, 0));
  ExecuteCalculateDrawProperties(root_.get());

  gfx::Transform expected_child_transform;
  expected_child_transform.PreconcatTransform(rotation_about_z);

  gfx::Transform expected_grand_child_transform;
  expected_grand_child_transform.PreconcatTransform(
      rotation_about_z);  // child's local transform is inherited
  // translation because of position occurs before layer's local transform.
  expected_grand_child_transform.Translate(8.0, 6.0);
  expected_grand_child_transform.PreconcatTransform(
      rotation_about_z);  // grand_child's local transform

  gfx::Transform expected_great_grand_child_transform =
      expected_grand_child_transform;

  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_child_transform,
                                  child->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_grand_child_transform,
                                  grand_child->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_great_grand_child_transform,
                                  great_grand_child->draw_transform());

  // Case 2: scroll delta of 10, 20
  child->SetScrollDelta(gfx::Vector2d(10, 0));
  grand_child->SetScrollDelta(gfx::Vector2d(5, 0));
  ExecuteCalculateDrawProperties(root_.get());

  // Here the child and grand_child are affected by scroll delta, but the fixed
  // position great_grand_child should not be affected.
  expected_child_transform.MakeIdentity();
  expected_child_transform.Translate(-10.0, 0.0);  // scroll delta
  expected_child_transform.PreconcatTransform(rotation_about_z);

  expected_grand_child_transform.MakeIdentity();
  expected_grand_child_transform.Translate(
      -10.0, 0.0);        // child's scroll delta is inherited
  expected_grand_child_transform.PreconcatTransform(
      rotation_about_z);  // child's local transform is inherited
  expected_grand_child_transform.Translate(-5.0,
                                           0.0);  // grand_child's scroll delta
  // translation because of position occurs before layer's local transform.
  expected_grand_child_transform.Translate(8.0, 6.0);
  expected_grand_child_transform.PreconcatTransform(
      rotation_about_z);  // grand_child's local transform

  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_child_transform,
                                  child->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_grand_child_transform,
                                  grand_child->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_great_grand_child_transform,
                                  great_grand_child->draw_transform());

  // Case 3: fixed-container size delta of 20, 20
  child->SetFixedContainerSizeDelta(gfx::Vector2d(20, 20));
  ExecuteCalculateDrawProperties(root_.get());

  // Top-left fixed-position layer should not be affected by container size.
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_child_transform,
                                  child->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_grand_child_transform,
                                  grand_child->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_great_grand_child_transform,
                                  great_grand_child->draw_transform());

  // Case 4: Bottom-right fixed-position layer.
  great_grand_child->SetPositionConstraint(fixed_to_bottom_right_);
  ExecuteCalculateDrawProperties(root_.get());

  // Bottom-right fixed-position layer moves as container resizes.
  expected_great_grand_child_transform.MakeIdentity();
  // Apply child layer transform.
  expected_great_grand_child_transform.PreconcatTransform(rotation_about_z);
  // Apply size delta from the child(container) layer.
  expected_great_grand_child_transform.Translate(20.0, 20.0);
  // Apply layer position from the grand child layer.
  expected_great_grand_child_transform.Translate(8.0, 6.0);
  // Apply grand child layer transform.
  expected_great_grand_child_transform.PreconcatTransform(rotation_about_z);

  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_child_transform,
                                  child->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_grand_child_transform,
                                  grand_child->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_great_grand_child_transform,
                                  great_grand_child->draw_transform());
}

TEST_F(LayerPositionConstraintTest,
     ScrollCompensationForFixedPositionWithIntermediateSurfaceAndTransforms) {
  // This test checks for correct scroll compensation when the fixed-position
  // container contributes to a different render surface than the fixed-position
  // layer. In this case, the surface draw transforms also have to be accounted
  // for when checking the scroll delta.
  LayerImpl* child = root_->children()[0];
  LayerImpl* grand_child = child->children()[0];
  LayerImpl* great_grand_child = grand_child->children()[0];

  child->SetIsContainerForFixedPositionLayers(true);
  grand_child->SetPosition(gfx::PointF(8.f, 6.f));
  grand_child->SetForceRenderSurface(true);
  great_grand_child->SetPositionConstraint(fixed_to_top_left_);
  great_grand_child->SetDrawsContent(true);

  gfx::Transform rotation_about_z;
  rotation_about_z.RotateAboutZAxis(90.0);
  grand_child->SetTransform(rotation_about_z);

  // Case 1: scroll delta of 0, 0
  child->SetScrollDelta(gfx::Vector2d(0, 0));
  ExecuteCalculateDrawProperties(root_.get());

  gfx::Transform expected_child_transform;
  gfx::Transform expected_surface_draw_transform;
  expected_surface_draw_transform.Translate(8.0, 6.0);
  expected_surface_draw_transform.PreconcatTransform(rotation_about_z);
  gfx::Transform expected_grand_child_transform;
  gfx::Transform expected_great_grand_child_transform;
  ASSERT_TRUE(grand_child->render_surface());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_child_transform,
                                  child->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      expected_surface_draw_transform,
      grand_child->render_surface()->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_grand_child_transform,
                                  grand_child->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_great_grand_child_transform,
                                  great_grand_child->draw_transform());

  // Case 2: scroll delta of 10, 30
  child->SetScrollDelta(gfx::Vector2d(10, 30));
  ExecuteCalculateDrawProperties(root_.get());

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
  expected_surface_draw_transform.PreconcatTransform(rotation_about_z);

  // The rotation and its inverse are needed to place the scroll delta
  // compensation in the correct space. This test will fail if the
  // rotation/inverse are backwards, too, so it requires perfect order of
  // operations.
  expected_great_grand_child_transform.MakeIdentity();
  expected_great_grand_child_transform.PreconcatTransform(
      Inverse(rotation_about_z));
  // explicit canceling out the scroll delta that gets embedded in the fixed
  // position layer's surface.
  expected_great_grand_child_transform.Translate(10.0, 30.0);
  expected_great_grand_child_transform.PreconcatTransform(rotation_about_z);

  ASSERT_TRUE(grand_child->render_surface());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_child_transform,
                                  child->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      expected_surface_draw_transform,
      grand_child->render_surface()->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_grand_child_transform,
                                  grand_child->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_great_grand_child_transform,
                                  great_grand_child->draw_transform());

  // Case 3: fixed-container size delta of 20, 20
  child->SetFixedContainerSizeDelta(gfx::Vector2d(20, 20));
  ExecuteCalculateDrawProperties(root_.get());

  // Top-left fixed-position layer should not be affected by container size.
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_child_transform,
                                  child->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_grand_child_transform,
                                  grand_child->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_great_grand_child_transform,
                                  great_grand_child->draw_transform());

  // Case 4: Bottom-right fixed-position layer.
  great_grand_child->SetPositionConstraint(fixed_to_bottom_right_);
  ExecuteCalculateDrawProperties(root_.get());

  // Bottom-right fixed-position layer moves as container resizes.
  expected_great_grand_child_transform.MakeIdentity();
  // The rotation and its inverse are needed to place the scroll delta
  // compensation in the correct space. This test will fail if the
  // rotation/inverse are backwards, too, so it requires perfect order of
  // operations.
  expected_great_grand_child_transform.PreconcatTransform(
      Inverse(rotation_about_z));
  // explicit canceling out the scroll delta that gets embedded in the fixed
  // position layer's surface.
  expected_great_grand_child_transform.Translate(10.0, 30.0);
  // Also apply size delta in the child(container) layer space.
  expected_great_grand_child_transform.Translate(20.0, 20.0);
  expected_great_grand_child_transform.PreconcatTransform(rotation_about_z);

  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_child_transform,
                                  child->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_grand_child_transform,
                                  grand_child->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_great_grand_child_transform,
                                  great_grand_child->draw_transform());
}

TEST_F(LayerPositionConstraintTest,
     ScrollCompensationForFixedPositionLayerWithMultipleIntermediateSurfaces) {
  // This test checks for correct scroll compensation when the fixed-position
  // container contributes to a different render surface than the fixed-position
  // layer, with additional render surfaces in-between. This checks that the
  // conversion to ancestor surfaces is accumulated properly in the final matrix
  // transform.
  LayerImpl* child = root_->children()[0];
  LayerImpl* grand_child = child->children()[0];
  LayerImpl* great_grand_child = grand_child->children()[0];

  // Add one more layer to the test tree for this scenario.
  {
    gfx::Transform identity;
    scoped_ptr<LayerImpl> fixed_position_child =
        LayerImpl::Create(host_impl_.active_tree(), 5);
    SetLayerPropertiesForTesting(fixed_position_child.get(),
                                 identity,
                                 identity,
                                 gfx::PointF(),
                                 gfx::PointF(),
                                 gfx::Size(100, 100),
                                 false);
    great_grand_child->AddChild(fixed_position_child.Pass());
  }
  LayerImpl* fixed_position_child = great_grand_child->children()[0];

  // Actually set up the scenario here.
  child->SetIsContainerForFixedPositionLayers(true);
  grand_child->SetPosition(gfx::PointF(8.f, 6.f));
  grand_child->SetForceRenderSurface(true);
  great_grand_child->SetPosition(gfx::PointF(40.f, 60.f));
  great_grand_child->SetForceRenderSurface(true);
  fixed_position_child->SetPositionConstraint(fixed_to_top_left_);
  fixed_position_child->SetDrawsContent(true);

  // The additional rotations, which are non-commutative with translations, help
  // to verify that we have correct order-of-operations in the final scroll
  // compensation.  Note that rotating about the center of the layer ensures we
  // do not accidentally clip away layers that we want to test.
  gfx::Transform rotation_about_z;
  rotation_about_z.Translate(50.0, 50.0);
  rotation_about_z.RotateAboutZAxis(90.0);
  rotation_about_z.Translate(-50.0, -50.0);
  grand_child->SetTransform(rotation_about_z);
  great_grand_child->SetTransform(rotation_about_z);

  // Case 1: scroll delta of 0, 0
  child->SetScrollDelta(gfx::Vector2d(0, 0));
  ExecuteCalculateDrawProperties(root_.get());

  gfx::Transform expected_child_transform;

  gfx::Transform expected_grand_child_surface_draw_transform;
  expected_grand_child_surface_draw_transform.Translate(8.0, 6.0);
  expected_grand_child_surface_draw_transform.PreconcatTransform(
      rotation_about_z);

  gfx::Transform expected_grand_child_transform;

  gfx::Transform expected_great_grand_child_surface_draw_transform;
  expected_great_grand_child_surface_draw_transform.Translate(40.0, 60.0);
  expected_great_grand_child_surface_draw_transform.PreconcatTransform(
      rotation_about_z);

  gfx::Transform expected_great_grand_child_transform;

  gfx::Transform expected_fixed_position_child_transform;

  ASSERT_TRUE(grand_child->render_surface());
  ASSERT_TRUE(great_grand_child->render_surface());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_child_transform,
                                  child->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      expected_grand_child_surface_draw_transform,
      grand_child->render_surface()->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_grand_child_transform,
                                  grand_child->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      expected_great_grand_child_surface_draw_transform,
      great_grand_child->render_surface()->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_great_grand_child_transform,
                                  great_grand_child->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_fixed_position_child_transform,
                                  fixed_position_child->draw_transform());

  // Case 2: scroll delta of 10, 30
  child->SetScrollDelta(gfx::Vector2d(10, 30));
  ExecuteCalculateDrawProperties(root_.get());

  expected_child_transform.MakeIdentity();
  expected_child_transform.Translate(-10.0, -30.0);  // scroll delta

  expected_grand_child_surface_draw_transform.MakeIdentity();
  expected_grand_child_surface_draw_transform.Translate(-10.0,
                                                        -30.0);  // scroll delta
  expected_grand_child_surface_draw_transform.Translate(8.0, 6.0);
  expected_grand_child_surface_draw_transform.PreconcatTransform(
      rotation_about_z);

  // grand_child, great_grand_child, and great_grand_child's surface are not
  // expected to change, since they are all not fixed, and they are all drawn
  // with respect to grand_child's surface that already has the scroll delta
  // accounted for.

  // But the great-great grandchild, "fixed_position_child", should have a
  // transform that explicitly cancels out the scroll delta.  The expected
  // transform is: compound_draw_transform.Inverse() * translate(positive scroll
  // delta) * compound_origin_transform from great_grand_childSurface's origin
  // to the root surface.
  gfx::Transform compound_draw_transform;
  compound_draw_transform.Translate(8.0,
                                    6.0);  // origin translation of grand_child
  compound_draw_transform.PreconcatTransform(
      rotation_about_z);                   // rotation of grand_child
  compound_draw_transform.Translate(
      40.0, 60.0);        // origin translation of great_grand_child
  compound_draw_transform.PreconcatTransform(
      rotation_about_z);  // rotation of great_grand_child

  expected_fixed_position_child_transform.MakeIdentity();
  expected_fixed_position_child_transform.PreconcatTransform(
      Inverse(compound_draw_transform));
  // explicit canceling out the scroll delta that gets embedded in the fixed
  // position layer's surface.
  expected_fixed_position_child_transform.Translate(10.0, 30.0);
  expected_fixed_position_child_transform.PreconcatTransform(
      compound_draw_transform);

  ASSERT_TRUE(grand_child->render_surface());
  ASSERT_TRUE(great_grand_child->render_surface());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_child_transform,
                                  child->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      expected_grand_child_surface_draw_transform,
      grand_child->render_surface()->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_grand_child_transform,
                                  grand_child->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      expected_great_grand_child_surface_draw_transform,
      great_grand_child->render_surface()->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_great_grand_child_transform,
                                  great_grand_child->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_fixed_position_child_transform,
                                  fixed_position_child->draw_transform());


  // Case 3: fixed-container size delta of 20, 20
  child->SetFixedContainerSizeDelta(gfx::Vector2d(20, 20));
  ExecuteCalculateDrawProperties(root_.get());

  // Top-left fixed-position layer should not be affected by container size.
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_child_transform,
                                  child->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_grand_child_transform,
                                  grand_child->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_great_grand_child_transform,
                                  great_grand_child->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_fixed_position_child_transform,
                                  fixed_position_child->draw_transform());

  // Case 4: Bottom-right fixed-position layer.
  fixed_position_child->SetPositionConstraint(fixed_to_bottom_right_);
  ExecuteCalculateDrawProperties(root_.get());

  // Bottom-right fixed-position layer moves as container resizes.
  expected_fixed_position_child_transform.MakeIdentity();
  expected_fixed_position_child_transform.PreconcatTransform(
      Inverse(compound_draw_transform));
  // explicit canceling out the scroll delta that gets embedded in the fixed
  // position layer's surface.
  expected_fixed_position_child_transform.Translate(10.0, 30.0);
  // Also apply size delta in the child(container) layer space.
  expected_fixed_position_child_transform.Translate(20.0, 20.0);
  expected_fixed_position_child_transform.PreconcatTransform(
      compound_draw_transform);

  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_child_transform,
                                  child->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_grand_child_transform,
                                  grand_child->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_great_grand_child_transform,
                                  great_grand_child->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_fixed_position_child_transform,
                                  fixed_position_child->draw_transform());
}

TEST_F(LayerPositionConstraintTest,
     ScrollCompensationForFixedPositionLayerWithContainerLayerThatHasSurface) {
  // This test checks for correct scroll compensation when the fixed-position
  // container itself has a render surface. In this case, the container layer
  // should be treated like a layer that contributes to a render target, and
  // that render target is completely irrelevant; it should not affect the
  // scroll compensation.
  LayerImpl* child = root_->children()[0];
  LayerImpl* grand_child = child->children()[0];

  child->SetIsContainerForFixedPositionLayers(true);
  child->SetForceRenderSurface(true);
  grand_child->SetPositionConstraint(fixed_to_top_left_);
  grand_child->SetDrawsContent(true);

  // Case 1: scroll delta of 0, 0
  child->SetScrollDelta(gfx::Vector2d(0, 0));
  ExecuteCalculateDrawProperties(root_.get());

  gfx::Transform expected_surface_draw_transform;
  expected_surface_draw_transform.Translate(0.0, 0.0);
  gfx::Transform expected_child_transform;
  gfx::Transform expected_grand_child_transform;
  ASSERT_TRUE(child->render_surface());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_surface_draw_transform,
                                  child->render_surface()->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_child_transform,
                                  child->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_grand_child_transform,
                                  grand_child->draw_transform());

  // Case 2: scroll delta of 10, 10
  child->SetScrollDelta(gfx::Vector2d(10, 10));
  ExecuteCalculateDrawProperties(root_.get());

  // The surface is translated by scroll delta, the child transform doesn't
  // change because it scrolls along with the surface, but the fixed position
  // grand_child needs to compensate for the scroll translation.
  expected_surface_draw_transform.MakeIdentity();
  expected_surface_draw_transform.Translate(-10.0, -10.0);
  expected_grand_child_transform.MakeIdentity();
  expected_grand_child_transform.Translate(10.0, 10.0);

  ASSERT_TRUE(child->render_surface());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_surface_draw_transform,
                                  child->render_surface()->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_child_transform,
                                  child->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_grand_child_transform,
                                  grand_child->draw_transform());

  // Case 3: fixed-container size delta of 20, 20
  child->SetFixedContainerSizeDelta(gfx::Vector2d(20, 20));
  ExecuteCalculateDrawProperties(root_.get());

  // Top-left fixed-position layer should not be affected by container size.
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_child_transform,
                                  child->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_grand_child_transform,
                                  grand_child->draw_transform());

  // Case 4: Bottom-right fixed-position layer.
  grand_child->SetPositionConstraint(fixed_to_bottom_right_);
  ExecuteCalculateDrawProperties(root_.get());

  // Bottom-right fixed-position layer moves as container resizes.
  expected_grand_child_transform.MakeIdentity();
  // The surface is translated by scroll delta, the child transform doesn't
  // change because it scrolls along with the surface, but the fixed position
  // grand_child needs to compensate for the scroll translation.
  expected_grand_child_transform.Translate(10.0, 10.0);
  // Apply size delta from the child(container) layer.
  expected_grand_child_transform.Translate(20.0, 20.0);

  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_child_transform,
                                  child->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_grand_child_transform,
                                  grand_child->draw_transform());
}

TEST_F(LayerPositionConstraintTest,
     ScrollCompensationForFixedPositionLayerThatIsAlsoFixedPositionContainer) {
  // This test checks the scenario where a fixed-position layer also happens to
  // be a container itself for a descendant fixed position layer. In particular,
  // the layer should not accidentally be fixed to itself.
  LayerImpl* child = root_->children()[0];
  LayerImpl* grand_child = child->children()[0];

  child->SetIsContainerForFixedPositionLayers(true);
  grand_child->SetPositionConstraint(fixed_to_top_left_);

  // This should not confuse the grand_child. If correct, the grand_child would
  // still be considered fixed to its container (i.e. "child").
  grand_child->SetIsContainerForFixedPositionLayers(true);

  // Case 1: scroll delta of 0, 0
  child->SetScrollDelta(gfx::Vector2d(0, 0));
  ExecuteCalculateDrawProperties(root_.get());

  gfx::Transform expected_child_transform;
  gfx::Transform expected_grand_child_transform;
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_child_transform,
                                  child->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_grand_child_transform,
                                  grand_child->draw_transform());

  // Case 2: scroll delta of 10, 10
  child->SetScrollDelta(gfx::Vector2d(10, 10));
  ExecuteCalculateDrawProperties(root_.get());

  // Here the child is affected by scroll delta, but the fixed position
  // grand_child should not be affected.
  expected_child_transform.MakeIdentity();
  expected_child_transform.Translate(-10.0, -10.0);
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_child_transform,
                                  child->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_grand_child_transform,
                                  grand_child->draw_transform());

  // Case 3: fixed-container size delta of 20, 20
  child->SetFixedContainerSizeDelta(gfx::Vector2d(20, 20));
  ExecuteCalculateDrawProperties(root_.get());

  // Top-left fixed-position layer should not be affected by container size.
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_child_transform,
                                  child->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_grand_child_transform,
                                  grand_child->draw_transform());

  // Case 4: Bottom-right fixed-position layer.
  grand_child->SetPositionConstraint(fixed_to_bottom_right_);
  ExecuteCalculateDrawProperties(root_.get());

  // Bottom-right fixed-position layer moves as container resizes.
  expected_grand_child_transform.MakeIdentity();
  // Apply size delta from the child(container) layer.
  expected_grand_child_transform.Translate(20.0, 20.0);

  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_child_transform,
                                  child->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_grand_child_transform,
                                  grand_child->draw_transform());
}

TEST_F(LayerPositionConstraintTest,
     ScrollCompensationForFixedPositionLayerThatHasNoContainer) {
  // This test checks scroll compensation when a fixed-position layer does not
  // find any ancestor that is a "containerForFixedPositionLayers". In this
  // situation, the layer should be fixed to the viewport -- not the root_layer,
  // which may have transforms of its own.
  LayerImpl* child = root_->children()[0];
  LayerImpl* grand_child = child->children()[0];

  gfx::Transform rotation_by_z;
  rotation_by_z.RotateAboutZAxis(90.0);

  root_->SetTransform(rotation_by_z);
  grand_child->SetPositionConstraint(fixed_to_top_left_);

  // Case 1: root scroll delta of 0, 0
  root_->SetScrollDelta(gfx::Vector2d(0, 0));
  ExecuteCalculateDrawProperties(root_.get());

  gfx::Transform identity_matrix;

  EXPECT_TRANSFORMATION_MATRIX_EQ(identity_matrix, child->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(identity_matrix,
                                  grand_child->draw_transform());

  // Case 2: root scroll delta of 10, 10
  root_->SetScrollDelta(gfx::Vector2d(10, 20));
  ExecuteCalculateDrawProperties(root_.get());

  // The child is affected by scroll delta, but it is already implcitly
  // accounted for by the child's target surface (i.e. the root render surface).
  // The grand_child is not affected by the scroll delta, so its draw transform
  // needs to explicitly inverse-compensate for the scroll that's embedded in
  // the target surface.
  gfx::Transform expected_grand_child_transform;
  expected_grand_child_transform.PreconcatTransform(Inverse(rotation_by_z));
  // explicit cancelling out the scroll delta that gets embedded in the fixed
  // position layer's surface.
  expected_grand_child_transform.Translate(10.0, 20.0);
  expected_grand_child_transform.PreconcatTransform(rotation_by_z);

  EXPECT_TRANSFORMATION_MATRIX_EQ(identity_matrix, child->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_grand_child_transform,
                                  grand_child->draw_transform());


  // Case 3: fixed-container size delta of 20, 20
  root_->SetFixedContainerSizeDelta(gfx::Vector2d(20, 20));
  ExecuteCalculateDrawProperties(root_.get());

  // Top-left fixed-position layer should not be affected by container size.
  EXPECT_TRANSFORMATION_MATRIX_EQ(identity_matrix, child->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_grand_child_transform,
                                  grand_child->draw_transform());

  // Case 4: Bottom-right fixed-position layer.
  grand_child->SetPositionConstraint(fixed_to_bottom_right_);
  ExecuteCalculateDrawProperties(root_.get());

  // Root layer is not the fixed-container anyway.
  EXPECT_TRANSFORMATION_MATRIX_EQ(identity_matrix, child->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_grand_child_transform,
                                  grand_child->draw_transform());
}

}  // namespace
}  // namespace cc
