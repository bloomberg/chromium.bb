// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions;

import android.content.Context;
import android.content.res.Resources;
import android.support.annotation.IntDef;
import android.util.AttributeSet;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.TextView;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.ntp.TitleUtil;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * The view for a site suggestion tile. Displays the title of the site beneath a large icon. If a
 * large icon isn't available, displays a rounded rectangle with a single letter in its place.
 */
public class TileView extends FrameLayout {
    @IntDef({Style.CLASSIC, Style.CONDENSED, Style.MODERN})
    @Retention(RetentionPolicy.SOURCE)
    public @interface Style {
        int CLASSIC = 0;
        int CONDENSED = 1;
        int MODERN = 2;
    }

    /** The url currently associated to this tile. */
    private String mUrl;

    private TextView mTitleView;
    private View mIconBackgroundView;
    private ImageView mIconView;
    private ImageView mBadgeView;

    /**
     * Constructor for inflating from XML.
     */
    public TileView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mTitleView = findViewById(R.id.tile_view_title);
        mIconBackgroundView = findViewById(R.id.tile_view_icon_background);
        mIconView = findViewById(R.id.tile_view_icon);
        mBadgeView = findViewById(R.id.offline_badge);
    }

    /**
     * Initializes the view using the data held by {@code tile}. This should be called immediately
     * after inflation.
     * @param tile The tile that holds the data to populate this view.
     * @param titleLines The number of text lines to use for the tile title.
     * @param tileStyle The visual style of the tile.
     */
    public void initialize(Tile tile, int titleLines, @Style int tileStyle) {
        mTitleView.setLines(titleLines);
        mUrl = tile.getUrl();

        Resources res = getResources();

        if (tileStyle == Style.MODERN) {
            mIconBackgroundView.setVisibility(View.VISIBLE);
            LayoutParams iconParams = (LayoutParams) mIconView.getLayoutParams();
            iconParams.width = res.getDimensionPixelOffset(R.dimen.tile_view_icon_size_modern);
            iconParams.height = res.getDimensionPixelOffset(R.dimen.tile_view_icon_size_modern);
            iconParams.setMargins(
                    0, res.getDimensionPixelOffset(R.dimen.tile_view_icon_margin_top_modern), 0, 0);
            mIconView.setLayoutParams(iconParams);
        } else if (tileStyle == Style.CONDENSED) {
            setPadding(0, 0, 0, 0);
            LayoutParams tileParams = (LayoutParams) getLayoutParams();
            tileParams.width = res.getDimensionPixelOffset(R.dimen.tile_view_width_condensed);
            setLayoutParams(tileParams);

            LayoutParams iconParams = (LayoutParams) mIconView.getLayoutParams();
            iconParams.setMargins(0,
                    res.getDimensionPixelOffset(R.dimen.tile_view_icon_margin_top_condensed), 0, 0);
            mIconView.setLayoutParams(iconParams);

            View highlightView = findViewById(R.id.tile_view_highlight);
            LayoutParams highlightParams = (LayoutParams) highlightView.getLayoutParams();
            highlightParams.setMargins(0,
                    res.getDimensionPixelOffset(R.dimen.tile_view_icon_margin_top_condensed), 0, 0);
            highlightView.setLayoutParams(highlightParams);

            LayoutParams titleParams = (LayoutParams) mTitleView.getLayoutParams();
            titleParams.setMargins(0,
                    res.getDimensionPixelOffset(R.dimen.tile_view_title_margin_top_condensed), 0,
                    0);
            mTitleView.setLayoutParams(titleParams);
        }

        renderTile(tile);
    }

    /** @return The url associated with this view. */
    public String getUrl() {
        return mUrl;
    }

    /**
     * Renders the icon held by the {@link Tile} or clears it from the view if the icon is null.
     */
    public void renderIcon(Tile tile) {
        mIconView.setImageDrawable(tile.getIcon());
    }

    /** Shows or hides the offline badge to reflect the offline availability of the {@link Tile}. */
    public void renderOfflineBadge(Tile tile) {
        mBadgeView.setVisibility(tile.isOfflineAvailable() ? VISIBLE : GONE);
    }

    /** Updates the view if there have been changes since the last time. */
    public void updateIfDataChanged(Tile tile) {
        if (!isUpToDate(tile)) renderTile(tile);
    }

    private boolean isUpToDate(Tile tile) {
        assert mUrl.equals(tile.getUrl());

        if (tile.getIcon() != mIconView.getDrawable()) return false;
        if (!tile.getTitle().equals(mTitleView.getText())) return false;
        if (tile.isOfflineAvailable() != (mBadgeView.getVisibility() == VISIBLE)) return false;
        return true;
    }

    private void renderTile(Tile tile) {
        // A TileView should not be reused across tiles having different urls, as registered
        // callbacks and handlers use it to look up the data and notify the rest of the system.
        assert mUrl.equals(tile.getUrl());
        mTitleView.setText(TitleUtil.getTitleForDisplay(tile.getTitle(), tile.getUrl()));
        renderOfflineBadge(tile);
        renderIcon(tile);
    }
}
