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

    virtual bool hasDelegatedContent() const OVERRIDE;
    virtual bool hasContributingDelegatedRenderPasses() const OVERRIDE;

    // This gives ownership of the RenderPasses to the layer.
    void setRenderPasses(ScopedPtrVector<RenderPass>&);
    void clearRenderPasses();

    // Set the size at which the frame should be displayed, with the origin at the layer's origin.
    // This must always contain at least the layer's bounds. A value of (0, 0) implies that the
    // frame should be displayed to fit exactly in the layer's bounds.
    void setDisplaySize(gfx::Size displaySize) { m_displaySize = displaySize; }

    virtual void didLoseOutputSurface() OVERRIDE;

    virtual RenderPass::Id firstContributingRenderPassId() const OVERRIDE;
    virtual RenderPass::Id nextContributingRenderPassId(RenderPass::Id) const OVERRIDE;

    void appendContributingRenderPasses(RenderPassSink&);
    virtual void appendQuads(QuadSink&, AppendQuadsData&) OVERRIDE;

private:
    DelegatedRendererLayerImpl(LayerTreeImpl* treeImpl, int id);

    RenderPass::Id convertDelegatedRenderPassId(RenderPass::Id delegatedRenderPassId) const;

    void appendRenderPassQuads(QuadSink&, AppendQuadsData&, const RenderPass* fromDelegatedRenderPass, gfx::Size frameSize) const;

    virtual const char* layerTypeAsString() const OVERRIDE;

    ScopedPtrVector<RenderPass> m_renderPassesInDrawOrder;
    base::hash_map<RenderPass::Id, int> m_renderPassesIndexById;
    gfx::Size m_displaySize;
};

}

#endif  // CC_DELEGATED_RENDERER_LAYER_IMPL_H_
