// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/property_tree.h"

#include "cc/input/main_thread_scrolling_reason.h"
#include "cc/proto/property_tree.pb.h"
#include "cc/test/geometry_test_utils.h"
#include "cc/trees/draw_property_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

TEST(PropertyTreeSerializationTest, TransformNodeDataSerialization) {
  TransformNodeData original;
  original.pre_local.Translate3d(1.f, 2.f, 3.f);
  original.local.Translate3d(3.f, 1.f, 5.f);
  original.post_local.Translate3d(1.f, 8.f, 3.f);
  original.to_parent.Translate3d(3.2f, 2.f, 3.f);
  original.to_target.Translate3d(2.6f, 2.f, 3.f);
  original.from_target.Translate3d(4.3f, 2.f, 3.f);
  original.to_screen.Translate3d(7.2f, 2.f, 4.5f);
  original.from_screen.Translate3d(2.f, 2.f, 7.f);
  original.target_id = 3;
  original.content_target_id = 4;
  original.source_node_id = 5;
  original.needs_local_transform_update = false;
  original.is_invertible = false;
  original.ancestors_are_invertible = false;
  original.is_animated = false;
  original.to_screen_is_animated = false;
  original.has_only_translation_animations = false;
  original.to_screen_has_scale_animation = false;
  original.flattens_inherited_transform = false;
  original.node_and_ancestors_are_flat = false;
  original.node_and_ancestors_have_only_integer_translation = false;
  original.scrolls = false;
  original.needs_sublayer_scale = false;
  original.affected_by_inner_viewport_bounds_delta_x = false;
  original.affected_by_inner_viewport_bounds_delta_y = false;
  original.affected_by_outer_viewport_bounds_delta_x = false;
  original.affected_by_outer_viewport_bounds_delta_y = false;
  original.in_subtree_of_page_scale_layer = false;
  original.post_local_scale_factor = 0.5f;
  original.local_maximum_animation_target_scale = 0.6f;
  original.local_starting_animation_scale = 0.7f;
  original.combined_maximum_animation_target_scale = 0.8f;
  original.combined_starting_animation_scale = 0.9f;
  original.sublayer_scale = gfx::Vector2dF(0.5f, 0.5f);
  original.scroll_offset = gfx::ScrollOffset(1.5f, 1.5f);
  original.scroll_snap = gfx::Vector2dF(0.4f, 0.4f);
  original.source_offset = gfx::Vector2dF(2.5f, 2.4f);
  original.source_to_parent = gfx::Vector2dF(3.2f, 3.2f);

  proto::TreeNode proto;
  original.ToProtobuf(&proto);
  TransformNodeData result;
  result.FromProtobuf(proto);

  EXPECT_EQ(original, result);
}

TEST(PropertyTreeSerializationTest, TransformNodeSerialization) {
  TransformNode original;
  original.id = 3;
  original.parent_id = 2;
  original.owner_id = 4;

  proto::TreeNode proto;
  original.ToProtobuf(&proto);
  TransformNode result;
  result.FromProtobuf(proto);

  EXPECT_EQ(original, result);
}

TEST(PropertyTreeSerializationTest, TransformTreeSerialization) {
  TransformTree original;
  TransformNode& root = *original.Node(0);
  root.data.target_id = 3;
  root.data.content_target_id = 4;
  TransformNode second;
  second.data.local.Translate3d(2.f, 2.f, 0.f);
  second.data.source_node_id = 0;
  second.data.target_id = 0;
  TransformNode third;
  third.data.scrolls = true;
  third.data.source_node_id = 1;
  third.data.target_id = 0;

  original.Insert(second, 0);
  original.Insert(third, 1);
  original.set_needs_update(true);

  original.set_page_scale_factor(0.5f);
  original.set_device_scale_factor(0.6f);
  gfx::Transform transform =
      gfx::Transform(1.05f, 2.15f, 3.14f, 4.13f, 5.12f, 6.11f, 7.1f, 8.9f, 9.8f,
                     10.7f, 11.6f, 12.5f, 13.4f, 14.3f, 15.2f, 16.1f);
  original.SetDeviceTransformScaleFactor(transform);
  original.AddNodeAffectedByInnerViewportBoundsDelta(0);
  original.AddNodeAffectedByOuterViewportBoundsDelta(1);

  proto::PropertyTree proto;
  original.ToProtobuf(&proto);
  TransformTree result;
  result.FromProtobuf(proto);

  EXPECT_EQ(original, result);
}

TEST(PropertyTreeSerializationTest, ClipNodeDataSerialization) {
  ClipNodeData original;
  original.clip = gfx::RectF(0.5f, 0.5f);
  original.combined_clip_in_target_space = gfx::RectF(0.6f, 0.6f);
  original.clip_in_target_space = gfx::RectF(0.7f, 0.7f);
  original.transform_id = 2;
  original.target_id = 3;
  original.applies_local_clip = false;
  original.layer_clipping_uses_only_local_clip = false;
  original.target_is_clipped = false;
  original.layers_are_clipped = false;
  original.layers_are_clipped_when_surfaces_disabled = false;
  original.resets_clip = false;

  proto::TreeNode proto;
  original.ToProtobuf(&proto);
  ClipNodeData result;
  result.FromProtobuf(proto);

  EXPECT_EQ(original, result);
}

