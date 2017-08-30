// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions;

import android.support.annotation.CallSuper;
import android.view.View;

import org.chromium.chrome.browser.ntp.cards.NewTabPageViewHolder;

import javax.annotation.Nullable;

/**
 * Describes a portion of UI responsible for rendering a group of sites. It abstracts general tasks
 * related to initialising and updating this UI.
 */
public abstract class SiteSectionViewHolder extends NewTabPageViewHolder {
    protected TileGroup mTileGroup;
    protected TileRenderer mTileRenderer;

    /**
     * Constructs a {@link SiteSectionViewHolder} used to display tiles in both NTP and Chrome Home.
     *
     * @param itemView The {@link View} for this item
     */
    public SiteSectionViewHolder(View itemView) {
        super(itemView);
    }

    /** Initialise the view, letting it know the data it will have to display. */
    @CallSuper
    public void bindDataSource(TileGroup tileGroup, TileRenderer tileRenderer) {
        mTileGroup = tileGroup;
        mTileRenderer = tileRenderer;
    }

    /**
     * Sets a new icon on the child view with a matching site.
     * @param tile The tile that holds the data to populate the tile view.
     */
    public void updateIconView(Tile tile) {
        TileView tileView = findTileView(tile.getData());
        if (tileView != null) tileView.renderIcon(tile);
    }

    /**
     * Updates the visibility of the offline badge on the child view with a matching site.
     * @param tile The tile that holds the data to populate the tile view.
     */
    public void updateOfflineBadge(Tile tile) {
        TileView tileView = findTileView(tile.getData());
        if (tileView != null) tileView.renderOfflineBadge(tile);
    }

    /** Clears the current data and displays the current state of the model. */
    public abstract void refreshData();

    @Nullable
    protected abstract TileView findTileView(SiteSuggestion data);

    /**
     * Callback to update all the tiles in the view holder.
     */
    public static class UpdateTilesCallback extends PartialBindCallback {
        @Override
        public void onResult(NewTabPageViewHolder holder) {
            ((SiteSectionViewHolder) holder).refreshData();
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
            ((SiteSectionViewHolder) holder).updateIconView(mTile);
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
            ((SiteSectionViewHolder) holder).updateOfflineBadge(mTile);
        }
    }
}
