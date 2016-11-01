// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/property_tree.h"

#include "base/memory/ptr_util.h"
#include "cc/input/main_thread_scrolling_reason.h"
#include "cc/proto/property_tree.pb.h"
#include "cc/test/geometry_test_utils.h"
#include "cc/trees/clip_node.h"
#include "cc/trees/draw_property_utils.h"
#include "cc/trees/effect_node.h"
#include "cc/trees/scroll_node.h"
#include "cc/trees/transform_node.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

TEST(PropertyTreeSerializationTest, TransformNodeSerialization) {
  TransformNode original;
  original.id = 3;
  original.parent_id = 2;
  original.owner_id = 4;
  original.pre_local.Translate3d(1.f, 2.f, 3.f);
  original.local.Translate3d(3.f, 1.f, 5.f);
  original.post_local.Translate3d(1.f, 8.f, 3.f);
  original.to_parent.Translate3d(3.2f, 2.f, 3.f);
  original.source_node_id = 5;
  original.needs_local_transform_update = false;
  original.is_invertible = false;
  original.ancestors_are_invertible = false;
  original.has_potential_animation = false;
  original.to_screen_is_potentially_animated = false;
  original.has_only_translation_animations = false;
  original.flattens_inherited_transform = false;
  original.node_and_ancestors_are_flat = false;
  original.node_and_ancestors_have_only_integer_translation = false;
  original.scrolls = false;
  original.moved_by_inner_viewport_bounds_delta_x = false;
  original.moved_by_inner_viewport_bounds_delta_y = false;
  original.moved_by_outer_viewport_bounds_delta_x = false;
  original.moved_by_outer_viewport_bounds_delta_y = false;
  original.in_subtree_of_page_scale_layer = false;
  original.post_local_scale_factor = 0.5f;
  original.scroll_offset = gfx::ScrollOffset(1.5f, 1.5f);
  original.snap_amount = gfx::Vector2dF(0.4f, 0.4f);
  original.source_offset = gfx::Vector2dF(2.5f, 2.4f);
  original.source_to_parent = gfx::Vector2dF(3.2f, 3.2f);

  proto::TreeNode proto;
  original.ToProtobuf(&proto);
  TransformNode result;
  result.FromProtobuf(proto);

  EXPECT_EQ(original, result);
}

TEST(PropertyTreeSerializationTest, TransformTreeSerialization) {
  PropertyTrees property_trees;
  TransformTree& original = property_trees.transform_tree;
  TransformNode& root = *original.Node(0);
  root.id = 0;
  root.owner_id = 1;
  original.SetTargetId(root.id, 3);
  original.SetContentTargetId(root.id, 4);
  TransformNode second;
  second.owner_id = 2;
  second.local.Translate3d(2.f, 2.f, 0.f);
  second.source_node_id = 0;
  second.id = original.Insert(second, 0);
  original.SetTargetId(second.id, 0);
  TransformNode third;
  third.owner_id = 3;
  third.scrolls = true;
  third.source_node_id = 1;
  third.id = original.Insert(third, 1);
  original.SetTargetId(third.id, 0);

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
  std::unordered_map<int, int> transform_id_to_index_map;
  result.FromProtobuf(proto, &transform_id_to_index_map);

  EXPECT_EQ(transform_id_to_index_map[1], 0);
  EXPECT_EQ(transform_id_to_index_map[2], 1);
  EXPECT_EQ(transform_id_to_index_map[3], 2);
  EXPECT_EQ(original, result);
}

TEST(PropertyTreeSerializationTest, ClipNodeSerialization) {
  ClipNode original;
  original.id = 3;
  original.parent_id = 2;
  original.owner_id = 4;
  original.clip = gfx::RectF(0.5f, 0.5f);
  original.combined_clip_in_target_space = gfx::RectF(0.6f, 0.6f);
  original.clip_in_target_space = gfx::RectF(0.7f, 0.7f);
  original.transform_id = 2;
  original.target_transform_id = 3;
  original.target_effect_id = 4;
  original.clip_type = ClipNode::ClipType::NONE;
  original.layer_clipping_uses_only_local_clip = false;
  original.target_is_clipped = false;
  original.layers_are_clipped = false;
  original.layers_are_clipped_when_surfaces_disabled = false;
  original.resets_clip = false;

  proto::TreeNode proto;
  original.ToProtobuf(&proto);
  ClipNode result;
  result.FromProtobuf(proto);

  EXPECT_EQ(original, result);
}

