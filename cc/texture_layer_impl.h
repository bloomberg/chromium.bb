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
    static scoped_ptr<TextureLayerImpl> Create(LayerTreeImpl* treeImpl, int id, bool usesMailbox)
    {
        return make_scoped_ptr(new TextureLayerImpl(treeImpl, id, usesMailbox));
    }
    virtual ~TextureLayerImpl();

    virtual scoped_ptr<LayerImpl> CreateLayerImpl(LayerTreeImpl*) OVERRIDE;
    virtual void PushPropertiesTo(LayerImpl*) OVERRIDE;

    virtual void WillDraw(ResourceProvider*) OVERRIDE;
    virtual void AppendQuads(QuadSink* quad_sink,
                             AppendQuadsData* append_quads_data) OVERRIDE;
    virtual void DidDraw(ResourceProvider*) OVERRIDE;

    virtual void DidLoseOutputSurface() OVERRIDE;

    virtual void DumpLayerProperties(std::string*, int indent) const OVERRIDE;
    virtual void DidBecomeActive() OVERRIDE;

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
    virtual bool CanClipSelf() const OVERRIDE;

    void setTextureMailbox(const TextureMailbox&);

private:
    TextureLayerImpl(LayerTreeImpl* treeImpl, int id, bool usesMailbox);

    virtual const char* LayerTypeAsString() const OVERRIDE;
    void freeTextureMailbox();

    unsigned m_textureId;
    ResourceProvider::ResourceId m_externalTextureResource;
    bool m_premultipliedAlpha;
    bool m_flipped;
    gfx::PointF m_uvTopLeft;
    gfx::PointF m_uvBottomRight;
    float m_vertexOpacity[4];

    TextureMailbox m_textureMailbox;
    bool m_usesMailbox;
    bool m_ownMailbox;
};

}

#endif  // CC_TEXTURE_LAYER_IMPL_H_
