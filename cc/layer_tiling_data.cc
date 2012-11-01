// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "config.h"

#include "cc/layer_tiling_data.h"

#include "base/logging.h"

using namespace std;

namespace cc {

scoped_ptr<LayerTilingData> LayerTilingData::create(const gfx::Size& tileSize, BorderTexelOption border)
{
    return make_scoped_ptr(new LayerTilingData(tileSize, border));
}

LayerTilingData::LayerTilingData(const gfx::Size& tileSize, BorderTexelOption border)
    : m_tilingData(cc::IntSize(tileSize), cc::IntSize(), border == HasBorderTexels)
{
    setTileSize(tileSize);
}

LayerTilingData::~LayerTilingData()
{
}

void LayerTilingData::setTileSize(const gfx::Size& size)
{
    if (tileSize() == size)
        return;

    reset();

    m_tilingData.setMaxTextureSize(cc::IntSize(size));
}

gfx::Size LayerTilingData::tileSize() const
{
    return cc::IntSize(m_tilingData.maxTextureSize());
}

void LayerTilingData::setBorderTexelOption(BorderTexelOption borderTexelOption)
{
    bool borderTexels = borderTexelOption == HasBorderTexels;
    if (hasBorderTexels() == borderTexels)
        return;

    reset();
    m_tilingData.setHasBorderTexels(borderTexels);
}

const LayerTilingData& LayerTilingData::operator=(const LayerTilingData& tiler)
{
    m_tilingData = tiler.m_tilingData;

    return *this;
}

void LayerTilingData::addTile(scoped_ptr<Tile> tile, int i, int j)
{
    DCHECK(!tileAt(i, j));
    tile->moveTo(i, j);
    m_tiles.add(make_pair(i, j), tile.Pass());
}

scoped_ptr<LayerTilingData::Tile> LayerTilingData::takeTile(int i, int j)
{
    return m_tiles.take_and_erase(make_pair(i, j));
}

LayerTilingData::Tile* LayerTilingData::tileAt(int i, int j) const
{
    return m_tiles.get(make_pair(i, j));
}

void LayerTilingData::reset()
{
    m_tiles.clear();
}

void LayerTilingData::contentRectToTileIndices(const gfx::Rect& contentRect, int& left, int& top, int& right, int& bottom) const
{
    // An empty rect doesn't result in an empty set of tiles, so don't pass an empty rect.
    // FIXME: Possibly we should fill a vector of tiles instead,
    //        since the normal use of this function is to enumerate some tiles.
    DCHECK(!contentRect.IsEmpty());

    left = m_tilingData.tileXIndexFromSrcCoord(contentRect.x());
    top = m_tilingData.tileYIndexFromSrcCoord(contentRect.y());
    right = m_tilingData.tileXIndexFromSrcCoord(contentRect.right() - 1);
    bottom = m_tilingData.tileYIndexFromSrcCoord(contentRect.bottom() - 1);
}

gfx::Rect LayerTilingData::tileRect(const Tile* tile) const
{
    gfx::Rect tileRect = cc::IntRect(m_tilingData.tileBoundsWithBorder(tile->i(), tile->j()));
    tileRect.set_size(tileSize());
    return tileRect;
}

Region LayerTilingData::opaqueRegionInContentRect(const gfx::Rect& contentRect) const
{
    if (contentRect.IsEmpty())
        return Region();

    Region opaqueRegion;
    int left, top, right, bottom;
    contentRectToTileIndices(contentRect, left, top, right, bottom);
    for (int j = top; j <= bottom; ++j) {
        for (int i = left; i <= right; ++i) {
            Tile* tile = tileAt(i, j);
            if (!tile)
                continue;

            gfx::Rect tileOpaqueRect = gfx::IntersectRects(contentRect, tile->opaqueRect());
            opaqueRegion.unite(cc::IntRect(tileOpaqueRect));
        }
    }
    return opaqueRegion;
}

void LayerTilingData::setBounds(const gfx::Size& size)
{
    m_tilingData.setTotalSize(cc::IntSize(size));
    if (size.IsEmpty()) {
        m_tiles.clear();
        return;
    }

    // Any tiles completely outside our new bounds are invalid and should be dropped.
    int left, top, right, bottom;
    contentRectToTileIndices(gfx::Rect(gfx::Point(), size), left, top, right, bottom);
    std::vector<TileMapKey> invalidTileKeys;
    for (TileMap::const_iterator it = m_tiles.begin(); it != m_tiles.end(); ++it) {
        if (it->first.first > right || it->first.second > bottom)
            invalidTileKeys.push_back(it->first);
    }
    for (size_t i = 0; i < invalidTileKeys.size(); ++i)
        m_tiles.erase(invalidTileKeys[i]);
}

gfx::Size LayerTilingData::bounds() const
{
    return cc::IntSize(m_tilingData.totalSize());
}

} // namespace cc
