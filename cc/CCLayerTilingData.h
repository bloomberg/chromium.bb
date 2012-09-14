// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#ifndef CCLayerTilingData_h
#define CCLayerTilingData_h

#if USE(ACCELERATED_COMPOSITING)

#include "IntRect.h"
#include "Region.h"
#include "TilingData.h"
#include <wtf/HashMap.h>
#include <wtf/HashTraits.h>
#include <wtf/PassOwnPtr.h>

namespace cc {

class CCLayerTilingData {
public:
    enum BorderTexelOption { HasBorderTexels, NoBorderTexels };

    static PassOwnPtr<CCLayerTilingData> create(const IntSize& tileSize, BorderTexelOption);

    bool hasEmptyBounds() const { return m_tilingData.hasEmptyBounds(); }
    int numTilesX() const { return m_tilingData.numTilesX(); }
    int numTilesY() const { return m_tilingData.numTilesY(); }
    IntRect tileBounds(int i, int j) const { return m_tilingData.tileBounds(i, j); }
    IntPoint textureOffset(int xIndex, int yIndex) const { return m_tilingData.textureOffset(xIndex, yIndex); }

    // Change the tile size. This may invalidate all the existing tiles.
    void setTileSize(const IntSize&);
    IntSize tileSize() const;
    // Change the border texel setting. This may invalidate all existing tiles.
    void setBorderTexelOption(BorderTexelOption);
    bool hasBorderTexels() const { return m_tilingData.borderTexels(); }

    bool isEmpty() const { return hasEmptyBounds() || !tiles().size(); }

    const CCLayerTilingData& operator=(const CCLayerTilingData&);

    class Tile {
        WTF_MAKE_NONCOPYABLE(Tile);
    public:
        Tile() : m_i(-1), m_j(-1) { }
        virtual ~Tile() { }

        int i() const { return m_i; }
        int j() const { return m_j; }
        void moveTo(int i, int j) { m_i = i; m_j = j; }

        const IntRect& opaqueRect() const { return m_opaqueRect; }
        void setOpaqueRect(const IntRect& opaqueRect) { m_opaqueRect = opaqueRect; }
    private:
        int m_i;
        int m_j;
        IntRect m_opaqueRect;
    };
    // Default hash key traits for integers disallow 0 and -1 as a key, so
    // use a custom hash trait which disallows -1 and -2 instead.
    typedef std::pair<int, int> TileMapKey;
    struct TileMapKeyTraits : HashTraits<TileMapKey> {
        static const bool emptyValueIsZero = false;
        static const bool needsDestruction = false;
        static TileMapKey emptyValue() { return std::make_pair(-1, -1); }
        static void constructDeletedValue(TileMapKey& slot) { slot = std::make_pair(-2, -2); }
        static bool isDeletedValue(TileMapKey value) { return value.first == -2 && value.second == -2; }
    };
    typedef HashMap<TileMapKey, OwnPtr<Tile>, DefaultHash<TileMapKey>::Hash, TileMapKeyTraits> TileMap;

    void addTile(PassOwnPtr<Tile>, int, int);
    PassOwnPtr<Tile> takeTile(int, int);
    Tile* tileAt(int, int) const;
    const TileMap& tiles() const { return m_tiles; }

    void setBounds(const IntSize&);
    IntSize bounds() const;

    void contentRectToTileIndices(const IntRect&, int &left, int &top, int &right, int &bottom) const;
    IntRect tileRect(const Tile*) const;

    Region opaqueRegionInContentRect(const IntRect&) const;

    void reset();

protected:
    CCLayerTilingData(const IntSize& tileSize, BorderTexelOption);

    TileMap m_tiles;
    TilingData m_tilingData;
};

}

#endif // USE(ACCELERATED_COMPOSITING)

#endif
