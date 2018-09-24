// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.explore_sites;

import android.content.Context;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.widget.GridLayout;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.widget.RoundedIconGenerator;

import java.util.List;

/**
 * View for a category name and site tiles.
 */
public class ExploreSitesCategoryCardView extends LinearLayout {
    private TextView mTitleView;
    private GridLayout mTileView;
    private RoundedIconGenerator mIconGenerator;

    public ExploreSitesCategoryCardView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mTitleView = findViewById(R.id.category_title);
        mTileView = findViewById(R.id.category_sites);
    }

    public void setIconGenerator(RoundedIconGenerator iconGenerator) {
        mIconGenerator = iconGenerator;
    }

    public void initialize(ExploreSitesCategory category) {
        updateTitle(category.getTitle());
        updateTileViews(category.getSites());
    }

    public void updateTitle(String categoryTitle) {
        mTitleView.setText(categoryTitle);
    }

    public void updateTileViews(List<ExploreSitesSite> sites) {
        // Remove extra views if too many.
        if (mTileView.getChildCount() > sites.size()) {
            mTileView.removeViews(sites.size(), mTileView.getChildCount() - sites.size());
        }
        // Add views if too few.
        if (mTileView.getChildCount() < sites.size()) {
            for (int i = mTileView.getChildCount(); i < sites.size(); i++) {
                mTileView.addView(LayoutInflater.from(getContext())
                                          .inflate(R.layout.explore_sites_tile_view, mTileView));
            }
        }
        // Initialize all the tiles again to update.
        for (int i = 0; i < sites.size(); i++) {
            ExploreSitesTileView tileView = (ExploreSitesTileView) mTileView.getChildAt(i);
            tileView.setIconGenerator(mIconGenerator);
            tileView.initialize(sites.get(i));
        }
    }
}
