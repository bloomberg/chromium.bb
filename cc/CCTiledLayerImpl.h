// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CCTiledLayerImpl_h
#define CCTiledLayerImpl_h

#include "CCLayerImpl.h"
#include <public/WebTransformationMatrix.h>

namespace cc {

class CCLayerTilingData;
class DrawableTile;

class CCTiledLayerImpl : public CCLayerImpl {
public:
    static PassOwnPtr<CCTiledLayerImpl> create(int id)
    {
        return adoptPtr(new CCTiledLayerImpl(id));
    }
    virtual ~CCTiledLayerImpl();

    virtual void appendQuads(CCQuadSink&, CCAppendQuadsData&) OVERRIDE;

    virtual CCResourceProvider::ResourceId contentsResourceId() const OVERRIDE;

    virtual void dumpLayerProperties(std::string*, int indent) const OVERRIDE;

    void setSkipsDraw(bool skipsDraw) { m_skipsDraw = skipsDraw; }
    void setTilingData(const CCLayerTilingData& tiler);
    void pushTileProperties(int, int, CCResourceProvider::ResourceId, const IntRect& opaqueRect);

    void setContentsSwizzled(bool contentsSwizzled) { m_contentsSwizzled = contentsSwizzled; }
    bool contentsSwizzled() const { return m_contentsSwizzled; }

    virtual Region visibleContentOpaqueRegion() const OVERRIDE;
    virtual void didLoseContext() OVERRIDE;

protected:
    explicit CCTiledLayerImpl(int id);
    // Exposed for testing.
    bool hasTileAt(int, int) const;
    bool hasTextureIdForTileAt(int, int) const;

private:

    virtual const char* layerTypeAsString() const OVERRIDE { return "ContentLayer"; }

    DrawableTile* tileAt(int, int) const;
    DrawableTile* createTile(int, int);

    bool m_skipsDraw;
    bool m_contentsSwizzled;

    OwnPtr<CCLayerTilingData> m_tiler;
};

}

#endif // CCTiledLayerImpl_h