TEST(PropertyTreeSerializationTest, ClipNodeSerialization) {
  ClipNode original;
  original.id = 3;
  original.parent_id = 2;
  original.owner_id = 4;

  proto::TreeNode proto;
  original.ToProtobuf(&proto);
  ClipNode result;
  result.FromProtobuf(proto);

  EXPECT_EQ(original, result);
}

TEST(PropertyTreeSerializationTest, ClipTreeSerialization) {
  ClipTree original;
  ClipNode& root = *original.Node(0);
  root.data.transform_id = 2;
  root.data.target_id = 1;
  ClipNode second;
  second.data.transform_id = 4;
  second.data.applies_local_clip = true;
  ClipNode third;
  third.data.target_id = 3;
  third.data.target_is_clipped = false;

  original.Insert(second, 0);
  original.Insert(third, 1);
  original.set_needs_update(true);

  proto::PropertyTree proto;
  original.ToProtobuf(&proto);
  ClipTree result;
  result.FromProtobuf(proto);

  EXPECT_EQ(original, result);
}

TEST(PropertyTreeSerializationTest, EffectNodeDataSerialization) {
  EffectNodeData original;
  original.opacity = 0.5f;
  original.screen_space_opacity = 0.6f;
  original.has_render_surface = false;
  original.transform_id = 2;
  original.clip_id = 3;

  proto::TreeNode proto;
  original.ToProtobuf(&proto);
  EffectNodeData result;
  result.FromProtobuf(proto);

  EXPECT_EQ(original, result);
}

TEST(PropertyTreeSerializationTest, EffectNodeSerialization) {
  EffectNode original;
  original.id = 3;
  original.parent_id = 2;
  original.owner_id = 4;

  proto::TreeNode proto;
  original.ToProtobuf(&proto);
  EffectNode result;
  result.FromProtobuf(proto);

  EXPECT_EQ(original, result);
}

TEST(PropertyTreeSerializationTest, EffectTreeSerialization) {
  EffectTree original;
  EffectNode& root = *original.Node(0);
  root.data.transform_id = 2;
  root.data.clip_id = 1;
  EffectNode second;
  second.data.transform_id = 4;
  second.data.opacity = true;
  EffectNode third;
  third.data.clip_id = 3;
  third.data.has_render_surface = false;

  original.Insert(second, 0);
  original.Insert(third, 1);
  original.set_needs_update(true);

  proto::PropertyTree proto;
  original.ToProtobuf(&proto);
  EffectTree result;
  result.FromProtobuf(proto);

  EXPECT_EQ(original, result);
}

TEST(PropertyTreeSerializationTest, ScrollNodeDataSerialization) {
  ScrollNodeData original;
  original.scrollable = true;
  original.main_thread_scrolling_reasons =
      MainThreadScrollingReason::kScrollbarScrolling;
  original.contains_non_fast_scrollable_region = false;
  original.scroll_clip_layer_bounds = gfx::Size(10, 10);
  original.bounds = gfx::Size(15, 15);
  original.max_scroll_offset_affected_by_page_scale = true;
  original.is_inner_viewport_scroll_layer = true;
  original.is_outer_viewport_scroll_layer = false;

  proto::TreeNode proto;
  original.ToProtobuf(&proto);
  ScrollNodeData result;
  result.FromProtobuf(proto);

  EXPECT_EQ(original, result);
}

TEST(PropertyTreeSerializationTest, ScrollNodeSerialization) {
  ScrollNode original;
  original.id = 3;
  original.parent_id = 2;
  original.owner_id = 4;

  proto::TreeNode proto;
  original.ToProtobuf(&proto);
  ScrollNode result;
  result.FromProtobuf(proto);

  EXPECT_EQ(original, result);
}

TEST(PropertyTreeSerializationTest, ScrollTreeSerialization) {
  ScrollTree original;
  ScrollNode second;
  second.data.scrollable = true;
  second.data.bounds = gfx::Size(15, 15);
  ScrollNode third;
  third.data.contains_non_fast_scrollable_region = true;

  original.Insert(second, 0);
  original.Insert(third, 1);

  proto::PropertyTree proto;
  original.ToProtobuf(&proto);
  ScrollTree result;
  result.FromProtobuf(proto);

  EXPECT_EQ(original, result);
}

TEST(PropertyTreeSerializationTest, PropertyTrees) {
  PropertyTrees original;
  original.transform_tree.Insert(TransformNode(), 0);
  original.transform_tree.Insert(TransformNode(), 1);
  original.clip_tree.Insert(ClipNode(), 0);
  original.clip_tree.Insert(ClipNode(), 1);
  original.effect_tree.Insert(EffectNode(), 0);
  original.effect_tree.Insert(EffectNode(), 1);
  original.scroll_tree.Insert(ScrollNode(), 0);
  original.scroll_tree.Insert(ScrollNode(), 1);
  original.needs_rebuild = false;
  original.non_root_surfaces_enabled = false;
  original.sequence_number = 3;

  proto::PropertyTrees proto;
  original.ToProtobuf(&proto);
  PropertyTrees result;
  result.FromProtobuf(proto);

  EXPECT_EQ(original, result);
}

class PropertyTreeTest : public testing::Test {
 public:
  PropertyTreeTest() : test_serialization_(false) {}