TEST(PropertyTreeSerializationTest, ClipNodeWithExpanderSerialization) {
  ClipNode original;
  original.id = 3;
  original.parent_id = 2;
  original.owner_id = 4;
  original.clip = gfx::RectF(0.5f, 0.5f);
  original.combined_clip_in_target_space = gfx::RectF(0.6f, 0.6f);
  original.clip_in_target_space = gfx::RectF(0.7f, 0.7f);
  original.transform_id = 2;
  original.target_transform_id = 3;
  original.target_effect_id = 4;
  original.clip_type = ClipNode::ClipType::EXPANDS_CLIP;
  original.clip_expander = base::MakeUnique<ClipExpander>(8);
  original.layer_clipping_uses_only_local_clip = false;
  original.target_is_clipped = false;
  original.layers_are_clipped = false;
  original.layers_are_clipped_when_surfaces_disabled = false;
  original.resets_clip = false;

  proto::TreeNode proto;
  original.ToProtobuf(&proto);
  ClipNode result;
  result.FromProtobuf(proto);

  EXPECT_EQ(original, result);
}

TEST(PropertyTreeSerializationTest, ClipTreeSerialization) {
  ClipTree original;
  ClipNode& root = *original.Node(0);
  root.owner_id = 1;
  root.transform_id = 2;
  root.target_transform_id = 1;
  root.target_effect_id = 1;
  ClipNode second;
  second.owner_id = 2;
  second.transform_id = 4;
  second.clip_type = ClipNode::ClipType::APPLIES_LOCAL_CLIP;
  ClipNode third;
  third.owner_id = 3;
  third.target_transform_id = 3;
  third.target_effect_id = 2;
  third.target_is_clipped = false;

  original.Insert(second, 0);
  original.Insert(third, 1);
  original.set_needs_update(true);

  proto::PropertyTree proto;
  original.ToProtobuf(&proto);
  ClipTree result;
  std::unordered_map<int, int> clip_id_to_index_map;
  result.FromProtobuf(proto, &clip_id_to_index_map);

  EXPECT_EQ(clip_id_to_index_map[1], 0);
  EXPECT_EQ(clip_id_to_index_map[2], 1);
  EXPECT_EQ(clip_id_to_index_map[3], 2);
  EXPECT_EQ(original, result);
}

TEST(PropertyTreeSerializationTest, EffectNodeSerialization) {
  EffectNode original;
  original.id = 3;
  original.parent_id = 2;
  original.owner_id = 4;
  original.opacity = 0.5f;
  original.screen_space_opacity = 0.6f;
  original.has_render_surface = false;
  original.transform_id = 2;
  original.clip_id = 3;
  original.mask_layer_id = 6;
  original.surface_contents_scale = gfx::Vector2dF(0.5f, 0.5f);

  proto::TreeNode proto;
  original.ToProtobuf(&proto);
  EffectNode result;
  result.FromProtobuf(proto);

  EXPECT_EQ(original, result);
}

TEST(PropertyTreeSerializationTest, EffectTreeSerialization) {
  EffectTree original;
  EffectNode& root = *original.Node(0);
  root.owner_id = 5;
  root.transform_id = 2;
  root.clip_id = 1;
  EffectNode second;
  second.owner_id = 6;
  second.transform_id = 4;
  second.opacity = true;
  second.mask_layer_id = 32;
  EffectNode third;
  third.owner_id = 7;
  third.clip_id = 3;
  third.has_render_surface = false;

  original.Insert(second, 0);
  original.Insert(third, 1);
  original.AddMaskLayerId(32);
  original.set_needs_update(true);

  proto::PropertyTree proto;
  original.ToProtobuf(&proto);
  EffectTree result;
  std::unordered_map<int, int> effect_id_to_index_map;
  result.FromProtobuf(proto, &effect_id_to_index_map);

  EXPECT_EQ(effect_id_to_index_map[5], 0);
  EXPECT_EQ(effect_id_to_index_map[6], 1);
  EXPECT_EQ(effect_id_to_index_map[7], 2);
  EXPECT_EQ(original, result);
}

TEST(PropertyTreeSerializationTest, ScrollNodeSerialization) {
  ScrollNode original;
  original.id = 3;
  original.parent_id = 2;
  original.owner_id = 4;
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
  ScrollNode result;
  result.FromProtobuf(proto);

  EXPECT_EQ(original, result);
}

