// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TILED_LAYER_H_
#define CC_TILED_LAYER_H_

#include "cc/cc_export.h"
#include "cc/contents_scaling_layer.h"
#include "cc/layer_tiling_data.h"

namespace cc {
class LayerUpdater;
class PrioritizedResourceManager;
class PrioritizedResource;
class UpdatableTile;

class CC_EXPORT TiledLayer : public ContentsScalingLayer {
public:
    enum TilingOption { AlwaysTile, NeverTile, AutoTile };

    virtual void setIsMask(bool) OVERRIDE;

    virtual void pushPropertiesTo(LayerImpl*) OVERRIDE;

    virtual bool blocksPendingCommit() const OVERRIDE;

    virtual bool drawsContent() const OVERRIDE;

    virtual void setNeedsDisplayRect(const gfx::RectF&) OVERRIDE;

    virtual void setLayerTreeHost(LayerTreeHost*) OVERRIDE;

    virtual void setTexturePriorities(const PriorityCalculator&) OVERRIDE;

    virtual Region visibleContentOpaqueRegion() const OVERRIDE;

    virtual void update(ResourceUpdateQueue&, const OcclusionTracker*, RenderingStats&) OVERRIDE;

protected:
    TiledLayer();
    virtual ~TiledLayer();

    void updateTileSizeAndTilingOption();
    void updateBounds();

    // Exposed to subclasses for testing.
    void setTileSize(const gfx::Size&);
    void setTextureFormat(unsigned textureFormat) { m_textureFormat = textureFormat; }
    void setBorderTexelOption(LayerTilingData::BorderTexelOption);
    size_t numPaintedTiles() { return m_tiler->tiles().size(); }

    virtual LayerUpdater* updater() const = 0;
    virtual void createUpdaterIfNeeded() = 0;

    // Set invalidations to be potentially repainted during update().
    void invalidateContentRect(const gfx::Rect& contentRect);

    // Reset state on tiles that will be used for updating the layer.
    void resetUpdateState();

    // After preparing an update, returns true if more painting is needed.
    bool needsIdlePaint();
    gfx::Rect idlePaintRect();

    bool skipsDraw() const { return m_skipsDraw; }

    // Virtual for testing
    virtual PrioritizedResourceManager* resourceManager() const;
    const LayerTilingData* tilerForTesting() const { return m_tiler.get(); }
    const PrioritizedResource* resourceAtForTesting(int, int) const;

private:
    virtual scoped_ptr<LayerImpl> createLayerImpl(LayerTreeImpl* treeImpl) OVERRIDE;

    void createTilerIfNeeded();
    void setTilingOption(TilingOption);

    bool tileOnlyNeedsPartialUpdate(UpdatableTile*);
    bool tileNeedsBufferedUpdate(UpdatableTile*);

    void markOcclusionsAndRequestTextures(int left, int top, int right, int bottom, const OcclusionTracker*);

    bool updateTiles(int left, int top, int right, int bottom, ResourceUpdateQueue&, const OcclusionTracker*, RenderingStats&, bool& didPaint);
    bool haveTexturesForTiles(int left, int top, int right, int bottom, bool ignoreOcclusions);
    gfx::Rect markTilesForUpdate(int left, int top, int right, int bottom, bool ignoreOcclusions);
    void updateTileTextures(const gfx::Rect& paintRect, int left, int top, int right, int bottom, ResourceUpdateQueue&, const OcclusionTracker*, RenderingStats&);
    void updateScrollPrediction();

    UpdatableTile* tileAt(int, int) const;
    UpdatableTile* createTile(int, int);

    bool isSmallAnimatedLayer() const;

    unsigned m_textureFormat;
    bool m_skipsDraw;
    bool m_failedUpdate;

    // Used for predictive painting.
    gfx::Vector2d m_predictedScroll;
    gfx::Rect m_predictedVisibleRect;
    gfx::Rect m_previousVisibleRect;
    gfx::Size m_previousContentBounds;

    TilingOption m_tilingOption;
    scoped_ptr<LayerTilingData> m_tiler;
};

}
#endif  // CC_TILED_LAYER_H_