 protected:
  void RunTest(bool test_serialization) {
    test_serialization_ = test_serialization;
    StartTest();
  }

  virtual void StartTest() = 0;

  TransformTree TransformTreeForTest(const TransformTree& transform_tree) {
    if (!test_serialization_) {
      return transform_tree;
    }
    TransformTree new_tree;
    proto::PropertyTree proto;
    transform_tree.ToProtobuf(&proto);
    new_tree.FromProtobuf(proto);

    new_tree.SetPropertyTrees(transform_tree.property_trees());

    EXPECT_EQ(transform_tree, new_tree);
    return new_tree;
  }

  EffectTree EffectTreeForTest(const EffectTree& effect_tree) {
    if (!test_serialization_) {
      return effect_tree;
    }
    EffectTree new_tree;
    proto::PropertyTree proto;
    effect_tree.ToProtobuf(&proto);
    new_tree.FromProtobuf(proto);

    EXPECT_EQ(effect_tree, new_tree);
    return new_tree;
  }

 private:
  bool test_serialization_;
};

#define DIRECT_PROPERTY_TREE_TEST_F(TEST_FIXTURE_NAME) \
  TEST_F(TEST_FIXTURE_NAME, RunDirect) { RunTest(false); }

#define SERIALIZED_PROPERTY_TREE_TEST_F(TEST_FIXTURE_NAME) \
  TEST_F(TEST_FIXTURE_NAME, RunSerialized) { RunTest(true); }

#define DIRECT_AND_SERIALIZED_PROPERTY_TREE_TEST_F(TEST_FIXTURE_NAME) \
  DIRECT_PROPERTY_TREE_TEST_F(TEST_FIXTURE_NAME);                     \
  SERIALIZED_PROPERTY_TREE_TEST_F(TEST_FIXTURE_NAME)

class PropertyTreeTestComputeTransformRoot : public PropertyTreeTest {
 protected:
  void StartTest() override {
    PropertyTrees property_trees;
    TransformTree tree = property_trees.transform_tree;
    TransformNode& root = *tree.Node(0);
    root.data.local.Translate(2, 2);
    root.data.target_id = 0;
    tree = TransformTreeForTest(tree);
    tree.UpdateTransforms(0);

    gfx::Transform expected;
    gfx::Transform transform;
    bool success = tree.ComputeTransform(0, 0, &transform);
    EXPECT_TRUE(success);
    EXPECT_TRANSFORMATION_MATRIX_EQ(expected, transform);

    transform.MakeIdentity();
    expected.Translate(2, 2);
    success = tree.ComputeTransform(0, -1, &transform);
    EXPECT_TRUE(success);
    EXPECT_TRANSFORMATION_MATRIX_EQ(expected, transform);

    transform.MakeIdentity();
    expected.MakeIdentity();
    expected.Translate(-2, -2);
    success = tree.ComputeTransform(-1, 0, &transform);
    EXPECT_TRUE(success);
    EXPECT_TRANSFORMATION_MATRIX_EQ(expected, transform);
  }
};

DIRECT_AND_SERIALIZED_PROPERTY_TREE_TEST_F(
    PropertyTreeTestComputeTransformRoot);

class PropertyTreeTestComputeTransformChild : public PropertyTreeTest {
 protected:
  void StartTest() override {
    PropertyTrees property_trees;
    TransformTree tree = property_trees.transform_tree;
    TransformNode& root = *tree.Node(0);
    root.data.local.Translate(2, 2);
    root.data.target_id = 0;
    tree.UpdateTransforms(0);

    TransformNode child;
    child.data.local.Translate(3, 3);
    child.data.target_id = 0;
    child.data.source_node_id = 0;

    tree.Insert(child, 0);
    tree = TransformTreeForTest(tree);
    tree.UpdateTransforms(1);

    gfx::Transform expected;
    gfx::Transform transform;

    expected.Translate(3, 3);
    bool success = tree.ComputeTransform(1, 0, &transform);
    EXPECT_TRUE(success);
    EXPECT_TRANSFORMATION_MATRIX_EQ(expected, transform);

    transform.MakeIdentity();
    expected.MakeIdentity();
    expected.Translate(-3, -3);
    success = tree.ComputeTransform(0, 1, &transform);
    EXPECT_TRUE(success);
    EXPECT_TRANSFORMATION_MATRIX_EQ(expected, transform);

    transform.MakeIdentity();
    expected.MakeIdentity();
    expected.Translate(5, 5);
    success = tree.ComputeTransform(1, -1, &transform);
    EXPECT_TRUE(success);
    EXPECT_TRANSFORMATION_MATRIX_EQ(expected, transform);

    transform.MakeIdentity();
    expected.MakeIdentity();
    expected.Translate(-5, -5);
    success = tree.ComputeTransform(-1, 1, &transform);
    EXPECT_TRUE(success);
    EXPECT_TRANSFORMATION_MATRIX_EQ(expected, transform);
  }
};

DIRECT_AND_SERIALIZED_PROPERTY_TREE_TEST_F(
    PropertyTreeTestComputeTransformChild);

