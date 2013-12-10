// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/layer_impl.h"

#include "cc/output/filter_operation.h"
#include "cc/output/filter_operations.h"
#include "cc/test/fake_impl_proxy.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "cc/test/fake_output_surface.h"
#include "cc/test/geometry_test_utils.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/single_thread_proxy.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/effects/SkBlurImageFilter.h"

namespace cc {
namespace {

#define EXECUTE_AND_VERIFY_SUBTREE_CHANGED(code_to_test)                       \
  root->ResetAllChangeTrackingForSubtree();                                    \
  code_to_test;                                                                \
  EXPECT_TRUE(root->LayerPropertyChanged());                                   \
  EXPECT_TRUE(child->LayerPropertyChanged());                                  \
  EXPECT_TRUE(grand_child->LayerPropertyChanged());

#define EXECUTE_AND_VERIFY_SUBTREE_DID_NOT_CHANGE(code_to_test)                \
  root->ResetAllChangeTrackingForSubtree();                                    \
  code_to_test;                                                                \
  EXPECT_FALSE(root->LayerPropertyChanged());                                  \
  EXPECT_FALSE(child->LayerPropertyChanged());                                 \
  EXPECT_FALSE(grand_child->LayerPropertyChanged());

#define EXECUTE_AND_VERIFY_ONLY_LAYER_CHANGED(code_to_test)                    \
  root->ResetAllChangeTrackingForSubtree();                                    \
  code_to_test;                                                                \
  EXPECT_TRUE(root->LayerPropertyChanged());                                   \
  EXPECT_FALSE(child->LayerPropertyChanged());                                 \
  EXPECT_FALSE(grand_child->LayerPropertyChanged());

#define EXECUTE_AND_VERIFY_ONLY_DESCENDANTS_CHANGED(code_to_test)              \
  root->ResetAllChangeTrackingForSubtree();                                    \
  code_to_test;                                                                \
  EXPECT_FALSE(root->LayerPropertyChanged());                                  \
  EXPECT_TRUE(child->LayerPropertyChanged());                                  \
  EXPECT_TRUE(grand_child->LayerPropertyChanged());

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

  root->SetScrollable(true);

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
  arbitrary_transform.Scale3d(0.1f, 0.2f, 0.3f);
  FilterOperations arbitrary_filters;
  arbitrary_filters.Append(FilterOperation::CreateOpacityFilter(0.5f));
  SkXfermode::Mode arbitrary_blend_mode = SkXfermode::kMultiply_Mode;

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
  EXECUTE_AND_VERIFY_SUBTREE_CHANGED(root->SetFilters(FilterOperations()));
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
  EXECUTE_AND_VERIFY_SUBTREE_CHANGED(root->SetHideLayerAndSubtree(true));
  EXECUTE_AND_VERIFY_SUBTREE_CHANGED(root->SetOpacity(arbitrary_number));
  EXECUTE_AND_VERIFY_SUBTREE_CHANGED(root->SetBlendMode(arbitrary_blend_mode));
  EXECUTE_AND_VERIFY_SUBTREE_CHANGED(root->SetTransform(arbitrary_transform));

  // Changing these properties only affects the layer itself.
  EXECUTE_AND_VERIFY_ONLY_LAYER_CHANGED(root->SetContentBounds(arbitrary_size));
  EXECUTE_AND_VERIFY_ONLY_LAYER_CHANGED(
      root->SetContentsScale(arbitrary_number, arbitrary_number));
  EXECUTE_AND_VERIFY_ONLY_LAYER_CHANGED(root->SetDrawsContent(true));
  EXECUTE_AND_VERIFY_ONLY_LAYER_CHANGED(
      root->SetBackgroundColor(arbitrary_color));
  EXECUTE_AND_VERIFY_ONLY_LAYER_CHANGED(
      root->SetBackgroundFilters(arbitrary_filters));

  // Changing these properties affects all layer's descendants,
  // but not the layer itself.
  EXECUTE_AND_VERIFY_ONLY_DESCENDANTS_CHANGED(
      root->SetSublayerTransform(arbitrary_transform));

