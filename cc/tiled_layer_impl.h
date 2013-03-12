// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TILED_LAYER_IMPL_H_
#define CC_TILED_LAYER_IMPL_H_

#include "cc/cc_export.h"
#include "cc/layer_impl.h"

namespace cc {

class LayerTilingData;
class DrawableTile;

class CC_EXPORT TiledLayerImpl : public LayerImpl {
public:
    static scoped_ptr<TiledLayerImpl> Create(LayerTreeImpl* treeImpl, int id)
    {
        return make_scoped_ptr(new TiledLayerImpl(treeImpl, id));
    }
    virtual ~TiledLayerImpl();

    virtual scoped_ptr<LayerImpl> CreateLayerImpl(LayerTreeImpl*) OVERRIDE;
    virtual void PushPropertiesTo(LayerImpl*) OVERRIDE;

    virtual void AppendQuads(QuadSink* quad_sink,
                             AppendQuadsData* append_quads_data) OVERRIDE;

    virtual ResourceProvider::ResourceId ContentsResourceId() const OVERRIDE;

    virtual void DumpLayerProperties(std::string*, int indent) const OVERRIDE;

    void setSkipsDraw(bool skipsDraw) { m_skipsDraw = skipsDraw; }
    void setTilingData(const LayerTilingData& tiler);
    void pushTileProperties(int, int, ResourceProvider::ResourceId, const gfx::Rect& opaqueRect, bool contentsSwizzled);
    void pushInvalidTile(int, int);

    virtual Region VisibleContentOpaqueRegion() const OVERRIDE;
    virtual void DidLoseOutputSurface() OVERRIDE;

protected:
    TiledLayerImpl(LayerTreeImpl* treeImpl, int id);
    // Exposed for testing.
    bool hasTileAt(int, int) const;
    bool hasResourceIdForTileAt(int, int) const;

    virtual void GetDebugBorderProperties(SkColor*, float* width) const OVERRIDE;

private:

    virtual const char* LayerTypeAsString() const OVERRIDE;

    DrawableTile* tileAt(int, int) const;
    DrawableTile* createTile(int, int);

    bool m_skipsDraw;

    scoped_ptr<LayerTilingData> m_tiler;
};

}

#endif  // CC_TILED_LAYER_IMPL_H_