class PropertyTreeTestComputeTransformSibling : public PropertyTreeTest {
 protected:
  void StartTest() override {
    PropertyTrees property_trees;
    TransformTree tree = property_trees.transform_tree;
    TransformNode& root = *tree.Node(0);
    root.data.local.Translate(2, 2);
    root.data.target_id = 0;
    tree.UpdateTransforms(0);

    TransformNode child;
    child.data.local.Translate(3, 3);
    child.data.source_node_id = 0;
    child.data.target_id = 0;

    TransformNode sibling;
    sibling.data.local.Translate(7, 7);
    sibling.data.source_node_id = 0;
    sibling.data.target_id = 0;

    tree.Insert(child, 0);
    tree.Insert(sibling, 0);

    tree = TransformTreeForTest(tree);

    tree.UpdateTransforms(1);
    tree.UpdateTransforms(2);

    gfx::Transform expected;
    gfx::Transform transform;

    expected.Translate(4, 4);
    bool success = tree.ComputeTransform(2, 1, &transform);
    EXPECT_TRUE(success);
    EXPECT_TRANSFORMATION_MATRIX_EQ(expected, transform);

    transform.MakeIdentity();
    expected.MakeIdentity();
    expected.Translate(-4, -4);
    success = tree.ComputeTransform(1, 2, &transform);
    EXPECT_TRUE(success);
    EXPECT_TRANSFORMATION_MATRIX_EQ(expected, transform);
  }
};

DIRECT_AND_SERIALIZED_PROPERTY_TREE_TEST_F(
    PropertyTreeTestComputeTransformSibling);

class PropertyTreeTestComputeTransformSiblingSingularAncestor
    : public PropertyTreeTest {
 protected:
  void StartTest() override {
    // In this test, we have the following tree:
    // root
    //   + singular
    //     + child
    //     + sibling
    // Since the lowest common ancestor of |child| and |sibling| has a singular
    // transform, we cannot use screen space transforms to compute change of
    // basis
    // transforms between these nodes.
    PropertyTrees property_trees;
    TransformTree tree = property_trees.transform_tree;
    TransformNode& root = *tree.Node(0);
    root.data.local.Translate(2, 2);
    root.data.target_id = 0;
    tree.UpdateTransforms(0);

    TransformNode singular;
    singular.data.local.matrix().set(2, 2, 0.0);
    singular.data.source_node_id = 0;
    singular.data.target_id = 0;

    TransformNode child;
    child.data.local.Translate(3, 3);
    child.data.source_node_id = 1;
    child.data.target_id = 0;

    TransformNode sibling;
    sibling.data.local.Translate(7, 7);
    sibling.data.source_node_id = 1;
    sibling.data.target_id = 0;

    tree.Insert(singular, 0);
    tree.Insert(child, 1);
    tree.Insert(sibling, 1);

    tree = TransformTreeForTest(tree);

    tree.UpdateTransforms(1);
    tree.UpdateTransforms(2);
    tree.UpdateTransforms(3);

    gfx::Transform expected;
    gfx::Transform transform;

    expected.Translate(4, 4);
    bool success = tree.ComputeTransform(3, 2, &transform);
    EXPECT_TRUE(success);
    EXPECT_TRANSFORMATION_MATRIX_EQ(expected, transform);

    transform.MakeIdentity();
    expected.MakeIdentity();
    expected.Translate(-4, -4);
    success = tree.ComputeTransform(2, 3, &transform);
    EXPECT_TRUE(success);
    EXPECT_TRANSFORMATION_MATRIX_EQ(expected, transform);
  }
};

DIRECT_AND_SERIALIZED_PROPERTY_TREE_TEST_F(
    PropertyTreeTestComputeTransformSiblingSingularAncestor);

