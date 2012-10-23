// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CCTextureLayerImpl_h
#define CCTextureLayerImpl_h

#include "cc/layer_impl.h"

namespace cc {

class TextureLayerImpl : public LayerImpl {
public:
    static scoped_ptr<TextureLayerImpl> create(int id)
    {
        return make_scoped_ptr(new TextureLayerImpl(id));
    }
    virtual ~TextureLayerImpl();

    virtual void willDraw(ResourceProvider*) OVERRIDE;
    virtual void appendQuads(QuadSink&, AppendQuadsData&) OVERRIDE;
    virtual void didDraw(ResourceProvider*) OVERRIDE;

    virtual void didLoseContext() OVERRIDE;

    virtual void dumpLayerProperties(std::string*, int indent) const OVERRIDE;

    unsigned textureId() const { return m_textureId; }
    void setTextureId(unsigned id) { m_textureId = id; }
    void setPremultipliedAlpha(bool premultipliedAlpha) { m_premultipliedAlpha = premultipliedAlpha; }
    void setFlipped(bool flipped) { m_flipped = flipped; }
    void setUVRect(const FloatRect& rect) { m_uvRect = rect; }

private:
    explicit TextureLayerImpl(int);

    virtual const char* layerTypeAsString() const OVERRIDE;

    unsigned m_textureId;
    ResourceProvider::ResourceId m_externalTextureResource;
    bool m_premultipliedAlpha;
    bool m_flipped;
    FloatRect m_uvRect;
};

}

#endif // CCTextureLayerImpl_h
