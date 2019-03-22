// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touchless;

import static org.chromium.chrome.browser.touchless.SiteSuggestionsCoordinator.CURRENT_INDEX_KEY;
import static org.chromium.chrome.browser.touchless.SiteSuggestionsCoordinator.ITEM_COUNT_KEY;
import static org.chromium.chrome.browser.touchless.SiteSuggestionsCoordinator.SUGGESTIONS_KEY;

import android.support.annotation.IntDef;
import android.view.ContextMenu;
import android.view.View;

import org.chromium.chrome.browser.UrlConstants;
import org.chromium.chrome.browser.native_page.ContextMenuManager;
import org.chromium.chrome.browser.suggestions.SuggestionsNavigationDelegate;
import org.chromium.chrome.browser.widget.RoundedIconGenerator;
import org.chromium.ui.modelutil.ForwardingListObservable;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.RecyclerViewAdapter;
import org.chromium.ui.mojom.WindowOpenDisposition;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Recycler view adapter for Touchless Suggestions carousel.
 *
 * Allows for an infinite side-scrolling carousel with snapping focus in the center.
 */
class SiteSuggestionsAdapter
        extends ForwardingListObservable<PropertyKey> implements RecyclerViewAdapter.Delegate<
                SiteSuggestionsViewHolderFactory.SiteSuggestionsViewHolder, PropertyKey> {
    @IntDef({ViewType.ALL_APPS_TYPE, ViewType.SUGGESTION_TYPE})
    @Retention(RetentionPolicy.SOURCE)
    public @interface ViewType {
        int ALL_APPS_TYPE = 0;
        int SUGGESTION_TYPE = 1;
    }

    private class SiteSuggestionInteractionDelegate
            implements ContextMenuManager.Delegate, View.OnCreateContextMenuListener {
        private PropertyModel mSuggestion;

        public SiteSuggestionInteractionDelegate(PropertyModel model) {
            mSuggestion = model;
        }

        @Override
        public void onCreateContextMenu(
                ContextMenu menu, View v, ContextMenu.ContextMenuInfo menuInfo) {
            mContextMenuManager.createContextMenu(menu, v, this);
        }

        @Override
        public void openItem(int windowDisposition) {
            mNavDelegate.navigateToSuggestionUrl(
                    windowDisposition, mSuggestion.get(SiteSuggestionModel.URL_KEY));
        }

        @Override
        public void removeItem() {
            // TODO(chili): Show a toast with message.
        }

        @Override
        public String getUrl() {
            return mSuggestion.get(SiteSuggestionModel.URL_KEY);
        }

        @Override
        public boolean isItemSupported(int menuItemId) {
            // TODO(chili): add context menu items.
            return false;
        }

        @Override
        public void onContextMenuCreated() {}
    }

    private PropertyModel mModel;
    private RoundedIconGenerator mIconGenerator;
    private SuggestionsNavigationDelegate mNavDelegate;
    private ContextMenuManager mContextMenuManager;

    SiteSuggestionsAdapter(PropertyModel model, RoundedIconGenerator iconGenerator,
            SuggestionsNavigationDelegate navigationDelegate,
            ContextMenuManager contextMenuManager) {
        mModel = model;
        mIconGenerator = iconGenerator;
        mNavDelegate = navigationDelegate;
        mContextMenuManager = contextMenuManager;

        mModel.get(SUGGESTIONS_KEY).addObserver(this);
    }

    @Override
    public int getItemCount() {
        if (mModel.get(SUGGESTIONS_KEY).size() > 0) {
            return Integer.MAX_VALUE;
        }
        return 1;
    }

    @Override
    public int getItemViewType(int position) {
        int itemCount = mModel.get(ITEM_COUNT_KEY);
        if (itemCount == 1 || position % itemCount == 0) return ViewType.ALL_APPS_TYPE;
        return ViewType.SUGGESTION_TYPE;
    }

    @Override
    public void onBindViewHolder(SiteSuggestionsViewHolderFactory.SiteSuggestionsViewHolder holder,
            int position, PropertyKey payload) {
        SiteSuggestionsTileView tile = (SiteSuggestionsTileView) holder.itemView;
        // Updates focus change listener.
        tile.setOnFocusChangeListener((View v, boolean hasFocus) -> {
            if (hasFocus) {
                mModel.set(CURRENT_INDEX_KEY, position);
            }
        });

        if (getItemViewType(position) == ViewType.ALL_APPS_TYPE) {
            // If explore sites, clicks navigate to the Explore URL.
            tile.setOnClickListener(
                    (view)
                            -> mNavDelegate.navigateToSuggestionUrl(
                                    WindowOpenDisposition.CURRENT_TAB, UrlConstants.EXPLORE_URL));
        } else if (getItemViewType(position) == ViewType.SUGGESTION_TYPE) {
            // If site suggestion, attach context menu handler; clicks navigate to site url.
            int itemCount = mModel.get(ITEM_COUNT_KEY);
            // Subtract 1 from position % MAX_TILES to account for "all apps" taking up one space.
            PropertyModel item = mModel.get(SUGGESTIONS_KEY).get((position % itemCount) - 1);
            if (payload == SiteSuggestionModel.ICON_KEY) {
                tile.updateIcon(item.get(SiteSuggestionModel.ICON_KEY),
                        item.get(SiteSuggestionModel.TITLE_KEY));
            } else {
                tile.initialize(mIconGenerator);
                tile.updateIcon(item.get(SiteSuggestionModel.ICON_KEY),
                        item.get(SiteSuggestionModel.TITLE_KEY));

                tile.setOnClickListener((View v)
                                                -> mNavDelegate.navigateToSuggestionUrl(
                                                        WindowOpenDisposition.CURRENT_TAB,
                                                        item.get(SiteSuggestionModel.URL_KEY)));

                SiteSuggestionInteractionDelegate interactionDelegate =
                        new SiteSuggestionInteractionDelegate(item);
                tile.setOnCreateContextMenuListener(interactionDelegate);
            }
        }
    }
}