class PropertyTreeTestTransformsWithFlattening : public PropertyTreeTest {
 protected:
  void StartTest() override {
    PropertyTrees property_trees;
    TransformTree tree = property_trees.transform_tree;

    int grand_parent = tree.Insert(TransformNode(), 0);
    tree.Node(grand_parent)->data.content_target_id = grand_parent;
    tree.Node(grand_parent)->data.target_id = grand_parent;
    tree.Node(grand_parent)->data.source_node_id = 0;

    gfx::Transform rotation_about_x;
    rotation_about_x.RotateAboutXAxis(15);

    int parent = tree.Insert(TransformNode(), grand_parent);
    tree.Node(parent)->data.needs_sublayer_scale = true;
    tree.Node(parent)->data.target_id = grand_parent;
    tree.Node(parent)->data.content_target_id = parent;
    tree.Node(parent)->data.source_node_id = grand_parent;
    tree.Node(parent)->data.local = rotation_about_x;

    int child = tree.Insert(TransformNode(), parent);
    tree.Node(child)->data.target_id = parent;
    tree.Node(child)->data.content_target_id = parent;
    tree.Node(child)->data.source_node_id = parent;
    tree.Node(child)->data.flattens_inherited_transform = true;
    tree.Node(child)->data.local = rotation_about_x;

    int grand_child = tree.Insert(TransformNode(), child);
    tree.Node(grand_child)->data.target_id = parent;
    tree.Node(grand_child)->data.content_target_id = parent;
    tree.Node(grand_child)->data.source_node_id = child;
    tree.Node(grand_child)->data.flattens_inherited_transform = true;
    tree.Node(grand_child)->data.local = rotation_about_x;

    tree.set_needs_update(true);
    tree = TransformTreeForTest(tree);
    ComputeTransforms(&tree);

    gfx::Transform flattened_rotation_about_x = rotation_about_x;
    flattened_rotation_about_x.FlattenTo2d();

    EXPECT_TRANSFORMATION_MATRIX_EQ(rotation_about_x,
                                    tree.Node(child)->data.to_target);

    EXPECT_TRANSFORMATION_MATRIX_EQ(
        flattened_rotation_about_x * rotation_about_x,
        tree.Node(child)->data.to_screen);

    EXPECT_TRANSFORMATION_MATRIX_EQ(
        flattened_rotation_about_x * rotation_about_x,
        tree.Node(grand_child)->data.to_target);

    EXPECT_TRANSFORMATION_MATRIX_EQ(flattened_rotation_about_x *
                                        flattened_rotation_about_x *
                                        rotation_about_x,
                                    tree.Node(grand_child)->data.to_screen);

    gfx::Transform grand_child_to_child;
    bool success =
        tree.ComputeTransform(grand_child, child, &grand_child_to_child);
    EXPECT_TRUE(success);
    EXPECT_TRANSFORMATION_MATRIX_EQ(rotation_about_x, grand_child_to_child);

    // Remove flattening at grand_child, and recompute transforms.
    tree.Node(grand_child)->data.flattens_inherited_transform = false;
    tree.set_needs_update(true);
    tree = TransformTreeForTest(tree);
    ComputeTransforms(&tree);

    EXPECT_TRANSFORMATION_MATRIX_EQ(rotation_about_x * rotation_about_x,
                                    tree.Node(grand_child)->data.to_target);

    EXPECT_TRANSFORMATION_MATRIX_EQ(
        flattened_rotation_about_x * rotation_about_x * rotation_about_x,
        tree.Node(grand_child)->data.to_screen);

    success = tree.ComputeTransform(grand_child, child, &grand_child_to_child);
    EXPECT_TRUE(success);
    EXPECT_TRANSFORMATION_MATRIX_EQ(rotation_about_x, grand_child_to_child);
  }
};

DIRECT_AND_SERIALIZED_PROPERTY_TREE_TEST_F(
    PropertyTreeTestTransformsWithFlattening);

class PropertyTreeTestMultiplicationOrder : public PropertyTreeTest {
 protected:
  void StartTest() override {
    PropertyTrees property_trees;
    TransformTree tree = property_trees.transform_tree;
    TransformNode& root = *tree.Node(0);
    root.data.local.Translate(2, 2);
    root.data.target_id = 0;
    tree.UpdateTransforms(0);

    TransformNode child;
    child.data.local.Scale(2, 2);
    child.data.target_id = 0;
    child.data.source_node_id = 0;

    tree.Insert(child, 0);
    tree = TransformTreeForTest(tree);
    tree.UpdateTransforms(1);

    gfx::Transform expected;
    expected.Translate(2, 2);
    expected.Scale(2, 2);

    gfx::Transform transform;
    gfx::Transform inverse;

    bool success = tree.ComputeTransform(1, -1, &transform);
    EXPECT_TRUE(success);
    EXPECT_TRANSFORMATION_MATRIX_EQ(expected, transform);

    success = tree.ComputeTransform(-1, 1, &inverse);
    EXPECT_TRUE(success);

    transform = transform * inverse;
    expected.MakeIdentity();
    EXPECT_TRANSFORMATION_MATRIX_EQ(expected, transform);
  }
};

DIRECT_AND_SERIALIZED_PROPERTY_TREE_TEST_F(PropertyTreeTestMultiplicationOrder);

class PropertyTreeTestComputeTransformWithUninvertibleTransform
    : public PropertyTreeTest {
 protected:
  void StartTest() override {
    PropertyTrees property_trees;
    TransformTree tree = property_trees.transform_tree;
    TransformNode& root = *tree.Node(0);
    root.data.target_id = 0;
    tree.UpdateTransforms(0);

    TransformNode child;
    child.data.local.Scale(0, 0);
    child.data.target_id = 0;
    child.data.source_node_id = 0;

    tree.Insert(child, 0);
    tree = TransformTreeForTest(tree);
    tree.UpdateTransforms(1);

    gfx::Transform expected;
    expected.Scale(0, 0);

    gfx::Transform transform;
    gfx::Transform inverse;

    bool success = tree.ComputeTransform(1, 0, &transform);
    EXPECT_TRUE(success);
    EXPECT_TRANSFORMATION_MATRIX_EQ(expected, transform);

    // To compute this would require inverting the 0 matrix, so we cannot
    // succeed.
    success = tree.ComputeTransform(0, 1, &inverse);
    EXPECT_FALSE(success);
  }
};

DIRECT_AND_SERIALIZED_PROPERTY_TREE_TEST_F(
    PropertyTreeTestComputeTransformWithUninvertibleTransform);

