// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/tiled_layer_impl.h"

#include "base/basictypes.h"
#include "base/stringprintf.h"
#include "cc/append_quads_data.h"
#include "cc/checkerboard_draw_quad.h"
#include "cc/debug_border_draw_quad.h"
#include "cc/layer_tiling_data.h"
#include "cc/math_util.h"
#include "cc/quad_sink.h"
#include "cc/solid_color_draw_quad.h"
#include "cc/tile_draw_quad.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/quad_f.h"

using namespace std;
using WebKit::WebTransformationMatrix;

namespace cc {

static const int debugTileBorderWidth = 1;
static const int debugTileBorderAlpha = 100;
static const int debugTileBorderColorRed = 80;
static const int debugTileBorderColorGreen = 200;
static const int debugTileBorderColorBlue = 200;
static const int debugTileBorderMissingTileColorRed = 255;
static const int debugTileBorderMissingTileColorGreen = 0;
static const int debugTileBorderMissingTileColorBlue = 0;

static const int defaultCheckerboardColorRed = 241;
static const int defaultCheckerboardColorGreen = 241;
static const int defaultCheckerboardColorBlue = 241;
static const int debugTileEvictedCheckerboardColorRed = 255;
static const int debugTileEvictedCheckerboardColorGreen = 200;
static const int debugTileEvictedCheckerboardColorBlue = 200;
static const int debugTileInvalidatedCheckerboardColorRed = 128;
static const int debugTileInvalidatedCheckerboardColorGreen = 200;
static const int debugTileInvalidatedCheckerboardColorBlue = 245;

class DrawableTile : public LayerTilingData::Tile {
public:
    static scoped_ptr<DrawableTile> create() { return make_scoped_ptr(new DrawableTile()); }

    ResourceProvider::ResourceId resourceId() const { return m_resourceId; }
    void setResourceId(ResourceProvider::ResourceId resourceId) { m_resourceId = resourceId; }
    bool contentsSwizzled() { return m_contentsSwizzled; }
    void setContentsSwizzled(bool contentsSwizzled) { m_contentsSwizzled = contentsSwizzled; }

private:
    DrawableTile()
        : m_resourceId(0)
        , m_contentsSwizzled(false) { }

    ResourceProvider::ResourceId m_resourceId;
    bool m_contentsSwizzled;

