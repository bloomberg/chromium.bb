// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.explore_sites;

import android.content.Context;
import android.util.AttributeSet;
import android.view.ViewGroup.MarginLayoutParams;
import android.widget.ImageView;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.ntp.TitleUtil;
import org.chromium.chrome.browser.widget.tile.TileWithTextView;

/**
 * A category tile for ExploreSites, containing an icon that is a composition of sites' favicons
 * within the category.  Alternatively, a MORE button.
 */
public class ExploreSitesCategoryTileView extends TileWithTextView {
    private static final int TITLE_LINES = 1;
    private static final boolean SUPPORTED_OFFLINE = false;

    /** The data currently associated to this tile. */
    private ExploreSitesCategory mCategory;

    /**
     * Constructor for inflating from XML.
     */
    public ExploreSitesCategoryTileView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    /**
     * Initializes the view using the data held by {@code tile}. This should be called immediately
     * after inflation.
     * @param category The object that holds the data to populate this view.
     */
    public void initialize(ExploreSitesCategory category) {
        super.initialize(TitleUtil.getTitleForDisplay(category.getTitle(), category.getUrl()),
                SUPPORTED_OFFLINE, category.getDrawable(), TITLE_LINES);
        mCategory = category;

        // Correct the properties of the icon for categories, it should be the entire size of the
        // icon background now.
        ImageView tileViewIcon = (ImageView) findViewById(R.id.tile_view_icon);
        tileViewIcon.setScaleType(ImageView.ScaleType.CENTER);
        MarginLayoutParams layoutParams = (MarginLayoutParams) tileViewIcon.getLayoutParams();
        int tileViewIconSize =
                getContext().getResources().getDimensionPixelSize(R.dimen.tile_view_icon_size);
        layoutParams.width = tileViewIconSize;
        layoutParams.height = tileViewIconSize;
        layoutParams.topMargin = getContext().getResources().getDimensionPixelSize(
                R.dimen.tile_view_icon_background_margin_top_modern);
        tileViewIcon.setLayoutParams(layoutParams);
    }

    /** Retrieves url associated with this view. */
    public String getUrl() {
        return mCategory.getUrl();
    }

    public ExploreSitesCategory getCategory() {
        return mCategory;
    }

    /** Renders icon based on tile data.  */
    public void renderIcon(ExploreSitesCategory category) {
        mCategory = category;
        setIconDrawable(category.getDrawable());
    }
}
