// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CCRenderPassDrawQuad_h
#define CCRenderPassDrawQuad_h

#include "CCDrawQuad.h"
#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "cc/render_pass.h"
#include "cc/resource_provider.h"

namespace cc {

class CCRenderPassDrawQuad : public CCDrawQuad {
public:
    static scoped_ptr<CCRenderPassDrawQuad> create(const CCSharedQuadState*, const gfx::Rect&, CCRenderPass::Id renderPassId, bool isReplica, CCResourceProvider::ResourceId maskResourceId, const gfx::Rect& contentsChangedSinceLastFrame, float maskTexCoordScaleX, float maskTexCoordScaleY, float maskTexCoordOffsetX, float maskTexCoordOffsetY);

    CCRenderPass::Id renderPassId() const { return m_renderPassId; }
    bool isReplica() const { return m_isReplica; }
    CCResourceProvider::ResourceId maskResourceId() const { return m_maskResourceId; }
    const gfx::Rect& contentsChangedSinceLastFrame() const { return m_contentsChangedSinceLastFrame; }

    static const CCRenderPassDrawQuad* materialCast(const CCDrawQuad*);
    float maskTexCoordScaleX() const { return m_maskTexCoordScaleX; }
    float maskTexCoordScaleY() const { return m_maskTexCoordScaleY; }
    float maskTexCoordOffsetX() const { return m_maskTexCoordOffsetX; }
    float maskTexCoordOffsetY() const { return m_maskTexCoordOffsetY; }

    scoped_ptr<CCRenderPassDrawQuad> copy(const CCSharedQuadState* copiedSharedQuadState, CCRenderPass::Id copiedRenderPassId) const;

private:
    CCRenderPassDrawQuad(const CCSharedQuadState*, const gfx::Rect&, CCRenderPass::Id renderPassId, bool isReplica, CCResourceProvider::ResourceId maskResourceId, const gfx::Rect& contentsChangedSinceLastFrame, float maskTexCoordScaleX, float maskTexCoordScaleY, float maskTexCoordOffsetX, float maskTexCoordOffsetY);

    CCRenderPass::Id m_renderPassId;
    bool m_isReplica;
    CCResourceProvider::ResourceId m_maskResourceId;
    gfx::Rect m_contentsChangedSinceLastFrame;
    float m_maskTexCoordScaleX;
    float m_maskTexCoordScaleY;
    float m_maskTexCoordOffsetX;
    float m_maskTexCoordOffsetY;

    DISALLOW_COPY_AND_ASSIGN(CCRenderPassDrawQuad);
};

}

#endif