TEST(PropertyTreeSerializationTest, ScrollTreeSerialization) {
  PropertyTrees property_trees;
  property_trees.is_main_thread = true;
  ScrollTree& original = property_trees.scroll_tree;
  ScrollNode second;
  second.owner_id = 10;
  second.scrollable = true;
  second.bounds = gfx::Size(15, 15);
  ScrollNode third;
  third.owner_id = 20;
  third.contains_non_fast_scrollable_region = true;

  original.Insert(second, 0);
  original.Insert(third, 1);

  original.set_currently_scrolling_node(1);
  original.SetScrollOffset(1, gfx::ScrollOffset(1, 2));

  proto::PropertyTree proto;
  original.ToProtobuf(&proto);
  ScrollTree result;
  std::unordered_map<int, int> scroll_id_to_index_map;
  result.FromProtobuf(proto, &scroll_id_to_index_map);

  EXPECT_EQ(original, result);
  EXPECT_EQ(scroll_id_to_index_map[10], 1);
  EXPECT_EQ(scroll_id_to_index_map[20], 2);

  original.clear();
  original.set_currently_scrolling_node(0);
  original.SetScrollOffset(2, gfx::ScrollOffset(1, 2));

  proto::PropertyTree proto2;
  original.ToProtobuf(&proto2);
  result = ScrollTree();
  scroll_id_to_index_map.clear();
  result.FromProtobuf(proto2, &scroll_id_to_index_map);

  EXPECT_EQ(original, result);
}

