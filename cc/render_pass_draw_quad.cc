// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/render_pass_draw_quad.h"

namespace cc {

scoped_ptr<RenderPassDrawQuad> RenderPassDrawQuad::create(const SharedQuadState* sharedQuadState, const gfx::Rect& quadRect, RenderPass::Id renderPassId, bool isReplica, const ResourceProvider::ResourceId maskResourceId, const gfx::Rect& contentsChangedSinceLastFrame, float maskTexCoordScaleX, float maskTexCoordScaleY, float maskTexCoordOffsetX, float maskTexCoordOffsetY)
{
    return make_scoped_ptr(new RenderPassDrawQuad(sharedQuadState, quadRect, renderPassId, isReplica, maskResourceId, contentsChangedSinceLastFrame, maskTexCoordScaleX, maskTexCoordScaleY, maskTexCoordOffsetX, maskTexCoordOffsetY));
}

RenderPassDrawQuad::RenderPassDrawQuad(const SharedQuadState* sharedQuadState, const gfx::Rect& quadRect, RenderPass::Id renderPassId, bool isReplica, ResourceProvider::ResourceId maskResourceId, const gfx::Rect& contentsChangedSinceLastFrame, float maskTexCoordScaleX, float maskTexCoordScaleY, float maskTexCoordOffsetX, float maskTexCoordOffsetY)
    : DrawQuad(sharedQuadState, DrawQuad::RENDER_PASS, quadRect, gfx::Rect())
    , m_renderPassId(renderPassId)
    , m_isReplica(isReplica)
    , m_maskResourceId(maskResourceId)
    , m_contentsChangedSinceLastFrame(contentsChangedSinceLastFrame)
    , m_maskTexCoordScaleX(maskTexCoordScaleX)
    , m_maskTexCoordScaleY(maskTexCoordScaleY)
    , m_maskTexCoordOffsetX(maskTexCoordOffsetX)
    , m_maskTexCoordOffsetY(maskTexCoordOffsetY)
{
    DCHECK(m_renderPassId.layerId > 0);
    DCHECK(m_renderPassId.index >= 0);
}

const RenderPassDrawQuad* RenderPassDrawQuad::materialCast(const DrawQuad* quad)
{
    DCHECK(quad->material() == DrawQuad::RENDER_PASS);
    return static_cast<const RenderPassDrawQuad*>(quad);
}

scoped_ptr<RenderPassDrawQuad> RenderPassDrawQuad::copy(const SharedQuadState* copiedSharedQuadState, RenderPass::Id copiedRenderPassId) const
{
    scoped_ptr<RenderPassDrawQuad> copyQuad(new RenderPassDrawQuad(*materialCast(this)));
    copyQuad->setSharedQuadState(copiedSharedQuadState);
    copyQuad->m_renderPassId = copiedRenderPassId;
    return copyQuad.Pass();
}

}  // namespace cc
