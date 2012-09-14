// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TiledLayerChromium_h
#define TiledLayerChromium_h

#if USE(ACCELERATED_COMPOSITING)

#include "CCLayerTilingData.h"
#include "LayerChromium.h"
#include "LayerTextureUpdater.h"

namespace cc {
class UpdatableTile;

class TiledLayerChromium : public LayerChromium {
public:
    enum TilingOption { AlwaysTile, NeverTile, AutoTile };

    virtual ~TiledLayerChromium();

    virtual void setIsMask(bool) OVERRIDE;

    virtual void pushPropertiesTo(CCLayerImpl*) OVERRIDE;

    virtual bool drawsContent() const OVERRIDE;
    virtual bool needsContentsScale() const OVERRIDE;

    virtual IntSize contentBounds() const OVERRIDE;

    virtual void setNeedsDisplayRect(const FloatRect&) OVERRIDE;

    virtual void setUseLCDText(bool) OVERRIDE;

    virtual void setLayerTreeHost(CCLayerTreeHost*) OVERRIDE;

    virtual void setTexturePriorities(const CCPriorityCalculator&) OVERRIDE;

    virtual Region visibleContentOpaqueRegion() const OVERRIDE;

    virtual void update(CCTextureUpdateQueue&, const CCOcclusionTracker*, CCRenderingStats&) OVERRIDE;

protected:
    TiledLayerChromium();

    void updateTileSizeAndTilingOption();
    void updateBounds();

    // Exposed to subclasses for testing.
    void setTileSize(const IntSize&);
    void setTextureFormat(GC3Denum textureFormat) { m_textureFormat = textureFormat; }
    void setBorderTexelOption(CCLayerTilingData::BorderTexelOption);
    void setSampledTexelFormat(LayerTextureUpdater::SampledTexelFormat sampledTexelFormat) { m_sampledTexelFormat = sampledTexelFormat; }
    size_t numPaintedTiles() { return m_tiler->tiles().size(); }

    virtual LayerTextureUpdater* textureUpdater() const = 0;
    virtual void createTextureUpdaterIfNeeded() = 0;

    // Set invalidations to be potentially repainted during update().
    void invalidateContentRect(const IntRect& contentRect);

    // Reset state on tiles that will be used for updating the layer.
    void resetUpdateState();

    // After preparing an update, returns true if more painting is needed.
    bool needsIdlePaint();
    IntRect idlePaintRect();

    bool skipsDraw() const { return m_skipsDraw; }

    // Virtual for testing
    virtual CCPrioritizedTextureManager* textureManager() const;

private:
    virtual PassOwnPtr<CCLayerImpl> createCCLayerImpl() OVERRIDE;

    void createTilerIfNeeded();
    void setTilingOption(TilingOption);

    bool tileOnlyNeedsPartialUpdate(UpdatableTile*);
    bool tileNeedsBufferedUpdate(UpdatableTile*);

    void markOcclusionsAndRequestTextures(int left, int top, int right, int bottom, const CCOcclusionTracker*);

    bool updateTiles(int left, int top, int right, int bottom, CCTextureUpdateQueue&, const CCOcclusionTracker*, CCRenderingStats&, bool& didPaint);
    bool haveTexturesForTiles(int left, int top, int right, int bottom, bool ignoreOcclusions);
    IntRect markTilesForUpdate(int left, int top, int right, int bottom, bool ignoreOcclusions);
    void updateTileTextures(const IntRect& paintRect, int left, int top, int right, int bottom, CCTextureUpdateQueue&, const CCOcclusionTracker*, CCRenderingStats&);

    UpdatableTile* tileAt(int, int) const;
    UpdatableTile* createTile(int, int);

    GC3Denum m_textureFormat;
    bool m_skipsDraw;
    bool m_failedUpdate;
    LayerTextureUpdater::SampledTexelFormat m_sampledTexelFormat;

    TilingOption m_tilingOption;
    OwnPtr<CCLayerTilingData> m_tiler;
};

}
#endif // USE(ACCELERATED_COMPOSITING)

#endif