    DISALLOW_COPY_AND_ASSIGN(DrawableTile);
};

TiledLayerImpl::TiledLayerImpl(int id)
    : LayerImpl(id)
    , m_skipsDraw(true)
{
}

TiledLayerImpl::~TiledLayerImpl()
{
}

ResourceProvider::ResourceId TiledLayerImpl::contentsResourceId() const
{
    // This function is only valid for single texture layers, e.g. masks.
    DCHECK(m_tiler);
    DCHECK(m_tiler->numTilesX() == 1);
    DCHECK(m_tiler->numTilesY() == 1);

    DrawableTile* tile = tileAt(0, 0);
    ResourceProvider::ResourceId resourceId = tile ? tile->resourceId() : 0;
    return resourceId;
}

void TiledLayerImpl::dumpLayerProperties(std::string* str, int indent) const
{
    str->append(indentString(indent));
    base::StringAppendF(str, "skipsDraw: %d\n", (!m_tiler || m_skipsDraw));
    LayerImpl::dumpLayerProperties(str, indent);
}

bool TiledLayerImpl::hasTileAt(int i, int j) const
{
    return m_tiler->tileAt(i, j);
}

bool TiledLayerImpl::hasResourceIdForTileAt(int i, int j) const
{
    return hasTileAt(i, j) && tileAt(i, j)->resourceId();
}

DrawableTile* TiledLayerImpl::tileAt(int i, int j) const
{
    return static_cast<DrawableTile*>(m_tiler->tileAt(i, j));
}

DrawableTile* TiledLayerImpl::createTile(int i, int j)
{
    scoped_ptr<DrawableTile> tile(DrawableTile::create());
    DrawableTile* addedTile = tile.get();
    m_tiler->addTile(tile.PassAs<LayerTilingData::Tile>(), i, j);
    return addedTile;
}

void TiledLayerImpl::appendQuads(QuadSink& quadSink, AppendQuadsData& appendQuadsData)
{
    const gfx::Rect& contentRect = visibleContentRect();

    if (!m_tiler || m_tiler->hasEmptyBounds() || contentRect.IsEmpty())
        return;

    SharedQuadState* sharedQuadState = quadSink.useSharedQuadState(createSharedQuadState());
    appendDebugBorderQuad(quadSink, sharedQuadState, appendQuadsData);

    int left, top, right, bottom;
    m_tiler->contentRectToTileIndices(contentRect, left, top, right, bottom);

    if (hasDebugBorders()) {
        for (int j = top; j <= bottom; ++j) {
            for (int i = left; i <= right; ++i) {
                DrawableTile* tile = tileAt(i, j);
                gfx::Rect tileRect = m_tiler->tileBounds(i, j);
                SkColor borderColor;

                if (m_skipsDraw || !tile || !tile->resourceId())
                    borderColor = SkColorSetARGB(debugTileBorderAlpha, debugTileBorderMissingTileColorRed, debugTileBorderMissingTileColorGreen, debugTileBorderMissingTileColorBlue);
                else
                    borderColor = SkColorSetARGB(debugTileBorderAlpha, debugTileBorderColorRed, debugTileBorderColorGreen, debugTileBorderColorBlue);
                quadSink.append(DebugBorderDrawQuad::create(sharedQuadState, tileRect, borderColor, debugTileBorderWidth).PassAs<DrawQuad>(), appendQuadsData);
            }
        }
    }

    if (m_skipsDraw)
        return;

    for (int j = top; j <= bottom; ++j) {
        for (int i = left; i <= right; ++i) {
            DrawableTile* tile = tileAt(i, j);
            gfx::Rect tileRect = m_tiler->tileBounds(i, j);
            gfx::Rect displayRect = tileRect;
            tileRect.Intersect(contentRect);

            // Skip empty tiles.
            if (tileRect.IsEmpty())
                continue;

            if (!tile || !tile->resourceId()) {
                if (drawCheckerboardForMissingTiles()) {
                    SkColor defaultColor = SkColorSetRGB(defaultCheckerboardColorRed, defaultCheckerboardColorGreen, defaultCheckerboardColorBlue);
                    SkColor evictedColor = SkColorSetRGB(debugTileEvictedCheckerboardColorRed, debugTileEvictedCheckerboardColorGreen, debugTileEvictedCheckerboardColorBlue);
                    SkColor invalidatedColor = SkColorSetRGB(debugTileInvalidatedCheckerboardColorRed, debugTileEvictedCheckerboardColorGreen, debugTileEvictedCheckerboardColorBlue);

                    SkColor checkerColor;
                    if (hasDebugBorders())
                        checkerColor = tile ? invalidatedColor : evictedColor;
                    else
                        checkerColor = defaultColor;

                    appendQuadsData.hadMissingTiles |= quadSink.append(CheckerboardDrawQuad::create(sharedQuadState, tileRect, checkerColor).PassAs<DrawQuad>(), appendQuadsData);
                } else
                    appendQuadsData.hadMissingTiles |= quadSink.append(SolidColorDrawQuad::create(sharedQuadState, tileRect, backgroundColor()).PassAs<DrawQuad>(), appendQuadsData);
                continue;
            }

            gfx::Rect tileOpaqueRect = tile->opaqueRect();
            tileOpaqueRect.Intersect(contentRect);

            // Keep track of how the top left has moved, so the texture can be
            // offset the same amount.
            gfx::Vector2d displayOffset = tileRect.origin() - displayRect.origin();
            gfx::Vector2d textureOffset = m_tiler->textureOffset(i, j) + displayOffset;
            float tileWidth = static_cast<float>(m_tiler->tileSize().width());
            float tileHeight = static_cast<float>(m_tiler->tileSize().height());
            gfx::Size textureSize(tileWidth, tileHeight);

            bool clipped = false;
            gfx::QuadF visibleContentInTargetQuad = MathUtil::mapQuad(drawTransform(), gfx::QuadF(visibleContentRect()), clipped);
            bool isAxisAlignedInTarget = !clipped && visibleContentInTargetQuad.IsRectilinear();
            bool useAA = m_tiler->hasBorderTexels() && !isAxisAlignedInTarget;

            bool leftEdgeAA = !i && useAA;
            bool topEdgeAA = !j && useAA;
            bool rightEdgeAA = i == m_tiler->numTilesX() - 1 && useAA;
            bool bottomEdgeAA = j == m_tiler->numTilesY() - 1 && useAA;

            quadSink.append(TileDrawQuad::create(sharedQuadState, tileRect, tileOpaqueRect, tile->resourceId(), textureOffset, textureSize, tile->contentsSwizzled(), leftEdgeAA, topEdgeAA, rightEdgeAA, bottomEdgeAA).PassAs<DrawQuad>(), appendQuadsData);
        }
    }
}

void TiledLayerImpl::setTilingData(const LayerTilingData& tiler)
{
    if (m_tiler)
        m_tiler->reset();
    else
        m_tiler = LayerTilingData::create(tiler.tileSize(), tiler.hasBorderTexels() ? LayerTilingData::HasBorderTexels : LayerTilingData::NoBorderTexels);
    *m_tiler = tiler;
}

void TiledLayerImpl::pushTileProperties(int i, int j, ResourceProvider::ResourceId resourceId, const gfx::Rect& opaqueRect, bool contentsSwizzled)
{
    DrawableTile* tile = tileAt(i, j);
    if (!tile)
        tile = createTile(i, j);
    tile->setResourceId(resourceId);
    tile->setOpaqueRect(opaqueRect);
    tile->setContentsSwizzled(contentsSwizzled);
}

void TiledLayerImpl::pushInvalidTile(int i, int j)
{
    DrawableTile* tile = tileAt(i, j);
    if (!tile)
        tile = createTile(i, j);
    tile->setResourceId(0);
    tile->setOpaqueRect(gfx::Rect());
    tile->setContentsSwizzled(false);
}

Region TiledLayerImpl::visibleContentOpaqueRegion() const
{
    if (m_skipsDraw)
        return Region();
    if (contentsOpaque())
        return visibleContentRect();
    return m_tiler->opaqueRegionInContentRect(visibleContentRect());
}

void TiledLayerImpl::didLoseContext()
{
    m_tiler->reset();
}

const char* TiledLayerImpl::layerTypeAsString() const
{
    return "ContentLayer";
}

}  // namespace cc
