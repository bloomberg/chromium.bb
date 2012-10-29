// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "config.h"

#include "cc/layer_tiling_data.h"

#include "base/logging.h"

using namespace std;

namespace cc {

scoped_ptr<LayerTilingData> LayerTilingData::create(const IntSize& tileSize, BorderTexelOption border)
{
    return make_scoped_ptr(new LayerTilingData(tileSize, border));
}

LayerTilingData::LayerTilingData(const IntSize& tileSize, BorderTexelOption border)
    : m_tilingData(tileSize, IntSize(), border == HasBorderTexels)
{
    setTileSize(tileSize);
}

LayerTilingData::~LayerTilingData()
{
}

void LayerTilingData::setTileSize(const IntSize& size)
{
    if (tileSize() == size)
        return;

    reset();

    m_tilingData.setMaxTextureSize(size);
}

IntSize LayerTilingData::tileSize() const
{
    return m_tilingData.maxTextureSize();
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

void LayerTilingData::contentRectToTileIndices(const IntRect& contentRect, int& left, int& top, int& right, int& bottom) const
{
    // An empty rect doesn't result in an empty set of tiles, so don't pass an empty rect.
    // FIXME: Possibly we should fill a vector of tiles instead,
    //        since the normal use of this function is to enumerate some tiles.
    DCHECK(!contentRect.isEmpty());

    left = m_tilingData.tileXIndexFromSrcCoord(contentRect.x());
    top = m_tilingData.tileYIndexFromSrcCoord(contentRect.y());
    right = m_tilingData.tileXIndexFromSrcCoord(contentRect.maxX() - 1);
    bottom = m_tilingData.tileYIndexFromSrcCoord(contentRect.maxY() - 1);
}

IntRect LayerTilingData::tileRect(const Tile* tile) const
{
    IntRect tileRect = m_tilingData.tileBoundsWithBorder(tile->i(), tile->j());
    tileRect.setSize(tileSize());
    return tileRect;
}

Region LayerTilingData::opaqueRegionInContentRect(const IntRect& contentRect) const
{
    if (contentRect.isEmpty())
        return Region();

    Region opaqueRegion;
    int left, top, right, bottom;
    contentRectToTileIndices(contentRect, left, top, right, bottom);
    for (int j = top; j <= bottom; ++j) {
        for (int i = left; i <= right; ++i) {
            Tile* tile = tileAt(i, j);
            if (!tile)
                continue;

            IntRect tileOpaqueRect = intersection(contentRect, tile->opaqueRect());
            opaqueRegion.unite(tileOpaqueRect);
        }
    }
    return opaqueRegion;
}

void LayerTilingData::setBounds(const IntSize& size)
{
    m_tilingData.setTotalSize(size);
    if (size.isEmpty()) {
        m_tiles.clear();
        return;
    }

    // Any tiles completely outside our new bounds are invalid and should be dropped.
    int left, top, right, bottom;
    contentRectToTileIndices(IntRect(IntPoint(), size), left, top, right, bottom);
    std::vector<TileMapKey> invalidTileKeys;
    for (TileMap::const_iterator it = m_tiles.begin(); it != m_tiles.end(); ++it) {
        if (it->first.first > right || it->first.second > bottom)
            invalidTileKeys.push_back(it->first);
    }
    for (size_t i = 0; i < invalidTileKeys.size(); ++i)
        m_tiles.erase(invalidTileKeys[i]);
}

IntSize LayerTilingData::bounds() const
{
    return m_tilingData.totalSize();
}

} // namespace cc
