// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/layer_proto_converter.h"

#include "cc/layers/empty_content_layer_client.h"
#include "cc/layers/layer.h"
#include "cc/layers/layer_settings.h"
#include "cc/layers/picture_layer.h"
#include "cc/proto/layer.pb.h"
#include "cc/test/fake_layer_tree_host.h"
#include "cc/test/fake_layer_tree_host_client.h"
#include "cc/test/test_task_graph_runner.h"
#include "cc/trees/layer_tree_settings.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {
class LayerProtoConverterTest : public testing::Test {
 public:
  LayerProtoConverterTest()
      : fake_client_(FakeLayerTreeHostClient::DIRECT_3D) {}

 protected:
  void SetUp() override {
    layer_tree_host_ =
        FakeLayerTreeHost::Create(&fake_client_, &task_graph_runner_);
  }

  void TearDown() override {
    layer_tree_host_->SetRootLayer(nullptr);
    layer_tree_host_ = nullptr;
  }

  TestTaskGraphRunner task_graph_runner_;
  FakeLayerTreeHostClient fake_client_;
  scoped_ptr<FakeLayerTreeHost> layer_tree_host_;
};

TEST_F(LayerProtoConverterTest, TestKeepingRoot) {
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
  root_node.set_type(proto::LayerType::LAYER);

  proto::LayerNode* child_a_node = root_node.add_children();
  child_a_node->set_id(442);
  child_a_node->set_type(proto::LayerType::LAYER);
  child_a_node->set_parent_id(old_root->id());  // root_node

  proto::LayerNode* child_b_node = root_node.add_children();
  child_b_node->set_id(443);
  child_b_node->set_type(proto::LayerType::LAYER);
  child_b_node->set_parent_id(old_root->id());  // root_node

  proto::LayerNode* child_c_node = child_b_node->add_children();
  child_c_node->set_id(444);
  child_c_node->set_type(proto::LayerType::LAYER);
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

TEST_F(LayerProtoConverterTest, TestNoExistingRoot) {
  /* Test deserialization of a tree that looks like:
         root
        /
       a
     There is no existing root node before serialization.
  */
  int new_root_id = 244;
  proto::LayerNode root_node;
  root_node.set_id(new_root_id);
  root_node.set_type(proto::LayerType::LAYER);

  proto::LayerNode* child_a_node = root_node.add_children();
  child_a_node->set_id(442);
  child_a_node->set_type(proto::LayerType::LAYER);
  child_a_node->set_parent_id(new_root_id);  // root_node

  scoped_refptr<Layer> new_root =
      LayerProtoConverter::DeserializeLayerHierarchy(nullptr, root_node);

  // The new root should not be the same as the old root.
  EXPECT_EQ(new_root_id, new_root->id());
  ASSERT_EQ(1u, new_root->children().size());
  scoped_refptr<Layer> child_a = new_root->children()[0];

  EXPECT_EQ(child_a_node->id(), child_a->id());
  EXPECT_EQ(0u, child_a->children().size());
}

TEST_F(LayerProtoConverterTest, TestSwappingRoot) {
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
  root_node.set_type(proto::LayerType::LAYER);

  proto::LayerNode* child_a_node = root_node.add_children();
  child_a_node->set_id(442);
  child_a_node->set_type(proto::LayerType::LAYER);
  child_a_node->set_parent_id(root_node.id());

  proto::LayerNode* child_b_node = root_node.add_children();
  child_b_node->set_id(443);
  child_b_node->set_type(proto::LayerType::LAYER);
  child_b_node->set_parent_id(root_node.id());

  proto::LayerNode* child_c_node = child_b_node->add_children();
  child_c_node->set_id(444);
  child_c_node->set_type(proto::LayerType::LAYER);
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

TEST_F(LayerProtoConverterTest, RecursivePropertiesSerialization) {
  /* Testing serialization of properties for a tree that looks like this:
          root+
          /  \
         a*   b*+[mask:*,replica]
        /      \
       c        d*
     Layers marked with * have changed properties.
     Layers marked with + have descendants with changed properties.
     Layer b also has a mask layer and a replica layer.
  */
  scoped_refptr<Layer> layer_src_root = Layer::Create(LayerSettings());
  scoped_refptr<Layer> layer_src_a = Layer::Create(LayerSettings());
  scoped_refptr<Layer> layer_src_b = Layer::Create(LayerSettings());
  scoped_refptr<Layer> layer_src_b_mask = Layer::Create(LayerSettings());
  scoped_refptr<Layer> layer_src_b_replica = Layer::Create(LayerSettings());
  scoped_refptr<Layer> layer_src_c = Layer::Create(LayerSettings());
  scoped_refptr<Layer> layer_src_d = Layer::Create(LayerSettings());
  layer_src_root->AddChild(layer_src_a);
  layer_src_root->AddChild(layer_src_b);
  layer_src_a->AddChild(layer_src_c);
  layer_src_b->AddChild(layer_src_d);
  layer_src_b->SetMaskLayer(layer_src_b_mask.get());
  layer_src_b->SetReplicaLayer(layer_src_b_replica.get());

  layer_src_a->SetNeedsPushProperties();
  layer_src_b->SetNeedsPushProperties();
  layer_src_b_mask->SetNeedsPushProperties();
  layer_src_d->SetNeedsPushProperties();

  proto::LayerUpdate layer_update;
  LayerProtoConverter::SerializeLayerProperties(layer_src_root.get(),
                                                &layer_update);

  // All flags for pushing properties should have been cleared.
  EXPECT_FALSE(layer_src_root->needs_push_properties());
  EXPECT_FALSE(layer_src_root->descendant_needs_push_properties());
  EXPECT_FALSE(layer_src_a->needs_push_properties());
  EXPECT_FALSE(layer_src_a->descendant_needs_push_properties());
  EXPECT_FALSE(layer_src_b->needs_push_properties());
  EXPECT_FALSE(layer_src_b->descendant_needs_push_properties());
  EXPECT_FALSE(layer_src_b_mask->needs_push_properties());
  EXPECT_FALSE(layer_src_b_mask->descendant_needs_push_properties());
  EXPECT_FALSE(layer_src_b_replica->needs_push_properties());
  EXPECT_FALSE(layer_src_b_replica->descendant_needs_push_properties());
  EXPECT_FALSE(layer_src_c->needs_push_properties());
  EXPECT_FALSE(layer_src_c->descendant_needs_push_properties());
  EXPECT_FALSE(layer_src_d->needs_push_properties());
  EXPECT_FALSE(layer_src_d->descendant_needs_push_properties());

  // Only 5 of the layers should have been serialized.
  ASSERT_EQ(5, layer_update.layers_size());
  EXPECT_EQ(layer_src_root->id(), layer_update.layers(0).id());
  proto::LayerProperties dest_root = layer_update.layers(0);
  EXPECT_EQ(layer_src_a->id(), layer_update.layers(1).id());
  proto::LayerProperties dest_a = layer_update.layers(1);
  EXPECT_EQ(layer_src_b->id(), layer_update.layers(2).id());
  proto::LayerProperties dest_b = layer_update.layers(2);
  EXPECT_EQ(layer_src_d->id(), layer_update.layers(3).id());
  proto::LayerProperties dest_d = layer_update.layers(3);
  EXPECT_EQ(layer_src_b_mask->id(), layer_update.layers(4).id());
  proto::LayerProperties dest_b_mask = layer_update.layers(4);

  // Ensure the properties and dependants metadata is correctly serialized.
  EXPECT_FALSE(dest_root.needs_push_properties());
  EXPECT_EQ(2, dest_root.num_dependents_need_push_properties());
  EXPECT_FALSE(dest_root.has_base());

  EXPECT_TRUE(dest_a.needs_push_properties());
  EXPECT_EQ(0, dest_a.num_dependents_need_push_properties());
  EXPECT_TRUE(dest_a.has_base());

  EXPECT_TRUE(dest_b.needs_push_properties());
  EXPECT_EQ(2, dest_b.num_dependents_need_push_properties());
  EXPECT_TRUE(dest_b.has_base());

  EXPECT_TRUE(dest_d.needs_push_properties());
  EXPECT_EQ(0, dest_d.num_dependents_need_push_properties());
  EXPECT_TRUE(dest_d.has_base());

  EXPECT_TRUE(dest_b_mask.needs_push_properties());
  EXPECT_EQ(0, dest_b_mask.num_dependents_need_push_properties());
  EXPECT_TRUE(dest_b_mask.has_base());
}

TEST_F(LayerProtoConverterTest, RecursivePropertiesSerializationSingleChild) {
  /* Testing serialization of properties for a tree that looks like this:
          root+
             \
              b*+[mask:*]
               \
                c
     Layers marked with * have changed properties.
     Layers marked with + have descendants with changed properties.
     Layer b also has a mask layer.
  */
  scoped_refptr<Layer> layer_src_root = Layer::Create(LayerSettings());
  scoped_refptr<Layer> layer_src_b = Layer::Create(LayerSettings());
  scoped_refptr<Layer> layer_src_b_mask = Layer::Create(LayerSettings());
  scoped_refptr<Layer> layer_src_c = Layer::Create(LayerSettings());
  layer_src_root->AddChild(layer_src_b);
  layer_src_b->AddChild(layer_src_c);
  layer_src_b->SetMaskLayer(layer_src_b_mask.get());

  layer_src_b->SetNeedsPushProperties();
  layer_src_b_mask->SetNeedsPushProperties();

  proto::LayerUpdate layer_update;
  LayerProtoConverter::SerializeLayerProperties(layer_src_root.get(),
                                                &layer_update);

  // All flags for pushing properties should have been cleared.
  EXPECT_FALSE(layer_src_root->needs_push_properties());
  EXPECT_FALSE(layer_src_root->descendant_needs_push_properties());
  EXPECT_FALSE(layer_src_b->needs_push_properties());
  EXPECT_FALSE(layer_src_b->descendant_needs_push_properties());
  EXPECT_FALSE(layer_src_b_mask->needs_push_properties());
  EXPECT_FALSE(layer_src_b_mask->descendant_needs_push_properties());
  EXPECT_FALSE(layer_src_c->needs_push_properties());
  EXPECT_FALSE(layer_src_c->descendant_needs_push_properties());

  // Only 3 of the layers should have been serialized.
  ASSERT_EQ(3, layer_update.layers_size());
  EXPECT_EQ(layer_src_root->id(), layer_update.layers(0).id());
  proto::LayerProperties dest_root = layer_update.layers(0);
  EXPECT_EQ(layer_src_b->id(), layer_update.layers(1).id());
  proto::LayerProperties dest_b = layer_update.layers(1);
  EXPECT_EQ(layer_src_b_mask->id(), layer_update.layers(2).id());
  proto::LayerProperties dest_b_mask = layer_update.layers(2);

  // Ensure the properties and dependants metadata is correctly serialized.
  EXPECT_FALSE(dest_root.needs_push_properties());
  EXPECT_EQ(1, dest_root.num_dependents_need_push_properties());
  EXPECT_FALSE(dest_root.has_base());

  EXPECT_TRUE(dest_b.needs_push_properties());
  EXPECT_EQ(1, dest_b.num_dependents_need_push_properties());
  EXPECT_TRUE(dest_b.has_base());

  EXPECT_TRUE(dest_b_mask.needs_push_properties());
  EXPECT_EQ(0, dest_b_mask.num_dependents_need_push_properties());
  EXPECT_TRUE(dest_b_mask.has_base());
}

TEST_F(LayerProtoConverterTest, DeserializeLayerProperties) {
  /* Testing deserialization of properties for a tree that looks like this:
          root*+
          /  \
         a    b+
               \
                c*
     Layers marked with * have changed properties.
     Layers marked with + have descendants with changed properties.
  */
  proto::LayerUpdate updates;

  scoped_refptr<Layer> root = Layer::Create(LayerSettings());
  root->SetLayerTreeHost(layer_tree_host_.get());
  proto::LayerProperties* root_props = updates.add_layers();
  root_props->set_id(root->id());
  root_props->set_needs_push_properties(true);
  root_props->set_num_dependents_need_push_properties(1);
  root_props->mutable_base();

  scoped_refptr<Layer> a = Layer::Create(LayerSettings());
  a->SetLayerTreeHost(layer_tree_host_.get());
  proto::LayerProperties* a_props = updates.add_layers();
  a_props->set_id(a->id());
  a_props->set_needs_push_properties(false);
  a_props->set_num_dependents_need_push_properties(0);
  root->AddChild(a);

  scoped_refptr<Layer> b = Layer::Create(LayerSettings());
  b->SetLayerTreeHost(layer_tree_host_.get());
  proto::LayerProperties* b_props = updates.add_layers();
  b_props->set_id(b->id());
  b_props->set_needs_push_properties(false);
  b_props->set_num_dependents_need_push_properties(1);
  root->AddChild(b);

  scoped_refptr<Layer> c = Layer::Create(LayerSettings());
  c->SetLayerTreeHost(layer_tree_host_.get());
  proto::LayerProperties* c_props = updates.add_layers();
  c_props->set_id(c->id());
  c_props->set_needs_push_properties(true);
  c_props->set_num_dependents_need_push_properties(0);
  c_props->mutable_base();
  b->AddChild(c);

  LayerProtoConverter::DeserializeLayerProperties(root.get(), updates);

  EXPECT_TRUE(root->needs_push_properties());
  EXPECT_TRUE(root->descendant_needs_push_properties());

  EXPECT_FALSE(a->needs_push_properties());
  EXPECT_FALSE(a->descendant_needs_push_properties());

  EXPECT_FALSE(b->needs_push_properties());
  EXPECT_TRUE(b->descendant_needs_push_properties());

  EXPECT_TRUE(c->needs_push_properties());
  EXPECT_FALSE(c->descendant_needs_push_properties());

  // Recursively clear out LayerTreeHost.
  root->SetLayerTreeHost(nullptr);
}

TEST_F(LayerProtoConverterTest, PictureLayerTypeSerialization) {
  // Make sure that PictureLayers serialize to the
  // proto::LayerType::PICTURE_LAYER type.
  scoped_refptr<PictureLayer> layer = PictureLayer::Create(
      LayerSettings(), EmptyContentLayerClient::GetInstance());

  proto::LayerNode layer_hierarchy;
  LayerProtoConverter::SerializeLayerHierarchy(layer.get(), &layer_hierarchy);
  EXPECT_EQ(proto::LayerType::PICTURE_LAYER, layer_hierarchy.type());
}

TEST_F(LayerProtoConverterTest, PictureLayerTypeDeserialization) {
  // Make sure that proto::LayerType::PICTURE_LAYER ends up building a
  // PictureLayer.
  scoped_refptr<Layer> old_root = PictureLayer::Create(
      LayerSettings(), EmptyContentLayerClient::GetInstance());
  proto::LayerNode root_node;
  root_node.set_id(old_root->id());
  root_node.set_type(proto::LayerType::PICTURE_LAYER);

  scoped_refptr<Layer> new_root =
      LayerProtoConverter::DeserializeLayerHierarchy(old_root, root_node);

  // Validate that the ids are equal.
  EXPECT_EQ(old_root->id(), new_root->id());

  // Check that the layer type is equal by using the type this layer would
  // serialize to.
  proto::LayerNode layer_node;
  new_root->SetTypeForProtoSerialization(&layer_node);
  EXPECT_EQ(proto::LayerType::PICTURE_LAYER, layer_node.type());
}

}  // namespace
}  // namespace cc
