// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.explore_sites;

import android.view.LayoutInflater;
import android.view.View;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.native_page.NativePageNavigationDelegate;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.suggestions.SuggestionsConfig.TileStyle;
import org.chromium.chrome.browser.suggestions.TileGridLayout;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.mojom.WindowOpenDisposition;

import java.util.List;

/**
 * Describes a portion of UI responsible for rendering a group of categories.
 * It abstracts general tasks related to initializing and fetching data for the UI.
 */
public class ExploreSitesSection {
    private static final int MAX_TILES = 4;

    @TileStyle
    private int mStyle;
    private Profile mProfile;
    private NativePageNavigationDelegate mNavigationDelegate;
    private TileGridLayout mExploreSection;

    public ExploreSitesSection(View view, Profile profile,
            NativePageNavigationDelegate navigationDelegate, @TileStyle int style) {
        mProfile = profile;
        mStyle = style;
        mExploreSection = (TileGridLayout) view;
        mExploreSection.setMaxRows(1);
        mExploreSection.setMaxColumns(MAX_TILES);
        mNavigationDelegate = navigationDelegate;
        initialize();
    }

    private void initialize() {
        ExploreSitesBridge.getEspCatalog(mProfile, this ::initializeCategoryTiles);
    }

    private void initializeCategoryTiles(List<ExploreSitesCategory> categoryList) {
        if (categoryList == null) return;
        if (categoryList.size() == 0) {
            // TODO(dewittj): Remove this once the fetcher works.
            categoryList.add(new ExploreSitesCategory(1, "this"));
            categoryList.add(new ExploreSitesCategory(1, "this"));
            categoryList.add(new ExploreSitesCategory(1, "this"));
            categoryList.add(new ExploreSitesCategory(1, "More"));
        }

        int tileCount = 0;
        for (final ExploreSitesCategory category : categoryList) {
            tileCount++;
            if (tileCount > MAX_TILES) break;
            ExploreSitesCategoryTileView tileView;
            if (mStyle == TileStyle.MODERN_CONDENSED) {
                tileView = (ExploreSitesCategoryTileView) LayoutInflater
                                   .from(mExploreSection.getContext())
                                   .inflate(R.layout.explore_sites_category_tile_view_condensed,
                                           mExploreSection, false);
            } else {
                tileView = (ExploreSitesCategoryTileView) LayoutInflater
                                   .from(mExploreSection.getContext())
                                   .inflate(R.layout.explore_sites_category_tile_view,
                                           mExploreSection, false);
            }

            tileView.initialize(category);
            mExploreSection.addView(tileView);
            tileView.setOnClickListener((View v) -> onClicked(category, v));
        }
    }

    private void onClicked(ExploreSitesCategory category, View v) {
        mNavigationDelegate.openUrl(WindowOpenDisposition.CURRENT_TAB,
                new LoadUrlParams(category.getUrl(), PageTransition.AUTO_BOOKMARK));
    }
}
