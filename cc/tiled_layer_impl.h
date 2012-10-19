// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CCTiledLayerImpl_h
#define CCTiledLayerImpl_h

#include "CCLayerImpl.h"
#include <public/WebTransformationMatrix.h>

namespace cc {

class LayerTilingData;
class DrawableTile;

class TiledLayerImpl : public LayerImpl {
public:
    static scoped_ptr<TiledLayerImpl> create(int id)
    {
        return make_scoped_ptr(new TiledLayerImpl(id));
    }
    virtual ~TiledLayerImpl();

    virtual void appendQuads(QuadSink&, AppendQuadsData&) OVERRIDE;

    virtual ResourceProvider::ResourceId contentsResourceId() const OVERRIDE;

    virtual void dumpLayerProperties(std::string*, int indent) const OVERRIDE;

    void setSkipsDraw(bool skipsDraw) { m_skipsDraw = skipsDraw; }
    void setTilingData(const LayerTilingData& tiler);
    void pushTileProperties(int, int, ResourceProvider::ResourceId, const IntRect& opaqueRect);
    void pushInvalidTile(int, int);

    void setContentsSwizzled(bool contentsSwizzled) { m_contentsSwizzled = contentsSwizzled; }
    bool contentsSwizzled() const { return m_contentsSwizzled; }

    virtual Region visibleContentOpaqueRegion() const OVERRIDE;
    virtual void didLoseContext() OVERRIDE;

protected:
    explicit TiledLayerImpl(int id);
    // Exposed for testing.
    bool hasTileAt(int, int) const;
    bool hasResourceIdForTileAt(int, int) const;

private:

    virtual const char* layerTypeAsString() const OVERRIDE;

    DrawableTile* tileAt(int, int) const;
    DrawableTile* createTile(int, int);

    bool m_skipsDraw;
    bool m_contentsSwizzled;

    scoped_ptr<LayerTilingData> m_tiler;
};

}

#endif // CCTiledLayerImpl_h