TEST(PropertyTreeSerializationTest, PropertyTrees) {
  PropertyTrees original;
  TransformNode transform_node1 = TransformNode();
  transform_node1.owner_id = 10;
  transform_node1.id = original.transform_tree.Insert(transform_node1, 0);
  TransformNode transform_node2 = TransformNode();
  transform_node2.owner_id = 20;
  transform_node2.id = original.transform_tree.Insert(transform_node2, 1);
  original.transform_id_to_index_map[10] = 1;
  original.transform_id_to_index_map[20] = 2;

  ClipNode clip_node1 = ClipNode();
  clip_node1.owner_id = 10;
  clip_node1.id = original.clip_tree.Insert(clip_node1, 0);
  ClipNode clip_node2 = ClipNode();
  clip_node2.owner_id = 22;
  clip_node2.id = original.clip_tree.Insert(clip_node2, 1);
  original.clip_id_to_index_map[10] = 1;
  original.clip_id_to_index_map[22] = 2;

  EffectNode effect_node1 = EffectNode();
  effect_node1.owner_id = 11;
  effect_node1.id = original.effect_tree.Insert(effect_node1, 0);
  EffectNode effect_node2 = EffectNode();
  effect_node2.owner_id = 23;
  effect_node2.id = original.effect_tree.Insert(effect_node2, 1);
  original.effect_id_to_index_map[11] = 1;
  original.effect_id_to_index_map[23] = 2;

  ScrollNode scroll_node1 = ScrollNode();
  scroll_node1.owner_id = 10;
  scroll_node1.id = original.scroll_tree.Insert(scroll_node1, 0);
  ScrollNode scroll_node2 = ScrollNode();
  scroll_node2.owner_id = 20;
  scroll_node2.id = original.scroll_tree.Insert(scroll_node2, 1);
  original.scroll_id_to_index_map[10] = 1;
  original.scroll_id_to_index_map[20] = 2;

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

  void SetupTransformTreeForTest(TransformTree* transform_tree) {
    if (!test_serialization_)
      return;

    TransformTree new_tree;
    proto::PropertyTree proto;
    transform_tree->ToProtobuf(&proto);
    std::unordered_map<int, int> transform_id_to_index_map;
    new_tree.FromProtobuf(proto, &transform_id_to_index_map);
    EXPECT_EQ(*transform_tree, new_tree);

    PropertyTrees* property_trees = transform_tree->property_trees();
    *transform_tree = new_tree;
    transform_tree->SetPropertyTrees(property_trees);
  }

  void SetupEffectTreeForTest(EffectTree* effect_tree) {
    if (!test_serialization_)
      return;

    EffectTree new_tree;
    proto::PropertyTree proto;
    effect_tree->ToProtobuf(&proto);
    std::unordered_map<int, int> effect_id_to_index_map;
    new_tree.FromProtobuf(proto, &effect_id_to_index_map);

    EXPECT_EQ(*effect_tree, new_tree);
    *effect_tree = new_tree;
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
    TransformTree& tree = property_trees.transform_tree;
    TransformNode& root = *tree.Node(0);
    root.id = 0;
    root.local.Translate(2, 2);
    tree.SetTargetId(root.id, 0);
    SetupTransformTreeForTest(&tree);
    tree.UpdateTransforms(0);

    gfx::Transform expected;
    gfx::Transform transform;
    bool success = tree.ComputeTransformForTesting(0, 0, &transform);
    EXPECT_TRUE(success);
    EXPECT_TRANSFORMATION_MATRIX_EQ(expected, transform);

    transform.MakeIdentity();
    expected.Translate(2, 2);
    success = tree.ComputeTransformForTesting(0, -1, &transform);
    EXPECT_TRUE(success);
    EXPECT_TRANSFORMATION_MATRIX_EQ(expected, transform);

    transform.MakeIdentity();
    expected.MakeIdentity();
    expected.Translate(-2, -2);
    success = tree.ComputeTransformForTesting(-1, 0, &transform);
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
    TransformTree& tree = property_trees.transform_tree;
    TransformNode& root = *tree.Node(0);
    root.local.Translate(2, 2);
    tree.SetTargetId(root.id, 0);
    tree.UpdateTransforms(0);

    TransformNode child;
    child.local.Translate(3, 3);
    child.source_node_id = 0;
    child.id = tree.Insert(child, 0);
    tree.SetTargetId(child.id, 0);

    SetupTransformTreeForTest(&tree);
    tree.UpdateTransforms(1);

    gfx::Transform expected;
    gfx::Transform transform;

    expected.Translate(3, 3);
    bool success = tree.ComputeTransformForTesting(1, 0, &transform);
    EXPECT_TRUE(success);
    EXPECT_TRANSFORMATION_MATRIX_EQ(expected, transform);

    transform.MakeIdentity();
    expected.MakeIdentity();
    expected.Translate(-3, -3);
    success = tree.ComputeTransformForTesting(0, 1, &transform);
    EXPECT_TRUE(success);
    EXPECT_TRANSFORMATION_MATRIX_EQ(expected, transform);

    transform.MakeIdentity();
    expected.MakeIdentity();
    expected.Translate(5, 5);
    success = tree.ComputeTransformForTesting(1, -1, &transform);
    EXPECT_TRUE(success);
    EXPECT_TRANSFORMATION_MATRIX_EQ(expected, transform);

    transform.MakeIdentity();
    expected.MakeIdentity();
    expected.Translate(-5, -5);
    success = tree.ComputeTransformForTesting(-1, 1, &transform);
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
    TransformTree& tree = property_trees.transform_tree;
    TransformNode& root = *tree.Node(0);
    root.local.Translate(2, 2);
    tree.SetTargetId(root.id, 0);
    tree.UpdateTransforms(0);

    TransformNode child;
    child.local.Translate(3, 3);
    child.source_node_id = 0;
    child.id = tree.Insert(child, 0);
    tree.SetTargetId(child.id, 0);

    TransformNode sibling;
    sibling.local.Translate(7, 7);
    sibling.source_node_id = 0;
    sibling.id = tree.Insert(sibling, 0);
    tree.SetTargetId(sibling.id, 0);

    SetupTransformTreeForTest(&tree);

    tree.UpdateTransforms(1);
    tree.UpdateTransforms(2);

    gfx::Transform expected;
    gfx::Transform transform;

    expected.Translate(4, 4);
    bool success = tree.ComputeTransformForTesting(2, 1, &transform);
    EXPECT_TRUE(success);
    EXPECT_TRANSFORMATION_MATRIX_EQ(expected, transform);

    transform.MakeIdentity();
    expected.MakeIdentity();
    expected.Translate(-4, -4);
    success = tree.ComputeTransformForTesting(1, 2, &transform);
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
    TransformTree& tree = property_trees.transform_tree;
    TransformNode& root = *tree.Node(0);
    root.local.Translate(2, 2);
    tree.SetTargetId(root.id, 0);
    tree.UpdateTransforms(0);

    TransformNode singular;
    singular.local.matrix().set(2, 2, 0.0);
    singular.source_node_id = 0;
    singular.id = tree.Insert(singular, 0);
    tree.SetTargetId(singular.id, 0);

    TransformNode child;
    child.local.Translate(3, 3);
    child.source_node_id = 1;
    child.id = tree.Insert(child, 1);
    tree.SetTargetId(child.id, 0);

    TransformNode sibling;
    sibling.local.Translate(7, 7);
    sibling.source_node_id = 1;
    sibling.id = tree.Insert(sibling, 1);
    tree.SetTargetId(sibling.id, 0);

    SetupTransformTreeForTest(&tree);

    tree.UpdateTransforms(1);
    tree.UpdateTransforms(2);
    tree.UpdateTransforms(3);

    gfx::Transform expected;
    gfx::Transform transform;

    expected.Translate(4, 4);
    bool success = tree.ComputeTransformForTesting(3, 2, &transform);
    EXPECT_TRUE(success);
    EXPECT_TRANSFORMATION_MATRIX_EQ(expected, transform);

    transform.MakeIdentity();
    expected.MakeIdentity();
    expected.Translate(-4, -4);
    success = tree.ComputeTransformForTesting(2, 3, &transform);
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
    TransformTree& tree = property_trees.transform_tree;
    EffectTree& effect_tree = property_trees.effect_tree;

    int grand_parent = tree.Insert(TransformNode(), 0);
    int effect_grand_parent = effect_tree.Insert(EffectNode(), 0);
    effect_tree.Node(effect_grand_parent)->has_render_surface = true;
    effect_tree.Node(effect_grand_parent)->transform_id = grand_parent;
    effect_tree.Node(effect_grand_parent)->surface_contents_scale =
        gfx::Vector2dF(1.f, 1.f);
    tree.SetContentTargetId(grand_parent, grand_parent);
    tree.SetTargetId(grand_parent, grand_parent);
    tree.Node(grand_parent)->source_node_id = 0;

    gfx::Transform rotation_about_x;
    rotation_about_x.RotateAboutXAxis(15);

    int parent = tree.Insert(TransformNode(), grand_parent);
    int effect_parent = effect_tree.Insert(EffectNode(), effect_grand_parent);
    effect_tree.Node(effect_parent)->transform_id = parent;
    effect_tree.Node(effect_parent)->has_render_surface = true;
    effect_tree.Node(effect_parent)->surface_contents_scale =
        gfx::Vector2dF(1.f, 1.f);
    tree.SetTargetId(parent, grand_parent);
    tree.SetContentTargetId(parent, parent);
    tree.Node(parent)->source_node_id = grand_parent;
    tree.Node(parent)->local = rotation_about_x;

    int child = tree.Insert(TransformNode(), parent);
    tree.SetTargetId(child, parent);
    tree.SetContentTargetId(child, parent);
    tree.Node(child)->source_node_id = parent;
    tree.Node(child)->flattens_inherited_transform = true;
    tree.Node(child)->local = rotation_about_x;

    int grand_child = tree.Insert(TransformNode(), child);
    tree.SetTargetId(grand_child, parent);
    tree.SetContentTargetId(grand_child, parent);
    tree.Node(grand_child)->source_node_id = child;
    tree.Node(grand_child)->flattens_inherited_transform = true;
    tree.Node(grand_child)->local = rotation_about_x;

    tree.set_needs_update(true);
    SetupTransformTreeForTest(&tree);
    draw_property_utils::ComputeTransforms(&tree);
    property_trees.ResetCachedData();

    gfx::Transform flattened_rotation_about_x = rotation_about_x;
    flattened_rotation_about_x.FlattenTo2d();

    gfx::Transform to_target;
    property_trees.GetToTarget(child, effect_parent, &to_target);
    EXPECT_TRANSFORMATION_MATRIX_EQ(rotation_about_x, to_target);

    EXPECT_TRANSFORMATION_MATRIX_EQ(
        flattened_rotation_about_x * rotation_about_x, tree.ToScreen(child));

    property_trees.GetToTarget(grand_child, effect_parent, &to_target);
    EXPECT_TRANSFORMATION_MATRIX_EQ(
        flattened_rotation_about_x * rotation_about_x, to_target);

    EXPECT_TRANSFORMATION_MATRIX_EQ(flattened_rotation_about_x *
                                        flattened_rotation_about_x *
                                        rotation_about_x,
                                    tree.ToScreen(grand_child));

    gfx::Transform grand_child_to_child;
    bool success = tree.ComputeTransformForTesting(grand_child, child,
                                                   &grand_child_to_child);
    EXPECT_TRUE(success);
    EXPECT_TRANSFORMATION_MATRIX_EQ(rotation_about_x, grand_child_to_child);

    // Remove flattening at grand_child, and recompute transforms.
    tree.Node(grand_child)->flattens_inherited_transform = false;
    tree.set_needs_update(true);
    SetupTransformTreeForTest(&tree);
    draw_property_utils::ComputeTransforms(&tree);

    property_trees.GetToTarget(grand_child, effect_parent, &to_target);
    EXPECT_TRANSFORMATION_MATRIX_EQ(rotation_about_x * rotation_about_x,
                                    to_target);

    EXPECT_TRANSFORMATION_MATRIX_EQ(
        flattened_rotation_about_x * rotation_about_x * rotation_about_x,
        tree.ToScreen(grand_child));

    success = tree.ComputeTransformForTesting(grand_child, child,
                                              &grand_child_to_child);
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
    TransformTree& tree = property_trees.transform_tree;
    TransformNode& root = *tree.Node(0);
    root.local.Translate(2, 2);
    tree.SetTargetId(root.id, 0);
    tree.UpdateTransforms(0);

    TransformNode child;
    child.local.Scale(2, 2);
    child.source_node_id = 0;
    child.id = tree.Insert(child, 0);
    tree.SetTargetId(child.id, 0);

    SetupTransformTreeForTest(&tree);
    tree.UpdateTransforms(1);

    gfx::Transform expected;
    expected.Translate(2, 2);
    expected.Scale(2, 2);

    gfx::Transform transform;
    gfx::Transform inverse;

    bool success = tree.ComputeTransformForTesting(1, -1, &transform);
    EXPECT_TRUE(success);
    EXPECT_TRANSFORMATION_MATRIX_EQ(expected, transform);

    success = tree.ComputeTransformForTesting(-1, 1, &inverse);
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
    TransformTree& tree = property_trees.transform_tree;
    TransformNode& root = *tree.Node(0);
    tree.SetTargetId(root.id, 0);
    tree.UpdateTransforms(0);

    TransformNode child;
    child.local.Scale(0, 0);
    child.source_node_id = 0;
    child.id = tree.Insert(child, 0);
    tree.SetTargetId(child.id, 0);

    SetupTransformTreeForTest(&tree);
    tree.UpdateTransforms(1);

    gfx::Transform expected;
    expected.Scale(0, 0);

    gfx::Transform transform;
    gfx::Transform inverse;

    bool success = tree.ComputeTransformForTesting(1, 0, &transform);
    EXPECT_TRUE(success);
    EXPECT_TRANSFORMATION_MATRIX_EQ(expected, transform);

    // To compute this would require inverting the 0 matrix, so we cannot
    // succeed.
    success = tree.ComputeTransformForTesting(0, 1, &inverse);
    EXPECT_FALSE(success);
  }
};

