// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions;

import org.chromium.chrome.browser.ntp.cards.NewTabPageViewHolder;

import java.util.List;

/**
 * A {@link SiteSectionView} specialised in displaying sites as a simple grid of tiles, through
 * {@link TileGridLayout}.
 */
public class TileGridViewHolder extends NewTabPageViewHolder implements SiteSectionView {
    private final TileGridLayout mSectionView;

    private TileGroup mTileGroup;
    private TileRenderer mTileRenderer;

    public TileGridViewHolder(TileGridLayout tileGridLayout, int maxRows, int maxColumns) {
        super(tileGridLayout);

        mSectionView = tileGridLayout;
        mSectionView.setMaxRows(maxRows);
        mSectionView.setMaxColumns(maxColumns);
    }

    @Override
    public void bindDataSource(TileGroup tileGroup, TileRenderer tileRenderer) {
        mTileGroup = tileGroup;
        mTileRenderer = tileRenderer;
    }

    @Override
    public void refreshData() {
        assert mTileGroup.getTiles().size() == 1;
        List<Tile> tiles = mTileGroup.getTiles().get(TileSectionType.PERSONALIZED);
        assert tiles != null;

        mTileRenderer.renderTileSection(tiles, mSectionView, mTileGroup.getTileSetupDelegate());
        mTileGroup.notifyTilesRendered();
    }

    /**
     * Sets a new icon on the child view with a matching site.
     * @param tile The tile that holds the data to populate the tile view.
     */
    @Override
    public void updateIconView(Tile tile) {
        TileView tileView = mSectionView.getTileView(tile.getData());
        if (tileView != null) tileView.renderIcon(tile);
    }

    /**
     * Updates the visibility of the offline badge on the child view with a matching site.
     * @param tile The tile that holds the data to populate the tile view.
     */
    @Override
    public void updateOfflineBadge(Tile tile) {
        TileView tileView = mSectionView.getTileView(tile.getData());
        if (tileView != null) tileView.renderOfflineBadge(tile);
    }

    /**
     * Callback to update all the tiles in the view holder.
     */
    public static class UpdateTilesCallback extends PartialBindCallback {
        @Override
        public void onResult(NewTabPageViewHolder holder) {
            assert holder instanceof TileGridViewHolder;
            ((SiteSectionView) holder).refreshData();
        }
    }

    /**
     * Callback to update the icon view for the view holder.
     */
    public static class UpdateIconViewCallback extends PartialBindCallback {
        private final Tile mTile;

        public UpdateIconViewCallback(Tile tile) {
            mTile = tile;
        }

        @Override
        public void onResult(NewTabPageViewHolder holder) {
            assert holder instanceof TileGridViewHolder;
            ((SiteSectionView) holder).updateIconView(mTile);
        }
    }

    /**
     * Callback to update the offline badge for the view holder.
     */
    public static class UpdateOfflineBadgeCallback extends PartialBindCallback {
        private final Tile mTile;

        public UpdateOfflineBadgeCallback(Tile tile) {
            mTile = tile;
        }

        @Override
        public void onResult(NewTabPageViewHolder holder) {
            assert holder instanceof TileGridViewHolder;
            ((SiteSectionView) holder).updateOfflineBadge(mTile);
        }
    }
}