  // Special case: check that SetBounds changes behavior depending on
  // masksToBounds.
  root->SetMasksToBounds(false);
  EXECUTE_AND_VERIFY_ONLY_LAYER_CHANGED(root->SetBounds(gfx::Size(135, 246)));
  root->SetMasksToBounds(true);
  // Should be a different size than previous call, to ensure it marks tree
  // changed.
  EXECUTE_AND_VERIFY_SUBTREE_CHANGED(root->SetBounds(arbitrary_size));

  EXECUTE_AND_VERIFY_SUBTREE_DID_NOT_CHANGE(
      root->SetIsRootForIsolatedGroup(true));

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
      root->SetContentBounds(arbitrary_size));
  EXECUTE_AND_VERIFY_SUBTREE_DID_NOT_CHANGE(
      root->SetContentsScale(arbitrary_number, arbitrary_number));
  EXECUTE_AND_VERIFY_SUBTREE_DID_NOT_CHANGE(root->SetContentsOpaque(true));
  EXECUTE_AND_VERIFY_SUBTREE_DID_NOT_CHANGE(root->SetOpacity(arbitrary_number));
  EXECUTE_AND_VERIFY_SUBTREE_DID_NOT_CHANGE(
      root->SetBlendMode(arbitrary_blend_mode));
  EXECUTE_AND_VERIFY_SUBTREE_DID_NOT_CHANGE(
      root->SetIsRootForIsolatedGroup(true));
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
  root->SetScrollable(true);

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
  arbitrary_transform.Scale3d(0.1f, 0.2f, 0.3f);
  FilterOperations arbitrary_filters;
  arbitrary_filters.Append(FilterOperation::CreateOpacityFilter(0.5f));
  SkXfermode::Mode arbitrary_blend_mode = SkXfermode::kMultiply_Mode;

  // Related filter functions.
  VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(root->SetFilters(arbitrary_filters));
  VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(root->SetFilters(arbitrary_filters));
  VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(root->SetFilters(FilterOperations()));
  VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(root->SetFilters(arbitrary_filters));

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
  VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(root->SetContentBounds(arbitrary_size));
  VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(
      root->SetContentsScale(arbitrary_number, arbitrary_number));
  VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(root->SetDrawsContent(true));
  VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(
      root->SetBackgroundColor(arbitrary_color));
  VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(
      root->SetBackgroundFilters(arbitrary_filters));
  VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(root->SetOpacity(arbitrary_number));
  VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(root->SetBlendMode(arbitrary_blend_mode));
  VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(root->SetTransform(arbitrary_transform));
  VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(
      root->SetSublayerTransform(arbitrary_transform));
  VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(root->SetBounds(arbitrary_size));

  // Unrelated functions, set to the same values, no needs update.
  VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(
      root->SetAnchorPointZ(arbitrary_number));
  VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(root->SetIsRootForIsolatedGroup(true));
  VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(root->SetFilters(arbitrary_filters));
  VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(root->SetMasksToBounds(true));
  VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(root->SetContentsOpaque(true));
  VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(root->SetPosition(arbitrary_point_f));
  VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(root->SetPreserves3d(true));
  VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(
      root->SetDoubleSided(false));  // constructor initializes it to "true".
  VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(
      root->SetContentBounds(arbitrary_size));
  VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(
      root->SetContentsScale(arbitrary_number, arbitrary_number));
  VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(root->SetDrawsContent(true));
  VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(
      root->SetBackgroundColor(arbitrary_color));
  VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(
      root->SetBackgroundFilters(arbitrary_filters));
  VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(root->SetOpacity(arbitrary_number));
  VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(
      root->SetBlendMode(arbitrary_blend_mode));
  VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(root->SetIsRootForIsolatedGroup(true));
  VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(
      root->SetTransform(arbitrary_transform));
  VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(
      root->SetSublayerTransform(arbitrary_transform));
  VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(root->SetBounds(arbitrary_size));
}

