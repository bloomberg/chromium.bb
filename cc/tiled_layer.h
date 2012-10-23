// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TiledLayerChromium_h
#define TiledLayerChromium_h

#include "cc/layer.h"
#include "cc/layer_updater.h"
#include "cc/layer_tiling_data.h"

namespace cc {
class UpdatableTile;

class TiledLayer : public Layer {
public:
    enum TilingOption { AlwaysTile, NeverTile, AutoTile };

    virtual void setIsMask(bool) OVERRIDE;

    virtual void pushPropertiesTo(LayerImpl*) OVERRIDE;

    virtual bool drawsContent() const OVERRIDE;
    virtual bool needsContentsScale() const OVERRIDE;

    virtual IntSize contentBounds() const OVERRIDE;

    virtual void setNeedsDisplayRect(const FloatRect&) OVERRIDE;

    virtual void setUseLCDText(bool) OVERRIDE;

    virtual void setLayerTreeHost(LayerTreeHost*) OVERRIDE;

    virtual void setTexturePriorities(const PriorityCalculator&) OVERRIDE;

    virtual Region visibleContentOpaqueRegion() const OVERRIDE;

    virtual void update(TextureUpdateQueue&, const OcclusionTracker*, RenderingStats&) OVERRIDE;

protected:
    TiledLayer();
    virtual ~TiledLayer();

    void updateTileSizeAndTilingOption();
    void updateBounds();

    // Exposed to subclasses for testing.
    void setTileSize(const IntSize&);
    void setTextureFormat(GLenum textureFormat) { m_textureFormat = textureFormat; }
    void setBorderTexelOption(LayerTilingData::BorderTexelOption);
    size_t numPaintedTiles() { return m_tiler->tiles().size(); }

    virtual LayerUpdater* updater() const = 0;
    virtual void createUpdaterIfNeeded() = 0;

    // Set invalidations to be potentially repainted during update().
    void invalidateContentRect(const IntRect& contentRect);

    // Reset state on tiles that will be used for updating the layer.
    void resetUpdateState();

    // After preparing an update, returns true if more painting is needed.
    bool needsIdlePaint();
    IntRect idlePaintRect();

    bool skipsDraw() const { return m_skipsDraw; }

    // Virtual for testing
    virtual PrioritizedTextureManager* textureManager() const;

private:
    virtual scoped_ptr<LayerImpl> createLayerImpl() OVERRIDE;

    void createTilerIfNeeded();
    void setTilingOption(TilingOption);

    bool tileOnlyNeedsPartialUpdate(UpdatableTile*);
    bool tileNeedsBufferedUpdate(UpdatableTile*);

    void markOcclusionsAndRequestTextures(int left, int top, int right, int bottom, const OcclusionTracker*);

    bool updateTiles(int left, int top, int right, int bottom, TextureUpdateQueue&, const OcclusionTracker*, RenderingStats&, bool& didPaint);
    bool haveTexturesForTiles(int left, int top, int right, int bottom, bool ignoreOcclusions);
    IntRect markTilesForUpdate(int left, int top, int right, int bottom, bool ignoreOcclusions);
    void updateTileTextures(const IntRect& paintRect, int left, int top, int right, int bottom, TextureUpdateQueue&, const OcclusionTracker*, RenderingStats&);

    UpdatableTile* tileAt(int, int) const;
    UpdatableTile* createTile(int, int);

    GLenum m_textureFormat;
    bool m_skipsDraw;
    bool m_failedUpdate;

    TilingOption m_tilingOption;
    scoped_ptr<LayerTilingData> m_tiler;
};

}
#endif
