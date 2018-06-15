// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.explore_sites;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.TextView;

import org.chromium.chrome.R;

/**
 * The View representing a single explore sites category.
 * Consists of a large image icon over descriptive text.
 */
public class ExploreSitesCategoryTileView extends FrameLayout {
    /** The data represented by this tile. */
    private ExploreSitesCategoryTile mCategoryData;

    private TextView mTitleView;
    private ImageView mIconView;

    /** Constructor for inflating from XML. */
    public ExploreSitesCategoryTileView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    public void onFinishInflate() {
        super.onFinishInflate();
        mTitleView = findViewById(R.id.explore_sites_category_tile_title);
        mIconView = findViewById(R.id.explore_sites_category_tile_icon);
    }

    public void initialize(ExploreSitesCategoryTile category) {
        mCategoryData = category;
        mTitleView.setText(mCategoryData.getCategoryName());
    }

    public void updateIcon(Drawable drawable) {
        mCategoryData.setIconDrawable(drawable);
        mIconView.setImageDrawable(drawable);
    }
}
