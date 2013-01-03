// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layer_tiling_data.h"

#include "base/logging.h"

namespace cc {

scoped_ptr<LayerTilingData> LayerTilingData::create(const gfx::Size& tileSize, BorderTexelOption border)
{
    return make_scoped_ptr(new LayerTilingData(tileSize, border));
}

LayerTilingData::LayerTilingData(const gfx::Size& tileSize, BorderTexelOption border)
    : m_tilingData(tileSize, gfx::Size(), border == HasBorderTexels)
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

    m_tilingData.SetMaxTextureSize(size);
}

gfx::Size LayerTilingData::tileSize() const
{
    return m_tilingData.max_texture_size();
}

void LayerTilingData::setBorderTexelOption(BorderTexelOption borderTexelOption)
{
    bool borderTexels = borderTexelOption == HasBorderTexels;
    if (hasBorderTexels() == borderTexels)
        return;

    reset();
    m_tilingData.SetHasBorderTexels(borderTexels);
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
    m_tiles.add(std::make_pair(i, j), tile.Pass());
}

scoped_ptr<LayerTilingData::Tile> LayerTilingData::takeTile(int i, int j)
{
    return m_tiles.take_and_erase(std::make_pair(i, j));
}

LayerTilingData::Tile* LayerTilingData::tileAt(int i, int j) const
{
    return m_tiles.get(std::make_pair(i, j));
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

    left = m_tilingData.TileXIndexFromSrcCoord(contentRect.x());
    top = m_tilingData.TileYIndexFromSrcCoord(contentRect.y());
    right = m_tilingData.TileXIndexFromSrcCoord(contentRect.right() - 1);
    bottom = m_tilingData.TileYIndexFromSrcCoord(contentRect.bottom() - 1);
}

gfx::Rect LayerTilingData::tileRect(const Tile* tile) const
{
    gfx::Rect tileRect = m_tilingData.TileBoundsWithBorder(tile->i(), tile->j());
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
            opaqueRegion.Union(tileOpaqueRect);
        }
    }
    return opaqueRegion;
}

void LayerTilingData::setBounds(const gfx::Size& size)
{
    m_tilingData.SetTotalSize(size);
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
    return m_tilingData.total_size();
}

}  // namespace cc