TEST(LayerImplTest, SafeOpaqueBackgroundColor) {
  FakeImplProxy proxy;
  FakeLayerTreeHostImpl host_impl(&proxy);
  EXPECT_TRUE(host_impl.InitializeRenderer(CreateFakeOutputSurface()));
  scoped_ptr<LayerImpl> layer = LayerImpl::Create(host_impl.active_tree(), 1);

  for (int contents_opaque = 0; contents_opaque < 2; ++contents_opaque) {
    for (int layer_opaque = 0; layer_opaque < 2; ++layer_opaque) {
      for (int host_opaque = 0; host_opaque < 2; ++host_opaque) {
        layer->SetContentsOpaque(!!contents_opaque);
        layer->SetBackgroundColor(layer_opaque ? SK_ColorRED
                                               : SK_ColorTRANSPARENT);
        host_impl.active_tree()->set_background_color(
            host_opaque ? SK_ColorRED : SK_ColorTRANSPARENT);

        SkColor safe_color = layer->SafeOpaqueBackgroundColor();
        if (contents_opaque) {
          EXPECT_EQ(SkColorGetA(safe_color), 255u)
              << "Flags: " << contents_opaque << ", " << layer_opaque << ", "
              << host_opaque << "\n";
        } else {
          EXPECT_NE(SkColorGetA(safe_color), 255u)
              << "Flags: " << contents_opaque << ", " << layer_opaque << ", "
              << host_opaque << "\n";
        }
      }
    }
  }
}

class LayerImplScrollTest : public testing::Test {
 public:
  LayerImplScrollTest() : host_impl_(&proxy_), root_id_(7) {
    host_impl_.active_tree()
        ->SetRootLayer(LayerImpl::Create(host_impl_.active_tree(), root_id_));
    host_impl_.active_tree()->root_layer()->SetScrollable(true);
  }

  LayerImpl* layer() { return host_impl_.active_tree()->root_layer(); }

 private:
  FakeImplProxy proxy_;
  FakeLayerTreeHostImpl host_impl_;
  int root_id_;
};

TEST_F(LayerImplScrollTest, ScrollByWithZeroOffset) {
  // Test that LayerImpl::ScrollBy only affects ScrollDelta and total scroll
  // offset is bounded by the range [0, max scroll offset].
  gfx::Vector2d max_scroll_offset(50, 80);
  layer()->SetMaxScrollOffset(max_scroll_offset);

  EXPECT_VECTOR_EQ(gfx::Vector2dF(), layer()->TotalScrollOffset());
  EXPECT_VECTOR_EQ(gfx::Vector2dF(), layer()->scroll_offset());
  EXPECT_VECTOR_EQ(gfx::Vector2dF(), layer()->ScrollDelta());

  layer()->ScrollBy(gfx::Vector2dF(-100, 100));
  EXPECT_VECTOR_EQ(gfx::Vector2dF(0, 80), layer()->TotalScrollOffset());

  EXPECT_VECTOR_EQ(layer()->ScrollDelta(), layer()->TotalScrollOffset());
  EXPECT_VECTOR_EQ(gfx::Vector2dF(), layer()->scroll_offset());

  layer()->ScrollBy(gfx::Vector2dF(100, -100));
  EXPECT_VECTOR_EQ(gfx::Vector2dF(50, 0), layer()->TotalScrollOffset());

  EXPECT_VECTOR_EQ(layer()->ScrollDelta(), layer()->TotalScrollOffset());
  EXPECT_VECTOR_EQ(gfx::Vector2dF(), layer()->scroll_offset());
}

