// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_DELEGATED_RENDERER_LAYER_IMPL_H_
#define CC_DELEGATED_RENDERER_LAYER_IMPL_H_

#include "cc/cc_export.h"
#include "cc/layer_impl.h"
#include "cc/scoped_ptr_vector.h"

namespace cc {

class CC_EXPORT DelegatedRendererLayerImpl : public LayerImpl {
public:
    static scoped_ptr<DelegatedRendererLayerImpl> create(LayerTreeImpl* treeImpl, int id) { return make_scoped_ptr(new DelegatedRendererLayerImpl(treeImpl, id)); }
    virtual ~DelegatedRendererLayerImpl();

    virtual int descendantsDrawContent() OVERRIDE;
    virtual bool hasContributingDelegatedRenderPasses() const OVERRIDE;

    // This gives ownership of the RenderPasses to the layer.
    void setRenderPasses(ScopedPtrVector<RenderPass>&);
    void clearRenderPasses();

    virtual void didLoseOutputSurface() OVERRIDE;

    virtual RenderPass::Id firstContributingRenderPassId() const OVERRIDE;
    virtual RenderPass::Id nextContributingRenderPassId(RenderPass::Id) const OVERRIDE;

    void appendContributingRenderPasses(RenderPassSink&);
    virtual void appendQuads(QuadSink&, AppendQuadsData&) OVERRIDE;

private:
    DelegatedRendererLayerImpl(LayerTreeImpl* treeImpl, int id);

    RenderPass::Id convertDelegatedRenderPassId(RenderPass::Id delegatedRenderPassId) const;

    void appendRenderPassQuads(QuadSink&, AppendQuadsData&, const RenderPass* fromDelegatedRenderPass) const;

    virtual const char* layerTypeAsString() const OVERRIDE;

    ScopedPtrVector<RenderPass> m_renderPassesInDrawOrder;
    base::hash_map<RenderPass::Id, int> m_renderPassesIndexById;
};

}

#endif  // CC_DELEGATED_RENDERER_LAYER_IMPL_H_
