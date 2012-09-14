// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CCRenderPassDrawQuad_h
#define CCRenderPassDrawQuad_h

#include "CCDrawQuad.h"
#include "CCRenderPass.h"
#include "CCResourceProvider.h"
#include "IntRect.h"
#include <wtf/PassOwnPtr.h>

namespace cc {

class CCRenderPassDrawQuad : public CCDrawQuad {
    WTF_MAKE_NONCOPYABLE(CCRenderPassDrawQuad);
public:
    static PassOwnPtr<CCRenderPassDrawQuad> create(const CCSharedQuadState*, const IntRect&, CCRenderPass::Id renderPassId, bool isReplica, CCResourceProvider::ResourceId maskResourceId, const IntRect& contentsChangedSinceLastFrame, float maskTexCoordScaleX, float maskTexCoordScaleY, float maskTexCoordOffsetX, float maskTexCoordOffsetY);

    CCRenderPass::Id renderPassId() const { return m_renderPassId; }
    bool isReplica() const { return m_isReplica; }
    CCResourceProvider::ResourceId maskResourceId() const { return m_maskResourceId; }
    const IntRect& contentsChangedSinceLastFrame() const { return m_contentsChangedSinceLastFrame; }

    static const CCRenderPassDrawQuad* materialCast(const CCDrawQuad*);
    float maskTexCoordScaleX() const { return m_maskTexCoordScaleX; }
    float maskTexCoordScaleY() const { return m_maskTexCoordScaleY; }
    float maskTexCoordOffsetX() const { return m_maskTexCoordOffsetX; }
    float maskTexCoordOffsetY() const { return m_maskTexCoordOffsetY; }

    PassOwnPtr<CCRenderPassDrawQuad> copy(const CCSharedQuadState* copiedSharedQuadState, CCRenderPass::Id copiedRenderPassId) const;

private:
    CCRenderPassDrawQuad(const CCSharedQuadState*, const IntRect&, CCRenderPass::Id renderPassId, bool isReplica, CCResourceProvider::ResourceId maskResourceId, const IntRect& contentsChangedSinceLastFrame, float maskTexCoordScaleX, float maskTexCoordScaleY, float maskTexCoordOffsetX, float maskTexCoordOffsetY);

    CCRenderPass::Id m_renderPassId;
    bool m_isReplica;
    CCResourceProvider::ResourceId m_maskResourceId;
    IntRect m_contentsChangedSinceLastFrame;
    float m_maskTexCoordScaleX;
    float m_maskTexCoordScaleY;
    float m_maskTexCoordOffsetX;
    float m_maskTexCoordOffsetY;
};

}

#endif