class PropertyTreeTestComputeTransformWithSublayerScale
    : public PropertyTreeTest {
 protected:
  void StartTest() override {
    PropertyTrees property_trees;
    TransformTree tree = property_trees.transform_tree;
    TransformNode& root = *tree.Node(0);
    root.data.target_id = 0;
    tree.UpdateTransforms(0);

    TransformNode grand_parent;
    grand_parent.data.local.Scale(2.f, 2.f);
    grand_parent.data.target_id = 0;
    grand_parent.data.source_node_id = 0;
    grand_parent.data.needs_sublayer_scale = true;
    int grand_parent_id = tree.Insert(grand_parent, 0);
    tree.UpdateTransforms(grand_parent_id);

    TransformNode parent;
    parent.data.local.Translate(15.f, 15.f);
    parent.data.target_id = grand_parent_id;
    parent.data.source_node_id = grand_parent_id;
    int parent_id = tree.Insert(parent, grand_parent_id);
    tree.UpdateTransforms(parent_id);

    TransformNode child;
    child.data.local.Scale(3.f, 3.f);
    child.data.target_id = grand_parent_id;
    child.data.source_node_id = parent_id;
    int child_id = tree.Insert(child, parent_id);
    tree.UpdateTransforms(child_id);

    TransformNode grand_child;
    grand_child.data.local.Scale(5.f, 5.f);
    grand_child.data.target_id = grand_parent_id;
    grand_child.data.source_node_id = child_id;
    grand_child.data.needs_sublayer_scale = true;
    int grand_child_id = tree.Insert(grand_child, child_id);
    tree = TransformTreeForTest(tree);
    tree.UpdateTransforms(grand_child_id);

    EXPECT_EQ(gfx::Vector2dF(2.f, 2.f),
              tree.Node(grand_parent_id)->data.sublayer_scale);
    EXPECT_EQ(gfx::Vector2dF(30.f, 30.f),
              tree.Node(grand_child_id)->data.sublayer_scale);

    // Compute transform from grand_parent to grand_child.
    gfx::Transform expected_transform_without_sublayer_scale;
    expected_transform_without_sublayer_scale.Scale(1.f / 15.f, 1.f / 15.f);
    expected_transform_without_sublayer_scale.Translate(-15.f, -15.f);

    gfx::Transform expected_transform_with_dest_sublayer_scale;
    expected_transform_with_dest_sublayer_scale.Scale(30.f, 30.f);
    expected_transform_with_dest_sublayer_scale.Scale(1.f / 15.f, 1.f / 15.f);
    expected_transform_with_dest_sublayer_scale.Translate(-15.f, -15.f);

    gfx::Transform expected_transform_with_source_sublayer_scale;
    expected_transform_with_source_sublayer_scale.Scale(1.f / 15.f, 1.f / 15.f);
    expected_transform_with_source_sublayer_scale.Translate(-15.f, -15.f);
    expected_transform_with_source_sublayer_scale.Scale(0.5f, 0.5f);

    gfx::Transform transform;
    bool success =
        tree.ComputeTransform(grand_parent_id, grand_child_id, &transform);
    EXPECT_TRUE(success);
    EXPECT_TRANSFORMATION_MATRIX_EQ(expected_transform_without_sublayer_scale,
                                    transform);

    success = tree.ComputeTransformWithDestinationSublayerScale(
        grand_parent_id, grand_child_id, &transform);
    EXPECT_TRUE(success);
    EXPECT_TRANSFORMATION_MATRIX_EQ(expected_transform_with_dest_sublayer_scale,
                                    transform);

    success = tree.ComputeTransformWithSourceSublayerScale(
        grand_parent_id, grand_child_id, &transform);
    EXPECT_TRUE(success);
    EXPECT_TRANSFORMATION_MATRIX_EQ(
        expected_transform_with_source_sublayer_scale, transform);

    // Now compute transform from grand_child to grand_parent.
    expected_transform_without_sublayer_scale.MakeIdentity();
    expected_transform_without_sublayer_scale.Translate(15.f, 15.f);
    expected_transform_without_sublayer_scale.Scale(15.f, 15.f);

    expected_transform_with_dest_sublayer_scale.MakeIdentity();
    expected_transform_with_dest_sublayer_scale.Scale(2.f, 2.f);
    expected_transform_with_dest_sublayer_scale.Translate(15.f, 15.f);
    expected_transform_with_dest_sublayer_scale.Scale(15.f, 15.f);

    expected_transform_with_source_sublayer_scale.MakeIdentity();
    expected_transform_with_source_sublayer_scale.Translate(15.f, 15.f);
    expected_transform_with_source_sublayer_scale.Scale(15.f, 15.f);
    expected_transform_with_source_sublayer_scale.Scale(1.f / 30.f, 1.f / 30.f);

    success =
        tree.ComputeTransform(grand_child_id, grand_parent_id, &transform);
    EXPECT_TRUE(success);
    EXPECT_TRANSFORMATION_MATRIX_EQ(expected_transform_without_sublayer_scale,
                                    transform);

    success = tree.ComputeTransformWithDestinationSublayerScale(
        grand_child_id, grand_parent_id, &transform);
    EXPECT_TRUE(success);
    EXPECT_TRANSFORMATION_MATRIX_EQ(expected_transform_with_dest_sublayer_scale,
                                    transform);

    success = tree.ComputeTransformWithSourceSublayerScale(
        grand_child_id, grand_parent_id, &transform);
    EXPECT_TRUE(success);
    EXPECT_TRANSFORMATION_MATRIX_EQ(
        expected_transform_with_source_sublayer_scale, transform);
  }
};

DIRECT_AND_SERIALIZED_PROPERTY_TREE_TEST_F(
    PropertyTreeTestComputeTransformWithSublayerScale);

