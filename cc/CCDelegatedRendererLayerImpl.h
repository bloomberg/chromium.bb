// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CCDelegatedRendererLayerImpl_h
#define CCDelegatedRendererLayerImpl_h

#include "CCLayerImpl.h"

namespace cc {

class CCDelegatedRendererLayerImpl : public CCLayerImpl {
public:
    static PassOwnPtr<CCDelegatedRendererLayerImpl> create(int id) { return adoptPtr(new CCDelegatedRendererLayerImpl(id)); }
    virtual ~CCDelegatedRendererLayerImpl();

    virtual bool descendantDrawsContent() OVERRIDE;
    virtual bool hasContributingDelegatedRenderPasses() const OVERRIDE;

    // This gives ownership of the RenderPasses to the layer.
    void setRenderPasses(OwnPtrVector<CCRenderPass>&);
    void clearRenderPasses();

    virtual void didLoseContext() OVERRIDE;

    virtual CCRenderPass::Id firstContributingRenderPassId() const OVERRIDE;
    virtual CCRenderPass::Id nextContributingRenderPassId(CCRenderPass::Id) const OVERRIDE;

    void appendContributingRenderPasses(CCRenderPassSink&);
    virtual void appendQuads(CCQuadSink&, CCAppendQuadsData&) OVERRIDE;

private:
    explicit CCDelegatedRendererLayerImpl(int);

    CCRenderPass::Id convertDelegatedRenderPassId(CCRenderPass::Id delegatedRenderPassId) const;

    void appendRenderPassQuads(CCQuadSink&, CCAppendQuadsData&, CCRenderPass* fromDelegatedRenderPass) const;

    PassOwnPtr<CCDrawQuad> createCopyOfQuad(const CCDrawQuad*);

    virtual const char* layerTypeAsString() const OVERRIDE { return "DelegatedRendererLayer"; }

    OwnPtrVector<CCRenderPass> m_renderPassesInDrawOrder;
    HashMap<CCRenderPass::Id, int> m_renderPassesIndexById;
};

}

#endif // CCDelegatedRendererLayerImpl_h
