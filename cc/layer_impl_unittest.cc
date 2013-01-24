// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layer_impl.h"

#include "cc/layer_tree_impl.h"
#include "cc/single_thread_proxy.h"
#include "cc/test/fake_impl_proxy.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "cc/test/fake_output_surface.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebFilterOperation.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebFilterOperations.h"
#include "third_party/skia/include/effects/SkBlurImageFilter.h"

using namespace WebKit;

namespace cc {
namespace {

#define EXECUTE_AND_VERIFY_SUBTREE_CHANGED(codeToTest)                  \
    root->resetAllChangeTrackingForSubtree();                           \
    codeToTest;                                                         \
    EXPECT_TRUE(root->layerPropertyChanged());                          \
    EXPECT_TRUE(child->layerPropertyChanged());                         \
    EXPECT_TRUE(grandChild->layerPropertyChanged());                    \
    EXPECT_FALSE(root->layerSurfacePropertyChanged())


#define EXECUTE_AND_VERIFY_SUBTREE_DID_NOT_CHANGE(codeToTest)           \
    root->resetAllChangeTrackingForSubtree();                           \
    codeToTest;                                                         \
    EXPECT_FALSE(root->layerPropertyChanged());                         \
    EXPECT_FALSE(child->layerPropertyChanged());                        \
    EXPECT_FALSE(grandChild->layerPropertyChanged());                   \
    EXPECT_FALSE(root->layerSurfacePropertyChanged())

#define EXECUTE_AND_VERIFY_ONLY_LAYER_CHANGED(codeToTest)               \
    root->resetAllChangeTrackingForSubtree();                           \
    codeToTest;                                                         \
    EXPECT_TRUE(root->layerPropertyChanged());                          \
    EXPECT_FALSE(child->layerPropertyChanged());                        \
    EXPECT_FALSE(grandChild->layerPropertyChanged());                   \
    EXPECT_FALSE(root->layerSurfacePropertyChanged())

#define EXECUTE_AND_VERIFY_ONLY_SURFACE_CHANGED(codeToTest)             \
    root->resetAllChangeTrackingForSubtree();                           \
    codeToTest;                                                         \
    EXPECT_FALSE(root->layerPropertyChanged());                         \
    EXPECT_FALSE(child->layerPropertyChanged());                        \
    EXPECT_FALSE(grandChild->layerPropertyChanged());                   \
    EXPECT_TRUE(root->layerSurfacePropertyChanged())

#define VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(codeToTest)                 \
    root->resetAllChangeTrackingForSubtree();                           \
    hostImpl.forcePrepareToDraw();                                      \
    EXPECT_FALSE(hostImpl.activeTree()->needs_update_draw_properties());\
    codeToTest;                                                         \
    EXPECT_TRUE(hostImpl.activeTree()->needs_update_draw_properties());

#define VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(codeToTest)              \
    root->resetAllChangeTrackingForSubtree();                           \
    hostImpl.forcePrepareToDraw();                                      \
    EXPECT_FALSE(hostImpl.activeTree()->needs_update_draw_properties());\
    codeToTest;                                                         \
    EXPECT_FALSE(hostImpl.activeTree()->needs_update_draw_properties());

TEST(LayerImplTest, verifyLayerChangesAreTrackedProperly)
{
    //
    // This test checks that layerPropertyChanged() has the correct behavior.
    //

    // The constructor on this will fake that we are on the correct thread.
    // Create a simple LayerImpl tree:
    FakeImplProxy proxy;
    FakeLayerTreeHostImpl hostImpl(&proxy);
    EXPECT_TRUE(hostImpl.initializeRenderer(createFakeOutputSurface()));
    scoped_ptr<LayerImpl> root = LayerImpl::create(hostImpl.activeTree(), 1);
    root->addChild(LayerImpl::create(hostImpl.activeTree(), 2));
    LayerImpl* child = root->children()[0];
    child->addChild(LayerImpl::create(hostImpl.activeTree(), 3));
    LayerImpl* grandChild = child->children()[0];

    // Adding children is an internal operation and should not mark layers as changed.
    EXPECT_FALSE(root->layerPropertyChanged());
    EXPECT_FALSE(child->layerPropertyChanged());
    EXPECT_FALSE(grandChild->layerPropertyChanged());

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
    EXECUTE_AND_VERIFY_SUBTREE_DID_NOT_CHANGE(root->setUpdateRect(arbitraryRectF));
    EXECUTE_AND_VERIFY_SUBTREE_DID_NOT_CHANGE(root->setMaxScrollOffset(arbitraryVector2d));

    // Changing these properties affects the entire subtree of layers.
    EXECUTE_AND_VERIFY_SUBTREE_CHANGED(root->setAnchorPoint(arbitraryPointF));
    EXECUTE_AND_VERIFY_SUBTREE_CHANGED(root->setAnchorPointZ(arbitraryNumber));
    EXECUTE_AND_VERIFY_SUBTREE_CHANGED(root->setFilters(arbitraryFilters));
    EXECUTE_AND_VERIFY_SUBTREE_CHANGED(root->setFilters(WebFilterOperations()));
    EXECUTE_AND_VERIFY_SUBTREE_CHANGED(root->setFilter(arbitraryFilter));
    EXECUTE_AND_VERIFY_SUBTREE_CHANGED(root->setMaskLayer(LayerImpl::create(hostImpl.activeTree(), 4)));
    EXECUTE_AND_VERIFY_SUBTREE_CHANGED(root->setMasksToBounds(true));
    EXECUTE_AND_VERIFY_SUBTREE_CHANGED(root->setContentsOpaque(true));
    EXECUTE_AND_VERIFY_SUBTREE_CHANGED(root->setReplicaLayer(LayerImpl::create(hostImpl.activeTree(), 5)));
    EXECUTE_AND_VERIFY_SUBTREE_CHANGED(root->setPosition(arbitraryPointF));
    EXECUTE_AND_VERIFY_SUBTREE_CHANGED(root->setPreserves3D(true));
    EXECUTE_AND_VERIFY_SUBTREE_CHANGED(root->setDoubleSided(false)); // constructor initializes it to "true".
    EXECUTE_AND_VERIFY_SUBTREE_CHANGED(root->scrollBy(arbitraryVector2d));
    EXECUTE_AND_VERIFY_SUBTREE_CHANGED(root->setScrollDelta(gfx::Vector2d()));
    EXECUTE_AND_VERIFY_SUBTREE_CHANGED(root->setScrollOffset(arbitraryVector2d));
    EXECUTE_AND_VERIFY_SUBTREE_CHANGED(root->setImplTransform(arbitraryTransform));

    // Changing these properties only affects the layer itself.
    EXECUTE_AND_VERIFY_ONLY_LAYER_CHANGED(root->setContentBounds(arbitrarySize));
    EXECUTE_AND_VERIFY_ONLY_LAYER_CHANGED(root->setContentsScale(arbitraryNumber, arbitraryNumber));
    EXECUTE_AND_VERIFY_ONLY_LAYER_CHANGED(root->setDrawsContent(true));
    EXECUTE_AND_VERIFY_ONLY_LAYER_CHANGED(root->setBackgroundColor(SK_ColorGRAY));
    EXECUTE_AND_VERIFY_ONLY_LAYER_CHANGED(root->setBackgroundFilters(arbitraryFilters));

    // Changing these properties only affects how render surface is drawn
    EXECUTE_AND_VERIFY_ONLY_SURFACE_CHANGED(root->setOpacity(arbitraryNumber));
    EXECUTE_AND_VERIFY_ONLY_SURFACE_CHANGED(root->setTransform(arbitraryTransform));

    // Special case: check that sublayer transform changes all layer's descendants, but not the layer itself.
    root->resetAllChangeTrackingForSubtree();
    root->setSublayerTransform(arbitraryTransform);
    EXPECT_FALSE(root->layerPropertyChanged());
    EXPECT_TRUE(child->layerPropertyChanged());
    EXPECT_TRUE(grandChild->layerPropertyChanged());

    // Special case: check that setBounds changes behavior depending on masksToBounds.
    root->setMasksToBounds(false);
    EXECUTE_AND_VERIFY_ONLY_LAYER_CHANGED(root->setBounds(gfx::Size(135, 246)));
    root->setMasksToBounds(true);
    EXECUTE_AND_VERIFY_SUBTREE_CHANGED(root->setBounds(arbitrarySize)); // should be a different size than previous call, to ensure it marks tree changed.

    // After setting all these properties already, setting to the exact same values again should
    // not cause any change.
    EXECUTE_AND_VERIFY_SUBTREE_DID_NOT_CHANGE(root->setAnchorPoint(arbitraryPointF));
    EXECUTE_AND_VERIFY_SUBTREE_DID_NOT_CHANGE(root->setAnchorPointZ(arbitraryNumber));
    EXECUTE_AND_VERIFY_SUBTREE_DID_NOT_CHANGE(root->setMasksToBounds(true));
    EXECUTE_AND_VERIFY_SUBTREE_DID_NOT_CHANGE(root->setPosition(arbitraryPointF));
    EXECUTE_AND_VERIFY_SUBTREE_DID_NOT_CHANGE(root->setPreserves3D(true));
    EXECUTE_AND_VERIFY_SUBTREE_DID_NOT_CHANGE(root->setTransform(arbitraryTransform));
    EXECUTE_AND_VERIFY_SUBTREE_DID_NOT_CHANGE(root->setDoubleSided(false)); // constructor initializes it to "true".
    EXECUTE_AND_VERIFY_SUBTREE_DID_NOT_CHANGE(root->setScrollDelta(gfx::Vector2d()));
    EXECUTE_AND_VERIFY_SUBTREE_DID_NOT_CHANGE(root->setScrollOffset(arbitraryVector2d));
    EXECUTE_AND_VERIFY_SUBTREE_DID_NOT_CHANGE(root->setImplTransform(arbitraryTransform));
    EXECUTE_AND_VERIFY_SUBTREE_DID_NOT_CHANGE(root->setContentBounds(arbitrarySize));
    EXECUTE_AND_VERIFY_SUBTREE_DID_NOT_CHANGE(root->setContentsScale(arbitraryNumber, arbitraryNumber));
    EXECUTE_AND_VERIFY_SUBTREE_DID_NOT_CHANGE(root->setContentsOpaque(true));
    EXECUTE_AND_VERIFY_SUBTREE_DID_NOT_CHANGE(root->setOpacity(arbitraryNumber));
    EXECUTE_AND_VERIFY_SUBTREE_DID_NOT_CHANGE(root->setDrawsContent(true));
    EXECUTE_AND_VERIFY_SUBTREE_DID_NOT_CHANGE(root->setSublayerTransform(arbitraryTransform));
    EXECUTE_AND_VERIFY_SUBTREE_DID_NOT_CHANGE(root->setBounds(arbitrarySize));
}

TEST(LayerImplTest, VerifyNeedsUpdateDrawProperties)
{
    FakeImplProxy proxy;
    FakeLayerTreeHostImpl hostImpl(&proxy);
    EXPECT_TRUE(hostImpl.initializeRenderer(createFakeOutputSurface()));
    scoped_ptr<LayerImpl> root = LayerImpl::create(hostImpl.activeTree(), 1);

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
    VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(root->setFilters(arbitraryFilters));
    VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(root->setFilters(arbitraryFilters));
    VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(root->setFilters(WebFilterOperations()));
    VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(root->setFilter(arbitraryFilter));
    VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(root->setFilter(arbitraryFilter));

    // Related scrolling functions.
    VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(root->setMaxScrollOffset(largeVector2d));
    VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(root->setMaxScrollOffset(largeVector2d));
    VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(root->scrollBy(arbitraryVector2d));
    VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(root->scrollBy(gfx::Vector2d()));
    root->setScrollDelta(gfx::Vector2d(0, 0));
    hostImpl.forcePrepareToDraw();
    VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(root->setScrollDelta(arbitraryVector2d));
    VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(root->setScrollDelta(arbitraryVector2d));
    VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(root->setScrollOffset(arbitraryVector2d));
    VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(root->setScrollOffset(arbitraryVector2d));

    // Unrelated functions, always set to new values, always set needs update.
    VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(root->setAnchorPointZ(arbitraryNumber));
    VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(root->setMaskLayer(LayerImpl::create(hostImpl.activeTree(), 4)));
    VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(root->setMasksToBounds(true));
    VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(root->setContentsOpaque(true));
    VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(root->setReplicaLayer(LayerImpl::create(hostImpl.activeTree(), 5)));
    VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(root->setPosition(arbitraryPointF));
    VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(root->setPreserves3D(true));
    VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(root->setDoubleSided(false)); // constructor initializes it to "true".
    VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(root->setImplTransform(arbitraryTransform));
    VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(root->setContentBounds(arbitrarySize));
    VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(root->setContentsScale(arbitraryNumber, arbitraryNumber));
    VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(root->setDrawsContent(true));
    VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(root->setBackgroundColor(SK_ColorGRAY));
    VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(root->setBackgroundFilters(arbitraryFilters));
    VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(root->setOpacity(arbitraryNumber));
    VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(root->setTransform(arbitraryTransform));
    VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(root->setSublayerTransform(arbitraryTransform));
    VERIFY_NEEDS_UPDATE_DRAW_PROPERTIES(root->setBounds(arbitrarySize));

    // Unrelated functions, set to the same values, no needs update.
    VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(root->setAnchorPointZ(arbitraryNumber));
    VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(root->setFilter(arbitraryFilter));
    VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(root->setMasksToBounds(true));
    VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(root->setContentsOpaque(true));
    VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(root->setPosition(arbitraryPointF));
    VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(root->setPreserves3D(true));
    VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(root->setDoubleSided(false)); // constructor initializes it to "true".
    VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(root->setImplTransform(arbitraryTransform));
    VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(root->setContentBounds(arbitrarySize));
    VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(root->setContentsScale(arbitraryNumber, arbitraryNumber));
    VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(root->setDrawsContent(true));
    VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(root->setBackgroundColor(SK_ColorGRAY));
    VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(root->setBackgroundFilters(arbitraryFilters));
    VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(root->setOpacity(arbitraryNumber));
    VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(root->setTransform(arbitraryTransform));
    VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(root->setSublayerTransform(arbitraryTransform));
    VERIFY_NO_NEEDS_UPDATE_DRAW_PROPERTIES(root->setBounds(arbitrarySize));
}

}  // namespace
}  // namespace cc
