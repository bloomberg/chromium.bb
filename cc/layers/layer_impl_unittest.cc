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

#define EXECUTE_AND_VERIFY_SUBTREE_CHANGED(codeToTest)                  \
    root->ResetAllChangeTrackingForSubtree();                           \
    codeToTest;                                                         \
    EXPECT_TRUE(root->LayerPropertyChanged());                          \
    EXPECT_TRUE(child->LayerPropertyChanged());                         \
    EXPECT_TRUE(grandChild->LayerPropertyChanged());                    \
    EXPECT_FALSE(root->LayerSurfacePropertyChanged())


#define EXECUTE_AND_VERIFY_SUBTREE_DID_NOT_CHANGE(codeToTest)           \
    root->ResetAllChangeTrackingForSubtree();                           \
    codeToTest;                                                         \
    EXPECT_FALSE(root->LayerPropertyChanged());                         \
    EXPECT_FALSE(child->LayerPropertyChanged());                        \
    EXPECT_FALSE(grandChild->LayerPropertyChanged());                   \
    EXPECT_FALSE(root->LayerSurfacePropertyChanged())

#define EXECUTE_AND_VERIFY_ONLY_LAYER_CHANGED(codeToTest)               \
    root->ResetAllChangeTrackingForSubtree();                           \
    codeToTest;                                                         \
    EXPECT_TRUE(root->LayerPropertyChanged());                          \
    EXPECT_FALSE(child->LayerPropertyChanged());                        \
    EXPECT_FALSE(grandChild->LayerPropertyChanged());                   \
    EXPECT_FALSE(root->LayerSurfacePropertyChanged())

#define EXECUTE_AND_VERIFY_ONLY_SURFACE_CHANGED(codeToTest)             \
    root->ResetAllChangeTrackingForSubtree();                           \
    codeToTest;                                                         \
    EXPECT_FALSE(root->LayerPropertyChanged());                         \
    EXPECT_FALSE(child->LayerPropertyChanged());                        \
    EXPECT_FALSE(grandChild->LayerPropertyChanged());                   \
    EXPECT_TRUE(root->LayerSurfacePropertyChanged())

#define VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(codeToTest)                   \
    root->ResetAllChangeTrackingForSubtree();                             \
    hostImpl.ForcePrepareToDraw();                                        \
    EXPECT_FALSE(hostImpl.active_tree()->needs_update_draw_properties()); \
    codeToTest;                                                           \
    EXPECT_TRUE(hostImpl.active_tree()->needs_update_draw_properties());

#define VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(codeToTest)                \
    root->ResetAllChangeTrackingForSubtree();                             \
    hostImpl.ForcePrepareToDraw();                                        \
    EXPECT_FALSE(hostImpl.active_tree()->needs_update_draw_properties()); \
    codeToTest;                                                           \
    EXPECT_FALSE(hostImpl.active_tree()->needs_update_draw_properties());

