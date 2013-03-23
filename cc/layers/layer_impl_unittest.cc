// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/layer_impl.h"

#include "cc/test/fake_impl_proxy.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "cc/test/fake_output_surface.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/single_thread_proxy.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebFilterOperation.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebFilterOperations.h"
#include "third_party/skia/include/effects/SkBlurImageFilter.h"

using namespace WebKit;

namespace cc {
namespace {

#define EXECUTE_AND_VERIFY_SUBTREE_CHANGED(code_to_test)                       \
  root->ResetAllChangeTrackingForSubtree();                                    \
  code_to_test;                                                                \
  EXPECT_TRUE(root->LayerPropertyChanged());                                   \
  EXPECT_TRUE(child->LayerPropertyChanged());                                  \
  EXPECT_TRUE(grand_child->LayerPropertyChanged());                            \
  EXPECT_FALSE(root->LayerSurfacePropertyChanged())

#define EXECUTE_AND_VERIFY_SUBTREE_DID_NOT_CHANGE(code_to_test)                \
  root->ResetAllChangeTrackingForSubtree();                                    \
  code_to_test;                                                                \
  EXPECT_FALSE(root->LayerPropertyChanged());                                  \
  EXPECT_FALSE(child->LayerPropertyChanged());                                 \
  EXPECT_FALSE(grand_child->LayerPropertyChanged());                           \
  EXPECT_FALSE(root->LayerSurfacePropertyChanged())

#define EXECUTE_AND_VERIFY_ONLY_LAYER_CHANGED(code_to_test)                    \
  root->ResetAllChangeTrackingForSubtree();                                    \
  code_to_test;                                                                \
  EXPECT_TRUE(root->LayerPropertyChanged());                                   \
  EXPECT_FALSE(child->LayerPropertyChanged());                                 \
  EXPECT_FALSE(grand_child->LayerPropertyChanged());                           \
  EXPECT_FALSE(root->LayerSurfacePropertyChanged())

#define EXECUTE_AND_VERIFY_ONLY_SURFACE_CHANGED(code_to_test)                  \
  root->ResetAllChangeTrackingForSubtree();                                    \
  code_to_test;                                                                \
  EXPECT_FALSE(root->LayerPropertyChanged());                                  \
  EXPECT_FALSE(child->LayerPropertyChanged());                                 \
  EXPECT_FALSE(grand_child->LayerPropertyChanged());                           \
  EXPECT_TRUE(root->LayerSurfacePropertyChanged())

#define VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(code_to_test)                      \
  root->ResetAllChangeTrackingForSubtree();                                    \
  host_impl.ForcePrepareToDraw();                                              \
  EXPECT_FALSE(host_impl.active_tree()->needs_update_draw_properties());       \
  code_to_test;                                                                \
  EXPECT_TRUE(host_impl.active_tree()->needs_update_draw_properties());

#define VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(code_to_test)                   \
  root->ResetAllChangeTrackingForSubtree();                                    \
  host_impl.ForcePrepareToDraw();                                              \
  EXPECT_FALSE(host_impl.active_tree()->needs_update_draw_properties());       \
  code_to_test;                                                                \
  EXPECT_FALSE(host_impl.active_tree()->needs_update_draw_properties());

TEST(LayerImplTest, VerifyLayerChangesAreTrackedProperly) {
  //
  // This test checks that layerPropertyChanged() has the correct behavior.
  //

  // The constructor on this will fake that we are on the correct thread.
  // Create a simple LayerImpl tree:
  FakeImplProxy proxy;
  FakeLayerTreeHostImpl host_impl(&proxy);
  EXPECT_TRUE(host_impl.InitializeRenderer(CreateFakeOutputSurface()));
  scoped_ptr<LayerImpl> root = LayerImpl::Create(host_impl.active_tree(), 1);
  root->AddChild(LayerImpl::Create(host_impl.active_tree(), 2));
  LayerImpl* child = root->children()[0];
  child->AddChild(LayerImpl::Create(host_impl.active_tree(), 3));
  LayerImpl* grand_child = child->children()[0];

  // Adding children is an internal operation and should not mark layers as
  // changed.
  EXPECT_FALSE(root->LayerPropertyChanged());
  EXPECT_FALSE(child->LayerPropertyChanged());
  EXPECT_FALSE(grand_child->LayerPropertyChanged());

  gfx::PointF arbitrary_point_f = gfx::PointF(0.125f, 0.25f);
  float arbitrary_number = 0.352f;
  gfx::Size arbitrary_size = gfx::Size(111, 222);
  gfx::Point arbitrary_point = gfx::Point(333, 444);
  gfx::Vector2d arbitrary_vector2d = gfx::Vector2d(111, 222);
  gfx::Rect arbitrary_rect = gfx::Rect(arbitrary_point, arbitrary_size);
  gfx::RectF arbitrary_rect_f =
      gfx::RectF(arbitrary_point_f, gfx::SizeF(1.234f, 5.678f));
  SkColor arbitrary_color = SkColorSetRGB(10, 20, 30);
  gfx::Transform arbitrary_transform;
  arbitrary_transform.Scale3d(0.1, 0.2, 0.3);
  WebFilterOperations arbitrary_filters;
  arbitrary_filters.append(WebFilterOperation::createOpacityFilter(0.5f));
  skia::RefPtr<SkImageFilter> arbitrary_filter =
      skia::AdoptRef(new SkBlurImageFilter(SK_Scalar1, SK_Scalar1));

  // These properties are internal, and should not be considered "change" when
  // they are used.
  EXECUTE_AND_VERIFY_SUBTREE_DID_NOT_CHANGE(
      root->set_update_rect(arbitrary_rect_f));
  EXECUTE_AND_VERIFY_SUBTREE_DID_NOT_CHANGE(
      root->SetMaxScrollOffset(arbitrary_vector2d));

  // Changing these properties affects the entire subtree of layers.
  EXECUTE_AND_VERIFY_SUBTREE_CHANGED(root->SetAnchorPoint(arbitrary_point_f));
  EXECUTE_AND_VERIFY_SUBTREE_CHANGED(root->SetAnchorPointZ(arbitrary_number));
  EXECUTE_AND_VERIFY_SUBTREE_CHANGED(root->SetFilters(arbitrary_filters));
  EXECUTE_AND_VERIFY_SUBTREE_CHANGED(root->SetFilters(WebFilterOperations()));
  EXECUTE_AND_VERIFY_SUBTREE_CHANGED(root->SetFilter(arbitrary_filter));
  EXECUTE_AND_VERIFY_SUBTREE_CHANGED(
      root->SetMaskLayer(LayerImpl::Create(host_impl.active_tree(), 4)));
  EXECUTE_AND_VERIFY_SUBTREE_CHANGED(root->SetMasksToBounds(true));
  EXECUTE_AND_VERIFY_SUBTREE_CHANGED(root->SetContentsOpaque(true));
  EXECUTE_AND_VERIFY_SUBTREE_CHANGED(
      root->SetReplicaLayer(LayerImpl::Create(host_impl.active_tree(), 5)));
  EXECUTE_AND_VERIFY_SUBTREE_CHANGED(root->SetPosition(arbitrary_point_f));
  EXECUTE_AND_VERIFY_SUBTREE_CHANGED(root->SetPreserves3d(true));
  EXECUTE_AND_VERIFY_SUBTREE_CHANGED(
      root->SetDoubleSided(false));  // constructor initializes it to "true".
  EXECUTE_AND_VERIFY_SUBTREE_CHANGED(root->ScrollBy(arbitrary_vector2d));
  EXECUTE_AND_VERIFY_SUBTREE_CHANGED(root->SetScrollDelta(gfx::Vector2d()));
  EXECUTE_AND_VERIFY_SUBTREE_CHANGED(root->SetScrollOffset(arbitrary_vector2d));
  EXECUTE_AND_VERIFY_SUBTREE_CHANGED(
      root->SetImplTransform(arbitrary_transform));

  // Changing these properties only affects the layer itself.
  EXECUTE_AND_VERIFY_ONLY_LAYER_CHANGED(root->SetContentBounds(arbitrary_size));
  EXECUTE_AND_VERIFY_ONLY_LAYER_CHANGED(
      root->SetContentsScale(arbitrary_number, arbitrary_number));
  EXECUTE_AND_VERIFY_ONLY_LAYER_CHANGED(root->SetDrawsContent(true));
  EXECUTE_AND_VERIFY_ONLY_LAYER_CHANGED(root->SetBackgroundColor(SK_ColorGRAY));
  EXECUTE_AND_VERIFY_ONLY_LAYER_CHANGED(
      root->SetBackgroundFilters(arbitrary_filters));

  // Changing these properties only affects how render surface is drawn
  EXECUTE_AND_VERIFY_ONLY_SURFACE_CHANGED(root->SetOpacity(arbitrary_number));
  EXECUTE_AND_VERIFY_ONLY_SURFACE_CHANGED(
      root->SetTransform(arbitrary_transform));

  // Special case: check that sublayer transform changes all layer's
  // descendants, but not the layer itself.
  root->ResetAllChangeTrackingForSubtree();
  root->SetSublayerTransform(arbitrary_transform);
  EXPECT_FALSE(root->LayerPropertyChanged());
  EXPECT_TRUE(child->LayerPropertyChanged());
  EXPECT_TRUE(grand_child->LayerPropertyChanged());

  // Special case: check that setBounds changes behavior depending on
  // masksToBounds.
  root->SetMasksToBounds(false);
  EXECUTE_AND_VERIFY_ONLY_LAYER_CHANGED(root->SetBounds(gfx::Size(135, 246)));
  root->SetMasksToBounds(true);
  // Should be a different size than previous call, to ensure it marks tree
  // changed.
  EXECUTE_AND_VERIFY_SUBTREE_CHANGED(root->SetBounds(arbitrary_size));

  // After setting all these properties already, setting to the exact same
  // values again should not cause any change.
  EXECUTE_AND_VERIFY_SUBTREE_DID_NOT_CHANGE(
      root->SetAnchorPoint(arbitrary_point_f));
  EXECUTE_AND_VERIFY_SUBTREE_DID_NOT_CHANGE(
      root->SetAnchorPointZ(arbitrary_number));
  EXECUTE_AND_VERIFY_SUBTREE_DID_NOT_CHANGE(root->SetMasksToBounds(true));
  EXECUTE_AND_VERIFY_SUBTREE_DID_NOT_CHANGE(
      root->SetPosition(arbitrary_point_f));
  EXECUTE_AND_VERIFY_SUBTREE_DID_NOT_CHANGE(root->SetPreserves3d(true));
  EXECUTE_AND_VERIFY_SUBTREE_DID_NOT_CHANGE(
      root->SetTransform(arbitrary_transform));
  EXECUTE_AND_VERIFY_SUBTREE_DID_NOT_CHANGE(
      root->SetDoubleSided(false));  // constructor initializes it to "true".
  EXECUTE_AND_VERIFY_SUBTREE_DID_NOT_CHANGE(
      root->SetScrollDelta(gfx::Vector2d()));
  EXECUTE_AND_VERIFY_SUBTREE_DID_NOT_CHANGE(
      root->SetScrollOffset(arbitrary_vector2d));
  EXECUTE_AND_VERIFY_SUBTREE_DID_NOT_CHANGE(
      root->SetImplTransform(arbitrary_transform));
  EXECUTE_AND_VERIFY_SUBTREE_DID_NOT_CHANGE(
      root->SetContentBounds(arbitrary_size));
  EXECUTE_AND_VERIFY_SUBTREE_DID_NOT_CHANGE(
      root->SetContentsScale(arbitrary_number, arbitrary_number));
  EXECUTE_AND_VERIFY_SUBTREE_DID_NOT_CHANGE(root->SetContentsOpaque(true));
  EXECUTE_AND_VERIFY_SUBTREE_DID_NOT_CHANGE(root->SetOpacity(arbitrary_number));
  EXECUTE_AND_VERIFY_SUBTREE_DID_NOT_CHANGE(root->SetDrawsContent(true));
  EXECUTE_AND_VERIFY_SUBTREE_DID_NOT_CHANGE(
      root->SetSublayerTransform(arbitrary_transform));
  EXECUTE_AND_VERIFY_SUBTREE_DID_NOT_CHANGE(root->SetBounds(arbitrary_size));
}

TEST(LayerImplTest, VerifyNeedsUpdateDrawProperties) {
  FakeImplProxy proxy;
  FakeLayerTreeHostImpl host_impl(&proxy);
  EXPECT_TRUE(host_impl.InitializeRenderer(CreateFakeOutputSurface()));
  scoped_ptr<LayerImpl> root = LayerImpl::Create(host_impl.active_tree(), 1);

  gfx::PointF arbitrary_point_f = gfx::PointF(0.125f, 0.25f);
  float arbitrary_number = 0.352f;
  gfx::Size arbitrary_size = gfx::Size(111, 222);
  gfx::Point arbitrary_point = gfx::Point(333, 444);
  gfx::Vector2d arbitrary_vector2d = gfx::Vector2d(111, 222);
  gfx::Vector2d large_vector2d = gfx::Vector2d(1000, 1000);
  gfx::Rect arbitrary_rect = gfx::Rect(arbitrary_point, arbitrary_size);
  gfx::RectF arbitrary_rect_f =
      gfx::RectF(arbitrary_point_f, gfx::SizeF(1.234f, 5.678f));
  SkColor arbitrary_color = SkColorSetRGB(10, 20, 30);
  gfx::Transform arbitrary_transform;
  arbitrary_transform.Scale3d(0.1, 0.2, 0.3);
  WebFilterOperations arbitrary_filters;
  arbitrary_filters.append(WebFilterOperation::createOpacityFilter(0.5f));
  skia::RefPtr<SkImageFilter> arbitrary_filter =
      skia::AdoptRef(new SkBlurImageFilter(SK_Scalar1, SK_Scalar1));

  // Related filter functions.
  VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(root->SetFilters(arbitrary_filters));
  VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(root->SetFilters(arbitrary_filters));
  VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(root->SetFilters(WebFilterOperations()));
  VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(root->SetFilter(arbitrary_filter));
  VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(root->SetFilter(arbitrary_filter));

  // Related scrolling functions.
  VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(root->SetMaxScrollOffset(large_vector2d));
  VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(
      root->SetMaxScrollOffset(large_vector2d));
  VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(root->ScrollBy(arbitrary_vector2d));
  VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(root->ScrollBy(gfx::Vector2d()));
  root->SetScrollDelta(gfx::Vector2d(0, 0));
  host_impl.ForcePrepareToDraw();
  VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(root->SetScrollDelta(arbitrary_vector2d));
  VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(
      root->SetScrollDelta(arbitrary_vector2d));
  VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(
      root->SetScrollOffset(arbitrary_vector2d));
  VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(
      root->SetScrollOffset(arbitrary_vector2d));

  // Unrelated functions, always set to new values, always set needs update.
  VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(root->SetAnchorPointZ(arbitrary_number));
  VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(
      root->SetMaskLayer(LayerImpl::Create(host_impl.active_tree(), 4)));
  VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(root->SetMasksToBounds(true));
  VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(root->SetContentsOpaque(true));
  VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(
      root->SetReplicaLayer(LayerImpl::Create(host_impl.active_tree(), 5)));
  VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(root->SetPosition(arbitrary_point_f));
  VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(root->SetPreserves3d(true));
  VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(
      root->SetDoubleSided(false));  // constructor initializes it to "true".
  VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(
      root->SetImplTransform(arbitrary_transform));
  VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(root->SetContentBounds(arbitrary_size));
  VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(
      root->SetContentsScale(arbitrary_number, arbitrary_number));
  VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(root->SetDrawsContent(true));
  VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(root->SetBackgroundColor(SK_ColorGRAY));
  VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(
      root->SetBackgroundFilters(arbitrary_filters));
  VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(root->SetOpacity(arbitrary_number));
  VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(root->SetTransform(arbitrary_transform));
  VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(
      root->SetSublayerTransform(arbitrary_transform));
  VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(root->SetBounds(arbitrary_size));

  // Unrelated functions, set to the same values, no needs update.
  VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(
      root->SetAnchorPointZ(arbitrary_number));
  VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(root->SetFilter(arbitrary_filter));
  VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(root->SetMasksToBounds(true));
  VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(root->SetContentsOpaque(true));
  VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(root->SetPosition(arbitrary_point_f));
  VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(root->SetPreserves3d(true));
  VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(
      root->SetDoubleSided(false));  // constructor initializes it to "true".
  VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(
      root->SetImplTransform(arbitrary_transform));
  VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(
      root->SetContentBounds(arbitrary_size));
  VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(
      root->SetContentsScale(arbitrary_number, arbitrary_number));
  VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(root->SetDrawsContent(true));
  VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(
      root->SetBackgroundColor(SK_ColorGRAY));
  VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(
      root->SetBackgroundFilters(arbitrary_filters));
  VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(root->SetOpacity(arbitrary_number));
  VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(
      root->SetTransform(arbitrary_transform));
  VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(
      root->SetSublayerTransform(arbitrary_transform));
  VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(root->SetBounds(arbitrary_size));
}

}  // namespace
}  // namespace cc
