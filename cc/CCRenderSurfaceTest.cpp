// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "CCRenderSurface.h"

#include "CCAppendQuadsData.h"
#include "CCLayerImpl.h"
#include "CCRenderPassSink.h"
#include "CCSharedQuadState.h"
#include "CCSingleThreadProxy.h"
#include "MockCCQuadCuller.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <public/WebTransformationMatrix.h>

using namespace cc;
using WebKit::WebTransformationMatrix;

namespace {

#define EXECUTE_AND_VERIFY_SURFACE_CHANGED(codeToTest)                  \
    renderSurface->resetPropertyChangedFlag();                          \
    codeToTest;                                                         \
    EXPECT_TRUE(renderSurface->surfacePropertyChanged())

#define EXECUTE_AND_VERIFY_SURFACE_DID_NOT_CHANGE(codeToTest)           \
    renderSurface->resetPropertyChangedFlag();                          \
    codeToTest;                                                         \
    EXPECT_FALSE(renderSurface->surfacePropertyChanged())

TEST(CCRenderSurfaceTest, verifySurfaceChangesAreTrackedProperly)
{
    //
    // This test checks that surfacePropertyChanged() has the correct behavior.
    //

    // This will fake that we are on the correct thread for testing purposes.
    DebugScopedSetImplThread setImplThread;

    OwnPtr<CCLayerImpl> owningLayer = CCLayerImpl::create(1);
    owningLayer->createRenderSurface();
    ASSERT_TRUE(owningLayer->renderSurface());
    CCRenderSurface* renderSurface = owningLayer->renderSurface();
    IntRect testRect = IntRect(IntPoint(3, 4), IntSize(5, 6));
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

    OwnPtr<CCLayerImpl> dummyMask = CCLayerImpl::create(1);
    WebTransformationMatrix dummyMatrix;
    dummyMatrix.translate(1.0, 2.0);

    // The rest of the surface properties are either internal and should not cause change,
    // or they are already accounted for by the owninglayer->layerPropertyChanged().
    EXECUTE_AND_VERIFY_SURFACE_DID_NOT_CHANGE(renderSurface->setDrawOpacity(0.5));
    EXECUTE_AND_VERIFY_SURFACE_DID_NOT_CHANGE(renderSurface->setDrawTransform(dummyMatrix));
    EXECUTE_AND_VERIFY_SURFACE_DID_NOT_CHANGE(renderSurface->setReplicaDrawTransform(dummyMatrix));
    EXECUTE_AND_VERIFY_SURFACE_DID_NOT_CHANGE(renderSurface->clearLayerList());
}

TEST(CCRenderSurfaceTest, sanityCheckSurfaceCreatesCorrectSharedQuadState)
{
    // This will fake that we are on the correct thread for testing purposes.
    DebugScopedSetImplThread setImplThread;

    OwnPtr<CCLayerImpl> rootLayer = CCLayerImpl::create(1);

    OwnPtr<CCLayerImpl> owningLayer = CCLayerImpl::create(2);
    owningLayer->createRenderSurface();
    ASSERT_TRUE(owningLayer->renderSurface());
    owningLayer->setRenderTarget(owningLayer.get());
    CCRenderSurface* renderSurface = owningLayer->renderSurface();

    rootLayer->addChild(owningLayer.release());

    IntRect contentRect = IntRect(IntPoint::zero(), IntSize(50, 50));
    IntRect clipRect = IntRect(IntPoint(5, 5), IntSize(40, 40));
    WebTransformationMatrix origin;

    origin.translate(30, 40);

    renderSurface->setDrawTransform(origin);
    renderSurface->setContentRect(contentRect);
    renderSurface->setClipRect(clipRect);
    renderSurface->setDrawOpacity(1);

    CCQuadList quadList;
    CCSharedQuadStateList sharedStateList;
    MockCCQuadCuller mockQuadCuller(quadList, sharedStateList);
    CCAppendQuadsData appendQuadsData;

    bool forReplica = false;
    renderSurface->appendQuads(mockQuadCuller, appendQuadsData, forReplica, CCRenderPass::Id(2, 0));

    ASSERT_EQ(1u, sharedStateList.size());
    CCSharedQuadState* sharedQuadState = sharedStateList[0].get();

    EXPECT_EQ(30, sharedQuadState->quadTransform.m41());
    EXPECT_EQ(40, sharedQuadState->quadTransform.m42());
    EXPECT_EQ(contentRect, IntRect(sharedQuadState->visibleContentRect));
    EXPECT_EQ(1, sharedQuadState->opacity);
    EXPECT_FALSE(sharedQuadState->opaque);
}

class TestCCRenderPassSink : public CCRenderPassSink {
public:
    virtual void appendRenderPass(PassOwnPtr<CCRenderPass> renderPass) OVERRIDE { m_renderPasses.append(renderPass); }

    const Vector<OwnPtr<CCRenderPass> >& renderPasses() const { return m_renderPasses; }

private:
    Vector<OwnPtr<CCRenderPass> > m_renderPasses;

};

TEST(CCRenderSurfaceTest, sanityCheckSurfaceCreatesCorrectRenderPass)
{
    // This will fake that we are on the correct thread for testing purposes.
    DebugScopedSetImplThread setImplThread;

    OwnPtr<CCLayerImpl> rootLayer = CCLayerImpl::create(1);

    OwnPtr<CCLayerImpl> owningLayer = CCLayerImpl::create(2);
    owningLayer->createRenderSurface();
    ASSERT_TRUE(owningLayer->renderSurface());
    owningLayer->setRenderTarget(owningLayer.get());
    CCRenderSurface* renderSurface = owningLayer->renderSurface();

    rootLayer->addChild(owningLayer.release());

    IntRect contentRect = IntRect(IntPoint::zero(), IntSize(50, 50));
    WebTransformationMatrix origin;
    origin.translate(30, 40);

    renderSurface->setScreenSpaceTransform(origin);
    renderSurface->setContentRect(contentRect);

    TestCCRenderPassSink passSink;

    renderSurface->appendRenderPasses(passSink);

    ASSERT_EQ(1u, passSink.renderPasses().size());
    CCRenderPass* pass = passSink.renderPasses()[0].get();

    EXPECT_EQ(CCRenderPass::Id(2, 0), pass->id());
    EXPECT_EQ(contentRect, pass->outputRect());
    EXPECT_EQ(origin, pass->transformToRootTarget());
}

} // namespace