TEST(LayerImplTest, verifyLayerChangesAreTrackedProperly)
{
    //
    // This test checks that layerPropertyChanged() has the correct behavior.
    //

    // The constructor on this will fake that we are on the correct thread.
    // Create a simple LayerImpl tree:
    FakeImplProxy proxy;
    FakeLayerTreeHostImpl hostImpl(&proxy);
    EXPECT_TRUE(hostImpl.InitializeRenderer(CreateFakeOutputSurface()));
    scoped_ptr<LayerImpl> root = LayerImpl::Create(hostImpl.active_tree(), 1);
    root->AddChild(LayerImpl::Create(hostImpl.active_tree(), 2));
    LayerImpl* child = root->children()[0];
    child->AddChild(LayerImpl::Create(hostImpl.active_tree(), 3));
    LayerImpl* grandChild = child->children()[0];

    // Adding children is an internal operation and should not mark layers as changed.
    EXPECT_FALSE(root->LayerPropertyChanged());
    EXPECT_FALSE(child->LayerPropertyChanged());
    EXPECT_FALSE(grandChild->LayerPropertyChanged());

    gfx::PointF arbitraryPointF = gfx::PointF(0.125f, 0.25f);
    float arbitraryNumber = 0.352f;
    gfx::Size arbitrarySize = gfx::Size(111, 222);
    gfx::Point arbitraryPoint = gfx::Point(333, 444);
    gfx::Vector2d arbitraryVector2d = gfx::Vector2d(111, 222);
    gfx::Rect arbitraryRect = gfx::Rect(arbitraryPoint, arbitrarySize);
    gfx::RectF arbitraryRectF = gfx::RectF(arbitraryPointF, gfx::SizeF(1.234f, 5.678f));
    SkColor arbitraryColor = SkColorSetRGB(10, 20, 30);
    gfx::Transform arbitraryTransform;
    arbitraryTransform.Scale3d(0.1, 0.2, 0.3);
    WebFilterOperations arbitraryFilters;
    arbitraryFilters.append(WebFilterOperation::createOpacityFilter(0.5));
    skia::RefPtr<SkImageFilter> arbitraryFilter = skia::AdoptRef(new SkBlurImageFilter(SK_Scalar1, SK_Scalar1));

    // These properties are internal, and should not be considered "change" when they are used.
    EXECUTE_AND_VERIFY_SUBTREE_DID_NOT_CHANGE(root->set_update_rect(arbitraryRectF));
    EXECUTE_AND_VERIFY_SUBTREE_DID_NOT_CHANGE(root->SetMaxScrollOffset(arbitraryVector2d));

    // Changing these properties affects the entire subtree of layers.
    EXECUTE_AND_VERIFY_SUBTREE_CHANGED(root->SetAnchorPoint(arbitraryPointF));
    EXECUTE_AND_VERIFY_SUBTREE_CHANGED(root->SetAnchorPointZ(arbitraryNumber));
    EXECUTE_AND_VERIFY_SUBTREE_CHANGED(root->SetFilters(arbitraryFilters));
    EXECUTE_AND_VERIFY_SUBTREE_CHANGED(root->SetFilters(WebFilterOperations()));
    EXECUTE_AND_VERIFY_SUBTREE_CHANGED(root->SetFilter(arbitraryFilter));
    EXECUTE_AND_VERIFY_SUBTREE_CHANGED(root->SetMaskLayer(LayerImpl::Create(hostImpl.active_tree(), 4)));
    EXECUTE_AND_VERIFY_SUBTREE_CHANGED(root->SetMasksToBounds(true));
    EXECUTE_AND_VERIFY_SUBTREE_CHANGED(root->SetContentsOpaque(true));
    EXECUTE_AND_VERIFY_SUBTREE_CHANGED(root->SetReplicaLayer(LayerImpl::Create(hostImpl.active_tree(), 5)));
    EXECUTE_AND_VERIFY_SUBTREE_CHANGED(root->SetPosition(arbitraryPointF));
    EXECUTE_AND_VERIFY_SUBTREE_CHANGED(root->SetPreserves3d(true));
    EXECUTE_AND_VERIFY_SUBTREE_CHANGED(root->SetDoubleSided(false)); // constructor initializes it to "true".
    EXECUTE_AND_VERIFY_SUBTREE_CHANGED(root->ScrollBy(arbitraryVector2d));
    EXECUTE_AND_VERIFY_SUBTREE_CHANGED(root->SetScrollDelta(gfx::Vector2d()));
    EXECUTE_AND_VERIFY_SUBTREE_CHANGED(root->SetScrollOffset(arbitraryVector2d));
    EXECUTE_AND_VERIFY_SUBTREE_CHANGED(root->SetImplTransform(arbitraryTransform));

    // Changing these properties only affects the layer itself.
    EXECUTE_AND_VERIFY_ONLY_LAYER_CHANGED(root->SetContentBounds(arbitrarySize));
    EXECUTE_AND_VERIFY_ONLY_LAYER_CHANGED(root->SetContentsScale(arbitraryNumber, arbitraryNumber));
    EXECUTE_AND_VERIFY_ONLY_LAYER_CHANGED(root->SetDrawsContent(true));
    EXECUTE_AND_VERIFY_ONLY_LAYER_CHANGED(root->SetBackgroundColor(SK_ColorGRAY));
    EXECUTE_AND_VERIFY_ONLY_LAYER_CHANGED(root->SetBackgroundFilters(arbitraryFilters));

    // Changing these properties only affects how render surface is drawn
    EXECUTE_AND_VERIFY_ONLY_SURFACE_CHANGED(root->SetOpacity(arbitraryNumber));
    EXECUTE_AND_VERIFY_ONLY_SURFACE_CHANGED(root->SetTransform(arbitraryTransform));

    // Special case: check that sublayer transform changes all layer's descendants, but not the layer itself.
    root->ResetAllChangeTrackingForSubtree();
    root->SetSublayerTransform(arbitraryTransform);
    EXPECT_FALSE(root->LayerPropertyChanged());
    EXPECT_TRUE(child->LayerPropertyChanged());
    EXPECT_TRUE(grandChild->LayerPropertyChanged());

    // Special case: check that setBounds changes behavior depending on masksToBounds.
    root->SetMasksToBounds(false);
    EXECUTE_AND_VERIFY_ONLY_LAYER_CHANGED(root->SetBounds(gfx::Size(135, 246)));
    root->SetMasksToBounds(true);
    EXECUTE_AND_VERIFY_SUBTREE_CHANGED(root->SetBounds(arbitrarySize)); // should be a different size than previous call, to ensure it marks tree changed.

    // After setting all these properties already, setting to the exact same values again should
    // not cause any change.
    EXECUTE_AND_VERIFY_SUBTREE_DID_NOT_CHANGE(root->SetAnchorPoint(arbitraryPointF));
    EXECUTE_AND_VERIFY_SUBTREE_DID_NOT_CHANGE(root->SetAnchorPointZ(arbitraryNumber));
    EXECUTE_AND_VERIFY_SUBTREE_DID_NOT_CHANGE(root->SetMasksToBounds(true));
    EXECUTE_AND_VERIFY_SUBTREE_DID_NOT_CHANGE(root->SetPosition(arbitraryPointF));
    EXECUTE_AND_VERIFY_SUBTREE_DID_NOT_CHANGE(root->SetPreserves3d(true));
    EXECUTE_AND_VERIFY_SUBTREE_DID_NOT_CHANGE(root->SetTransform(arbitraryTransform));
    EXECUTE_AND_VERIFY_SUBTREE_DID_NOT_CHANGE(root->SetDoubleSided(false)); // constructor initializes it to "true".
    EXECUTE_AND_VERIFY_SUBTREE_DID_NOT_CHANGE(root->SetScrollDelta(gfx::Vector2d()));
    EXECUTE_AND_VERIFY_SUBTREE_DID_NOT_CHANGE(root->SetScrollOffset(arbitraryVector2d));
    EXECUTE_AND_VERIFY_SUBTREE_DID_NOT_CHANGE(root->SetImplTransform(arbitraryTransform));
    EXECUTE_AND_VERIFY_SUBTREE_DID_NOT_CHANGE(root->SetContentBounds(arbitrarySize));
    EXECUTE_AND_VERIFY_SUBTREE_DID_NOT_CHANGE(root->SetContentsScale(arbitraryNumber, arbitraryNumber));
    EXECUTE_AND_VERIFY_SUBTREE_DID_NOT_CHANGE(root->SetContentsOpaque(true));
    EXECUTE_AND_VERIFY_SUBTREE_DID_NOT_CHANGE(root->SetOpacity(arbitraryNumber));
    EXECUTE_AND_VERIFY_SUBTREE_DID_NOT_CHANGE(root->SetDrawsContent(true));
    EXECUTE_AND_VERIFY_SUBTREE_DID_NOT_CHANGE(root->SetSublayerTransform(arbitraryTransform));
    EXECUTE_AND_VERIFY_SUBTREE_DID_NOT_CHANGE(root->SetBounds(arbitrarySize));
}

