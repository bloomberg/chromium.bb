// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CCTextureLayerImpl_h
#define CCTextureLayerImpl_h

#include "CCLayerImpl.h"

namespace WebCore {

class CCTextureLayerImpl : public CCLayerImpl {
public:
    static PassOwnPtr<CCTextureLayerImpl> create(int id)
    {
        return adoptPtr(new CCTextureLayerImpl(id));
    }
    virtual ~CCTextureLayerImpl();

    virtual void willDraw(CCResourceProvider*) OVERRIDE;
    virtual void appendQuads(CCQuadSink&, CCAppendQuadsData&) OVERRIDE;
    virtual void didDraw(CCResourceProvider*) OVERRIDE;

    virtual void didLoseContext() OVERRIDE;

    virtual void dumpLayerProperties(std::string*, int indent) const OVERRIDE;

    unsigned textureId() const { return m_textureId; }
    void setTextureId(unsigned id) { m_textureId = id; }
    void setPremultipliedAlpha(bool premultipliedAlpha) { m_premultipliedAlpha = premultipliedAlpha; }
    void setFlipped(bool flipped) { m_flipped = flipped; }
    void setUVRect(const FloatRect& rect) { m_uvRect = rect; }

private:
    explicit CCTextureLayerImpl(int);

    virtual const char* layerTypeAsString() const OVERRIDE { return "TextureLayer"; }

    unsigned m_textureId;
    CCResourceProvider::ResourceId m_externalTextureResource;
    bool m_premultipliedAlpha;
    bool m_flipped;
    FloatRect m_uvRect;
};

}

#endif // CCTextureLayerImpl_h
