// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "CCRenderPassDrawQuad.h"

namespace cc {

PassOwnPtr<CCRenderPassDrawQuad> CCRenderPassDrawQuad::create(const CCSharedQuadState* sharedQuadState, const IntRect& quadRect, CCRenderPass::Id renderPassId, bool isReplica, const CCResourceProvider::ResourceId maskResourceId, const IntRect& contentsChangedSinceLastFrame, float maskTexCoordScaleX, float maskTexCoordScaleY, float maskTexCoordOffsetX, float maskTexCoordOffsetY)
{
    return adoptPtr(new CCRenderPassDrawQuad(sharedQuadState, quadRect, renderPassId, isReplica, maskResourceId, contentsChangedSinceLastFrame, maskTexCoordScaleX, maskTexCoordScaleY, maskTexCoordOffsetX, maskTexCoordOffsetY));
}

CCRenderPassDrawQuad::CCRenderPassDrawQuad(const CCSharedQuadState* sharedQuadState, const IntRect& quadRect, CCRenderPass::Id renderPassId, bool isReplica, CCResourceProvider::ResourceId maskResourceId, const IntRect& contentsChangedSinceLastFrame, float maskTexCoordScaleX, float maskTexCoordScaleY, float maskTexCoordOffsetX, float maskTexCoordOffsetY)
    : CCDrawQuad(sharedQuadState, CCDrawQuad::RenderPass, quadRect)
    , m_renderPassId(renderPassId)
    , m_isReplica(isReplica)
    , m_maskResourceId(maskResourceId)
    , m_contentsChangedSinceLastFrame(contentsChangedSinceLastFrame)
    , m_maskTexCoordScaleX(maskTexCoordScaleX)
    , m_maskTexCoordScaleY(maskTexCoordScaleY)
    , m_maskTexCoordOffsetX(maskTexCoordOffsetX)
    , m_maskTexCoordOffsetY(maskTexCoordOffsetY)
{
    ASSERT(m_renderPassId.layerId > 0);
    ASSERT(m_renderPassId.index >= 0);
}

const CCRenderPassDrawQuad* CCRenderPassDrawQuad::materialCast(const CCDrawQuad* quad)
{
    ASSERT(quad->material() == CCDrawQuad::RenderPass);
    return static_cast<const CCRenderPassDrawQuad*>(quad);
}

PassOwnPtr<CCRenderPassDrawQuad> CCRenderPassDrawQuad::copy(const CCSharedQuadState* copiedSharedQuadState, CCRenderPass::Id copiedRenderPassId) const
{
    unsigned bytes = size();
    ASSERT(bytes);

    OwnPtr<CCRenderPassDrawQuad> copyQuad(adoptPtr(reinterpret_cast<CCRenderPassDrawQuad*>(new char[bytes])));
    memcpy(copyQuad.get(), this, bytes);
    copyQuad->setSharedQuadState(copiedSharedQuadState);
    copyQuad->m_renderPassId = copiedRenderPassId;

    return copyQuad.release();
}

}
