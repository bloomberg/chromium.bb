// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/blink/web_compositor_mutable_state_impl.h"

#include "cc/animation/layer_tree_mutation.h"
#include "cc/blink/web_compositor_mutable_state_provider_impl.h"
#include "cc/test/fake_impl_task_runner_provider.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "cc/test/fake_output_surface.h"
#include "cc/test/layer_tree_host_common_test.h"
#include "cc/test/test_shared_bitmap_manager.h"
#include "cc/test/test_task_graph_runner.h"
#include "cc/trees/layer_tree_host_impl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc_blink {
namespace {

using cc::FakeImplTaskRunnerProvider;
using cc::FakeLayerTreeHostImpl;
using cc::FakeOutputSurface;
using cc::LayerImpl;
using cc::LayerImplList;
using cc::LayerTreeHostCommonTest;
using cc::LayerTreeMutation;
using cc::LayerTreeMutationMap;
using cc::LayerTreeSettings;
using cc::OutputSurface;
using cc::TestTaskGraphRunner;
using cc::TestSharedBitmapManager;

using blink::WebCompositorMutableState;

class WebCompositorMutableStateTest : public LayerTreeHostCommonTest {
 public:
  WebCompositorMutableStateTest()
      : output_surface_(FakeOutputSurface::Create3d()) {
    LayerTreeSettings settings;
    settings.layer_transforms_should_scale_layer_contents = true;
    settings.verify_property_trees = true;
    host_impl_.reset(new FakeLayerTreeHostImpl(settings, &task_runner_provider_,
                                               &shared_bitmap_manager_,
                                               &task_graph_runner_));
    host_impl_->SetVisible(true);
    EXPECT_TRUE(host_impl_->InitializeRenderer(output_surface_.get()));
  }

  FakeLayerTreeHostImpl& host_impl() { return *host_impl_; }

  LayerImpl* root_layer() { return host_impl_->active_tree()->root_layer(); }

 private:
  TestSharedBitmapManager shared_bitmap_manager_;
  TestTaskGraphRunner task_graph_runner_;
  FakeImplTaskRunnerProvider task_runner_provider_;
  scoped_ptr<OutputSurface> output_surface_;
  scoped_ptr<FakeLayerTreeHostImpl> host_impl_;
};

TEST_F(WebCompositorMutableStateTest, NoMutableState) {
  // In this test, there are no layers with either an element id or mutable
  // properties. We should not be able to get any mutable state.
  scoped_ptr<LayerImpl> root = LayerImpl::Create(host_impl().active_tree(), 42);

  gfx::Transform identity_matrix;
  gfx::Point3F transform_origin;
  gfx::PointF position;
  gfx::Size bounds(100, 100);
  SetLayerPropertiesForTesting(root.get(), identity_matrix, transform_origin,
                               position, bounds, true, false, true);
  root->SetDrawsContent(true);

  host_impl().SetViewportSize(root->bounds());
  host_impl().active_tree()->SetRootLayer(std::move(root));
  host_impl().UpdateNumChildrenAndDrawPropertiesForActiveTree();

  LayerTreeMutationMap mutations;
  WebCompositorMutableStateProviderImpl provider(host_impl().active_tree(),
                                                 &mutations);
  scoped_ptr<WebCompositorMutableState> state(provider.getMutableStateFor(42));
  EXPECT_FALSE(state);
}

TEST_F(WebCompositorMutableStateTest, MutableStateNoMutableProperties) {
  // In this test, there is a layer with an element id, but no mutable
  // properties. This should behave just as if we'd had no element id.
  scoped_ptr<LayerImpl> root = LayerImpl::Create(host_impl().active_tree(), 42);

  gfx::Transform identity_matrix;
  gfx::Point3F transform_origin;
  gfx::PointF position;
  gfx::Size bounds(100, 100);
  SetLayerPropertiesForTesting(root.get(), identity_matrix, transform_origin,
                               position, bounds, true, false, true);
  root->SetDrawsContent(true);
  root->SetElementId(42);

  host_impl().SetViewportSize(root->bounds());
  host_impl().active_tree()->SetRootLayer(std::move(root));
  host_impl().UpdateNumChildrenAndDrawPropertiesForActiveTree();

  LayerTreeMutationMap mutations;
  WebCompositorMutableStateProviderImpl provider(host_impl().active_tree(),
                                                 &mutations);
  scoped_ptr<WebCompositorMutableState> state(provider.getMutableStateFor(42));
  EXPECT_FALSE(state);
}

TEST_F(WebCompositorMutableStateTest, MutableStateMutableProperties) {
  // In this test, there is a layer with an element id and mutable properties.
  // In this case, we should get a valid mutable state for this element id that
  // has a real effect on the corresponding layer.
  scoped_ptr<LayerImpl> root = LayerImpl::Create(host_impl().active_tree(), 42);

  gfx::Transform identity_matrix;
  gfx::Point3F transform_origin;
  gfx::PointF position;
  gfx::Size bounds(100, 100);
  SetLayerPropertiesForTesting(root.get(), identity_matrix, transform_origin,
                               position, bounds, true, false, true);
  root->SetDrawsContent(true);
  root->SetElementId(42);
  root->SetMutableProperties(
      cc::kMutablePropertyOpacity | cc::kMutablePropertyTransform |
      cc::kMutablePropertyScrollLeft | cc::kMutablePropertyScrollTop);

  host_impl().SetViewportSize(root->bounds());
  host_impl().active_tree()->SetRootLayer(std::move(root));
  host_impl().UpdateNumChildrenAndDrawPropertiesForActiveTree();

  LayerTreeMutationMap mutations;
  WebCompositorMutableStateProviderImpl provider(host_impl().active_tree(),
                                                 &mutations);

  scoped_ptr<WebCompositorMutableState> state(provider.getMutableStateFor(42));
  EXPECT_TRUE(state.get());

  EXPECT_EQ(1.0, root_layer()->opacity());
  EXPECT_EQ(identity_matrix.ToString(), root_layer()->transform().ToString());
  EXPECT_EQ(0.0, root_layer()->CurrentScrollOffset().x());
  EXPECT_EQ(0.0, root_layer()->CurrentScrollOffset().y());

  gfx::Transform zero(0, 0, 0, 0, 0, 0);
  state->setOpacity(0.5);
  state->setTransform(zero.matrix());
  state->setScrollLeft(1.0);
  state->setScrollTop(1.0);

  EXPECT_EQ(0.5, root_layer()->opacity());
  EXPECT_EQ(zero.ToString(), root_layer()->transform().ToString());
  EXPECT_EQ(1.0, root_layer()->CurrentScrollOffset().x());
  EXPECT_EQ(1.0, root_layer()->CurrentScrollOffset().y());

  // The corresponding mutation should reflect the changed values.
  EXPECT_EQ(1ul, mutations.size());

  const LayerTreeMutation& mutation = mutations[42];
  EXPECT_TRUE(mutation.is_opacity_mutated());
  EXPECT_TRUE(mutation.is_transform_mutated());
  EXPECT_TRUE(mutation.is_scroll_left_mutated());
  EXPECT_TRUE(mutation.is_scroll_top_mutated());

  EXPECT_EQ(0.5, mutation.opacity());
  EXPECT_EQ(zero.ToString(), gfx::Transform(mutation.transform()).ToString());
  EXPECT_EQ(1.0, mutation.scroll_left());
  EXPECT_EQ(1.0, mutation.scroll_top());
}

}  // namespace
}  // namespace cc_blink
