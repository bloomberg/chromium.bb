// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.explore_sites;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.support.graphics.drawable.VectorDrawableCompat;
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

import java.util.ArrayList;
import java.util.List;

/**
 * Describes a portion of UI responsible for rendering a group of categories.
 * It abstracts general tasks related to initializing and fetching data for the UI.
 */
public class ExploreSitesSection {
    private static final int MAX_CATEGORIES = 3;

    /** These should be kept in sync with //chrome/browser/android/explore_sites/catalog.proto */
    private static final int SPORT = 3;
    private static final int SHOPPING = 5;
    private static final int FOOD = 15;

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
        mExploreSection.setMaxColumns(MAX_CATEGORIES + 1);
        mNavigationDelegate = navigationDelegate;
        initialize();
    }

    private void initialize() {
        ExploreSitesBridge.getEspCatalog(mProfile, this ::initializeCategoryTiles);
    }

    private Drawable getVectorDrawable(int resource) {
        return VectorDrawableCompat.create(
                getContext().getResources(), resource, getContext().getTheme());
    }

    private Context getContext() {
        return mExploreSection.getContext();
    }

    /**
     * Creates the predetermined categories for when we don't yet have a catalog from the
     * ExploreSites API server.
     */
    private List<ExploreSitesCategory> createDefaultCategoryTiles() {
        List<ExploreSitesCategory> categoryList = new ArrayList<>();

        // Sport category.
        ExploreSitesCategory category = new ExploreSitesCategory(
                SPORT, getContext().getString(R.string.explore_sites_default_category_sports));
        category.setDrawable(getVectorDrawable(R.drawable.ic_directions_run_blue_24dp));
        categoryList.add(category);

        // Shopping category.
        category = new ExploreSitesCategory(
                SHOPPING, getContext().getString(R.string.explore_sites_default_category_shopping));
        category.setDrawable(getVectorDrawable(R.drawable.ic_shopping_basket_blue_24dp));
        categoryList.add(category);

        // Food category.
        category = new ExploreSitesCategory(
                FOOD, getContext().getString(R.string.explore_sites_default_category_cooking));
        category.setDrawable(getVectorDrawable(R.drawable.ic_restaurant_menu_blue_24dp));
        categoryList.add(category);
        return categoryList;
    }

    private ExploreSitesCategory createMoreTileCategory() {
        ExploreSitesCategory category = new ExploreSitesCategory(
                ExploreSitesCategory.MORE_BUTTON_ID, getContext().getString(R.string.more));
        category.setDrawable(getVectorDrawable(R.drawable.ic_arrow_forward_blue_24dp));
        return category;
    }

    private void createTileView(ExploreSitesCategory category) {
        ExploreSitesCategoryTileView tileView;
        if (mStyle == TileStyle.MODERN_CONDENSED) {
            tileView = (ExploreSitesCategoryTileView) LayoutInflater.from(getContext())
                               .inflate(R.layout.explore_sites_category_tile_view_condensed,
                                       mExploreSection, false);
        } else {
            tileView = (ExploreSitesCategoryTileView) LayoutInflater.from(getContext())
                               .inflate(R.layout.explore_sites_category_tile_view, mExploreSection,
                                       false);
        }
        tileView.initialize(category);
        mExploreSection.addView(tileView);
        tileView.setOnClickListener((View v) -> onClicked(category, v));
    }

    private void initializeCategoryTiles(List<ExploreSitesCategory> categoryList) {
        if (categoryList == null || categoryList.size() == 0) {
            categoryList = createDefaultCategoryTiles();
        }

        int tileCount = 0;
        for (final ExploreSitesCategory category : categoryList) {
            tileCount++;
            if (tileCount > MAX_CATEGORIES) break;
            createTileView(category);
        }
        createTileView(createMoreTileCategory());
    }

    private void onClicked(ExploreSitesCategory category, View v) {
        mNavigationDelegate.openUrl(WindowOpenDisposition.CURRENT_TAB,
                new LoadUrlParams(category.getUrl(), PageTransition.AUTO_BOOKMARK));
    }
}