TEST_F(LayerImplScrollTest, ScrollByWithNonZeroOffset) {
  gfx::Vector2d max_scroll_offset(50, 80);
  gfx::Vector2d scroll_offset(10, 5);
  layer()->SetMaxScrollOffset(max_scroll_offset);
  layer()->SetScrollOffset(scroll_offset);

  EXPECT_VECTOR_EQ(scroll_offset, layer()->TotalScrollOffset());
  EXPECT_VECTOR_EQ(scroll_offset, layer()->scroll_offset());
  EXPECT_VECTOR_EQ(gfx::Vector2dF(), layer()->ScrollDelta());

  layer()->ScrollBy(gfx::Vector2dF(-100, 100));
  EXPECT_VECTOR_EQ(gfx::Vector2dF(0, 80), layer()->TotalScrollOffset());

  EXPECT_VECTOR_EQ(layer()->ScrollDelta() + scroll_offset,
                   layer()->TotalScrollOffset());
  EXPECT_VECTOR_EQ(scroll_offset, layer()->scroll_offset());

  layer()->ScrollBy(gfx::Vector2dF(100, -100));
  EXPECT_VECTOR_EQ(gfx::Vector2dF(50, 0), layer()->TotalScrollOffset());

  EXPECT_VECTOR_EQ(layer()->ScrollDelta() + scroll_offset,
                   layer()->TotalScrollOffset());
  EXPECT_VECTOR_EQ(scroll_offset, layer()->scroll_offset());
}

class ScrollDelegateIgnore : public LayerScrollOffsetDelegate {
 public:
  virtual void SetMaxScrollOffset(gfx::Vector2dF max_scroll_offset) OVERRIDE {}
  virtual void SetTotalScrollOffset(gfx::Vector2dF new_value) OVERRIDE {}
  virtual gfx::Vector2dF GetTotalScrollOffset() OVERRIDE {
    return fixed_offset_;
  }
  virtual bool IsExternalFlingActive() const OVERRIDE { return false; }

  void set_fixed_offset(gfx::Vector2dF fixed_offset) {
    fixed_offset_ = fixed_offset;
  }

  virtual void SetTotalPageScaleFactor(float page_scale_factor) OVERRIDE {}
  virtual void SetScrollableSize(gfx::SizeF scrollable_size) OVERRIDE {}

 private:
  gfx::Vector2dF fixed_offset_;
};

TEST_F(LayerImplScrollTest, ScrollByWithIgnoringDelegate) {
  gfx::Vector2d max_scroll_offset(50, 80);
  gfx::Vector2d scroll_offset(10, 5);
  layer()->SetMaxScrollOffset(max_scroll_offset);
  layer()->SetScrollOffset(scroll_offset);

  EXPECT_VECTOR_EQ(scroll_offset, layer()->TotalScrollOffset());
  EXPECT_VECTOR_EQ(scroll_offset, layer()->scroll_offset());
  EXPECT_VECTOR_EQ(gfx::Vector2dF(), layer()->ScrollDelta());

  ScrollDelegateIgnore delegate;
  gfx::Vector2dF fixed_offset(32, 12);
  delegate.set_fixed_offset(fixed_offset);
  layer()->SetScrollOffsetDelegate(&delegate);

  EXPECT_VECTOR_EQ(fixed_offset, layer()->TotalScrollOffset());
  EXPECT_VECTOR_EQ(scroll_offset, layer()->scroll_offset());

  layer()->ScrollBy(gfx::Vector2dF(-100, 100));

  EXPECT_VECTOR_EQ(fixed_offset, layer()->TotalScrollOffset());
  EXPECT_VECTOR_EQ(scroll_offset, layer()->scroll_offset());

  layer()->SetScrollOffsetDelegate(NULL);

  EXPECT_VECTOR_EQ(fixed_offset, layer()->TotalScrollOffset());
  EXPECT_VECTOR_EQ(scroll_offset, layer()->scroll_offset());

  gfx::Vector2dF scroll_delta(1, 1);
  layer()->ScrollBy(scroll_delta);

  EXPECT_VECTOR_EQ(fixed_offset + scroll_delta, layer()->TotalScrollOffset());
  EXPECT_VECTOR_EQ(scroll_offset, layer()->scroll_offset());
}

