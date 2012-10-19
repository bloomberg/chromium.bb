// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CCRenderPassDrawQuad_h
#define CCRenderPassDrawQuad_h

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "CCDrawQuad.h"
#include "CCRenderPass.h"
#include "CCResourceProvider.h"
#include "IntRect.h"

namespace cc {

class RenderPassDrawQuad : public DrawQuad {
public:
    static scoped_ptr<RenderPassDrawQuad> create(const SharedQuadState*, const IntRect&, RenderPass::Id renderPassId, bool isReplica, ResourceProvider::ResourceId maskResourceId, const IntRect& contentsChangedSinceLastFrame, float maskTexCoordScaleX, float maskTexCoordScaleY, float maskTexCoordOffsetX, float maskTexCoordOffsetY);

    RenderPass::Id renderPassId() const { return m_renderPassId; }
    bool isReplica() const { return m_isReplica; }
    ResourceProvider::ResourceId maskResourceId() const { return m_maskResourceId; }
    const IntRect& contentsChangedSinceLastFrame() const { return m_contentsChangedSinceLastFrame; }

    static const RenderPassDrawQuad* materialCast(const DrawQuad*);
    float maskTexCoordScaleX() const { return m_maskTexCoordScaleX; }
    float maskTexCoordScaleY() const { return m_maskTexCoordScaleY; }
    float maskTexCoordOffsetX() const { return m_maskTexCoordOffsetX; }
    float maskTexCoordOffsetY() const { return m_maskTexCoordOffsetY; }

    scoped_ptr<RenderPassDrawQuad> copy(const SharedQuadState* copiedSharedQuadState, RenderPass::Id copiedRenderPassId) const;

private:
    RenderPassDrawQuad(const SharedQuadState*, const IntRect&, RenderPass::Id renderPassId, bool isReplica, ResourceProvider::ResourceId maskResourceId, const IntRect& contentsChangedSinceLastFrame, float maskTexCoordScaleX, float maskTexCoordScaleY, float maskTexCoordOffsetX, float maskTexCoordOffsetY);

    RenderPass::Id m_renderPassId;
    bool m_isReplica;
    ResourceProvider::ResourceId m_maskResourceId;
    IntRect m_contentsChangedSinceLastFrame;
    float m_maskTexCoordScaleX;
    float m_maskTexCoordScaleY;
    float m_maskTexCoordOffsetX;
    float m_maskTexCoordOffsetY;

    DISALLOW_COPY_AND_ASSIGN(RenderPassDrawQuad);
};

}

#endif
