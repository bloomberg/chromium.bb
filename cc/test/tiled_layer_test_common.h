// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_TILED_LAYER_TEST_COMMON_H_
#define CC_TEST_TILED_LAYER_TEST_COMMON_H_

#include "cc/layer_updater.h"
#include "cc/prioritized_resource.h"
#include "cc/region.h"
#include "cc/resource_provider.h"
#include "cc/resource_update_queue.h"
#include "cc/texture_copier.h"
#include "cc/texture_uploader.h"
#include "cc/tiled_layer.h"
#include "cc/tiled_layer_impl.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"

namespace cc {

class FakeTiledLayer;

class FakeLayerUpdater : public cc::LayerUpdater {
public:
    class Resource : public cc::LayerUpdater::Resource {
    public:
        Resource(FakeLayerUpdater*, scoped_ptr<cc::PrioritizedResource>);
        virtual ~Resource();

        virtual void Update(cc::ResourceUpdateQueue* queue, gfx::Rect source_rect, gfx::Vector2d dest_offset, bool partial_update, cc::RenderingStats* stats) OVERRIDE;

    private:
        FakeLayerUpdater* m_layer;
        SkBitmap m_bitmap;
    };

    FakeLayerUpdater();

    virtual scoped_ptr<cc::LayerUpdater::Resource> CreateResource(cc::PrioritizedResourceManager* resource) OVERRIDE;

    virtual void PrepareToUpdate(gfx::Rect content_rect, gfx::Size tile_size, float contents_width_scale, float contents_height_scale, gfx::Rect* resulting_opaque_rect, RenderingStats* stats) OVERRIDE;
    // Sets the rect to invalidate during the next call to prepareToUpdate(). After the next
    // call to prepareToUpdate() the rect is reset.
    void setRectToInvalidate(const gfx::Rect&, FakeTiledLayer*);
    // Last rect passed to prepareToUpdate().
    const gfx::Rect& lastUpdateRect()  const { return m_lastUpdateRect; }

    // Number of times prepareToUpdate has been invoked.
    int prepareCount() const { return m_prepareCount; }
    void clearPrepareCount() { m_prepareCount = 0; }

    // Number of times update() has been invoked on a texture.
    int updateCount() const { return m_updateCount; }
    void clearUpdateCount() { m_updateCount = 0; }
    void update() { m_updateCount++; }

    void setOpaquePaintRect(const gfx::Rect& opaquePaintRect) { m_opaquePaintRect = opaquePaintRect; }

protected:
    virtual ~FakeLayerUpdater();

private:
    int m_prepareCount;
    int m_updateCount;
    gfx::Rect m_rectToInvalidate;
    gfx::Rect m_lastUpdateRect;
    gfx::Rect m_opaquePaintRect;
    scoped_refptr<FakeTiledLayer> m_layer;
};

class FakeTiledLayerImpl : public cc::TiledLayerImpl {
public:
    FakeTiledLayerImpl(LayerTreeImpl* treeImpl, int id);
    virtual ~FakeTiledLayerImpl();

    using cc::TiledLayerImpl::hasTileAt;
    using cc::TiledLayerImpl::hasResourceIdForTileAt;
};

class FakeTiledLayer : public cc::TiledLayer {
public:
    explicit FakeTiledLayer(cc::PrioritizedResourceManager*);

    static gfx::Size tileSize() { return gfx::Size(100, 100); }

    using cc::TiledLayer::InvalidateContentRect;
    using cc::TiledLayer::NeedsIdlePaint;
    using cc::TiledLayer::SkipsDraw;
    using cc::TiledLayer::NumPaintedTiles;
    using cc::TiledLayer::IdlePaintRect;

    virtual void SetNeedsDisplayRect(const gfx::RectF&) OVERRIDE;
    const gfx::RectF& lastNeedsDisplayRect() const { return m_lastNeedsDisplayRect; }

    virtual void SetTexturePriorities(const cc::PriorityCalculator&) OVERRIDE;

    virtual cc::PrioritizedResourceManager* ResourceManager() const OVERRIDE;
    FakeLayerUpdater* fakeLayerUpdater() { return m_fakeUpdater.get(); }
    gfx::RectF updateRect() { return update_rect_; }

    // Simulate calcDrawProperties.
    void updateContentsScale(float idealContentsScale);

protected:
    virtual cc::LayerUpdater* Updater() const OVERRIDE;
    virtual void CreateUpdaterIfNeeded() OVERRIDE { }
    virtual ~FakeTiledLayer();

private:
    scoped_refptr<FakeLayerUpdater> m_fakeUpdater;
    cc::PrioritizedResourceManager* m_resourceManager;
    gfx::RectF m_lastNeedsDisplayRect;
};

class FakeTiledLayerWithScaledBounds : public FakeTiledLayer {
public:
    explicit FakeTiledLayerWithScaledBounds(cc::PrioritizedResourceManager*);

    void setContentBounds(const gfx::Size& contentBounds);
    virtual void CalculateContentsScale(float idealContentsScale,
                                        bool animatingTransformToScreen,
                                        float* contentsScaleX,
                                        float* contentsScaleY,
                                        gfx::Size* contentBounds) OVERRIDE;

protected:
    virtual ~FakeTiledLayerWithScaledBounds();
    gfx::Size m_forcedContentBounds;
};

}  // namespace cc

#endif  // CC_TEST_TILED_LAYER_TEST_COMMON_H_
