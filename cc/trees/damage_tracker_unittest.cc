// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/damage_tracker.h"

#include "cc/base/math_util.h"
#include "cc/layers/layer_impl.h"
#include "cc/test/fake_impl_proxy.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "cc/test/geometry_test_utils.h"
#include "cc/trees/layer_tree_host_common.h"
#include "cc/trees/single_thread_proxy.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebFilterOperation.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebFilterOperations.h"
#include "third_party/skia/include/effects/SkBlurImageFilter.h"
#include "ui/gfx/quad_f.h"

using namespace WebKit;

namespace cc {
namespace {

void executeCalculateDrawProperties(LayerImpl* root, std::vector<LayerImpl*>& renderSurfaceLayerList)
{
    int dummyMaxTextureSize = 512;

    // Sanity check: The test itself should create the root layer's render surface, so
    //               that the surface (and its damage tracker) can persist across multiple
    //               calls to this function.
    ASSERT_TRUE(root->render_surface());
    ASSERT_FALSE(renderSurfaceLayerList.size());

    LayerTreeHostCommon::CalculateDrawProperties(root, root->bounds(), 1, 1, dummyMaxTextureSize, false, &renderSurfaceLayerList, false);
}

void clearDamageForAllSurfaces(LayerImpl* layer)
{
    if (layer->render_surface())
        layer->render_surface()->damage_tracker()->DidDrawDamagedArea();

    // Recursively clear damage for any existing surface.
    for (size_t i = 0; i < layer->children().size(); ++i)
        clearDamageForAllSurfaces(layer->children()[i]);
}

void emulateDrawingOneFrame(LayerImpl* root)
{
    // This emulates only the steps that are relevant to testing the damage tracker:
    //   1. computing the render passes and layerlists
    //   2. updating all damage trackers in the correct order
    //   3. resetting all updateRects and propertyChanged flags for all layers and surfaces.

    std::vector<LayerImpl*> renderSurfaceLayerList;
    executeCalculateDrawProperties(root, renderSurfaceLayerList);

    // Iterate back-to-front, so that damage correctly propagates from descendant surfaces to ancestors.
    for (int i = renderSurfaceLayerList.size() - 1; i >= 0; --i) {
        RenderSurfaceImpl* targetSurface = renderSurfaceLayerList[i]->render_surface();
        targetSurface->damage_tracker()->UpdateDamageTrackingState(targetSurface->layer_list(), targetSurface->OwningLayerId(), targetSurface->SurfacePropertyChangedOnlyFromDescendant(), targetSurface->content_rect(), renderSurfaceLayerList[i]->mask_layer(), renderSurfaceLayerList[i]->filters(), renderSurfaceLayerList[i]->filter().get());
    }

    root->ResetAllChangeTrackingForSubtree();
}

class DamageTrackerTest : public testing::Test {
public:
    DamageTrackerTest()
        : m_hostImpl(&m_proxy)
    {
    }

    scoped_ptr<LayerImpl> createTestTreeWithOneSurface()
    {
        scoped_ptr<LayerImpl> root = LayerImpl::Create(m_hostImpl.active_tree(), 1);
        scoped_ptr<LayerImpl> child = LayerImpl::Create(m_hostImpl.active_tree(), 2);

        root->SetPosition(gfx::PointF());
        root->SetAnchorPoint(gfx::PointF());
        root->SetBounds(gfx::Size(500, 500));
        root->SetContentBounds(gfx::Size(500, 500));
        root->SetDrawsContent(true);
        root->CreateRenderSurface();
        root->render_surface()->SetContentRect(gfx::Rect(gfx::Point(), gfx::Size(500, 500)));

        child->SetPosition(gfx::PointF(100, 100));
        child->SetAnchorPoint(gfx::PointF());
        child->SetBounds(gfx::Size(30, 30));
        child->SetContentBounds(gfx::Size(30, 30));
        child->SetDrawsContent(true);
        root->AddChild(child.Pass());

        return root.Pass();
    }

    scoped_ptr<LayerImpl> createTestTreeWithTwoSurfaces()
    {
        // This test tree has two render surfaces: one for the root, and one for
        // child1. Additionally, the root has a second child layer, and child1 has two
        // children of its own.

        scoped_ptr<LayerImpl> root = LayerImpl::Create(m_hostImpl.active_tree(), 1);
        scoped_ptr<LayerImpl> child1 = LayerImpl::Create(m_hostImpl.active_tree(), 2);
        scoped_ptr<LayerImpl> child2 = LayerImpl::Create(m_hostImpl.active_tree(), 3);
        scoped_ptr<LayerImpl> grandChild1 = LayerImpl::Create(m_hostImpl.active_tree(), 4);
        scoped_ptr<LayerImpl> grandChild2 = LayerImpl::Create(m_hostImpl.active_tree(), 5);

        root->SetPosition(gfx::PointF());
        root->SetAnchorPoint(gfx::PointF());
        root->SetBounds(gfx::Size(500, 500));
        root->SetContentBounds(gfx::Size(500, 500));
        root->SetDrawsContent(true);
        root->CreateRenderSurface();
        root->render_surface()->SetContentRect(gfx::Rect(gfx::Point(), gfx::Size(500, 500)));

        child1->SetPosition(gfx::PointF(100, 100));
        child1->SetAnchorPoint(gfx::PointF());
        child1->SetBounds(gfx::Size(30, 30));
        child1->SetContentBounds(gfx::Size(30, 30));
        child1->SetOpacity(0.5); // with a child that drawsContent, this will cause the layer to create its own renderSurface.
        child1->SetDrawsContent(false); // this layer does not draw, but is intended to create its own renderSurface.
        child1->SetForceRenderSurface(true);

        child2->SetPosition(gfx::PointF(11, 11));
        child2->SetAnchorPoint(gfx::PointF());
        child2->SetBounds(gfx::Size(18, 18));
        child2->SetContentBounds(gfx::Size(18, 18));
        child2->SetDrawsContent(true);

        grandChild1->SetPosition(gfx::PointF(200, 200));
        grandChild1->SetAnchorPoint(gfx::PointF());
        grandChild1->SetBounds(gfx::Size(6, 8));
        grandChild1->SetContentBounds(gfx::Size(6, 8));
        grandChild1->SetDrawsContent(true);

        grandChild2->SetPosition(gfx::PointF(190, 190));
        grandChild2->SetAnchorPoint(gfx::PointF());
        grandChild2->SetBounds(gfx::Size(6, 8));
        grandChild2->SetContentBounds(gfx::Size(6, 8));
        grandChild2->SetDrawsContent(true);

        child1->AddChild(grandChild1.Pass());
        child1->AddChild(grandChild2.Pass());
        root->AddChild(child1.Pass());
        root->AddChild(child2.Pass());

        return root.Pass();
    }

    scoped_ptr<LayerImpl> createAndSetUpTestTreeWithOneSurface()
    {
        scoped_ptr<LayerImpl> root = createTestTreeWithOneSurface();

        // Setup includes going past the first frame which always damages everything, so
        // that we can actually perform specific tests.
        emulateDrawingOneFrame(root.get());

        return root.Pass();
    }