TEST(LayerImplTest, VerifyNeedsUpdateDrawProperties)
{
    FakeImplProxy proxy;
    FakeLayerTreeHostImpl hostImpl(&proxy);
    EXPECT_TRUE(hostImpl.InitializeRenderer(CreateFakeOutputSurface()));
    scoped_ptr<LayerImpl> root = LayerImpl::Create(hostImpl.active_tree(), 1);

    gfx::PointF arbitraryPointF = gfx::PointF(0.125f, 0.25f);
    float arbitraryNumber = 0.352f;
    gfx::Size arbitrarySize = gfx::Size(111, 222);
    gfx::Point arbitraryPoint = gfx::Point(333, 444);
    gfx::Vector2d arbitraryVector2d = gfx::Vector2d(111, 222);
    gfx::Vector2d largeVector2d = gfx::Vector2d(1000, 1000);
    gfx::Rect arbitraryRect = gfx::Rect(arbitraryPoint, arbitrarySize);
    gfx::RectF arbitraryRectF = gfx::RectF(arbitraryPointF, gfx::SizeF(1.234f, 5.678f));
    SkColor arbitraryColor = SkColorSetRGB(10, 20, 30);
    gfx::Transform arbitraryTransform;
    arbitraryTransform.Scale3d(0.1, 0.2, 0.3);
    WebFilterOperations arbitraryFilters;
    arbitraryFilters.append(WebFilterOperation::createOpacityFilter(0.5));
    skia::RefPtr<SkImageFilter> arbitraryFilter = skia::AdoptRef(new SkBlurImageFilter(SK_Scalar1, SK_Scalar1));

    // Related filter functions.
    VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(root->SetFilters(arbitraryFilters));
    VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(root->SetFilters(arbitraryFilters));
    VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(root->SetFilters(WebFilterOperations()));
    VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(root->SetFilter(arbitraryFilter));
    VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(root->SetFilter(arbitraryFilter));

    // Related scrolling functions.
    VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(root->SetMaxScrollOffset(largeVector2d));
    VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(root->SetMaxScrollOffset(largeVector2d));
    VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(root->ScrollBy(arbitraryVector2d));
    VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(root->ScrollBy(gfx::Vector2d()));
    root->SetScrollDelta(gfx::Vector2d(0, 0));
    hostImpl.ForcePrepareToDraw();
    VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(root->SetScrollDelta(arbitraryVector2d));
    VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(root->SetScrollDelta(arbitraryVector2d));
    VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(root->SetScrollOffset(arbitraryVector2d));
    VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(root->SetScrollOffset(arbitraryVector2d));

    // Unrelated functions, always set to new values, always set needs update.
    VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(root->SetAnchorPointZ(arbitraryNumber));
    VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(root->SetMaskLayer(LayerImpl::Create(hostImpl.active_tree(), 4)));
    VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(root->SetMasksToBounds(true));
    VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(root->SetContentsOpaque(true));
    VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(root->SetReplicaLayer(LayerImpl::Create(hostImpl.active_tree(), 5)));
    VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(root->SetPosition(arbitraryPointF));
    VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(root->SetPreserves3d(true));
    VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(root->SetDoubleSided(false)); // constructor initializes it to "true".
    VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(root->SetImplTransform(arbitraryTransform));
    VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(root->SetContentBounds(arbitrarySize));
    VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(root->SetContentsScale(arbitraryNumber, arbitraryNumber));
    VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(root->SetDrawsContent(true));
    VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(root->SetBackgroundColor(SK_ColorGRAY));
    VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(root->SetBackgroundFilters(arbitraryFilters));
    VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(root->SetOpacity(arbitraryNumber));
    VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(root->SetTransform(arbitraryTransform));
    VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(root->SetSublayerTransform(arbitraryTransform));
    VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(root->SetBounds(arbitrarySize));

    // Unrelated functions, set to the same values, no needs update.
    VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(root->SetAnchorPointZ(arbitraryNumber));
    VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(root->SetFilter(arbitraryFilter));
    VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(root->SetMasksToBounds(true));
    VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(root->SetContentsOpaque(true));
    VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(root->SetPosition(arbitraryPointF));
    VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(root->SetPreserves3d(true));
    VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(root->SetDoubleSided(false)); // constructor initializes it to "true".
    VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(root->SetImplTransform(arbitraryTransform));
    VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(root->SetContentBounds(arbitrarySize));
    VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(root->SetContentsScale(arbitraryNumber, arbitraryNumber));
    VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(root->SetDrawsContent(true));
    VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(root->SetBackgroundColor(SK_ColorGRAY));
    VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(root->SetBackgroundFilters(arbitraryFilters));
    VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(root->SetOpacity(arbitraryNumber));
    VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(root->SetTransform(arbitraryTransform));
    VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(root->SetSublayerTransform(arbitraryTransform));
    VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(root->SetBounds(arbitrarySize));
}

}  // namespace
}  // namespace cc
