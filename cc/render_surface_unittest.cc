// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/render_surface_impl.h"

#include "cc/append_quads_data.h"
#include "cc/layer_impl.h"
#include "cc/render_pass_sink.h"
#include "cc/scoped_ptr_vector.h"
#include "cc/shared_quad_state.h"
#include "cc/single_thread_proxy.h"
#include "cc/test/fake_impl_proxy.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "cc/test/geometry_test_utils.h"
#include "cc/test/mock_quad_culler.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/transform.h"

namespace cc {
namespace {

#define EXECUTE_AND_VERIFY_SURFACE_CHANGED(codeToTest)                  \
    renderSurface->resetPropertyChangedFlag();                          \
    codeToTest;                                                         \
    EXPECT_TRUE(renderSurface->surfacePropertyChanged())

#define EXECUTE_AND_VERIFY_SURFACE_DID_NOT_CHANGE(codeToTest)           \
    renderSurface->resetPropertyChangedFlag();                          \
    codeToTest;                                                         \
    EXPECT_FALSE(renderSurface->surfacePropertyChanged())

TEST(RenderSurfaceTest, verifySurfaceChangesAreTrackedProperly)
{
    //
    // This test checks that surfacePropertyChanged() has the correct behavior.
    //

    FakeImplProxy proxy;
    FakeLayerTreeHostImpl hostImpl(&proxy);
    scoped_ptr<LayerImpl> owningLayer = LayerImpl::create(hostImpl.activeTree(), 1);
    owningLayer->createRenderSurface();
    ASSERT_TRUE(owningLayer->renderSurface());
    RenderSurfaceImpl* renderSurface = owningLayer->renderSurface();
    gfx::Rect testRect = gfx::Rect(gfx::Point(3, 4), gfx::Size(5, 6));
    owningLayer->resetAllChangeTrackingForSubtree();

    // Currently, the contentRect, clipRect, and owningLayer->layerPropertyChanged() are
    // the only sources of change.
    EXECUTE_AND_VERIFY_SURFACE_CHANGED(renderSurface->setClipRect(testRect));
    EXECUTE_AND_VERIFY_SURFACE_CHANGED(renderSurface->setContentRect(testRect));

    owningLayer->setOpacity(0.5f);
    EXPECT_TRUE(renderSurface->surfacePropertyChanged());
    owningLayer->resetAllChangeTrackingForSubtree();

    // Setting the surface properties to the same values again should not be considered "change".
    EXECUTE_AND_VERIFY_SURFACE_DID_NOT_CHANGE(renderSurface->setClipRect(testRect));
    EXECUTE_AND_VERIFY_SURFACE_DID_NOT_CHANGE(renderSurface->setContentRect(testRect));

    scoped_ptr<LayerImpl> dummyMask = LayerImpl::create(hostImpl.activeTree(), 2);
    gfx::Transform dummyMatrix;
    dummyMatrix.Translate(1.0, 2.0);

    // The rest of the surface properties are either internal and should not cause change,
    // or they are already accounted for by the owninglayer->layerPropertyChanged().
    EXECUTE_AND_VERIFY_SURFACE_DID_NOT_CHANGE(renderSurface->setDrawOpacity(0.5));
    EXECUTE_AND_VERIFY_SURFACE_DID_NOT_CHANGE(renderSurface->setDrawTransform(dummyMatrix));
    EXECUTE_AND_VERIFY_SURFACE_DID_NOT_CHANGE(renderSurface->setReplicaDrawTransform(dummyMatrix));
    EXECUTE_AND_VERIFY_SURFACE_DID_NOT_CHANGE(renderSurface->clearLayerLists());
}

TEST(RenderSurfaceTest, sanityCheckSurfaceCreatesCorrectSharedQuadState)
{
    FakeImplProxy proxy;
    FakeLayerTreeHostImpl hostImpl(&proxy);
    scoped_ptr<LayerImpl> rootLayer = LayerImpl::create(hostImpl.activeTree(), 1);

    scoped_ptr<LayerImpl> owningLayer = LayerImpl::create(hostImpl.activeTree(), 2);
    owningLayer->createRenderSurface();
    ASSERT_TRUE(owningLayer->renderSurface());
    owningLayer->drawProperties().render_target = owningLayer.get();
    RenderSurfaceImpl* renderSurface = owningLayer->renderSurface();

    rootLayer->addChild(owningLayer.Pass());

    gfx::Rect contentRect = gfx::Rect(gfx::Point(), gfx::Size(50, 50));
    gfx::Rect clipRect = gfx::Rect(gfx::Point(5, 5), gfx::Size(40, 40));
    gfx::Transform origin;

    origin.Translate(30, 40);

    renderSurface->setDrawTransform(origin);
    renderSurface->setContentRect(contentRect);
    renderSurface->setClipRect(clipRect);
    renderSurface->setDrawOpacity(1);

    QuadList quadList;
    SharedQuadStateList sharedStateList;
    MockQuadCuller mockQuadCuller(quadList, sharedStateList);
    AppendQuadsData appendQuadsData;

    bool forReplica = false;
    renderSurface->appendQuads(mockQuadCuller, appendQuadsData, forReplica, RenderPass::Id(2, 0));

    ASSERT_EQ(1u, sharedStateList.size());
    SharedQuadState* sharedQuadState = sharedStateList[0];

    EXPECT_EQ(30, sharedQuadState->content_to_target_transform.matrix().getDouble(0, 3));
    EXPECT_EQ(40, sharedQuadState->content_to_target_transform.matrix().getDouble(1, 3));
    EXPECT_RECT_EQ(contentRect, gfx::Rect(sharedQuadState->visible_content_rect));
    EXPECT_EQ(1, sharedQuadState->opacity);
}

class TestRenderPassSink : public RenderPassSink {
public:
    virtual void appendRenderPass(scoped_ptr<RenderPass> renderPass) OVERRIDE { m_renderPasses.push_back(renderPass.Pass()); }

    const ScopedPtrVector<RenderPass>& renderPasses() const { return m_renderPasses; }

private:
    ScopedPtrVector<RenderPass> m_renderPasses;
};

TEST(RenderSurfaceTest, sanityCheckSurfaceCreatesCorrectRenderPass)
{
    FakeImplProxy proxy;
    FakeLayerTreeHostImpl hostImpl(&proxy);
    scoped_ptr<LayerImpl> rootLayer = LayerImpl::create(hostImpl.activeTree(), 1);

    scoped_ptr<LayerImpl> owningLayer = LayerImpl::create(hostImpl.activeTree(), 2);
    owningLayer->createRenderSurface();
    ASSERT_TRUE(owningLayer->renderSurface());
    owningLayer->drawProperties().render_target = owningLayer.get();
    RenderSurfaceImpl* renderSurface = owningLayer->renderSurface();

    rootLayer->addChild(owningLayer.Pass());

    gfx::Rect contentRect = gfx::Rect(gfx::Point(), gfx::Size(50, 50));
    gfx::Transform origin;
    origin.Translate(30, 40);

    renderSurface->setScreenSpaceTransform(origin);
    renderSurface->setContentRect(contentRect);

    TestRenderPassSink passSink;

    renderSurface->appendRenderPasses(passSink);

    ASSERT_EQ(1u, passSink.renderPasses().size());
    RenderPass* pass = passSink.renderPasses()[0];

    EXPECT_EQ(RenderPass::Id(2, 0), pass->id);
    EXPECT_RECT_EQ(contentRect, pass->output_rect);
    EXPECT_EQ(origin, pass->transform_to_root_target);
}

}  // namespace
}  // namespace cc
