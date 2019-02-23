// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions;

import android.content.res.Resources;
import android.view.ViewGroup;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.widget.displaystyle.UiConfig;

import java.util.List;
/**
 * A {@link SiteSectionViewHolder} specialised in displaying sites as a simple grid of tiles,
 * through
 * {@link TileGridLayout}.
 */
public class TileGridViewHolder extends SiteSectionViewHolder {
    private final TileGridLayout mSectionView;

    public TileGridViewHolder(ViewGroup view, int maxRows, int maxColumns, UiConfig uiConfig) {
        super(view);

        mSectionView = (TileGridLayout) itemView;
        mSectionView.setMaxRows(maxRows);
        mSectionView.setMaxColumns(maxColumns);

        Resources res = itemView.getResources();
        int defaultLateralMargin =
                res.getDimensionPixelSize(R.dimen.tile_grid_layout_padding_start);
        int wideLateralMargin = res.getDimensionPixelSize(R.dimen.ntp_wide_card_lateral_margins);
    }

    @Override
    public void refreshData() {
        assert mTileGroup.getTileSections().size() == 1;
        List<Tile> tiles = mTileGroup.getTileSections().get(TileSectionType.PERSONALIZED);
        assert tiles != null;

        mTileRenderer.renderTileSection(tiles, mSectionView, mTileGroup.getTileSetupDelegate());
        mTileGroup.notifyTilesRendered();
    }

    @Override
    public SuggestionsTileView findTileView(SiteSuggestion data) {
        return mSectionView.getTileView(data);
    }

    @Override
    public void bindDataSource(TileGroup tileGroup, TileRenderer tileRenderer) {
        super.bindDataSource(tileGroup, tileRenderer);
    }

    @Override
    public void recycle() {
        super.recycle();
    }
}
