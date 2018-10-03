// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.explore_sites;

import android.content.Context;
import android.util.AttributeSet;
import android.view.ContextMenu;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.View.OnCreateContextMenuListener;
import android.widget.GridLayout;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.native_page.ContextMenuManager;
import org.chromium.chrome.browser.native_page.NativePageNavigationDelegate;
import org.chromium.chrome.browser.widget.RoundedIconGenerator;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.mojom.WindowOpenDisposition;

import java.util.List;

/**
 * View for a category name and site tiles.
 */
public class ExploreSitesCategoryCardView extends LinearLayout {
    private static final int MAX_TILE_COUNT = 8;

    private TextView mTitleView;
    private GridLayout mTileView;
    private RoundedIconGenerator mIconGenerator;
    private ContextMenuManager mContextMenuManager;
    private NativePageNavigationDelegate mNavigationDelegate;

    private class CategoryCardInteractionDelegate
            implements ContextMenuManager.Delegate, OnClickListener, OnCreateContextMenuListener {
        private ExploreSitesSite mSite;
        public CategoryCardInteractionDelegate(ExploreSitesSite site) {
            mSite = site;
        }

        @Override
        public void onClick(View view) {
            mNavigationDelegate.openUrl(WindowOpenDisposition.CURRENT_TAB,
                    new LoadUrlParams(getUrl(), PageTransition.AUTO_BOOKMARK));
        }

        @Override
        public void onCreateContextMenu(
                ContextMenu menu, View v, ContextMenu.ContextMenuInfo menuInfo) {
            mContextMenuManager.createContextMenu(menu, v, this);
        }

        @Override
        public void openItem(int windowDisposition) {
            mNavigationDelegate.openUrl(
                    windowDisposition, new LoadUrlParams(getUrl(), PageTransition.AUTO_BOOKMARK));
        }

        @Override
        public void removeItem() {} // TODO(chili): Add removal method.

        @Override
        public String getUrl() {
            return mSite.getUrl();
        }

        @Override
        public boolean isItemSupported(@ContextMenuManager.ContextMenuItemId int menuItemId) {
            if (menuItemId == ContextMenuManager.ContextMenuItemId.LEARN_MORE) {
                return false;
            }
            return true;
        }

        @Override
        public void onContextMenuCreated(){};
    }

    public ExploreSitesCategoryCardView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mTitleView = findViewById(R.id.category_title);
        mTileView = findViewById(R.id.category_sites);
    }

    public void setCategory(ExploreSitesCategory category, RoundedIconGenerator iconGenerator,
            ContextMenuManager contextMenuManager,
            NativePageNavigationDelegate navigationDelegate) {
        mIconGenerator = iconGenerator;
        mContextMenuManager = contextMenuManager;
        mNavigationDelegate = navigationDelegate;

        updateTitle(category.getTitle());
        updateTileViews(category.getSites());
    }

    public void updateTitle(String categoryTitle) {
        mTitleView.setText(categoryTitle);
    }

    public void updateTileViews(List<ExploreSitesSite> sites) {
        // Remove extra tiles if too many.
        if (mTileView.getChildCount() > sites.size()) {
            mTileView.removeViews(sites.size(), mTileView.getChildCount() - sites.size());
        }

        // Maximum number of sites to show.
        int tileMax = Math.min(MAX_TILE_COUNT, sites.size());

        // Add tiles if too few
        if (mTileView.getChildCount() < tileMax) {
            for (int i = mTileView.getChildCount(); i < tileMax; i++) {
                mTileView.addView(LayoutInflater.from(getContext())
                                          .inflate(R.layout.explore_sites_tile_view, mTileView,
                                                  /* attachToRoot = */ false));
            }
        }

        // Initialize all the non-empty tiles again to update.
        for (int i = 0; i < tileMax; i++) {
            ExploreSitesTileView tileView = (ExploreSitesTileView) mTileView.getChildAt(i);
            final ExploreSitesSite site = sites.get(i);
            tileView.initialize(site, mIconGenerator);

            CategoryCardInteractionDelegate interactionDelegate =
                    new CategoryCardInteractionDelegate(site);
            tileView.setOnClickListener(interactionDelegate);
            tileView.setOnCreateContextMenuListener(interactionDelegate);
        }
    }
}