class PropertyTreeTestComputeTransformToTargetWithZeroSublayerScale
    : public PropertyTreeTest {
 protected:
  void StartTest() override {
    PropertyTrees property_trees;
    TransformTree tree = property_trees.transform_tree;
    TransformNode& root = *tree.Node(0);
    root.data.target_id = 0;
    tree.UpdateTransforms(0);

    TransformNode grand_parent;
    grand_parent.data.local.Scale(2.f, 0.f);
    grand_parent.data.target_id = 0;
    grand_parent.data.source_node_id = 0;
    grand_parent.data.needs_sublayer_scale = true;
    int grand_parent_id = tree.Insert(grand_parent, 0);
    tree.Node(grand_parent_id)->data.content_target_id = grand_parent_id;
    tree.UpdateTransforms(grand_parent_id);

    TransformNode parent;
    parent.data.local.Translate(1.f, 1.f);
    parent.data.target_id = grand_parent_id;
    parent.data.content_target_id = grand_parent_id;
    parent.data.source_node_id = grand_parent_id;
    int parent_id = tree.Insert(parent, grand_parent_id);
    tree.UpdateTransforms(parent_id);

    TransformNode child;
    child.data.local.Translate(3.f, 4.f);
    child.data.target_id = grand_parent_id;
    child.data.content_target_id = grand_parent_id;
    child.data.source_node_id = parent_id;
    int child_id = tree.Insert(child, parent_id);
    tree = TransformTreeForTest(tree);
    tree.UpdateTransforms(child_id);

    gfx::Transform expected_transform;
    expected_transform.Translate(4.f, 5.f);

    gfx::Transform transform;
    bool success = tree.ComputeTransform(child_id, grand_parent_id, &transform);
    EXPECT_TRUE(success);
    EXPECT_TRANSFORMATION_MATRIX_EQ(expected_transform, transform);

    tree.Node(grand_parent_id)->data.local.MakeIdentity();
    tree.Node(grand_parent_id)->data.local.Scale(0.f, 2.f);
    tree.Node(grand_parent_id)->data.needs_local_transform_update = true;
    tree.set_needs_update(true);
    tree = TransformTreeForTest(tree);

    ComputeTransforms(&tree);

    success = tree.ComputeTransform(child_id, grand_parent_id, &transform);
    EXPECT_TRUE(success);
    EXPECT_TRANSFORMATION_MATRIX_EQ(expected_transform, transform);

    tree.Node(grand_parent_id)->data.local.MakeIdentity();
    tree.Node(grand_parent_id)->data.local.Scale(0.f, 0.f);
    tree.Node(grand_parent_id)->data.needs_local_transform_update = true;
    tree.set_needs_update(true);
    tree = TransformTreeForTest(tree);

    ComputeTransforms(&tree);

    success = tree.ComputeTransform(child_id, grand_parent_id, &transform);
    EXPECT_TRUE(success);
    EXPECT_TRANSFORMATION_MATRIX_EQ(expected_transform, transform);
  }
};

DIRECT_AND_SERIALIZED_PROPERTY_TREE_TEST_F(
    PropertyTreeTestComputeTransformToTargetWithZeroSublayerScale);

class PropertyTreeTestFlatteningWhenDestinationHasOnlyFlatAncestors
    : public PropertyTreeTest {
 protected:
  void StartTest() override {
    // This tests that flattening is performed correctly when
    // destination and its ancestors are flat, but there are 3d transforms
    // and flattening between the source and destination.
    PropertyTrees property_trees;
    TransformTree tree = property_trees.transform_tree;

    int parent = tree.Insert(TransformNode(), 0);
    tree.Node(parent)->data.content_target_id = parent;
    tree.Node(parent)->data.target_id = parent;
    tree.Node(parent)->data.source_node_id = 0;
    tree.Node(parent)->data.local.Translate(2, 2);

    gfx::Transform rotation_about_x;
    rotation_about_x.RotateAboutXAxis(15);

    int child = tree.Insert(TransformNode(), parent);
    tree.Node(child)->data.content_target_id = child;
    tree.Node(child)->data.target_id = child;
    tree.Node(child)->data.source_node_id = parent;
    tree.Node(child)->data.local = rotation_about_x;

    int grand_child = tree.Insert(TransformNode(), child);
    tree.Node(grand_child)->data.content_target_id = grand_child;
    tree.Node(grand_child)->data.target_id = grand_child;
    tree.Node(grand_child)->data.source_node_id = child;
    tree.Node(grand_child)->data.flattens_inherited_transform = true;

    tree.set_needs_update(true);
    tree = TransformTreeForTest(tree);
    ComputeTransforms(&tree);

    gfx::Transform flattened_rotation_about_x = rotation_about_x;
    flattened_rotation_about_x.FlattenTo2d();

    gfx::Transform grand_child_to_parent;
    bool success =
        tree.ComputeTransform(grand_child, parent, &grand_child_to_parent);
    EXPECT_TRUE(success);
    EXPECT_TRANSFORMATION_MATRIX_EQ(flattened_rotation_about_x,
                                    grand_child_to_parent);
  }
};

DIRECT_AND_SERIALIZED_PROPERTY_TREE_TEST_F(
    PropertyTreeTestFlatteningWhenDestinationHasOnlyFlatAncestors);