DIRECT_AND_SERIALIZED_PROPERTY_TREE_TEST_F(
    PropertyTreeTestComputeTransformWithUninvertibleTransform);

class PropertyTreeTestComputeTransformToTargetWithZeroSurfaceContentsScale
    : public PropertyTreeTest {
 protected:
  void StartTest() override {
    PropertyTrees property_trees;
    TransformTree& tree = property_trees.transform_tree;
    TransformNode& root = *tree.Node(0);
    tree.SetTargetId(root.id, 0);
    tree.UpdateTransforms(0);

    TransformNode grand_parent;
    grand_parent.local.Scale(2.f, 0.f);
    grand_parent.source_node_id = 0;
    int grand_parent_id = tree.Insert(grand_parent, 0);
    tree.SetTargetId(grand_parent_id, 0);
    tree.SetContentTargetId(grand_parent_id, grand_parent_id);
    tree.UpdateTransforms(grand_parent_id);

    TransformNode parent;
    parent.local.Translate(1.f, 1.f);
    parent.source_node_id = grand_parent_id;
    int parent_id = tree.Insert(parent, grand_parent_id);
    tree.SetTargetId(parent_id, grand_parent_id);
    tree.SetContentTargetId(parent_id, grand_parent_id);
    tree.UpdateTransforms(parent_id);

    TransformNode child;
    child.local.Translate(3.f, 4.f);
    child.source_node_id = parent_id;
    int child_id = tree.Insert(child, parent_id);
    tree.SetTargetId(child_id, grand_parent_id);
    tree.SetContentTargetId(child_id, grand_parent_id);
    SetupTransformTreeForTest(&tree);
    tree.UpdateTransforms(child_id);

    gfx::Transform expected_transform;
    expected_transform.Translate(4.f, 5.f);

    gfx::Transform transform;
    bool success =
        tree.ComputeTransformForTesting(child_id, grand_parent_id, &transform);
    EXPECT_TRUE(success);
    EXPECT_TRANSFORMATION_MATRIX_EQ(expected_transform, transform);

    tree.Node(grand_parent_id)->local.MakeIdentity();
    tree.Node(grand_parent_id)->local.Scale(0.f, 2.f);
    tree.Node(grand_parent_id)->needs_local_transform_update = true;
    tree.set_needs_update(true);
    SetupTransformTreeForTest(&tree);

    draw_property_utils::ComputeTransforms(&tree);

    success =
        tree.ComputeTransformForTesting(child_id, grand_parent_id, &transform);
    EXPECT_TRUE(success);
    EXPECT_TRANSFORMATION_MATRIX_EQ(expected_transform, transform);

    tree.Node(grand_parent_id)->local.MakeIdentity();
    tree.Node(grand_parent_id)->local.Scale(0.f, 0.f);
    tree.Node(grand_parent_id)->needs_local_transform_update = true;
    tree.set_needs_update(true);
    SetupTransformTreeForTest(&tree);

    draw_property_utils::ComputeTransforms(&tree);

    success =
        tree.ComputeTransformForTesting(child_id, grand_parent_id, &transform);
    EXPECT_TRUE(success);
    EXPECT_TRANSFORMATION_MATRIX_EQ(expected_transform, transform);
  }
};

