// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEXTURE_LAYER_IMPL_H_
#define CC_TEXTURE_LAYER_IMPL_H_

#include <string>

#include "base/callback.h"
#include "cc/cc_export.h"
#include "cc/layer_impl.h"

namespace cc {

class CC_EXPORT TextureLayerImpl : public LayerImpl {
public:
    static scoped_ptr<TextureLayerImpl> create(LayerTreeImpl* treeImpl, int id, bool usesMailbox)
    {
        return make_scoped_ptr(new TextureLayerImpl(treeImpl, id, usesMailbox));
    }
    virtual ~TextureLayerImpl();

    virtual scoped_ptr<LayerImpl> createLayerImpl(LayerTreeImpl*) OVERRIDE;
    virtual void pushPropertiesTo(LayerImpl*) OVERRIDE;

    virtual void willDraw(ResourceProvider*) OVERRIDE;
    virtual void appendQuads(QuadSink&, AppendQuadsData&) OVERRIDE;
    virtual void didDraw(ResourceProvider*) OVERRIDE;

    virtual void didLoseOutputSurface() OVERRIDE;

    virtual void dumpLayerProperties(std::string*, int indent) const OVERRIDE;

    unsigned textureId() const { return m_textureId; }
    void setTextureId(unsigned id) { m_textureId = id; }
    void setPremultipliedAlpha(bool premultipliedAlpha) { m_premultipliedAlpha = premultipliedAlpha; }
    void setFlipped(bool flipped) { m_flipped = flipped; }
    void setUVTopLeft(gfx::PointF topLeft) { m_uvTopLeft = topLeft; }
    void setUVBottomRight(gfx::PointF bottomRight) { m_uvBottomRight = bottomRight; }

    // 1--2
    // |  |
    // 0--3
    void setVertexOpacity(const float vertexOpacity[4]);
    virtual bool canClipSelf() const OVERRIDE;

    void setTextureMailbox(const TextureMailbox&);

private:
    TextureLayerImpl(LayerTreeImpl* treeImpl, int id, bool usesMailbox);

    virtual const char* layerTypeAsString() const OVERRIDE;

    unsigned m_textureId;
    ResourceProvider::ResourceId m_externalTextureResource;
    bool m_premultipliedAlpha;
    bool m_flipped;
    gfx::PointF m_uvTopLeft;
    gfx::PointF m_uvBottomRight;
    float m_vertexOpacity[4];

    bool m_hasPendingMailbox;
    TextureMailbox m_pendingTextureMailbox;
    bool m_usesMailbox;
};

}

#endif  // CC_TEXTURE_LAYER_IMPL_H_
