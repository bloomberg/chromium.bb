// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/layer_proto_converter.h"

#include "cc/layers/layer.h"
#include "cc/proto/layer.pb.h"
#include "cc/trees/layer_tree_settings.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {

TEST(LayerProtoConverterTest, TestKeepingRoot) {
  /* Test deserialization of a tree that looks like:
         root
        /   \
       a     b
              \
               c
     The old root node will be reused during deserialization.
  */
  scoped_refptr<Layer> old_root = Layer::Create(LayerSettings());
  proto::LayerNode root_node;
  root_node.set_id(old_root->id());
  root_node.set_type(proto::LayerType::Base);

  proto::LayerNode* child_a_node = root_node.add_children();
  child_a_node->set_id(442);
  child_a_node->set_type(proto::LayerType::Base);
  child_a_node->set_parent_id(old_root->id());  // root_node

  proto::LayerNode* child_b_node = root_node.add_children();
  child_b_node->set_id(443);
  child_b_node->set_type(proto::LayerType::Base);
  child_b_node->set_parent_id(old_root->id());  // root_node

  proto::LayerNode* child_c_node = child_b_node->add_children();
  child_c_node->set_id(444);
  child_c_node->set_type(proto::LayerType::Base);
  child_c_node->set_parent_id(child_b_node->id());

  scoped_refptr<Layer> new_root =
      LayerProtoConverter::DeserializeLayerHierarchy(old_root, root_node);

  // The new root should not be the same as the old root.
  EXPECT_EQ(old_root->id(), new_root->id());
  ASSERT_EQ(2u, new_root->children().size());
  scoped_refptr<Layer> child_a = new_root->children()[0];
  scoped_refptr<Layer> child_b = new_root->children()[1];

  EXPECT_EQ(child_a_node->id(), child_a->id());
  EXPECT_EQ(child_b_node->id(), child_b->id());

  EXPECT_EQ(0u, child_a->children().size());
  ASSERT_EQ(1u, child_b->children().size());

  scoped_refptr<Layer> child_c = child_b->children()[0];
  EXPECT_EQ(child_c_node->id(), child_c->id());
}

TEST(LayerProtoConverterTest, TestSwappingRoot) {
  /* Test deserialization of a tree that looks like:
         root
        /   \
       a     b
              \
               c
     The old root node will be swapped out during deserialization.
  */
  proto::LayerNode root_node;
  root_node.set_id(441);
  root_node.set_type(proto::LayerType::Base);

  proto::LayerNode* child_a_node = root_node.add_children();
  child_a_node->set_id(442);
  child_a_node->set_type(proto::LayerType::Base);
  child_a_node->set_parent_id(root_node.id());

  proto::LayerNode* child_b_node = root_node.add_children();
  child_b_node->set_id(443);
  child_b_node->set_type(proto::LayerType::Base);
  child_b_node->set_parent_id(root_node.id());

  proto::LayerNode* child_c_node = child_b_node->add_children();
  child_c_node->set_id(444);
  child_c_node->set_type(proto::LayerType::Base);
  child_c_node->set_parent_id(child_b_node->id());

  scoped_refptr<Layer> old_root = Layer::Create(LayerSettings());
  scoped_refptr<Layer> new_root =
      LayerProtoConverter::DeserializeLayerHierarchy(old_root, root_node);

  // The new root should not be the same as the old root.
  EXPECT_EQ(root_node.id(), new_root->id());
  ASSERT_EQ(2u, new_root->children().size());
  scoped_refptr<Layer> child_a = new_root->children()[0];
  scoped_refptr<Layer> child_b = new_root->children()[1];

  EXPECT_EQ(child_a_node->id(), child_a->id());
  EXPECT_EQ(child_b_node->id(), child_b->id());

  EXPECT_EQ(0u, child_a->children().size());
  ASSERT_EQ(1u, child_b->children().size());

  scoped_refptr<Layer> child_c = child_b->children()[0];
  EXPECT_EQ(child_c_node->id(), child_c->id());
}

}  // namespace cc