DIRECT_AND_SERIALIZED_PROPERTY_TREE_TEST_F(
    PropertyTreeTestComputeTransformToTargetWithZeroSurfaceContentsScale);

class PropertyTreeTestFlatteningWhenDestinationHasOnlyFlatAncestors
    : public PropertyTreeTest {
 protected:
  void StartTest() override {
    // This tests that flattening is performed correctly when
    // destination and its ancestors are flat, but there are 3d transforms
    // and flattening between the source and destination.
    PropertyTrees property_trees;
    TransformTree& tree = property_trees.transform_tree;

    int parent = tree.Insert(TransformNode(), 0);
    tree.SetContentTargetId(parent, parent);
    tree.SetTargetId(parent, parent);
    tree.Node(parent)->source_node_id = 0;
    tree.Node(parent)->local.Translate(2, 2);

    gfx::Transform rotation_about_x;
    rotation_about_x.RotateAboutXAxis(15);

    int child = tree.Insert(TransformNode(), parent);
    tree.SetContentTargetId(child, child);
    tree.SetTargetId(child, child);
    tree.Node(child)->source_node_id = parent;
    tree.Node(child)->local = rotation_about_x;

    int grand_child = tree.Insert(TransformNode(), child);
    tree.SetContentTargetId(grand_child, grand_child);
    tree.SetTargetId(grand_child, grand_child);
    tree.Node(grand_child)->source_node_id = child;
    tree.Node(grand_child)->flattens_inherited_transform = true;

    tree.set_needs_update(true);
    SetupTransformTreeForTest(&tree);
    draw_property_utils::ComputeTransforms(&tree);

    gfx::Transform flattened_rotation_about_x = rotation_about_x;
    flattened_rotation_about_x.FlattenTo2d();

    gfx::Transform grand_child_to_parent;
    bool success = tree.ComputeTransformForTesting(grand_child, parent,
                                                   &grand_child_to_parent);
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
    SetupEffectTreeForTest(&tree);

    EXPECT_EQ(tree.Node(child)->screen_space_opacity, 1.f);
    tree.Node(parent)->opacity = 0.5f;
    tree.set_needs_update(true);
    SetupEffectTreeForTest(&tree);
    draw_property_utils::ComputeEffects(&tree);
    EXPECT_EQ(tree.Node(child)->screen_space_opacity, 0.5f);

    tree.Node(child)->opacity = 0.5f;
    tree.set_needs_update(true);
    SetupEffectTreeForTest(&tree);
    draw_property_utils::ComputeEffects(&tree);
    EXPECT_EQ(tree.Node(child)->screen_space_opacity, 0.25f);
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
    TransformTree& tree = property_trees.transform_tree;

    int parent = tree.Insert(TransformNode(), 0);
    tree.SetTargetId(parent, parent);
    tree.Node(parent)->source_node_id = 0;
    tree.Node(parent)->local.Translate(1.5f, 1.5f);

    int child = tree.Insert(TransformNode(), parent);
    tree.SetTargetId(child, parent);
    tree.Node(child)->local.Translate(1, 1);
    tree.Node(child)->source_node_id = parent;
    tree.set_needs_update(true);
    SetupTransformTreeForTest(&tree);
    draw_property_utils::ComputeTransforms(&tree);
    EXPECT_FALSE(
        tree.Node(parent)->node_and_ancestors_have_only_integer_translation);
    EXPECT_FALSE(
        tree.Node(child)->node_and_ancestors_have_only_integer_translation);

    tree.Node(parent)->local.Translate(0.5f, 0.5f);
    tree.Node(child)->local.Translate(0.5f, 0.5f);
    tree.Node(parent)->needs_local_transform_update = true;
    tree.Node(child)->needs_local_transform_update = true;
    tree.set_needs_update(true);
    SetupTransformTreeForTest(&tree);
    draw_property_utils::ComputeTransforms(&tree);
    EXPECT_TRUE(
        tree.Node(parent)->node_and_ancestors_have_only_integer_translation);
    EXPECT_FALSE(
        tree.Node(child)->node_and_ancestors_have_only_integer_translation);

    tree.Node(child)->local.Translate(0.5f, 0.5f);
    tree.Node(child)->needs_local_transform_update = true;
    tree.SetTargetId(child, child);
    tree.set_needs_update(true);
    SetupTransformTreeForTest(&tree);
    draw_property_utils::ComputeTransforms(&tree);
    EXPECT_TRUE(
        tree.Node(parent)->node_and_ancestors_have_only_integer_translation);
    EXPECT_TRUE(
        tree.Node(child)->node_and_ancestors_have_only_integer_translation);
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
    TransformTree& tree = property_trees.transform_tree;
    EffectTree& effect_tree = property_trees.effect_tree;

    int parent = tree.Insert(TransformNode(), 0);
    int effect_parent = effect_tree.Insert(EffectNode(), 0);
    effect_tree.Node(effect_parent)->has_render_surface = true;
    tree.SetTargetId(parent, parent);
    tree.Node(parent)->scrolls = true;
    tree.Node(parent)->source_node_id = 0;

    int child = tree.Insert(TransformNode(), parent);
    TransformNode* child_node = tree.Node(child);
    tree.SetTargetId(child, parent);
    child_node->scrolls = true;
    child_node->local.Scale3d(6.0f, 6.0f, 0.0f);
    child_node->local.Translate(1.3f, 1.3f);
    child_node->source_node_id = parent;
    tree.set_needs_update(true);

    SetupTransformTreeForTest(&tree);
    draw_property_utils::ComputeTransforms(&tree);
    property_trees.ResetCachedData();

    gfx::Transform from_target;
    gfx::Transform to_target;
    property_trees.GetToTarget(child, effect_parent, &to_target);
    EXPECT_FALSE(to_target.GetInverse(&from_target));
    // The following checks are to ensure that snapping is skipped because of
    // singular transform (and not because of other reasons which also cause
    // snapping to be skipped).
    EXPECT_TRUE(child_node->scrolls);
    property_trees.GetToTarget(child, effect_parent, &to_target);
    EXPECT_TRUE(to_target.IsScaleOrTranslation());
    EXPECT_FALSE(child_node->to_screen_is_potentially_animated);
    EXPECT_FALSE(child_node->ancestors_are_invertible);

    gfx::Transform rounded;
    property_trees.GetToTarget(child, effect_parent, &rounded);
    rounded.RoundTranslationComponents();
    property_trees.GetToTarget(child, effect_parent, &to_target);
    EXPECT_NE(to_target, rounded);
  }
};

DIRECT_AND_SERIALIZED_PROPERTY_TREE_TEST_F(
    PropertyTreeTestSingularTransformSnapTest);

#undef DIRECT_AND_SERIALIZED_PROPERTY_TREE_TEST_F
#undef SERIALIZED_PROPERTY_TREE_TEST_F
#undef DIRECT_PROPERTY_TREE_TEST_F

}  // namespace
}  // namespace cc
