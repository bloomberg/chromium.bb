// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions;

import android.content.Context;
import android.util.AttributeSet;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.TextView;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.ntp.TitleUtil;

/**
 * The view for a site suggestion tile. Displays the title of the site beneath a large icon. If a
 * large icon isn't available, displays a rounded rectangle with a single letter in its place.
 */
public class TileView extends FrameLayout {
    /**
     * The tile that holds the data to populate this view.
     */
    private Tile mTile;

    /**
     * Constructor for inflating from XML.
     */
    public TileView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    /**
     * Initializes the view using the data held by {@code tile}. This should be called immediately
     * after inflation.
     * @param tile The tile that holds the data to populate this view.
     */
    public void initialize(Tile tile) {
        mTile = tile;
        ((TextView) findViewById(R.id.tile_view_title))
                .setText(TitleUtil.getTitleForDisplay(mTile.getTitle(), mTile.getUrl()));
        renderIcon();
        findViewById(R.id.offline_badge)
                .setVisibility(mTile.isOfflineAvailable() ? View.VISIBLE : View.GONE);
    }

    /**
     * @return The tile that holds the data to populate this view.
     */
    public Tile getTile() {
        return mTile;
    }

    /**
     * Renders the icon held by the {@link Tile} or clears it from the view if the icon is null.
     */
    public void renderIcon() {
        ((ImageView) findViewById(R.id.tile_view_icon)).setImageDrawable(mTile.getIcon());
    }
}
