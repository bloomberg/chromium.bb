// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/base/scoped_ptr_vector.h"
#include "cc/layers/append_quads_data.h"
#include "cc/layers/layer_impl.h"
#include "cc/layers/render_pass_sink.h"
#include "cc/layers/render_surface_impl.h"
#include "cc/quads/shared_quad_state.h"
#include "cc/test/fake_impl_proxy.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "cc/test/geometry_test_utils.h"
#include "cc/test/mock_quad_culler.h"
#include "cc/trees/single_thread_proxy.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/transform.h"

namespace cc {
namespace {

#define EXECUTE_AND_VERIFY_SURFACE_CHANGED(codeToTest)                  \
    renderSurface->ResetPropertyChangedFlag();                          \
    codeToTest;                                                         \
    EXPECT_TRUE(renderSurface->SurfacePropertyChanged())

#define EXECUTE_AND_VERIFY_SURFACE_DID_NOT_CHANGE(codeToTest)           \
    renderSurface->ResetPropertyChangedFlag();                          \
    codeToTest;                                                         \
    EXPECT_FALSE(renderSurface->SurfacePropertyChanged())

TEST(RenderSurfaceTest, verifySurfaceChangesAreTrackedProperly)
{
    //
    // This test checks that SurfacePropertyChanged() has the correct behavior.
    //

    FakeImplProxy proxy;
    FakeLayerTreeHostImpl hostImpl(&proxy);
    scoped_ptr<LayerImpl> owningLayer = LayerImpl::Create(hostImpl.active_tree(), 1);
    owningLayer->CreateRenderSurface();
    ASSERT_TRUE(owningLayer->render_surface());
    RenderSurfaceImpl* renderSurface = owningLayer->render_surface();
    gfx::Rect testRect = gfx::Rect(gfx::Point(3, 4), gfx::Size(5, 6));
    owningLayer->ResetAllChangeTrackingForSubtree();

    // Currently, the contentRect, clipRect, and owningLayer->layerPropertyChanged() are
    // the only sources of change.
    EXECUTE_AND_VERIFY_SURFACE_CHANGED(renderSurface->SetClipRect(testRect));
    EXECUTE_AND_VERIFY_SURFACE_CHANGED(renderSurface->SetContentRect(testRect));

    owningLayer->SetOpacity(0.5f);
    EXPECT_TRUE(renderSurface->SurfacePropertyChanged());
    owningLayer->ResetAllChangeTrackingForSubtree();

    // Setting the surface properties to the same values again should not be considered "change".
    EXECUTE_AND_VERIFY_SURFACE_DID_NOT_CHANGE(renderSurface->SetClipRect(testRect));
    EXECUTE_AND_VERIFY_SURFACE_DID_NOT_CHANGE(renderSurface->SetContentRect(testRect));

    scoped_ptr<LayerImpl> dummyMask = LayerImpl::Create(hostImpl.active_tree(), 2);
    gfx::Transform dummyMatrix;
    dummyMatrix.Translate(1.0, 2.0);

    // The rest of the surface properties are either internal and should not cause change,
    // or they are already accounted for by the owninglayer->layerPropertyChanged().
    EXECUTE_AND_VERIFY_SURFACE_DID_NOT_CHANGE(renderSurface->SetDrawOpacity(0.5f));
    EXECUTE_AND_VERIFY_SURFACE_DID_NOT_CHANGE(renderSurface->SetDrawTransform(dummyMatrix));
    EXECUTE_AND_VERIFY_SURFACE_DID_NOT_CHANGE(renderSurface->SetReplicaDrawTransform(dummyMatrix));
    EXECUTE_AND_VERIFY_SURFACE_DID_NOT_CHANGE(renderSurface->ClearLayerLists());
}

TEST(RenderSurfaceTest, sanityCheckSurfaceCreatesCorrectSharedQuadState)
{
    FakeImplProxy proxy;
    FakeLayerTreeHostImpl hostImpl(&proxy);
    scoped_ptr<LayerImpl> rootLayer = LayerImpl::Create(hostImpl.active_tree(), 1);

    scoped_ptr<LayerImpl> owningLayer = LayerImpl::Create(hostImpl.active_tree(), 2);
    owningLayer->CreateRenderSurface();
    ASSERT_TRUE(owningLayer->render_surface());
    owningLayer->draw_properties().render_target = owningLayer.get();
    RenderSurfaceImpl* renderSurface = owningLayer->render_surface();

    rootLayer->AddChild(owningLayer.Pass());

    gfx::Rect contentRect = gfx::Rect(gfx::Point(), gfx::Size(50, 50));
    gfx::Rect clipRect = gfx::Rect(gfx::Point(5, 5), gfx::Size(40, 40));
    gfx::Transform origin;

    origin.Translate(30, 40);

    renderSurface->SetDrawTransform(origin);
    renderSurface->SetContentRect(contentRect);
    renderSurface->SetClipRect(clipRect);
    renderSurface->SetDrawOpacity(1.f);

    QuadList quadList;
    SharedQuadStateList sharedStateList;
    MockQuadCuller mockQuadCuller(&quadList, &sharedStateList);
    AppendQuadsData appendQuadsData;

    bool forReplica = false;
    renderSurface->AppendQuads(&mockQuadCuller, &appendQuadsData, forReplica, RenderPass::Id(2, 0));

    ASSERT_EQ(1u, sharedStateList.size());
    SharedQuadState* sharedQuadState = sharedStateList[0];

    EXPECT_EQ(30, sharedQuadState->content_to_target_transform.matrix().getDouble(0, 3));
    EXPECT_EQ(40, sharedQuadState->content_to_target_transform.matrix().getDouble(1, 3));
    EXPECT_RECT_EQ(contentRect, gfx::Rect(sharedQuadState->visible_content_rect));
    EXPECT_EQ(1, sharedQuadState->opacity);
}

class TestRenderPassSink : public RenderPassSink {
public:
    virtual void AppendRenderPass(scoped_ptr<RenderPass> renderPass) OVERRIDE { m_renderPasses.push_back(renderPass.Pass()); }

