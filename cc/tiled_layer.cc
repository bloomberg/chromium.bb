// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/tiled_layer.h"

#include "base/auto_reset.h"
#include "base/basictypes.h"
#include "build/build_config.h"
#include "cc/layer_impl.h"
#include "cc/layer_tree_host.h"
#include "cc/layer_updater.h"
#include "cc/overdraw_metrics.h"
#include "cc/prioritized_resource.h"
#include "cc/priority_calculator.h"
#include "cc/tiled_layer_impl.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "ui/gfx/rect_conversions.h"

namespace cc {

// Maximum predictive expansion of the visible area.
static const int maxPredictiveTilesCount = 2;

// Number of rows/columns of tiles to pre-paint.
// We should increase these further as all textures are
// prioritized and we insure performance doesn't suffer.
static const int prepaintRows = 4;
static const int prepaintColumns = 2;


class UpdatableTile : public LayerTilingData::Tile {
public:
    static scoped_ptr<UpdatableTile> create(scoped_ptr<LayerUpdater::Resource> updaterResource)
    {
        return make_scoped_ptr(new UpdatableTile(updaterResource.Pass()));
    }

    LayerUpdater::Resource* updaterResource() { return m_updaterResource.get(); }
    PrioritizedResource* managedResource() { return m_updaterResource->texture(); }

    bool isDirty() const { return !dirtyRect.IsEmpty(); }

    // Reset update state for the current frame. This should occur before painting
    // for all layers. Since painting one layer can invalidate another layer
    // after it has already painted, mark all non-dirty tiles as valid before painting
    // such that invalidations during painting won't prevent them from being pushed.
    void resetUpdateState()
    {
        updateRect = gfx::Rect();
        occluded = false;
        partialUpdate = false;
        validForFrame = !isDirty();
    }

    // This promises to update the tile and therefore also guarantees the tile
    // will be valid for this frame. dirtyRect is copied into updateRect so
    // we can continue to track re-entrant invalidations that occur during painting.
    void markForUpdate()
    {
        validForFrame = true;
        updateRect = dirtyRect;
        dirtyRect = gfx::Rect();
    }

    gfx::Rect dirtyRect;
    gfx::Rect updateRect;
    bool partialUpdate;
    bool validForFrame;
    bool occluded;

private:
    explicit UpdatableTile(scoped_ptr<LayerUpdater::Resource> updaterResource)
        : partialUpdate(false)
        , validForFrame(false)
        , occluded(false)
        , m_updaterResource(updaterResource.Pass())
    {
    }

    scoped_ptr<LayerUpdater::Resource> m_updaterResource;