class PropertyTreeTestScreenSpaceOpacityUpdateTest : public PropertyTreeTest {
 protected:
  void StartTest() override {
    // This tests that screen space opacity is updated for the subtree when
    // opacity of a node changes.
    EffectTree tree;

    int parent = tree.Insert(EffectNode(), 0);
    int child = tree.Insert(EffectNode(), parent);
    tree = EffectTreeForTest(tree);

    EXPECT_EQ(tree.Node(child)->data.screen_space_opacity, 1.f);
    tree.Node(parent)->data.opacity = 0.5f;
    tree.set_needs_update(true);
    tree = EffectTreeForTest(tree);
    ComputeEffects(&tree);
    EXPECT_EQ(tree.Node(child)->data.screen_space_opacity, 0.5f);

    tree.Node(child)->data.opacity = 0.5f;
    tree.set_needs_update(true);
    tree = EffectTreeForTest(tree);
    ComputeEffects(&tree);
    EXPECT_EQ(tree.Node(child)->data.screen_space_opacity, 0.25f);
  }
};

DIRECT_AND_SERIALIZED_PROPERTY_TREE_TEST_F(
    PropertyTreeTestScreenSpaceOpacityUpdateTest);

class PropertyTreeTestNonIntegerTranslationTest : public PropertyTreeTest {
 protected:
  void StartTest() override {
    // This tests that when a node has non-integer translation, the information
    // is propagated to the subtree.
    PropertyTrees property_trees;
    TransformTree tree = property_trees.transform_tree;

    int parent = tree.Insert(TransformNode(), 0);
    tree.Node(parent)->data.target_id = parent;
    tree.Node(parent)->data.local.Translate(1.5f, 1.5f);

    int child = tree.Insert(TransformNode(), parent);
    tree.Node(child)->data.target_id = parent;
    tree.Node(child)->data.local.Translate(1, 1);
    tree.set_needs_update(true);
    tree = TransformTreeForTest(tree);
    ComputeTransforms(&tree);
    EXPECT_FALSE(tree.Node(parent)
                     ->data.node_and_ancestors_have_only_integer_translation);
    EXPECT_FALSE(tree.Node(child)
                     ->data.node_and_ancestors_have_only_integer_translation);

    tree.Node(parent)->data.local.Translate(0.5f, 0.5f);
    tree.Node(child)->data.local.Translate(0.5f, 0.5f);
    tree.set_needs_update(true);
    tree = TransformTreeForTest(tree);
    ComputeTransforms(&tree);
    EXPECT_TRUE(tree.Node(parent)
                    ->data.node_and_ancestors_have_only_integer_translation);
    EXPECT_FALSE(tree.Node(child)
                     ->data.node_and_ancestors_have_only_integer_translation);

    tree.Node(child)->data.local.Translate(0.5f, 0.5f);
    tree.Node(child)->data.target_id = child;
    tree.set_needs_update(true);
    tree = TransformTreeForTest(tree);
    ComputeTransforms(&tree);
    EXPECT_TRUE(tree.Node(parent)
                    ->data.node_and_ancestors_have_only_integer_translation);
    EXPECT_TRUE(tree.Node(child)
                    ->data.node_and_ancestors_have_only_integer_translation);
  }
};

DIRECT_AND_SERIALIZED_PROPERTY_TREE_TEST_F(
    PropertyTreeTestNonIntegerTranslationTest);

class PropertyTreeTestSingularTransformSnapTest : public PropertyTreeTest {
 protected:
  void StartTest() override {
    // This tests that to_target transform is not snapped when it has a singular
    // transform.
    PropertyTrees property_trees;
    TransformTree tree = property_trees.transform_tree;

    int parent = tree.Insert(TransformNode(), 0);
    tree.Node(parent)->data.target_id = parent;
    tree.Node(parent)->data.scrolls = true;

    int child = tree.Insert(TransformNode(), parent);
    TransformNode* child_node = tree.Node(child);
    child_node->data.target_id = parent;
    child_node->data.scrolls = true;
    child_node->data.local.Scale3d(6.0f, 6.0f, 0.0f);
    child_node->data.local.Translate(1.3f, 1.3f);
    tree.set_needs_update(true);

    tree = TransformTreeForTest(tree);
    ComputeTransforms(&tree);

    gfx::Transform from_target;
    EXPECT_FALSE(child_node->data.to_target.GetInverse(&from_target));
    // The following checks are to ensure that snapping is skipped because of
    // singular transform (and not because of other reasons which also cause
    // snapping to be skipped).
    EXPECT_TRUE(child_node->data.scrolls);
    EXPECT_TRUE(child_node->data.to_target.IsScaleOrTranslation());
    EXPECT_FALSE(child_node->data.to_screen_is_animated);
    EXPECT_FALSE(child_node->data.ancestors_are_invertible);

    gfx::Transform rounded = child_node->data.to_target;
    rounded.RoundTranslationComponents();
    EXPECT_NE(child_node->data.to_target, rounded);
  }
};

DIRECT_AND_SERIALIZED_PROPERTY_TREE_TEST_F(
    PropertyTreeTestSingularTransformSnapTest);

#undef DIRECT_AND_SERIALIZED_PROPERTY_TREE_TEST_F
#undef SERIALIZED_PROPERTY_TREE_TEST_F
#undef DIRECT_PROPERTY_TREE_TEST_F

}  // namespace
}  // namespace cc