class ScrollDelegateAccept : public LayerScrollOffsetDelegate {
 public:
  virtual void SetMaxScrollOffset(gfx::Vector2dF max_scroll_offset) OVERRIDE {}
  virtual void SetTotalScrollOffset(gfx::Vector2dF new_value) OVERRIDE {
    current_offset_ = new_value;
  }
  virtual gfx::Vector2dF GetTotalScrollOffset() OVERRIDE {
    return current_offset_;
  }
  virtual bool IsExternalFlingActive() const OVERRIDE { return false; }
  virtual void SetTotalPageScaleFactor(float page_scale_factor) OVERRIDE {}
  virtual void SetScrollableSize(gfx::SizeF scrollable_size) OVERRIDE {}

 private:
  gfx::Vector2dF current_offset_;
};

TEST_F(LayerImplScrollTest, ScrollByWithAcceptingDelegate) {
  gfx::Vector2d max_scroll_offset(50, 80);
  gfx::Vector2d scroll_offset(10, 5);
  layer()->SetMaxScrollOffset(max_scroll_offset);
  layer()->SetScrollOffset(scroll_offset);

  EXPECT_VECTOR_EQ(scroll_offset, layer()->TotalScrollOffset());
  EXPECT_VECTOR_EQ(scroll_offset, layer()->scroll_offset());
  EXPECT_VECTOR_EQ(gfx::Vector2dF(), layer()->ScrollDelta());

  ScrollDelegateAccept delegate;
  layer()->SetScrollOffsetDelegate(&delegate);

  EXPECT_VECTOR_EQ(scroll_offset, layer()->TotalScrollOffset());
  EXPECT_VECTOR_EQ(scroll_offset, layer()->scroll_offset());
  EXPECT_VECTOR_EQ(gfx::Vector2dF(), layer()->ScrollDelta());

  layer()->ScrollBy(gfx::Vector2dF(-100, 100));

  EXPECT_VECTOR_EQ(gfx::Vector2dF(0, 80), layer()->TotalScrollOffset());
  EXPECT_VECTOR_EQ(scroll_offset, layer()->scroll_offset());

  layer()->SetScrollOffsetDelegate(NULL);

  EXPECT_VECTOR_EQ(gfx::Vector2dF(0, 80), layer()->TotalScrollOffset());
  EXPECT_VECTOR_EQ(scroll_offset, layer()->scroll_offset());

  gfx::Vector2dF scroll_delta(1, 1);
  layer()->ScrollBy(scroll_delta);

  EXPECT_VECTOR_EQ(gfx::Vector2dF(1, 80), layer()->TotalScrollOffset());
  EXPECT_VECTOR_EQ(scroll_offset, layer()->scroll_offset());
}

TEST_F(LayerImplScrollTest, ApplySentScrollsNoDelegate) {
  gfx::Vector2d max_scroll_offset(50, 80);
  gfx::Vector2d scroll_offset(10, 5);
  gfx::Vector2dF scroll_delta(20.5f, 8.5f);
  gfx::Vector2d sent_scroll_delta(12, -3);

  layer()->SetMaxScrollOffset(max_scroll_offset);
  layer()->SetScrollOffset(scroll_offset);
  layer()->ScrollBy(scroll_delta);
  layer()->SetSentScrollDelta(sent_scroll_delta);

  EXPECT_VECTOR_EQ(scroll_offset + scroll_delta, layer()->TotalScrollOffset());
  EXPECT_VECTOR_EQ(scroll_delta, layer()->ScrollDelta());
  EXPECT_VECTOR_EQ(scroll_offset, layer()->scroll_offset());
  EXPECT_VECTOR_EQ(sent_scroll_delta, layer()->sent_scroll_delta());

  layer()->ApplySentScrollDeltasFromAbortedCommit();

  EXPECT_VECTOR_EQ(scroll_offset + scroll_delta, layer()->TotalScrollOffset());
  EXPECT_VECTOR_EQ(scroll_delta - sent_scroll_delta, layer()->ScrollDelta());
  EXPECT_VECTOR_EQ(scroll_offset + sent_scroll_delta, layer()->scroll_offset());
  EXPECT_VECTOR_EQ(gfx::Vector2d(), layer()->sent_scroll_delta());
}

