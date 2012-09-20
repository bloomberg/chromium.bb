// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "CCDelegatedRendererLayerImpl.h"

#include "CCAppendQuadsData.h"
#include "CCQuadSink.h"
#include "CCRenderPassDrawQuad.h"
#include "CCRenderPassSink.h"

namespace cc {

CCDelegatedRendererLayerImpl::CCDelegatedRendererLayerImpl(int id)
    : CCLayerImpl(id)
{
}

CCDelegatedRendererLayerImpl::~CCDelegatedRendererLayerImpl()
{
    clearRenderPasses();
}

bool CCDelegatedRendererLayerImpl::descendantDrawsContent()
{
    // FIXME: This could possibly return false even though there are some
    // quads present as they could all be from a single layer (or set of
    // layers without children). If this happens, then make a test that
    // ensures the opacity is being changed on quads in the root RenderPass
    // when this layer doesn't own a RenderSurface.
    return !m_renderPassesInDrawOrder.isEmpty();
}

bool CCDelegatedRendererLayerImpl::hasContributingDelegatedRenderPasses() const
{
    // The root RenderPass for the layer is merged with its target
    // RenderPass in each frame. So we only have extra RenderPasses
    // to merge when we have a non-root RenderPass present.
    return m_renderPassesInDrawOrder.size() > 1;
}

void CCDelegatedRendererLayerImpl::setRenderPasses(Vector<OwnPtr<CCRenderPass> >& renderPassesInDrawOrder)
{
    FloatRect oldRootDamage;
    if (!m_renderPassesInDrawOrder.isEmpty())
        oldRootDamage = m_renderPassesInDrawOrder.last()->damageRect();

    clearRenderPasses();

    for (size_t i = 0; i < renderPassesInDrawOrder.size(); ++i) {
        m_renderPassesIndexById.set(renderPassesInDrawOrder[i]->id(), i);
        m_renderPassesInDrawOrder.append(renderPassesInDrawOrder[i].release());
    }
    renderPassesInDrawOrder.clear();

    if (!m_renderPassesInDrawOrder.isEmpty()) {
        FloatRect newRootDamage = m_renderPassesInDrawOrder.last()->damageRect();
        m_renderPassesInDrawOrder.last()->setDamageRect(unionRect(oldRootDamage, newRootDamage));
    }
}

void CCDelegatedRendererLayerImpl::clearRenderPasses()
{
    // FIXME: Release the resources back to the nested compositor.
    m_renderPassesIndexById.clear();
    m_renderPassesInDrawOrder.clear();
}

void CCDelegatedRendererLayerImpl::didLoseContext()
{
    clearRenderPasses();
}

static inline int indexToId(int index) { return index + 1; }
static inline int idToIndex(int id) { return id - 1; }

CCRenderPass::Id CCDelegatedRendererLayerImpl::firstContributingRenderPassId() const
{
    return CCRenderPass::Id(id(), indexToId(0));
}

CCRenderPass::Id CCDelegatedRendererLayerImpl::nextContributingRenderPassId(CCRenderPass::Id previous) const
{
    return CCRenderPass::Id(previous.layerId, previous.index + 1);
}

CCRenderPass::Id CCDelegatedRendererLayerImpl::convertDelegatedRenderPassId(CCRenderPass::Id delegatedRenderPassId) const
{
    unsigned delegatedRenderPassIndex = m_renderPassesIndexById.get(delegatedRenderPassId);
    return CCRenderPass::Id(id(), indexToId(delegatedRenderPassIndex));
}

void CCDelegatedRendererLayerImpl::appendContributingRenderPasses(CCRenderPassSink& renderPassSink)
{
    ASSERT(hasContributingDelegatedRenderPasses());

    for (size_t i = 0; i < m_renderPassesInDrawOrder.size() - 1; ++i) {
        CCRenderPass::Id outputRenderPassId = convertDelegatedRenderPassId(m_renderPassesInDrawOrder[i]->id());

        // Don't clash with the RenderPass we generate if we own a RenderSurface.
        ASSERT(outputRenderPassId.index > 0);

        renderPassSink.appendRenderPass(m_renderPassesInDrawOrder[i]->copy(outputRenderPassId));
    }
}

void CCDelegatedRendererLayerImpl::appendQuads(CCQuadSink& quadSink, CCAppendQuadsData& appendQuadsData)
{
    if (m_renderPassesInDrawOrder.isEmpty())
        return;

    CCRenderPass::Id targetRenderPassId = appendQuadsData.renderPassId;

    // If the index of the renderPassId is 0, then it is a renderPass generated for a layer
    // in this compositor, not the delegated renderer. Then we want to merge our root renderPass with
    // the target renderPass. Otherwise, it is some renderPass which we added from the delegated
    // renderer.
    bool shouldMergeRootRenderPassWithTarget = !targetRenderPassId.index;
    if (shouldMergeRootRenderPassWithTarget) {
        // Verify that the renderPass we are appending to is created our renderTarget.
        ASSERT(targetRenderPassId.layerId == renderTarget()->id());

        CCRenderPass* rootDelegatedRenderPass = m_renderPassesInDrawOrder.last().get();
        appendRenderPassQuads(quadSink, appendQuadsData, rootDelegatedRenderPass);
    } else {
        // Verify that the renderPass we are appending to was created by us.
        ASSERT(targetRenderPassId.layerId == id());

        int renderPassIndex = idToIndex(targetRenderPassId.index);
        CCRenderPass* delegatedRenderPass = m_renderPassesInDrawOrder[renderPassIndex].get();
        appendRenderPassQuads(quadSink, appendQuadsData, delegatedRenderPass);
    }
}

void CCDelegatedRendererLayerImpl::appendRenderPassQuads(CCQuadSink& quadSink, CCAppendQuadsData& appendQuadsData, CCRenderPass* delegatedRenderPass) const
{
    const CCSharedQuadState* currentSharedQuadState = 0;
    CCSharedQuadState* copiedSharedQuadState = 0;
    for (size_t i = 0; i < delegatedRenderPass->quadList().size(); ++i) {
        CCDrawQuad* quad = delegatedRenderPass->quadList()[i].get();

        if (quad->sharedQuadState() != currentSharedQuadState) {
            currentSharedQuadState = quad->sharedQuadState();
            copiedSharedQuadState = quadSink.useSharedQuadState(currentSharedQuadState->copy());
        }
        ASSERT(copiedSharedQuadState);

        bool targetIsFromDelegatedRendererLayer = appendQuadsData.renderPassId.layerId == id();
        if (!targetIsFromDelegatedRendererLayer) {
            // Should be the root render pass.
            ASSERT(delegatedRenderPass == m_renderPassesInDrawOrder.last());
            // This layer must be drawing to a renderTarget other than itself.
            ASSERT(renderTarget() != this);

            copiedSharedQuadState->quadTransform = copiedSharedQuadState->quadTransform * drawTransform();
            copiedSharedQuadState->opacity *= drawOpacity();
        }

        OwnPtr<CCDrawQuad> copyQuad;
        if (quad->material() != CCDrawQuad::RenderPass)
            copyQuad = quad->copy(copiedSharedQuadState);
        else {
            CCRenderPass::Id contributingDelegatedRenderPassId = CCRenderPassDrawQuad::materialCast(quad)->renderPassId();
            CCRenderPass::Id contributingRenderPassId = convertDelegatedRenderPassId(contributingDelegatedRenderPassId);
            ASSERT(contributingRenderPassId != appendQuadsData.renderPassId);

            copyQuad = CCRenderPassDrawQuad::materialCast(quad)->copy(copiedSharedQuadState, contributingRenderPassId);
        }
        ASSERT(copyQuad);

        quadSink.append(copyQuad.release(), appendQuadsData);
    }
}

}
