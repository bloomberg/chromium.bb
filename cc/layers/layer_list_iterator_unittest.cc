// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/layer_list_iterator.h"

#include "base/memory/scoped_ptr.h"
#include "cc/test/fake_impl_task_runner_provider.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "cc/test/fake_output_surface.h"
#include "cc/test/test_shared_bitmap_manager.h"
#include "cc/test/test_task_graph_runner.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

TEST(LayerListIteratorTest, VerifyTraversalOrder) {
  // Unfortunate preamble.
  FakeImplTaskRunnerProvider task_runner_provider;
  TestSharedBitmapManager shared_bitmap_manager;
  TestTaskGraphRunner task_graph_runner;
  scoped_ptr<OutputSurface> output_surface = FakeOutputSurface::Create3d();
  FakeLayerTreeHostImpl host_impl(&task_runner_provider, &shared_bitmap_manager,
                                  &task_graph_runner);
  host_impl.SetVisible(true);
  EXPECT_TRUE(host_impl.InitializeRenderer(output_surface.get()));

  // This test constructs the following tree.
  // 1
  // +-2
  // | +-3
  // | +-4
  // + 5
  //   +-6
  //   +-7
  // We expect to visit all seven layers in that order.
  scoped_ptr<LayerImpl> layer1 = LayerImpl::Create(host_impl.active_tree(), 1);
  scoped_ptr<LayerImpl> layer2 = LayerImpl::Create(host_impl.active_tree(), 2);
  scoped_ptr<LayerImpl> layer3 = LayerImpl::Create(host_impl.active_tree(), 3);
  scoped_ptr<LayerImpl> layer4 = LayerImpl::Create(host_impl.active_tree(), 4);
  scoped_ptr<LayerImpl> layer5 = LayerImpl::Create(host_impl.active_tree(), 5);
  scoped_ptr<LayerImpl> layer6 = LayerImpl::Create(host_impl.active_tree(), 6);
  scoped_ptr<LayerImpl> layer7 = LayerImpl::Create(host_impl.active_tree(), 7);

  layer2->AddChild(std::move(layer3));
  layer2->AddChild(std::move(layer4));

  layer5->AddChild(std::move(layer6));
  layer5->AddChild(std::move(layer7));

  layer1->AddChild(std::move(layer2));
  layer1->AddChild(std::move(layer5));

  int i = 1;
  LayerListIterator it(layer1.get());
  LayerListIterator end(nullptr);
  for (; it != end; ++it, ++i) {
    EXPECT_EQ(i, it->id());
  }
  EXPECT_EQ(8, i);
}

TEST(LayerListIteratorTest, VerifySingleLayer) {
  // Unfortunate preamble.
  FakeImplTaskRunnerProvider task_runner_provider;
  TestSharedBitmapManager shared_bitmap_manager;
  TestTaskGraphRunner task_graph_runner;
  scoped_ptr<OutputSurface> output_surface = FakeOutputSurface::Create3d();
  FakeLayerTreeHostImpl host_impl(&task_runner_provider, &shared_bitmap_manager,
                                  &task_graph_runner);
  host_impl.SetVisible(true);
  EXPECT_TRUE(host_impl.InitializeRenderer(output_surface.get()));

  // This test constructs a tree consisting of a single layer.
  scoped_ptr<LayerImpl> layer1 = LayerImpl::Create(host_impl.active_tree(), 1);
  int i = 1;
  LayerListIterator it(layer1.get());
  LayerListIterator end(nullptr);
  for (; it != end; ++it, ++i) {
    EXPECT_EQ(i, it->id());
  }
  EXPECT_EQ(2, i);
}

TEST(LayerListIteratorTest, VerifyNullFirstLayer) {
  // Ensures that if an iterator is constructed with a nullptr, that it can be
  // iterated without issue and that it remains equal to any other
  // null-initialized iterator.
  LayerListIterator it(nullptr);
  LayerListIterator end(nullptr);

  EXPECT_EQ(it, end);
  ++it;
  EXPECT_EQ(it, end);
}