TEST_F(LayerImplScrollTest, ApplySentScrollsWithIgnoringDelegate) {
  gfx::Vector2d max_scroll_offset(50, 80);
  gfx::Vector2d scroll_offset(10, 5);
  gfx::Vector2d sent_scroll_delta(12, -3);
  gfx::Vector2dF fixed_offset(32, 12);

  layer()->SetMaxScrollOffset(max_scroll_offset);
  layer()->SetScrollOffset(scroll_offset);
  ScrollDelegateIgnore delegate;
  delegate.set_fixed_offset(fixed_offset);
  layer()->SetScrollOffsetDelegate(&delegate);
  layer()->SetSentScrollDelta(sent_scroll_delta);

  EXPECT_VECTOR_EQ(fixed_offset, layer()->TotalScrollOffset());
  EXPECT_VECTOR_EQ(scroll_offset, layer()->scroll_offset());
  EXPECT_VECTOR_EQ(sent_scroll_delta, layer()->sent_scroll_delta());

  layer()->ApplySentScrollDeltasFromAbortedCommit();

  EXPECT_VECTOR_EQ(fixed_offset, layer()->TotalScrollOffset());
  EXPECT_VECTOR_EQ(scroll_offset + sent_scroll_delta, layer()->scroll_offset());
  EXPECT_VECTOR_EQ(gfx::Vector2d(), layer()->sent_scroll_delta());
}

TEST_F(LayerImplScrollTest, ApplySentScrollsWithAcceptingDelegate) {
  gfx::Vector2d max_scroll_offset(50, 80);
  gfx::Vector2d scroll_offset(10, 5);
  gfx::Vector2d sent_scroll_delta(12, -3);
  gfx::Vector2dF scroll_delta(20.5f, 8.5f);

  layer()->SetMaxScrollOffset(max_scroll_offset);
  layer()->SetScrollOffset(scroll_offset);
  ScrollDelegateAccept delegate;
  layer()->SetScrollOffsetDelegate(&delegate);
  layer()->ScrollBy(scroll_delta);
  layer()->SetSentScrollDelta(sent_scroll_delta);

  EXPECT_VECTOR_EQ(scroll_offset + scroll_delta, layer()->TotalScrollOffset());
  EXPECT_VECTOR_EQ(scroll_offset, layer()->scroll_offset());
  EXPECT_VECTOR_EQ(sent_scroll_delta, layer()->sent_scroll_delta());

  layer()->ApplySentScrollDeltasFromAbortedCommit();

  EXPECT_VECTOR_EQ(scroll_offset + scroll_delta, layer()->TotalScrollOffset());
  EXPECT_VECTOR_EQ(scroll_offset + sent_scroll_delta, layer()->scroll_offset());
  EXPECT_VECTOR_EQ(gfx::Vector2d(), layer()->sent_scroll_delta());
}

// The user-scrollability breaks for zoomed-in pages. So disable this.
// http://crbug.com/322223
TEST_F(LayerImplScrollTest, DISABLED_ScrollUserUnscrollableLayer) {
  gfx::Vector2d max_scroll_offset(50, 80);
  gfx::Vector2d scroll_offset(10, 5);
  gfx::Vector2dF scroll_delta(20.5f, 8.5f);

  layer()->set_user_scrollable_vertical(false);
  layer()->SetMaxScrollOffset(max_scroll_offset);
  layer()->SetScrollOffset(scroll_offset);
  gfx::Vector2dF unscrolled = layer()->ScrollBy(scroll_delta);

  EXPECT_VECTOR_EQ(gfx::Vector2dF(0, 8.5f), unscrolled);
  EXPECT_VECTOR_EQ(gfx::Vector2dF(30.5f, 5), layer()->TotalScrollOffset());
}

}  // namespace
}  // namespace cc