    scoped_ptr<LayerImpl> createAndSetUpTestTreeWithTwoSurfaces()
    {
        scoped_ptr<LayerImpl> root = createTestTreeWithTwoSurfaces();

        // Setup includes going past the first frame which always damages everything, so
        // that we can actually perform specific tests.
        emulateDrawingOneFrame(root.get());

        return root.Pass();
    }

protected:
    FakeImplProxy m_proxy;
    FakeLayerTreeHostImpl m_hostImpl;
};

TEST_F(DamageTrackerTest, sanityCheckTestTreeWithOneSurface)
{
    // Sanity check that the simple test tree will actually produce the expected render
    // surfaces and layer lists.

    scoped_ptr<LayerImpl> root = createAndSetUpTestTreeWithOneSurface();

    EXPECT_EQ(2u, root->render_surface()->layer_list().size());
    EXPECT_EQ(1, root->render_surface()->layer_list()[0]->id());
    EXPECT_EQ(2, root->render_surface()->layer_list()[1]->id());

    gfx::RectF rootDamageRect = root->render_surface()->damage_tracker()->current_damage_rect();
    EXPECT_FLOAT_RECT_EQ(gfx::RectF(0, 0, 500, 500), rootDamageRect);
}

TEST_F(DamageTrackerTest, sanityCheckTestTreeWithTwoSurfaces)
{
    // Sanity check that the complex test tree will actually produce the expected render
    // surfaces and layer lists.

    scoped_ptr<LayerImpl> root = createAndSetUpTestTreeWithTwoSurfaces();

    LayerImpl* child1 = root->children()[0];
    LayerImpl* child2 = root->children()[1];
    gfx::RectF childDamageRect = child1->render_surface()->damage_tracker()->current_damage_rect();
    gfx::RectF rootDamageRect = root->render_surface()->damage_tracker()->current_damage_rect();

    ASSERT_TRUE(child1->render_surface());
    EXPECT_FALSE(child2->render_surface());
    EXPECT_EQ(3u, root->render_surface()->layer_list().size());
    EXPECT_EQ(2u, child1->render_surface()->layer_list().size());

    // The render surface for child1 only has a contentRect that encloses grandChild1 and grandChild2, because child1 does not draw content.
    EXPECT_FLOAT_RECT_EQ(gfx::RectF(190, 190, 16, 18), childDamageRect);
    EXPECT_FLOAT_RECT_EQ(gfx::RectF(0, 0, 500, 500), rootDamageRect);
}

TEST_F(DamageTrackerTest, verifyDamageForUpdateRects)
{
    scoped_ptr<LayerImpl> root = createAndSetUpTestTreeWithOneSurface();
    LayerImpl* child = root->children()[0];

    // CASE 1: Setting the update rect should cause the corresponding damage to the surface.
    //
    clearDamageForAllSurfaces(root.get());
    child->set_update_rect(gfx::RectF(10, 11, 12, 13));
    emulateDrawingOneFrame(root.get());

    // Damage position on the surface should be: position of updateRect (10, 11) relative to the child (100, 100).
    gfx::RectF rootDamageRect = root->render_surface()->damage_tracker()->current_damage_rect();
    EXPECT_FLOAT_RECT_EQ(gfx::RectF(110, 111, 12, 13), rootDamageRect);

    // CASE 2: The same update rect twice in a row still produces the same damage.
    //
    clearDamageForAllSurfaces(root.get());
    child->set_update_rect(gfx::RectF(10, 11, 12, 13));
    emulateDrawingOneFrame(root.get());
    rootDamageRect = root->render_surface()->damage_tracker()->current_damage_rect();
    EXPECT_FLOAT_RECT_EQ(gfx::RectF(110, 111, 12, 13), rootDamageRect);

    // CASE 3: Setting a different update rect should cause damage on the new update region, but no additional exposed old region.
    //
    clearDamageForAllSurfaces(root.get());
    child->set_update_rect(gfx::RectF(20, 25, 1, 2));
    emulateDrawingOneFrame(root.get());

    // Damage position on the surface should be: position of updateRect (20, 25) relative to the child (100, 100).
    rootDamageRect = root->render_surface()->damage_tracker()->current_damage_rect();
    EXPECT_FLOAT_RECT_EQ(gfx::RectF(120, 125, 1, 2), rootDamageRect);
}

TEST_F(DamageTrackerTest, verifyDamageForPropertyChanges)
{
    scoped_ptr<LayerImpl> root = createAndSetUpTestTreeWithOneSurface();
    LayerImpl* child = root->children()[0];

    // CASE 1: The layer's property changed flag takes priority over update rect.
    //
    clearDamageForAllSurfaces(root.get());
    child->set_update_rect(gfx::RectF(10, 11, 12, 13));
    child->SetOpacity(0.5);
    emulateDrawingOneFrame(root.get());

    // Sanity check - we should not have accidentally created a separate render surface for the translucent layer.
    ASSERT_FALSE(child->render_surface());
    ASSERT_EQ(2u, root->render_surface()->layer_list().size());

    // Damage should be the entire child layer in targetSurface space.
    gfx::RectF expectedRect = gfx::RectF(100, 100, 30, 30);
    gfx::RectF rootDamageRect = root->render_surface()->damage_tracker()->current_damage_rect();
    EXPECT_FLOAT_RECT_EQ(expectedRect, rootDamageRect);

    // CASE 2: If a layer moves due to property change, it damages both the new location
    //         and the old (exposed) location. The old location is the entire old layer,
    //         not just the updateRect.

    // Cycle one frame of no change, just to sanity check that the next rect is not because of the old damage state.
    clearDamageForAllSurfaces(root.get());
    emulateDrawingOneFrame(root.get());
    EXPECT_TRUE(root->render_surface()->damage_tracker()->current_damage_rect().IsEmpty());

    // Then, test the actual layer movement.
    clearDamageForAllSurfaces(root.get());
    child->SetPosition(gfx::PointF(200, 230));
    emulateDrawingOneFrame(root.get());

    // Expect damage to be the combination of the previous one and the new one.
    expectedRect.Union(gfx::RectF(200, 230, 30, 30));
    rootDamageRect = root->render_surface()->damage_tracker()->current_damage_rect();
    EXPECT_FLOAT_RECT_EQ(expectedRect, rootDamageRect);
}

TEST_F(DamageTrackerTest, verifyDamageForTransformedLayer)
{
    // If a layer is transformed, the damage rect should still enclose the entire
    // transformed layer.

    scoped_ptr<LayerImpl> root = createAndSetUpTestTreeWithOneSurface();
    LayerImpl* child = root->children()[0];

    gfx::Transform rotation;
    rotation.Rotate(45);

    clearDamageForAllSurfaces(root.get());
    child->SetAnchorPoint(gfx::PointF(0.5, 0.5));
    child->SetPosition(gfx::PointF(85, 85));
    emulateDrawingOneFrame(root.get());

    // Sanity check that the layer actually moved to (85, 85), damaging its old location and new location.
    gfx::RectF rootDamageRect = root->render_surface()->damage_tracker()->current_damage_rect();
    EXPECT_FLOAT_RECT_EQ(gfx::RectF(85, 85, 45, 45), rootDamageRect);

    // With the anchor on the layer's center, now we can test the rotation more
    // intuitively, since it applies about the layer's anchor.
    clearDamageForAllSurfaces(root.get());
    child->SetTransform(rotation);
    emulateDrawingOneFrame(root.get());

    // Since the child layer is square, rotation by 45 degrees about the center should
    // increase the size of the expected rect by sqrt(2), centered around (100, 100). The
    // old exposed region should be fully contained in the new region.
    double expectedWidth = 30 * sqrt(2.0);
    double expectedPosition = 100 - 0.5 * expectedWidth;
    gfx::RectF expectedRect(expectedPosition, expectedPosition, expectedWidth, expectedWidth);
    rootDamageRect = root->render_surface()->damage_tracker()->current_damage_rect();
    EXPECT_FLOAT_RECT_EQ(expectedRect, rootDamageRect);
}

TEST_F(DamageTrackerTest, verifyDamageForPerspectiveClippedLayer)
{
    // If a layer has a perspective transform that causes w < 0, then not clipping the
    // layer can cause an invalid damage rect. This test checks that the w < 0 case is
    // tracked properly.
    //
    // The transform is constructed so that if w < 0 clipping is not performed, the
    // incorrect rect will be very small, specifically: position (500.972504, 498.544617) and size 0.056610 x 2.910767.
    // Instead, the correctly transformed rect should actually be very huge (i.e. in theory, -infinity on the left),
    // and positioned so that the right-most bound rect will be approximately 501 units in root surface space.
    //

    scoped_ptr<LayerImpl> root = createAndSetUpTestTreeWithOneSurface();
    LayerImpl* child = root->children()[0];

    gfx::Transform transform;
    transform.Translate3d(500, 500, 0);
    transform.ApplyPerspectiveDepth(1);
    transform.RotateAboutYAxis(45);
    transform.Translate3d(-50, -50, 0);

    // Set up the child
    child->SetPosition(gfx::PointF(0, 0));
    child->SetBounds(gfx::Size(100, 100));
    child->SetContentBounds(gfx::Size(100, 100));
    child->SetTransform(transform);
    emulateDrawingOneFrame(root.get());

    // Sanity check that the child layer's bounds would actually get clipped by w < 0,
    // otherwise this test is not actually testing the intended scenario.
    gfx::QuadF testQuad(gfx::RectF(gfx::PointF(), gfx::SizeF(100, 100)));
    bool clipped = false;
    MathUtil::MapQuad(transform, testQuad, &clipped);
    EXPECT_TRUE(clipped);

    // Damage the child without moving it.
    clearDamageForAllSurfaces(root.get());
    child->SetOpacity(0.5);
    emulateDrawingOneFrame(root.get());

    // The expected damage should cover the entire root surface (500x500), but we don't
    // care whether the damage rect was clamped or is larger than the surface for this test.
    gfx::RectF rootDamageRect = root->render_surface()->damage_tracker()->current_damage_rect();
    gfx::RectF damageWeCareAbout = gfx::RectF(gfx::PointF(), gfx::SizeF(500, 500));
    EXPECT_TRUE(rootDamageRect.Contains(damageWeCareAbout));
}

TEST_F(DamageTrackerTest, verifyDamageForBlurredSurface)
{
    scoped_ptr<LayerImpl> root = createAndSetUpTestTreeWithOneSurface();
    LayerImpl* child = root->children()[0];

    WebFilterOperations filters;
    filters.append(WebFilterOperation::createBlurFilter(5));
    int outsetTop, outsetRight, outsetBottom, outsetLeft;
    filters.getOutsets(outsetTop, outsetRight, outsetBottom, outsetLeft);

    // Setting the filter will damage the whole surface.
    clearDamageForAllSurfaces(root.get());
    root->SetFilters(filters);
    emulateDrawingOneFrame(root.get());

    // Setting the update rect should cause the corresponding damage to the surface, blurred based on the size of the blur filter.
    clearDamageForAllSurfaces(root.get());
    child->set_update_rect(gfx::RectF(10, 11, 12, 13));
    emulateDrawingOneFrame(root.get());

    // Damage position on the surface should be: position of updateRect (10, 11) relative to the child (100, 100), but expanded by the blur outsets.
    gfx::RectF rootDamageRect = root->render_surface()->damage_tracker()->current_damage_rect();
    gfx::RectF expectedDamageRect = gfx::RectF(110, 111, 12, 13);
    expectedDamageRect.Inset(-outsetLeft, -outsetTop, -outsetRight, -outsetBottom);
    EXPECT_FLOAT_RECT_EQ(expectedDamageRect, rootDamageRect);
}

TEST_F(DamageTrackerTest, verifyDamageForImageFilter)
{
    scoped_ptr<LayerImpl> root = createAndSetUpTestTreeWithOneSurface();
    LayerImpl* child = root->children()[0];
    gfx::RectF rootDamageRect, childDamageRect;

    // Allow us to set damage on child too.
    child->SetDrawsContent(true);

    skia::RefPtr<SkImageFilter> filter = skia::AdoptRef(new SkBlurImageFilter(SkIntToScalar(2), SkIntToScalar(2)));
    // Setting the filter will damage the whole surface.
    clearDamageForAllSurfaces(root.get());
    child->SetFilter(filter);
    emulateDrawingOneFrame(root.get());
    rootDamageRect = root->render_surface()->damage_tracker()->current_damage_rect();
    childDamageRect = child->render_surface()->damage_tracker()->current_damage_rect();
    EXPECT_FLOAT_RECT_EQ(gfx::RectF(100, 100, 30, 30), rootDamageRect);
    EXPECT_FLOAT_RECT_EQ(gfx::RectF(0, 0, 30, 30), childDamageRect);

    // CASE 1: Setting the update rect should damage the whole surface (for now)
    clearDamageForAllSurfaces(root.get());
    child->set_update_rect(gfx::RectF(0, 0, 1, 1));
    emulateDrawingOneFrame(root.get());

    rootDamageRect = root->render_surface()->damage_tracker()->current_damage_rect();
    childDamageRect = child->render_surface()->damage_tracker()->current_damage_rect();
    EXPECT_FLOAT_RECT_EQ(gfx::RectF(100, 100, 30, 30), rootDamageRect);
    EXPECT_FLOAT_RECT_EQ(gfx::RectF(0, 0, 30, 30), childDamageRect);
}

TEST_F(DamageTrackerTest, verifyDamageForBackgroundBlurredChild)
{
    scoped_ptr<LayerImpl> root = createAndSetUpTestTreeWithTwoSurfaces();
    LayerImpl* child1 = root->children()[0];
    LayerImpl* child2 = root->children()[1];

    // Allow us to set damage on child1 too.
    child1->SetDrawsContent(true);

    WebFilterOperations filters;
    filters.append(WebFilterOperation::createBlurFilter(2));
    int outsetTop, outsetRight, outsetBottom, outsetLeft;
    filters.getOutsets(outsetTop, outsetRight, outsetBottom, outsetLeft);

    // Setting the filter will damage the whole surface.
    clearDamageForAllSurfaces(root.get());
    child1->SetBackgroundFilters(filters);
    emulateDrawingOneFrame(root.get());

    // CASE 1: Setting the update rect should cause the corresponding damage to
    // the surface, blurred based on the size of the child's background blur
    // filter.
    clearDamageForAllSurfaces(root.get());
    root->set_update_rect(gfx::RectF(297, 297, 2, 2));
    emulateDrawingOneFrame(root.get());

    gfx::RectF rootDamageRect = root->render_surface()->damage_tracker()->current_damage_rect();
    // Damage position on the surface should be a composition of the damage on the root and on child2.
    // Damage on the root should be: position of updateRect (297, 297), but expanded by the blur outsets.
    gfx::RectF expectedDamageRect = gfx::RectF(297, 297, 2, 2);
    expectedDamageRect.Inset(-outsetLeft, -outsetTop, -outsetRight, -outsetBottom);
    EXPECT_FLOAT_RECT_EQ(expectedDamageRect, rootDamageRect);

    // CASE 2: Setting the update rect should cause the corresponding damage to
    // the surface, blurred based on the size of the child's background blur
    // filter. Since the damage extends to the right/bottom outside of the
    // blurred layer, only the left/top should end up expanded.
    clearDamageForAllSurfaces(root.get());
    root->set_update_rect(gfx::RectF(297, 297, 30, 30));
    emulateDrawingOneFrame(root.get());

    rootDamageRect = root->render_surface()->damage_tracker()->current_damage_rect();
    // Damage position on the surface should be a composition of the damage on the root and on child2.
    // Damage on the root should be: position of updateRect (297, 297), but expanded on the left/top
    // by the blur outsets.
    expectedDamageRect = gfx::RectF(297, 297, 30, 30);
    expectedDamageRect.Inset(-outsetLeft, -outsetTop, 0, 0);
    EXPECT_FLOAT_RECT_EQ(expectedDamageRect, rootDamageRect);

    // CASE 3: Setting this update rect outside the blurred contentBounds of the blurred
    // child1 will not cause it to be expanded.
    clearDamageForAllSurfaces(root.get());
    root->set_update_rect(gfx::RectF(30, 30, 2, 2));
    emulateDrawingOneFrame(root.get());

    rootDamageRect = root->render_surface()->damage_tracker()->current_damage_rect();
    // Damage on the root should be: position of updateRect (30, 30), not
    // expanded.
    expectedDamageRect = gfx::RectF(30, 30, 2, 2);
    EXPECT_FLOAT_RECT_EQ(expectedDamageRect, rootDamageRect);

    // CASE 4: Setting this update rect inside the blurred contentBounds but outside the
    // original contentBounds of the blurred child1 will cause it to be expanded.
    clearDamageForAllSurfaces(root.get());
    root->set_update_rect(gfx::RectF(99, 99, 1, 1));
    emulateDrawingOneFrame(root.get());

    rootDamageRect = root->render_surface()->damage_tracker()->current_damage_rect();
    // Damage on the root should be: position of updateRect (99, 99), expanded
    // by the blurring on child1, but since it is 1 pixel outside the layer, the
    // expanding should be reduced by 1.
    expectedDamageRect = gfx::RectF(99, 99, 1, 1);
    expectedDamageRect.Inset(-outsetLeft + 1, -outsetTop + 1, -outsetRight, -outsetBottom);
    EXPECT_FLOAT_RECT_EQ(expectedDamageRect, rootDamageRect);

    // CASE 5: Setting the update rect on child2, which is above child1, will
    // not get blurred by child1, so it does not need to get expanded.
    clearDamageForAllSurfaces(root.get());
    child2->set_update_rect(gfx::RectF(0, 0, 1, 1));
    emulateDrawingOneFrame(root.get());

    rootDamageRect = root->render_surface()->damage_tracker()->current_damage_rect();
    // Damage on child2 should be: position of updateRect offset by the child's position (11, 11), and not expanded by anything.
    expectedDamageRect = gfx::RectF(11, 11, 1, 1);
    EXPECT_FLOAT_RECT_EQ(expectedDamageRect, rootDamageRect);

    // CASE 6: Setting the update rect on child1 will also blur the damage, so
    // that any pixels needed for the blur are redrawn in the current frame.
    clearDamageForAllSurfaces(root.get());
    child1->set_update_rect(gfx::RectF(0, 0, 1, 1));
    emulateDrawingOneFrame(root.get());

    rootDamageRect = root->render_surface()->damage_tracker()->current_damage_rect();
    // Damage on child1 should be: position of updateRect offset by the child's position (100, 100), and expanded by the damage.
    expectedDamageRect = gfx::RectF(100, 100, 1, 1);
    expectedDamageRect.Inset(-outsetLeft, -outsetTop, -outsetRight, -outsetBottom);
    EXPECT_FLOAT_RECT_EQ(expectedDamageRect, rootDamageRect);
}

TEST_F(DamageTrackerTest, verifyDamageForAddingAndRemovingLayer)
{
    scoped_ptr<LayerImpl> root = createAndSetUpTestTreeWithOneSurface();
    LayerImpl* child1 = root->children()[0];

    // CASE 1: Adding a new layer should cause the appropriate damage.
    //
    clearDamageForAllSurfaces(root.get());
    {
        scoped_ptr<LayerImpl> child2 = LayerImpl::Create(m_hostImpl.active_tree(), 3);
        child2->SetPosition(gfx::PointF(400, 380));
        child2->SetAnchorPoint(gfx::PointF());
        child2->SetBounds(gfx::Size(6, 8));
        child2->SetContentBounds(gfx::Size(6, 8));
        child2->SetDrawsContent(true);
        root->AddChild(child2.Pass());
    }
    emulateDrawingOneFrame(root.get());

    // Sanity check - all 3 layers should be on the same render surface; render surfaces are tested elsewhere.
    ASSERT_EQ(3u, root->render_surface()->layer_list().size());

    gfx::RectF rootDamageRect = root->render_surface()->damage_tracker()->current_damage_rect();
    EXPECT_FLOAT_RECT_EQ(gfx::RectF(400, 380, 6, 8), rootDamageRect);

    // CASE 2: If the layer is removed, its entire old layer becomes exposed, not just the
    //         last update rect.

    // Advance one frame without damage so that we know the damage rect is not leftover from the previous case.
    clearDamageForAllSurfaces(root.get());
    emulateDrawingOneFrame(root.get());
    EXPECT_TRUE(root->render_surface()->damage_tracker()->current_damage_rect().IsEmpty());

    // Then, test removing child1.
    root->RemoveChild(child1);
    child1 = NULL;
    emulateDrawingOneFrame(root.get());
    rootDamageRect = root->render_surface()->damage_tracker()->current_damage_rect();
    EXPECT_FLOAT_RECT_EQ(gfx::RectF(100, 100, 30, 30), rootDamageRect);
}

TEST_F(DamageTrackerTest, verifyDamageForNewUnchangedLayer)
{
    // If child2 is added to the layer tree, but it doesn't have any explicit damage of
    // its own, it should still indeed damage the target surface.

    scoped_ptr<LayerImpl> root = createAndSetUpTestTreeWithOneSurface();

    clearDamageForAllSurfaces(root.get());
    {
        scoped_ptr<LayerImpl> child2 = LayerImpl::Create(m_hostImpl.active_tree(), 3);
        child2->SetPosition(gfx::PointF(400, 380));
        child2->SetAnchorPoint(gfx::PointF());
        child2->SetBounds(gfx::Size(6, 8));
        child2->SetContentBounds(gfx::Size(6, 8));
        child2->SetDrawsContent(true);
        child2->ResetAllChangeTrackingForSubtree();
        // Sanity check the initial conditions of the test, if these asserts trigger, it
        // means the test no longer actually covers the intended scenario.
        ASSERT_FALSE(child2->LayerPropertyChanged());
        ASSERT_TRUE(child2->update_rect().IsEmpty());
        root->AddChild(child2.Pass());
    }
    emulateDrawingOneFrame(root.get());

    // Sanity check - all 3 layers should be on the same render surface; render surfaces are tested elsewhere.
    ASSERT_EQ(3u, root->render_surface()->layer_list().size());

    gfx::RectF rootDamageRect = root->render_surface()->damage_tracker()->current_damage_rect();
    EXPECT_FLOAT_RECT_EQ(gfx::RectF(400, 380, 6, 8), rootDamageRect);
}

TEST_F(DamageTrackerTest, verifyDamageForMultipleLayers)
{
    scoped_ptr<LayerImpl> root = createAndSetUpTestTreeWithOneSurface();
    LayerImpl* child1 = root->children()[0];

    // In this test we don't want the above tree manipulation to be considered part of the same frame.
    clearDamageForAllSurfaces(root.get());
    {
        scoped_ptr<LayerImpl> child2 = LayerImpl::Create(m_hostImpl.active_tree(), 3);
        child2->SetPosition(gfx::PointF(400, 380));
        child2->SetAnchorPoint(gfx::PointF());
        child2->SetBounds(gfx::Size(6, 8));
        child2->SetContentBounds(gfx::Size(6, 8));
        child2->SetDrawsContent(true);
        root->AddChild(child2.Pass());
    }
    LayerImpl* child2 = root->children()[1];
    emulateDrawingOneFrame(root.get());

    // Damaging two layers simultaneously should cause combined damage.
    // - child1 update rect in surface space: gfx::RectF(100, 100, 1, 2);
    // - child2 update rect in surface space: gfx::RectF(400, 380, 3, 4);
    clearDamageForAllSurfaces(root.get());
    child1->set_update_rect(gfx::RectF(0, 0, 1, 2));
    child2->set_update_rect(gfx::RectF(0, 0, 3, 4));
    emulateDrawingOneFrame(root.get());
    gfx::RectF rootDamageRect = root->render_surface()->damage_tracker()->current_damage_rect();
    EXPECT_FLOAT_RECT_EQ(gfx::RectF(100, 100, 303, 284), rootDamageRect);
}

TEST_F(DamageTrackerTest, verifyDamageForNestedSurfaces)
{
    scoped_ptr<LayerImpl> root = createAndSetUpTestTreeWithTwoSurfaces();
    LayerImpl* child1 = root->children()[0];
    LayerImpl* child2 = root->children()[1];
    LayerImpl* grandChild1 = root->children()[0]->children()[0];
    gfx::RectF childDamageRect;
    gfx::RectF rootDamageRect;

    // CASE 1: Damage to a descendant surface should propagate properly to ancestor surface.
    //
    clearDamageForAllSurfaces(root.get());
    grandChild1->SetOpacity(0.5);
    emulateDrawingOneFrame(root.get());
    childDamageRect = child1->render_surface()->damage_tracker()->current_damage_rect();
    rootDamageRect = root->render_surface()->damage_tracker()->current_damage_rect();
    EXPECT_FLOAT_RECT_EQ(gfx::RectF(200, 200, 6, 8), childDamageRect);
    EXPECT_FLOAT_RECT_EQ(gfx::RectF(300, 300, 6, 8), rootDamageRect);

    // CASE 2: Same as previous case, but with additional damage elsewhere that should be properly unioned.
    // - child1 surface damage in root surface space: gfx::RectF(300, 300, 6, 8);
    // - child2 damage in root surface space: gfx::RectF(11, 11, 18, 18);
    clearDamageForAllSurfaces(root.get());
    grandChild1->SetOpacity(0.7f);
    child2->SetOpacity(0.7f);
    emulateDrawingOneFrame(root.get());
    childDamageRect = child1->render_surface()->damage_tracker()->current_damage_rect();
    rootDamageRect = root->render_surface()->damage_tracker()->current_damage_rect();
    EXPECT_FLOAT_RECT_EQ(gfx::RectF(200, 200, 6, 8), childDamageRect);
    EXPECT_FLOAT_RECT_EQ(gfx::RectF(11, 11, 295, 297), rootDamageRect);
}

TEST_F(DamageTrackerTest, verifyDamageForSurfaceChangeFromDescendantLayer)
{
    // If descendant layer changes and affects the content bounds of the render surface,
    // then the entire descendant surface should be damaged, and it should damage its
    // ancestor surface with the old and new surface regions.

    // This is a tricky case, since only the first grandChild changes, but the entire
    // surface should be marked dirty.

    scoped_ptr<LayerImpl> root = createAndSetUpTestTreeWithTwoSurfaces();
    LayerImpl* child1 = root->children()[0];
    LayerImpl* grandChild1 = root->children()[0]->children()[0];
    gfx::RectF childDamageRect;
    gfx::RectF rootDamageRect;

    clearDamageForAllSurfaces(root.get());
    grandChild1->SetPosition(gfx::PointF(195, 205));
    emulateDrawingOneFrame(root.get());
    childDamageRect = child1->render_surface()->damage_tracker()->current_damage_rect();
    rootDamageRect = root->render_surface()->damage_tracker()->current_damage_rect();

    // The new surface bounds should be damaged entirely, even though only one of the layers changed.
    EXPECT_FLOAT_RECT_EQ(gfx::RectF(190, 190, 11, 23), childDamageRect);

    // Damage to the root surface should be the union of child1's *entire* render surface
    // (in target space), and its old exposed area (also in target space).
    EXPECT_FLOAT_RECT_EQ(gfx::RectF(290, 290, 16, 23), rootDamageRect);
}

TEST_F(DamageTrackerTest, verifyDamageForSurfaceChangeFromAncestorLayer)
{
    // An ancestor/owning layer changes that affects the position/transform of the render
    // surface. Note that in this case, the layerPropertyChanged flag already propagates
    // to the subtree (tested in LayerImpltest), which damages the entire child1
    // surface, but the damage tracker still needs the correct logic to compute the
    // exposed region on the root surface.

    // FIXME: the expectations of this test case should change when we add support for a
    //        unique scissorRect per renderSurface. In that case, the child1 surface
    //        should be completely unchanged, since we are only transforming it, while the
    //        root surface would be damaged appropriately.

    scoped_ptr<LayerImpl> root = createAndSetUpTestTreeWithTwoSurfaces();
    LayerImpl* child1 = root->children()[0];
    gfx::RectF childDamageRect;
    gfx::RectF rootDamageRect;

    clearDamageForAllSurfaces(root.get());
    child1->SetPosition(gfx::PointF(50, 50));
    emulateDrawingOneFrame(root.get());
    childDamageRect = child1->render_surface()->damage_tracker()->current_damage_rect();
    rootDamageRect = root->render_surface()->damage_tracker()->current_damage_rect();

    // The new surface bounds should be damaged entirely.
    EXPECT_FLOAT_RECT_EQ(gfx::RectF(190, 190, 16, 18), childDamageRect);

    // The entire child1 surface and the old exposed child1 surface should damage the root surface.
    //  - old child1 surface in target space: gfx::RectF(290, 290, 16, 18)
    //  - new child1 surface in target space: gfx::RectF(240, 240, 16, 18)
    EXPECT_FLOAT_RECT_EQ(gfx::RectF(240, 240, 66, 68), rootDamageRect);
}

TEST_F(DamageTrackerTest, verifyDamageForAddingAndRemovingRenderSurfaces)
{
    scoped_ptr<LayerImpl> root = createAndSetUpTestTreeWithTwoSurfaces();
    LayerImpl* child1 = root->children()[0];
    gfx::RectF childDamageRect;
    gfx::RectF rootDamageRect;

    // CASE 1: If a descendant surface disappears, its entire old area becomes exposed.
    //
    clearDamageForAllSurfaces(root.get());
    child1->SetOpacity(1);
    child1->SetForceRenderSurface(false);
    emulateDrawingOneFrame(root.get());

    // Sanity check that there is only one surface now.
    ASSERT_FALSE(child1->render_surface());
    ASSERT_EQ(4u, root->render_surface()->layer_list().size());

    rootDamageRect = root->render_surface()->damage_tracker()->current_damage_rect();
    EXPECT_FLOAT_RECT_EQ(gfx::RectF(290, 290, 16, 18), rootDamageRect);

    // CASE 2: If a descendant surface appears, its entire old area becomes exposed.

    // Cycle one frame of no change, just to sanity check that the next rect is not because of the old damage state.
    clearDamageForAllSurfaces(root.get());
    emulateDrawingOneFrame(root.get());
    rootDamageRect = root->render_surface()->damage_tracker()->current_damage_rect();
    EXPECT_TRUE(rootDamageRect.IsEmpty());

    // Then change the tree so that the render surface is added back.
    clearDamageForAllSurfaces(root.get());
    child1->SetOpacity(0.5);
    child1->SetForceRenderSurface(true);
    emulateDrawingOneFrame(root.get());

    // Sanity check that there is a new surface now.
    ASSERT_TRUE(child1->render_surface());
    EXPECT_EQ(3u, root->render_surface()->layer_list().size());
    EXPECT_EQ(2u, child1->render_surface()->layer_list().size());

    childDamageRect = child1->render_surface()->damage_tracker()->current_damage_rect();
    rootDamageRect = root->render_surface()->damage_tracker()->current_damage_rect();
    EXPECT_FLOAT_RECT_EQ(gfx::RectF(190, 190, 16, 18), childDamageRect);
    EXPECT_FLOAT_RECT_EQ(gfx::RectF(290, 290, 16, 18), rootDamageRect);
}

TEST_F(DamageTrackerTest, verifyNoDamageWhenNothingChanged)
{
    scoped_ptr<LayerImpl> root = createAndSetUpTestTreeWithTwoSurfaces();
    LayerImpl* child1 = root->children()[0];
    gfx::RectF childDamageRect;
    gfx::RectF rootDamageRect;

    // CASE 1: If nothing changes, the damage rect should be empty.
    //
    clearDamageForAllSurfaces(root.get());
    emulateDrawingOneFrame(root.get());
    childDamageRect = child1->render_surface()->damage_tracker()->current_damage_rect();
    rootDamageRect = root->render_surface()->damage_tracker()->current_damage_rect();
    EXPECT_TRUE(childDamageRect.IsEmpty());
    EXPECT_TRUE(rootDamageRect.IsEmpty());

    // CASE 2: If nothing changes twice in a row, the damage rect should still be empty.
    //
    clearDamageForAllSurfaces(root.get());
    emulateDrawingOneFrame(root.get());
    childDamageRect = child1->render_surface()->damage_tracker()->current_damage_rect();
    rootDamageRect = root->render_surface()->damage_tracker()->current_damage_rect();
    EXPECT_TRUE(childDamageRect.IsEmpty());
    EXPECT_TRUE(rootDamageRect.IsEmpty());
}

TEST_F(DamageTrackerTest, verifyNoDamageForUpdateRectThatDoesNotDrawContent)
{
    scoped_ptr<LayerImpl> root = createAndSetUpTestTreeWithTwoSurfaces();
    LayerImpl* child1 = root->children()[0];
    gfx::RectF childDamageRect;
    gfx::RectF rootDamageRect;

    // In our specific tree, the update rect of child1 should not cause any damage to any
    // surface because it does not actually draw content.
    clearDamageForAllSurfaces(root.get());
    child1->set_update_rect(gfx::RectF(0, 0, 1, 2));
    emulateDrawingOneFrame(root.get());
    childDamageRect = child1->render_surface()->damage_tracker()->current_damage_rect();
    rootDamageRect = root->render_surface()->damage_tracker()->current_damage_rect();
    EXPECT_TRUE(childDamageRect.IsEmpty());
    EXPECT_TRUE(rootDamageRect.IsEmpty());
}

TEST_F(DamageTrackerTest, verifyDamageForReplica)
{
    scoped_ptr<LayerImpl> root = createAndSetUpTestTreeWithTwoSurfaces();
    LayerImpl* child1 = root->children()[0];
    LayerImpl* grandChild1 = child1->children()[0];
    LayerImpl* grandChild2 = child1->children()[1];

    // Damage on a surface that has a reflection should cause the target surface to
    // receive the surface's damage and the surface's reflected damage.

    // For this test case, we modify grandChild2, and add grandChild3 to extend the bounds
    // of child1's surface. This way, we can test reflection changes without changing
    // contentBounds of the surface.
    grandChild2->SetPosition(gfx::PointF(180, 180));
    {
        scoped_ptr<LayerImpl> grandChild3 = LayerImpl::Create(m_hostImpl.active_tree(), 6);
        grandChild3->SetPosition(gfx::PointF(240, 240));
        grandChild3->SetAnchorPoint(gfx::PointF());
        grandChild3->SetBounds(gfx::Size(10, 10));
        grandChild3->SetContentBounds(gfx::Size(10, 10));
        grandChild3->SetDrawsContent(true);
        child1->AddChild(grandChild3.Pass());
    }
    child1->SetOpacity(0.5);
    emulateDrawingOneFrame(root.get());

    // CASE 1: adding a reflection about the left edge of grandChild1.
    //
    clearDamageForAllSurfaces(root.get());
    {
        scoped_ptr<LayerImpl> grandChild1Replica = LayerImpl::Create(m_hostImpl.active_tree(), 7);
        grandChild1Replica->SetPosition(gfx::PointF());
        grandChild1Replica->SetAnchorPoint(gfx::PointF());
        gfx::Transform reflection;
        reflection.Scale3d(-1, 1, 1);
        grandChild1Replica->SetTransform(reflection);
        grandChild1->SetReplicaLayer(grandChild1Replica.Pass());
    }
    emulateDrawingOneFrame(root.get());

    gfx::RectF grandChildDamageRect = grandChild1->render_surface()->damage_tracker()->current_damage_rect();
    gfx::RectF childDamageRect = child1->render_surface()->damage_tracker()->current_damage_rect();
    gfx::RectF rootDamageRect = root->render_surface()->damage_tracker()->current_damage_rect();

    // The grandChild surface damage should not include its own replica. The child
    // surface damage should include the normal and replica surfaces.
    EXPECT_FLOAT_RECT_EQ(gfx::RectF(0, 0, 6, 8), grandChildDamageRect);
    EXPECT_FLOAT_RECT_EQ(gfx::RectF(194, 200, 12, 8), childDamageRect);
    EXPECT_FLOAT_RECT_EQ(gfx::RectF(294, 300, 12, 8), rootDamageRect);

    // CASE 2: moving the descendant surface should cause both the original and reflected
    //         areas to be damaged on the target.
    clearDamageForAllSurfaces(root.get());
    gfx::Rect oldContentRect = child1->render_surface()->content_rect();
    grandChild1->SetPosition(gfx::PointF(195, 205));
    emulateDrawingOneFrame(root.get());
    ASSERT_EQ(oldContentRect.width(), child1->render_surface()->content_rect().width());
    ASSERT_EQ(oldContentRect.height(), child1->render_surface()->content_rect().height());

    grandChildDamageRect = grandChild1->render_surface()->damage_tracker()->current_damage_rect();
    childDamageRect = child1->render_surface()->damage_tracker()->current_damage_rect();
    rootDamageRect = root->render_surface()->damage_tracker()->current_damage_rect();

    // The child surface damage should include normal and replica surfaces for both old and new locations.
    //  - old location in target space: gfx::RectF(194, 200, 12, 8)
    //  - new location in target space: gfx::RectF(189, 205, 12, 8)
    EXPECT_FLOAT_RECT_EQ(gfx::RectF(0, 0, 6, 8), grandChildDamageRect);
    EXPECT_FLOAT_RECT_EQ(gfx::RectF(189, 200, 17, 13), childDamageRect);
    EXPECT_FLOAT_RECT_EQ(gfx::RectF(289, 300, 17, 13), rootDamageRect);

    // CASE 3: removing the reflection should cause the entire region including reflection
    //         to damage the target surface.
    clearDamageForAllSurfaces(root.get());
    grandChild1->SetReplicaLayer(scoped_ptr<LayerImpl>());
    emulateDrawingOneFrame(root.get());
    ASSERT_EQ(oldContentRect.width(), child1->render_surface()->content_rect().width());
    ASSERT_EQ(oldContentRect.height(), child1->render_surface()->content_rect().height());

    EXPECT_FALSE(grandChild1->render_surface());
    childDamageRect = child1->render_surface()->damage_tracker()->current_damage_rect();
    rootDamageRect = root->render_surface()->damage_tracker()->current_damage_rect();

    EXPECT_FLOAT_RECT_EQ(gfx::RectF(189, 205, 12, 8), childDamageRect);
    EXPECT_FLOAT_RECT_EQ(gfx::RectF(289, 305, 12, 8), rootDamageRect);
}

TEST_F(DamageTrackerTest, verifyDamageForMask)
{
    scoped_ptr<LayerImpl> root = createAndSetUpTestTreeWithOneSurface();
    LayerImpl* child = root->children()[0];

    // In the current implementation of the damage tracker, changes to mask layers should
    // damage the entire corresponding surface.

    clearDamageForAllSurfaces(root.get());

    // Set up the mask layer.
    {
        scoped_ptr<LayerImpl> maskLayer = LayerImpl::Create(m_hostImpl.active_tree(), 3);
        maskLayer->SetPosition(child->position());
        maskLayer->SetAnchorPoint(gfx::PointF());
        maskLayer->SetBounds(child->bounds());
        maskLayer->SetContentBounds(child->bounds());
        child->SetMaskLayer(maskLayer.Pass());
    }
    LayerImpl* maskLayer = child->mask_layer();

    // Add opacity and a grandChild so that the render surface persists even after we remove the mask.
    child->SetOpacity(0.5);
    {
        scoped_ptr<LayerImpl> grandChild = LayerImpl::Create(m_hostImpl.active_tree(), 4);
        grandChild->SetPosition(gfx::PointF(2, 2));
        grandChild->SetAnchorPoint(gfx::PointF());
        grandChild->SetBounds(gfx::Size(2, 2));
        grandChild->SetContentBounds(gfx::Size(2, 2));
        grandChild->SetDrawsContent(true);
        child->AddChild(grandChild.Pass());
    }
    emulateDrawingOneFrame(root.get());

    // Sanity check that a new surface was created for the child.
    ASSERT_TRUE(child->render_surface());

    // CASE 1: the updateRect on a mask layer should damage the entire target surface.
    //
    clearDamageForAllSurfaces(root.get());
    maskLayer->set_update_rect(gfx::RectF(1, 2, 3, 4));
    emulateDrawingOneFrame(root.get());
    gfx::RectF childDamageRect = child->render_surface()->damage_tracker()->current_damage_rect();
    EXPECT_FLOAT_RECT_EQ(gfx::RectF(0, 0, 30, 30), childDamageRect);

    // CASE 2: a property change on the mask layer should damage the entire target surface.
    //

    // Advance one frame without damage so that we know the damage rect is not leftover from the previous case.
    clearDamageForAllSurfaces(root.get());
    emulateDrawingOneFrame(root.get());
    childDamageRect = child->render_surface()->damage_tracker()->current_damage_rect();
    EXPECT_TRUE(childDamageRect.IsEmpty());

    // Then test the property change.
    clearDamageForAllSurfaces(root.get());
    maskLayer->SetStackingOrderChanged(true);

    emulateDrawingOneFrame(root.get());
    childDamageRect = child->render_surface()->damage_tracker()->current_damage_rect();
    EXPECT_FLOAT_RECT_EQ(gfx::RectF(0, 0, 30, 30), childDamageRect);

    // CASE 3: removing the mask also damages the entire target surface.
    //

    // Advance one frame without damage so that we know the damage rect is not leftover from the previous case.
    clearDamageForAllSurfaces(root.get());
    emulateDrawingOneFrame(root.get());
    childDamageRect = child->render_surface()->damage_tracker()->current_damage_rect();
    EXPECT_TRUE(childDamageRect.IsEmpty());

    // Then test mask removal.
    clearDamageForAllSurfaces(root.get());
    child->SetMaskLayer(scoped_ptr<LayerImpl>());
    ASSERT_TRUE(child->LayerPropertyChanged());
    emulateDrawingOneFrame(root.get());

    // Sanity check that a render surface still exists.
    ASSERT_TRUE(child->render_surface());

    childDamageRect = child->render_surface()->damage_tracker()->current_damage_rect();
    EXPECT_FLOAT_RECT_EQ(gfx::RectF(0, 0, 30, 30), childDamageRect);
}

TEST_F(DamageTrackerTest, verifyDamageForReplicaMask)
{
    scoped_ptr<LayerImpl> root = createAndSetUpTestTreeWithTwoSurfaces();
    LayerImpl* child1 = root->children()[0];
    LayerImpl* grandChild1 = child1->children()[0];

    // Changes to a replica's mask should not damage the original surface, because it is
    // not masked. But it does damage the ancestor target surface.

    clearDamageForAllSurfaces(root.get());

    // Create a reflection about the left edge of grandChild1.
    {
        scoped_ptr<LayerImpl> grandChild1Replica = LayerImpl::Create(m_hostImpl.active_tree(), 6);
        grandChild1Replica->SetPosition(gfx::PointF());
        grandChild1Replica->SetAnchorPoint(gfx::PointF());
        gfx::Transform reflection;
        reflection.Scale3d(-1, 1, 1);
        grandChild1Replica->SetTransform(reflection);
        grandChild1->SetReplicaLayer(grandChild1Replica.Pass());
    }
    LayerImpl* grandChild1Replica = grandChild1->replica_layer();

    // Set up the mask layer on the replica layer
    {
        scoped_ptr<LayerImpl> replicaMaskLayer = LayerImpl::Create(m_hostImpl.active_tree(), 7);
        replicaMaskLayer->SetPosition(gfx::PointF());
        replicaMaskLayer->SetAnchorPoint(gfx::PointF());
        replicaMaskLayer->SetBounds(grandChild1->bounds());
        replicaMaskLayer->SetContentBounds(grandChild1->bounds());
        grandChild1Replica->SetMaskLayer(replicaMaskLayer.Pass());
    }
    LayerImpl* replicaMaskLayer = grandChild1Replica->mask_layer();

    emulateDrawingOneFrame(root.get());

    // Sanity check that the appropriate render surfaces were created
    ASSERT_TRUE(grandChild1->render_surface());

    // CASE 1: a property change on the mask should damage only the reflected region on the target surface.
    clearDamageForAllSurfaces(root.get());
    replicaMaskLayer->SetStackingOrderChanged(true);
    emulateDrawingOneFrame(root.get());

    gfx::RectF grandChildDamageRect = grandChild1->render_surface()->damage_tracker()->current_damage_rect();
    gfx::RectF childDamageRect = child1->render_surface()->damage_tracker()->current_damage_rect();

    EXPECT_TRUE(grandChildDamageRect.IsEmpty());
    EXPECT_FLOAT_RECT_EQ(gfx::RectF(194, 200, 6, 8), childDamageRect);

    // CASE 2: removing the replica mask damages only the reflected region on the target surface.
    //
    clearDamageForAllSurfaces(root.get());
    grandChild1Replica->SetMaskLayer(scoped_ptr<LayerImpl>());
    emulateDrawingOneFrame(root.get());

    grandChildDamageRect = grandChild1->render_surface()->damage_tracker()->current_damage_rect();
    childDamageRect = child1->render_surface()->damage_tracker()->current_damage_rect();

    EXPECT_TRUE(grandChildDamageRect.IsEmpty());
    EXPECT_FLOAT_RECT_EQ(gfx::RectF(194, 200, 6, 8), childDamageRect);
}

TEST_F(DamageTrackerTest, verifyDamageForReplicaMaskWithAnchor)
{
    scoped_ptr<LayerImpl> root = createAndSetUpTestTreeWithTwoSurfaces();
    LayerImpl* child1 = root->children()[0];
    LayerImpl* grandChild1 = child1->children()[0];

    // Verify that the correct replicaOriginTransform is used for the replicaMask;
    clearDamageForAllSurfaces(root.get());

    grandChild1->SetAnchorPoint(gfx::PointF(1, 0)); // This is not exactly the anchor being tested, but by convention its expected to be the same as the replica's anchor point.

    {
        scoped_ptr<LayerImpl> grandChild1Replica = LayerImpl::Create(m_hostImpl.active_tree(), 6);
        grandChild1Replica->SetPosition(gfx::PointF());
        grandChild1Replica->SetAnchorPoint(gfx::PointF(1, 0)); // This is the anchor being tested.
        gfx::Transform reflection;
        reflection.Scale3d(-1, 1, 1);
        grandChild1Replica->SetTransform(reflection);
        grandChild1->SetReplicaLayer(grandChild1Replica.Pass());
    }
    LayerImpl* grandChild1Replica = grandChild1->replica_layer();

    // Set up the mask layer on the replica layer
    {
        scoped_ptr<LayerImpl> replicaMaskLayer = LayerImpl::Create(m_hostImpl.active_tree(), 7);
        replicaMaskLayer->SetPosition(gfx::PointF());
        replicaMaskLayer->SetAnchorPoint(gfx::PointF()); // note, this is not the anchor being tested.
        replicaMaskLayer->SetBounds(grandChild1->bounds());
        replicaMaskLayer->SetContentBounds(grandChild1->bounds());
        grandChild1Replica->SetMaskLayer(replicaMaskLayer.Pass());
    }
    LayerImpl* replicaMaskLayer = grandChild1Replica->mask_layer();

    emulateDrawingOneFrame(root.get());

    // Sanity check that the appropriate render surfaces were created
    ASSERT_TRUE(grandChild1->render_surface());

    // A property change on the replicaMask should damage the reflected region on the target surface.
    clearDamageForAllSurfaces(root.get());
    replicaMaskLayer->SetStackingOrderChanged(true);

    emulateDrawingOneFrame(root.get());

    gfx::RectF childDamageRect = child1->render_surface()->damage_tracker()->current_damage_rect();
    EXPECT_FLOAT_RECT_EQ(gfx::RectF(206, 200, 6, 8), childDamageRect);
}

TEST_F(DamageTrackerTest, verifyDamageWhenForcedFullDamage)
{
    scoped_ptr<LayerImpl> root = createAndSetUpTestTreeWithOneSurface();
    LayerImpl* child = root->children()[0];

    // Case 1: This test ensures that when the tracker is forced to have full damage, that
    //         it takes priority over any other partial damage.
    //
    clearDamageForAllSurfaces(root.get());
    child->set_update_rect(gfx::RectF(10, 11, 12, 13));
    root->render_surface()->damage_tracker()->ForceFullDamageNextUpdate();
    emulateDrawingOneFrame(root.get());
    gfx::RectF rootDamageRect = root->render_surface()->damage_tracker()->current_damage_rect();
    EXPECT_FLOAT_RECT_EQ(gfx::RectF(0, 0, 500, 500), rootDamageRect);

    // Case 2: An additional sanity check that forcing full damage works even when nothing
    //         on the layer tree changed.
    //
    clearDamageForAllSurfaces(root.get());
    root->render_surface()->damage_tracker()->ForceFullDamageNextUpdate();
    emulateDrawingOneFrame(root.get());
    rootDamageRect = root->render_surface()->damage_tracker()->current_damage_rect();
    EXPECT_FLOAT_RECT_EQ(gfx::RectF(0, 0, 500, 500), rootDamageRect);
}

TEST_F(DamageTrackerTest, verifyDamageForEmptyLayerList)
{
    // Though it should never happen, its a good idea to verify that the damage tracker
    // does not crash when it receives an empty layerList.

    scoped_ptr<LayerImpl> root = LayerImpl::Create(m_hostImpl.active_tree(), 1);
    root->CreateRenderSurface();

    ASSERT_TRUE(root == root->render_target());
    RenderSurfaceImpl* targetSurface = root->render_surface();
    targetSurface->ClearLayerLists();
    targetSurface->damage_tracker()->UpdateDamageTrackingState(targetSurface->layer_list(), targetSurface->OwningLayerId(), false, gfx::Rect(), 0, WebFilterOperations(), 0);

    gfx::RectF damageRect = targetSurface->damage_tracker()->current_damage_rect();
    EXPECT_TRUE(damageRect.IsEmpty());
}

TEST_F(DamageTrackerTest, verifyDamageAccumulatesUntilReset)
{
    // If damage is not cleared, it should accumulate.

    scoped_ptr<LayerImpl> root = createAndSetUpTestTreeWithOneSurface();
    LayerImpl* child = root->children()[0];

    clearDamageForAllSurfaces(root.get());
    child->set_update_rect(gfx::RectF(10, 11, 1, 2));
    emulateDrawingOneFrame(root.get());

    // Sanity check damage after the first frame; this isnt the actual test yet.
    gfx::RectF rootDamageRect = root->render_surface()->damage_tracker()->current_damage_rect();
    EXPECT_FLOAT_RECT_EQ(gfx::RectF(110, 111, 1, 2), rootDamageRect);

    // New damage, without having cleared the previous damage, should be unioned to the previous one.
    child->set_update_rect(gfx::RectF(20, 25, 1, 2));
    emulateDrawingOneFrame(root.get());
    rootDamageRect = root->render_surface()->damage_tracker()->current_damage_rect();
    EXPECT_FLOAT_RECT_EQ(gfx::RectF(110, 111, 11, 16), rootDamageRect);

    // If we notify the damage tracker that we drew the damaged area, then damage should be emptied.
    root->render_surface()->damage_tracker()->DidDrawDamagedArea();
    rootDamageRect = root->render_surface()->damage_tracker()->current_damage_rect();
    EXPECT_TRUE(rootDamageRect.IsEmpty());

    // Damage should remain empty even after one frame, since there's yet no new damage
    emulateDrawingOneFrame(root.get());
    rootDamageRect = root->render_surface()->damage_tracker()->current_damage_rect();
    EXPECT_TRUE(rootDamageRect.IsEmpty());
}

}  // namespace
}  // namespace cc