    DISALLOW_COPY_AND_ASSIGN(UpdatableTile);
};

TiledLayer::TiledLayer()
    : ContentsScalingLayer()
    , m_textureFormat(GL_INVALID_ENUM)
    , m_skipsDraw(false)
    , m_failedUpdate(false)
    , m_tilingOption(AutoTile)
{
    m_tiler = LayerTilingData::create(gfx::Size(), LayerTilingData::HasBorderTexels);
}

TiledLayer::~TiledLayer()
{
}

scoped_ptr<LayerImpl> TiledLayer::createLayerImpl(LayerTreeImpl* treeImpl)
{
    return TiledLayerImpl::create(treeImpl, id()).PassAs<LayerImpl>();
}

void TiledLayer::updateTileSizeAndTilingOption()
{
    DCHECK(layerTreeHost());

    gfx::Size defaultTileSize = layerTreeHost()->settings().defaultTileSize;
    gfx::Size maxUntiledLayerSize = layerTreeHost()->settings().maxUntiledLayerSize;
    int layerWidth = contentBounds().width();
    int layerHeight = contentBounds().height();

    gfx::Size tileSize(std::min(defaultTileSize.width(), layerWidth), std::min(defaultTileSize.height(), layerHeight));

    // Tile if both dimensions large, or any one dimension large and the other
    // extends into a second tile but the total layer area isn't larger than that
    // of the largest possible untiled layer. This heuristic allows for long skinny layers
    // (e.g. scrollbars) that are Nx1 tiles to minimize wasted texture space but still avoids
    // creating very large tiles.
    bool anyDimensionLarge = layerWidth > maxUntiledLayerSize.width() || layerHeight > maxUntiledLayerSize.height();
    bool anyDimensionOneTile = (layerWidth <= defaultTileSize.width() || layerHeight <= defaultTileSize.height())
                                      && (layerWidth * layerHeight) <= (maxUntiledLayerSize.width() * maxUntiledLayerSize.height());
    bool autoTiled = anyDimensionLarge && !anyDimensionOneTile;

    bool isTiled;
    if (m_tilingOption == AlwaysTile)
        isTiled = true;
    else if (m_tilingOption == NeverTile)
        isTiled = false;
    else
        isTiled = autoTiled;

    gfx::Size requestedSize = isTiled ? tileSize : contentBounds();
    const int maxSize = layerTreeHost()->rendererCapabilities().maxTextureSize;
    requestedSize.ClampToMax(gfx::Size(maxSize, maxSize));
    setTileSize(requestedSize);
}

void TiledLayer::updateBounds()
{
    gfx::Size oldBounds = m_tiler->bounds();
    gfx::Size newBounds = contentBounds();
    if (oldBounds == newBounds)
        return;
    m_tiler->setBounds(newBounds);

    // Invalidate any areas that the new bounds exposes.
    Region oldRegion = gfx::Rect(gfx::Point(), oldBounds);
    Region newRegion = gfx::Rect(gfx::Point(), newBounds);
    newRegion.Subtract(oldRegion);
    for (Region::Iterator newRects(newRegion); newRects.has_rect(); newRects.next())
        invalidateContentRect(newRects.rect());
}

void TiledLayer::setTileSize(const gfx::Size& size)
{
    m_tiler->setTileSize(size);
}

void TiledLayer::setBorderTexelOption(LayerTilingData::BorderTexelOption borderTexelOption)
{
    m_tiler->setBorderTexelOption(borderTexelOption);
}

bool TiledLayer::drawsContent() const
{
    if (!ContentsScalingLayer::drawsContent())
        return false;

    bool hasMoreThanOneTile = m_tiler->numTilesX() > 1 || m_tiler->numTilesY() > 1;
    if (m_tilingOption == NeverTile && hasMoreThanOneTile)
        return false;

    return true;
}

void TiledLayer::setTilingOption(TilingOption tilingOption)
{
    m_tilingOption = tilingOption;
}

void TiledLayer::setIsMask(bool isMask)
{
    setTilingOption(isMask ? NeverTile : AutoTile);
}

void TiledLayer::pushPropertiesTo(LayerImpl* layer)
{
    ContentsScalingLayer::pushPropertiesTo(layer);

    TiledLayerImpl* tiledLayer = static_cast<TiledLayerImpl*>(layer);

    tiledLayer->setSkipsDraw(m_skipsDraw);
    tiledLayer->setTilingData(*m_tiler);
    std::vector<UpdatableTile*> invalidTiles;

    for (LayerTilingData::TileMap::const_iterator iter = m_tiler->tiles().begin(); iter != m_tiler->tiles().end(); ++iter) {
        int i = iter->first.first;
        int j = iter->first.second;
        UpdatableTile* tile = static_cast<UpdatableTile*>(iter->second);
        // FIXME: This should not ever be null.
        if (!tile)
            continue;

        if (!tile->managedResource()->haveBackingTexture()) {
            // Evicted tiles get deleted from both layers
            invalidTiles.push_back(tile);
            continue;
        }

        if (!tile->validForFrame) {
            // Invalidated tiles are set so they can get different debug colors.
            tiledLayer->pushInvalidTile(i, j);
            continue;
        }

        tiledLayer->pushTileProperties(i, j, tile->managedResource()->resourceId(), tile->opaqueRect(), tile->managedResource()->contentsSwizzled());
    }
    for (std::vector<UpdatableTile*>::const_iterator iter = invalidTiles.begin(); iter != invalidTiles.end(); ++iter)
        m_tiler->takeTile((*iter)->i(), (*iter)->j());
}

bool TiledLayer::blocksPendingCommit() const
{
    return true;
}

PrioritizedResourceManager* TiledLayer::resourceManager() const
{
    if (!layerTreeHost())
        return 0;
    return layerTreeHost()->contentsTextureManager();
}

const PrioritizedResource* TiledLayer::resourceAtForTesting(int i, int j) const
{
    UpdatableTile* tile = tileAt(i, j);
    if (!tile)
        return 0;
    return tile->managedResource();
}

void TiledLayer::setLayerTreeHost(LayerTreeHost* host)
{
    if (host && host != layerTreeHost()) {
        for (LayerTilingData::TileMap::const_iterator iter = m_tiler->tiles().begin(); iter != m_tiler->tiles().end(); ++iter) {
            UpdatableTile* tile = static_cast<UpdatableTile*>(iter->second);
            // FIXME: This should not ever be null.
            if (!tile)
                continue;
            tile->managedResource()->setTextureManager(host->contentsTextureManager());
        }
    }
    ContentsScalingLayer::setLayerTreeHost(host);
}

UpdatableTile* TiledLayer::tileAt(int i, int j) const
{
    return static_cast<UpdatableTile*>(m_tiler->tileAt(i, j));
}

UpdatableTile* TiledLayer::createTile(int i, int j)
{
    createUpdaterIfNeeded();

    scoped_ptr<UpdatableTile> tile(UpdatableTile::create(updater()->createResource(resourceManager())));
    tile->managedResource()->setDimensions(m_tiler->tileSize(), m_textureFormat);

    UpdatableTile* addedTile = tile.get();
    m_tiler->addTile(tile.PassAs<LayerTilingData::Tile>(), i, j);

    addedTile->dirtyRect = m_tiler->tileRect(addedTile);

    // Temporary diagnostic crash.
    CHECK(addedTile);
    CHECK(tileAt(i, j));

    return addedTile;
}

void TiledLayer::setNeedsDisplayRect(const gfx::RectF& dirtyRect)
{
    invalidateContentRect(layerRectToContentRect(dirtyRect));
    ContentsScalingLayer::setNeedsDisplayRect(dirtyRect);
}

void TiledLayer::invalidateContentRect(const gfx::Rect& contentRect)
{
    updateBounds();
    if (m_tiler->isEmpty() || contentRect.IsEmpty() || m_skipsDraw)
        return;

    for (LayerTilingData::TileMap::const_iterator iter = m_tiler->tiles().begin(); iter != m_tiler->tiles().end(); ++iter) {
        UpdatableTile* tile = static_cast<UpdatableTile*>(iter->second);
        DCHECK(tile);
        // FIXME: This should not ever be null.
        if (!tile)
            continue;
        gfx::Rect bound = m_tiler->tileRect(tile);
        bound.Intersect(contentRect);
        tile->dirtyRect.Union(bound);
    }
}

// Returns true if tile is dirty and only part of it needs to be updated.
bool TiledLayer::tileOnlyNeedsPartialUpdate(UpdatableTile* tile)
{
    return !tile->dirtyRect.Contains(m_tiler->tileRect(tile)) && tile->managedResource()->haveBackingTexture();
}

bool TiledLayer::updateTiles(int left, int top, int right, int bottom, ResourceUpdateQueue& queue, const OcclusionTracker* occlusion, RenderingStats& stats, bool& didPaint)
{
    didPaint = false;
    createUpdaterIfNeeded();

    bool ignoreOcclusions = !occlusion;
    if (!haveTexturesForTiles(left, top, right, bottom, ignoreOcclusions)) {
        m_failedUpdate = true;
        return false;
    }

    gfx::Rect paintRect = markTilesForUpdate(left, top, right, bottom, ignoreOcclusions);

    if (occlusion)
        occlusion->overdrawMetrics().didPaint(paintRect);

    if (paintRect.IsEmpty())
        return true;

    didPaint = true;
    updateTileTextures(paintRect, left, top, right, bottom, queue, occlusion, stats);
    return true;
}

void TiledLayer::markOcclusionsAndRequestTextures(int left, int top, int right, int bottom, const OcclusionTracker* occlusion)
{
    // There is some difficult dependancies between occlusions, recording occlusion metrics
    // and requesting memory so those are encapsulated in this function:
    // - We only want to call requestLate on unoccluded textures (to preserve
    //   memory for other layers when near OOM).
    // - We only want to record occlusion metrics if all memory requests succeed.

    int occludedTileCount = 0;
    bool succeeded = true;
    for (int j = top; j <= bottom; ++j) {
        for (int i = left; i <= right; ++i) {
            UpdatableTile* tile = tileAt(i, j);
            DCHECK(tile); // Did setTexturePriorities get skipped?
            // FIXME: This should not ever be null.
            if (!tile)
                continue;
            DCHECK(!tile->occluded); // Did resetUpdateState get skipped? Are we doing more than one occlusion pass?
            gfx::Rect visibleTileRect = gfx::IntersectRects(m_tiler->tileBounds(i, j), visibleContentRect());
            if (occlusion && occlusion->occluded(renderTarget(), visibleTileRect, drawTransform(), drawTransformIsAnimating(), drawableContentRect())) {
                tile->occluded = true;
                occludedTileCount++;
            } else {
                succeeded &= tile->managedResource()->requestLate();
            }
        }
    }

    if (!succeeded)
        return;
    if (occlusion)
        occlusion->overdrawMetrics().didCullTilesForUpload(occludedTileCount);
}

bool TiledLayer::haveTexturesForTiles(int left, int top, int right, int bottom, bool ignoreOcclusions)
{
    for (int j = top; j <= bottom; ++j) {
        for (int i = left; i <= right; ++i) {
            UpdatableTile* tile = tileAt(i, j);
            DCHECK(tile); // Did setTexturePriorites get skipped?
            // FIXME: This should not ever be null.
            if (!tile)
                continue;

            // Ensure the entire tile is dirty if we don't have the texture.
            if (!tile->managedResource()->haveBackingTexture())
                tile->dirtyRect = m_tiler->tileRect(tile);

            // If using occlusion and the visible region of the tile is occluded,
            // don't reserve a texture or update the tile.
            if (tile->occluded && !ignoreOcclusions)
                continue;

            if (!tile->managedResource()->canAcquireBackingTexture())
                return false;
        }
    }
    return true;
}

gfx::Rect TiledLayer::markTilesForUpdate(int left, int top, int right, int bottom, bool ignoreOcclusions)
{
  gfx::Rect paintRect;
    for (int j = top; j <= bottom; ++j) {
        for (int i = left; i <= right; ++i) {
            UpdatableTile* tile = tileAt(i, j);
            DCHECK(tile); // Did setTexturePriorites get skipped?
            // FIXME: This should not ever be null.
            if (!tile)
                continue;
            if (tile->occluded && !ignoreOcclusions)
                continue;
            // FIXME: Decide if partial update should be allowed based on cost
            // of update. https://bugs.webkit.org/show_bug.cgi?id=77376
            if (tile->isDirty() && layerTreeHost() && layerTreeHost()->bufferedUpdates()) {
                // If we get a partial update, we use the same texture, otherwise return the
                // current texture backing, so we don't update visible textures non-atomically.
                // If the current backing is in-use, it won't be deleted until after the commit
                // as the texture manager will not allow deletion or recycling of in-use textures.
                if (tileOnlyNeedsPartialUpdate(tile) && layerTreeHost()->requestPartialTextureUpdate())
                    tile->partialUpdate = true;
                else {
                    tile->dirtyRect = m_tiler->tileRect(tile);
                    tile->managedResource()->returnBackingTexture();
                }
            }

            paintRect.Union(tile->dirtyRect);
            tile->markForUpdate();
        }
    }
    return paintRect;
}

void TiledLayer::updateTileTextures(const gfx::Rect& paintRect, int left, int top, int right, int bottom, ResourceUpdateQueue& queue, const OcclusionTracker* occlusion, RenderingStats& stats)
{
    // The updateRect should be in layer space. So we have to convert the paintRect from content space to layer space.
    float widthScale = bounds().width() / static_cast<float>(contentBounds().width());
    float heightScale = bounds().height() / static_cast<float>(contentBounds().height());
    m_updateRect = gfx::ScaleRect(paintRect, widthScale, heightScale);

    // Calling prepareToUpdate() calls into WebKit to paint, which may have the side
    // effect of disabling compositing, which causes our reference to the texture updater to be deleted.
    // However, we can't free the memory backing the SkCanvas until the paint finishes,
    // so we grab a local reference here to hold the updater alive until the paint completes.
    scoped_refptr<LayerUpdater> protector(updater());
    gfx::Rect paintedOpaqueRect;
    updater()->prepareToUpdate(paintRect, m_tiler->tileSize(), 1 / widthScale, 1 / heightScale, paintedOpaqueRect, stats);

    for (int j = top; j <= bottom; ++j) {
        for (int i = left; i <= right; ++i) {
            UpdatableTile* tile = tileAt(i, j);
            DCHECK(tile); // Did setTexturePriorites get skipped?
            // FIXME: This should not ever be null.
            if (!tile)
                continue;

            gfx::Rect tileRect = m_tiler->tileBounds(i, j);

            // Use updateRect as the above loop copied the dirty rect for this frame to updateRect.
            const gfx::Rect& dirtyRect = tile->updateRect;
            if (dirtyRect.IsEmpty())
                continue;

            // Save what was painted opaque in the tile. Keep the old area if the paint didn't touch it, and didn't paint some
            // other part of the tile opaque.
            gfx::Rect tilePaintedRect = gfx::IntersectRects(tileRect, paintRect);
            gfx::Rect tilePaintedOpaqueRect = gfx::IntersectRects(tileRect, paintedOpaqueRect);
            if (!tilePaintedRect.IsEmpty()) {
                gfx::Rect paintInsideTileOpaqueRect = gfx::IntersectRects(tile->opaqueRect(), tilePaintedRect);
                bool paintInsideTileOpaqueRectIsNonOpaque = !tilePaintedOpaqueRect.Contains(paintInsideTileOpaqueRect);
                bool opaquePaintNotInsideTileOpaqueRect = !tilePaintedOpaqueRect.IsEmpty() && !tile->opaqueRect().Contains(tilePaintedOpaqueRect);

                if (paintInsideTileOpaqueRectIsNonOpaque || opaquePaintNotInsideTileOpaqueRect)
                    tile->setOpaqueRect(tilePaintedOpaqueRect);
            }

            // sourceRect starts as a full-sized tile with border texels included.
            gfx::Rect sourceRect = m_tiler->tileRect(tile);
            sourceRect.Intersect(dirtyRect);
            // Paint rect not guaranteed to line up on tile boundaries, so
            // make sure that sourceRect doesn't extend outside of it.
            sourceRect.Intersect(paintRect);

            tile->updateRect = sourceRect;

            if (sourceRect.IsEmpty())
                continue;

            const gfx::Point anchor = m_tiler->tileRect(tile).origin();

            // Calculate tile-space rectangle to upload into.
            gfx::Vector2d destOffset = sourceRect.origin() - anchor;
            CHECK(destOffset.x() >= 0);
            CHECK(destOffset.y() >= 0);

            // Offset from paint rectangle to this tile's dirty rectangle.
            gfx::Vector2d paintOffset = sourceRect.origin() - paintRect.origin();
            CHECK(paintOffset.x() >= 0);
            CHECK(paintOffset.y() >= 0);
            CHECK(paintOffset.x() + sourceRect.width() <= paintRect.width());
            CHECK(paintOffset.y() + sourceRect.height() <= paintRect.height());

            tile->updaterResource()->update(queue, sourceRect, destOffset, tile->partialUpdate, stats);
            if (occlusion)
                occlusion->overdrawMetrics().didUpload(gfx::Transform(), sourceRect, tile->opaqueRect());

        }
    }
}

// This picks a small animated layer to be anything less than one viewport. This
// is specifically for page transitions which are viewport-sized layers. The extra
// tile of padding is due to these layers being slightly larger than the viewport
// in some cases.
bool TiledLayer::isSmallAnimatedLayer() const
{
    if (!drawTransformIsAnimating() && !screenSpaceTransformIsAnimating())
        return false;
    gfx::Size viewportSize = layerTreeHost() ? layerTreeHost()->deviceViewportSize() : gfx::Size();
    gfx::Rect contentRect(gfx::Point(), contentBounds());
    return contentRect.width() <= viewportSize.width() + m_tiler->tileSize().width()
        && contentRect.height() <= viewportSize.height() + m_tiler->tileSize().height();
}

namespace {
// FIXME: Remove this and make this based on distance once distance can be calculated
// for offscreen layers. For now, prioritize all small animated layers after 512
// pixels of pre-painting.
void setPriorityForTexture(const gfx::Rect& visibleRect,
                           const gfx::Rect& tileRect,
                           bool drawsToRoot,
                           bool isSmallAnimatedLayer,
                           PrioritizedResource* texture)
{
    int priority = PriorityCalculator::lowestPriority();
    if (!visibleRect.IsEmpty())
        priority = PriorityCalculator::priorityFromDistance(visibleRect, tileRect, drawsToRoot);
    if (isSmallAnimatedLayer)
        priority = PriorityCalculator::maxPriority(priority, PriorityCalculator::smallAnimatedLayerMinPriority());
    if (priority != PriorityCalculator::lowestPriority())
        texture->setRequestPriority(priority);
}
} // namespace

void TiledLayer::setTexturePriorities(const PriorityCalculator& priorityCalc)
{
    updateBounds();
    resetUpdateState();
    updateScrollPrediction();

    if (m_tiler->hasEmptyBounds())
        return;

    bool drawsToRoot = !renderTarget()->parent();
    bool smallAnimatedLayer = isSmallAnimatedLayer();

    // Minimally create the tiles in the desired pre-paint rect.
    gfx::Rect createTilesRect = idlePaintRect();
    if (smallAnimatedLayer)
        createTilesRect = gfx::Rect(gfx::Point(), contentBounds());
    if (!createTilesRect.IsEmpty()) {
        int left, top, right, bottom;
        m_tiler->contentRectToTileIndices(createTilesRect, left, top, right, bottom);
        for (int j = top; j <= bottom; ++j) {
            for (int i = left; i <= right; ++i) {
                if (!tileAt(i, j))
                    createTile(i, j);
            }
        }
    }

    // Now update priorities on all tiles we have in the layer, no matter where they are.
    for (LayerTilingData::TileMap::const_iterator iter = m_tiler->tiles().begin(); iter != m_tiler->tiles().end(); ++iter) {
        UpdatableTile* tile = static_cast<UpdatableTile*>(iter->second);
        // FIXME: This should not ever be null.
        if (!tile)
            continue;
        gfx::Rect tileRect = m_tiler->tileRect(tile);
        setPriorityForTexture(m_predictedVisibleRect, tileRect, drawsToRoot, smallAnimatedLayer, tile->managedResource());
    }
}

Region TiledLayer::visibleContentOpaqueRegion() const
{
    if (m_skipsDraw)
        return Region();
    if (contentsOpaque())
        return visibleContentRect();
    return m_tiler->opaqueRegionInContentRect(visibleContentRect());
}

void TiledLayer::resetUpdateState()
{
    m_skipsDraw = false;
    m_failedUpdate = false;

    LayerTilingData::TileMap::const_iterator end = m_tiler->tiles().end();
    for (LayerTilingData::TileMap::const_iterator iter = m_tiler->tiles().begin(); iter != end; ++iter) {
        UpdatableTile* tile = static_cast<UpdatableTile*>(iter->second);
        // FIXME: This should not ever be null.
        if (!tile)
            continue;
        tile->resetUpdateState();
    }
}

namespace {
gfx::Rect expandRectByDelta(gfx::Rect rect, gfx::Vector2d delta) {
    int width  = rect.width() + abs(delta.x());
    int height = rect.height() + abs(delta.y());
    int x = rect.x() + ((delta.x() < 0)  ? delta.x()  : 0);
    int y = rect.y() + ((delta.y() < 0) ? delta.y() : 0);
    return gfx::Rect(x, y, width, height);
}
}

void TiledLayer::updateScrollPrediction()
{
    // This scroll prediction is very primitive and should be replaced by a
    // a recursive calculation on all layers which uses actual scroll/animation
    // velocities. To insure this doesn't miss-predict, we only use it to predict
    // the visibleRect if:
    // - contentBounds() hasn't changed.
    // - visibleRect.size() hasn't changed.
    // These two conditions prevent rotations, scales, pinch-zooms etc. where
    // the prediction would be incorrect.
    gfx::Vector2d delta = visibleContentRect().CenterPoint() - m_previousVisibleRect.CenterPoint();
    m_predictedScroll = -delta;
    m_predictedVisibleRect = visibleContentRect();
    if (m_previousContentBounds == contentBounds() && m_previousVisibleRect.size() == visibleContentRect().size()) {
        // Only expand the visible rect in the major scroll direction, to prevent
        // massive paints due to diagonal scrolls.
        gfx::Vector2d majorScrollDelta = (abs(delta.x()) > abs(delta.y())) ? gfx::Vector2d(delta.x(), 0) : gfx::Vector2d(0, delta.y());
        m_predictedVisibleRect = expandRectByDelta(visibleContentRect(), majorScrollDelta);

        // Bound the prediction to prevent unbounded paints, and clamp to content bounds.
        gfx::Rect bound = visibleContentRect();
        bound.Inset(-m_tiler->tileSize().width() * maxPredictiveTilesCount,
                    -m_tiler->tileSize().height() * maxPredictiveTilesCount);
        bound.Intersect(gfx::Rect(gfx::Point(), contentBounds()));
        m_predictedVisibleRect.Intersect(bound);
    }
    m_previousContentBounds = contentBounds();
    m_previousVisibleRect = visibleContentRect();
}

void TiledLayer::update(ResourceUpdateQueue& queue, const OcclusionTracker* occlusion, RenderingStats& stats)
{
    DCHECK(!m_skipsDraw && !m_failedUpdate); // Did resetUpdateState get skipped?

    {
        base::AutoReset<bool> ignoreSetNeedsCommit(&m_ignoreSetNeedsCommit, true);

        ContentsScalingLayer::update(queue, occlusion, stats);
        updateBounds();
    }

    if (m_tiler->hasEmptyBounds() || !drawsContent())
        return;

    bool didPaint = false;

    // Animation pre-paint. If the layer is small, try to paint it all
    // immediately whether or not it is occluded, to avoid paint/upload
    // hiccups while it is animating.
    if (isSmallAnimatedLayer()) {
        int left, top, right, bottom;
        m_tiler->contentRectToTileIndices(gfx::Rect(gfx::Point(), contentBounds()), left, top, right, bottom);
        updateTiles(left, top, right, bottom, queue, 0, stats, didPaint);
        if (didPaint)
            return;
        // This was an attempt to paint the entire layer so if we fail it's okay,
        // just fallback on painting visible etc. below.
        m_failedUpdate = false;
    }

    if (m_predictedVisibleRect.IsEmpty())
        return;

    // Visible painting. First occlude visible tiles and paint the non-occluded tiles.
    int left, top, right, bottom;
    m_tiler->contentRectToTileIndices(m_predictedVisibleRect, left, top, right, bottom);
    markOcclusionsAndRequestTextures(left, top, right, bottom, occlusion);
    m_skipsDraw = !updateTiles(left, top, right, bottom, queue, occlusion, stats, didPaint);
    if (m_skipsDraw)
        m_tiler->reset();
    if (m_skipsDraw || didPaint)
        return;

    // If we have already painting everything visible. Do some pre-painting while idle.
    gfx::Rect idlePaintContentRect = idlePaintRect();
    if (idlePaintContentRect.IsEmpty())
        return;

    // Prepaint anything that was occluded but inside the layer's visible region.
    if (!updateTiles(left, top, right, bottom, queue, 0, stats, didPaint) || didPaint)
        return;

    int prepaintLeft, prepaintTop, prepaintRight, prepaintBottom;
    m_tiler->contentRectToTileIndices(idlePaintContentRect, prepaintLeft, prepaintTop, prepaintRight, prepaintBottom);

    // Then expand outwards one row/column at a time until we find a dirty row/column
    // to update. Increment along the major and minor scroll directions first.
    gfx::Vector2d delta = -m_predictedScroll;
    delta = gfx::Vector2d(delta.x() == 0 ? 1 : delta.x(),
                          delta.y() == 0 ? 1 : delta.y());
    gfx::Vector2d majorDelta = (abs(delta.x()) >  abs(delta.y())) ? gfx::Vector2d(delta.x(), 0) : gfx::Vector2d(0, delta.y());
    gfx::Vector2d minorDelta = (abs(delta.x()) <= abs(delta.y())) ? gfx::Vector2d(delta.x(), 0) : gfx::Vector2d(0, delta.y());
    gfx::Vector2d deltas[4] = {majorDelta, minorDelta, -majorDelta, -minorDelta};
    for(int i = 0; i < 4; i++) {
        if (deltas[i].y() > 0) {
            while (bottom < prepaintBottom) {
                ++bottom;
                if (!updateTiles(left, bottom, right, bottom, queue, 0, stats, didPaint) || didPaint)
                    return;
            }
        }
        if (deltas[i].y() < 0) {
            while (top > prepaintTop) {
                --top;
                if (!updateTiles(left, top, right, top, queue, 0, stats, didPaint) || didPaint)
                    return;
            }
        }
        if (deltas[i].x() < 0) {
            while (left > prepaintLeft) {
                --left;
                if (!updateTiles(left, top, left, bottom, queue, 0, stats, didPaint) || didPaint)
                    return;
            }
        }
        if (deltas[i].x() > 0) {
            while (right < prepaintRight) {
                ++right;
                if (!updateTiles(right, top, right, bottom, queue, 0, stats, didPaint) || didPaint)
                    return;
            }
        }
    }
}

bool TiledLayer::needsIdlePaint()
{
    // Don't trigger more paints if we failed (as we'll just fail again).
    if (m_failedUpdate || visibleContentRect().IsEmpty() || m_tiler->hasEmptyBounds() || !drawsContent())
        return false;

    gfx::Rect idlePaintContentRect = idlePaintRect();
    if (idlePaintContentRect.IsEmpty())
        return false;

    int left, top, right, bottom;
    m_tiler->contentRectToTileIndices(idlePaintContentRect, left, top, right, bottom);

    for (int j = top; j <= bottom; ++j) {
        for (int i = left; i <= right; ++i) {
            UpdatableTile* tile = tileAt(i, j);
            DCHECK(tile); // Did setTexturePriorities get skipped?
            if (!tile)
                continue;

            bool updated = !tile->updateRect.IsEmpty();
            bool canAcquire = tile->managedResource()->canAcquireBackingTexture();
            bool dirty = tile->isDirty() || !tile->managedResource()->haveBackingTexture();
            if (!updated && canAcquire && dirty)
                return true;
        }
    }
    return false;
}

gfx::Rect TiledLayer::idlePaintRect()
{
    // Don't inflate an empty rect.
    if (visibleContentRect().IsEmpty())
      return gfx::Rect();

    gfx::Rect prepaintRect = visibleContentRect();
    prepaintRect.Inset(-m_tiler->tileSize().width() * prepaintColumns,
                       -m_tiler->tileSize().height() * prepaintRows);
    gfx::Rect contentRect(gfx::Point(), contentBounds());
    prepaintRect.Intersect(contentRect);

    return prepaintRect;
}

}  // namespace cc
