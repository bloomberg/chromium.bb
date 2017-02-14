// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions;

import android.view.LayoutInflater;
import android.view.ViewGroup;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.ntp.ContextMenuManager;
import org.chromium.chrome.browser.ntp.cards.ItemViewType;
import org.chromium.chrome.browser.ntp.cards.NewTabPageViewHolder;
import org.chromium.chrome.browser.ntp.cards.OptionalLeaf;

/**
 * The model and controller for a group of site suggestion tiles that will be rendered in a grid.
 */
public class TileGrid extends OptionalLeaf implements TileGroup.Observer {
    private static final int MAX_TILES = 4;
    private static final int MAX_ROWS = 1;

    private final TileGroup mTileGroup;

    public TileGrid(SuggestionsUiDelegate uiDelegate, ContextMenuManager contextMenuManager,
            TileGroup.Delegate tileGroupDelegate) {
        mTileGroup = new TileGroup(
                uiDelegate, contextMenuManager, tileGroupDelegate, /* observer = */ this);
        mTileGroup.startObserving(MAX_TILES);
    }

    @Override
    @ItemViewType
    protected int getItemViewType() {
        return ItemViewType.TILE_GRID;
    }

    @Override
    protected void onBindViewHolder(NewTabPageViewHolder holder) {
        assert holder instanceof ViewHolder;
        ((ViewHolder) holder).onBindViewHolder(mTileGroup);
    }

    @Override
    public void onTileDataChanged() {
        setVisible(mTileGroup.getTiles().length != 0);
        if (isVisible()) notifyItemChanged(0);
    }

    @Override
    public void onTileCountChanged() {
        onTileDataChanged();
    }

    @Override
    public void onTileIconChanged(Tile tile) {
        if (isVisible()) notifyItemChanged(0, new ViewHolder.UpdateIconViewCallback(tile));
    }

    @Override
    public void onLoadTaskAdded() {}

    @Override
    public void onLoadTaskCompleted() {}

    /**
     * The {@code ViewHolder} for the {@link TileGrid}.
     */
    public static class ViewHolder extends NewTabPageViewHolder {
        private final TileGridLayout mLayout;

        public ViewHolder(ViewGroup parentView) {
            super(LayoutInflater.from(parentView.getContext())
                            .inflate(R.layout.suggestions_site_tile_grid, parentView, false));
            mLayout = (TileGridLayout) itemView;
        }

        public void onBindViewHolder(TileGroup tileGroup) {
            mLayout.setMaxRows(MAX_ROWS);
            tileGroup.renderTileViews(mLayout, /* trackLoadTasks = */ false);
        }

        public void updateIconView(Tile tile) {
            mLayout.updateIconView(tile);
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
                assert holder instanceof ViewHolder;
                ((ViewHolder) holder).updateIconView(mTile);
            }
        }
    }
}