    const ScopedPtrVector<RenderPass>& renderPasses() const { return m_renderPasses; }

private:
    ScopedPtrVector<RenderPass> m_renderPasses;
};

TEST(RenderSurfaceTest, sanityCheckSurfaceCreatesCorrectRenderPass)
{
    FakeImplProxy proxy;
    FakeLayerTreeHostImpl hostImpl(&proxy);
    scoped_ptr<LayerImpl> rootLayer = LayerImpl::Create(hostImpl.active_tree(), 1);

    scoped_ptr<LayerImpl> owningLayer = LayerImpl::Create(hostImpl.active_tree(), 2);
    owningLayer->CreateRenderSurface();
    ASSERT_TRUE(owningLayer->render_surface());
    owningLayer->draw_properties().render_target = owningLayer.get();
    RenderSurfaceImpl* renderSurface = owningLayer->render_surface();

    rootLayer->AddChild(owningLayer.Pass());

    gfx::Rect contentRect = gfx::Rect(gfx::Point(), gfx::Size(50, 50));
    gfx::Transform origin;
    origin.Translate(30, 40);

    renderSurface->SetScreenSpaceTransform(origin);
    renderSurface->SetContentRect(contentRect);

    TestRenderPassSink passSink;

    renderSurface->AppendRenderPasses(&passSink);

    ASSERT_EQ(1u, passSink.renderPasses().size());
    RenderPass* pass = passSink.renderPasses()[0];

    EXPECT_EQ(RenderPass::Id(2, 0), pass->id);
    EXPECT_RECT_EQ(contentRect, pass->output_rect);
    EXPECT_EQ(origin, pass->transform_to_root_target);
}

}  // namespace
}  // namespace cc