TEST(LayerListReverseIteratorTest, VerifyTraversalOrder) {
  // Unfortunate preamble.
  FakeImplTaskRunnerProvider task_runner_provider;
  TestSharedBitmapManager shared_bitmap_manager;
  TestTaskGraphRunner task_graph_runner;
  scoped_ptr<OutputSurface> output_surface = FakeOutputSurface::Create3d();
  FakeLayerTreeHostImpl host_impl(&task_runner_provider, &shared_bitmap_manager,
                                  &task_graph_runner);
  host_impl.SetVisible(true);
  EXPECT_TRUE(host_impl.InitializeRenderer(output_surface.get()));

  // This test constructs the following tree.
  // 1
  // +-2
  // | +-3
  // | +-4
  // + 5
  //   +-6
  //   +-7
  // We expect to visit all seven layers in reverse order.
  scoped_ptr<LayerImpl> layer1 = LayerImpl::Create(host_impl.active_tree(), 1);
  scoped_ptr<LayerImpl> layer2 = LayerImpl::Create(host_impl.active_tree(), 2);
  scoped_ptr<LayerImpl> layer3 = LayerImpl::Create(host_impl.active_tree(), 3);
  scoped_ptr<LayerImpl> layer4 = LayerImpl::Create(host_impl.active_tree(), 4);
  scoped_ptr<LayerImpl> layer5 = LayerImpl::Create(host_impl.active_tree(), 5);
  scoped_ptr<LayerImpl> layer6 = LayerImpl::Create(host_impl.active_tree(), 6);
  scoped_ptr<LayerImpl> layer7 = LayerImpl::Create(host_impl.active_tree(), 7);

  layer2->AddChild(std::move(layer3));
  layer2->AddChild(std::move(layer4));

  layer5->AddChild(std::move(layer6));
  layer5->AddChild(std::move(layer7));

  layer1->AddChild(std::move(layer2));
  layer1->AddChild(std::move(layer5));

  int i = 7;
  LayerListReverseIterator it(layer1.get());
  LayerListReverseIterator end(nullptr);
  for (; it != end; ++it, --i) {
    EXPECT_EQ(i, it->id());
  }
  EXPECT_EQ(0, i);
}

TEST(LayerListReverseIteratorTest, VerifySingleLayer) {
  // Unfortunate preamble.
  FakeImplTaskRunnerProvider task_runner_provider;
  TestSharedBitmapManager shared_bitmap_manager;
  TestTaskGraphRunner task_graph_runner;
  scoped_ptr<OutputSurface> output_surface = FakeOutputSurface::Create3d();
  FakeLayerTreeHostImpl host_impl(&task_runner_provider, &shared_bitmap_manager,
                                  &task_graph_runner);
  host_impl.SetVisible(true);
  EXPECT_TRUE(host_impl.InitializeRenderer(output_surface.get()));

  // This test constructs a tree consisting of a single layer.
  scoped_ptr<LayerImpl> layer1 = LayerImpl::Create(host_impl.active_tree(), 1);
  int i = 1;
  LayerListReverseIterator it(layer1.get());
  LayerListReverseIterator end(nullptr);
  for (; it != end; ++it, --i) {
    EXPECT_EQ(i, it->id());
  }
  EXPECT_EQ(0, i);
}

TEST(LayerListReverseIteratorTest, VerifyNullFirstLayer) {
  // Ensures that if an iterator is constructed with a nullptr, that it can be
  // iterated without issue and that it remains equal to any other
  // null-initialized iterator.
  LayerListReverseIterator it(nullptr);
  LayerListReverseIterator end(nullptr);

  EXPECT_EQ(it, end);
  ++it;
  EXPECT_EQ(it, end);
}

}  // namespace
}  // namespace cc
