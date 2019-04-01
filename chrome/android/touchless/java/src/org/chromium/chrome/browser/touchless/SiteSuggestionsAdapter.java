// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touchless;

import static org.chromium.chrome.browser.touchless.SiteSuggestionsCoordinator.CURRENT_INDEX_KEY;
import static org.chromium.chrome.browser.touchless.SiteSuggestionsCoordinator.ITEM_COUNT_KEY;
import static org.chromium.chrome.browser.touchless.SiteSuggestionsCoordinator.SUGGESTIONS_KEY;

import android.support.annotation.IntDef;
import android.support.annotation.Nullable;
import android.support.graphics.drawable.VectorDrawableCompat;
import android.support.v7.widget.LinearLayoutManager;
import android.view.ContextMenu;
import android.view.View;
import android.widget.TextView;

import org.chromium.chrome.browser.UrlConstants;
import org.chromium.chrome.browser.native_page.ContextMenuManager;
import org.chromium.chrome.browser.suggestions.SuggestionsNavigationDelegate;
import org.chromium.chrome.browser.widget.RoundedIconGenerator;
import org.chromium.chrome.touchless.R;
import org.chromium.ui.modelutil.ForwardingListObservable;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyObservable;
import org.chromium.ui.modelutil.RecyclerViewAdapter;
import org.chromium.ui.mojom.WindowOpenDisposition;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Recycler view adapter for Touchless Suggestions carousel.
 *
 * Allows for an almost-infinite side-scrolling carousel with snapping focus in the center if
 * there are 2 or more items; does not scroll if there is 1 or fewer items.
 *
 * Focus changes on containing items will cause the passed-in title view to change.
 */
class SiteSuggestionsAdapter extends ForwardingListObservable<PropertyKey>
        implements RecyclerViewAdapter.Delegate<
                           SiteSuggestionsViewHolderFactory.SiteSuggestionsViewHolder, PropertyKey>,
                   PropertyObservable.PropertyObserver<PropertyKey> {
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
    private LinearLayoutManager mLayoutManager;
    private TextView mTitleView;

    /**
     * @param model The main property model coming from {@link SiteSuggestionsCoordinator}. Contains
     *         properties for a list of suggestions, number of items, and current focused index.
     * @param iconGenerator An icon generator for creating icons.
     * @param navigationDelegate delegate for navigation controls
     * @param contextMenuManager handles context menu creation
     * @param layoutManager the layout manager controlling this recyclerview and adapter
     * @param titleView the view to update site title when focus changes.
     */
    SiteSuggestionsAdapter(PropertyModel model, RoundedIconGenerator iconGenerator,
            SuggestionsNavigationDelegate navigationDelegate, ContextMenuManager contextMenuManager,
            LinearLayoutManager layoutManager, TextView titleView) {
        mModel = model;
        mIconGenerator = iconGenerator;
        mNavDelegate = navigationDelegate;
        mContextMenuManager = contextMenuManager;
        mLayoutManager = layoutManager;
        mTitleView = titleView;

        mModel.get(SUGGESTIONS_KEY).addObserver(this);
        mModel.addObserver(this);
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
        if (isAllAppsPosition(position)) return ViewType.ALL_APPS_TYPE;
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

        if (holder.getItemViewType() == ViewType.ALL_APPS_TYPE) {
            tile.setIconDrawable(VectorDrawableCompat.create(tile.getResources(),
                    R.drawable.ic_apps_black_24dp, tile.getContext().getTheme()));
            // If explore sites, clicks navigate to the Explore URL.
            tile.setOnClickListener(
                    (view)
                            -> mNavDelegate.navigateToSuggestionUrl(
                                    WindowOpenDisposition.CURRENT_TAB, UrlConstants.EXPLORE_URL));
        } else if (holder.getItemViewType() == ViewType.SUGGESTION_TYPE) {
            // If site suggestion, attach context menu handler; clicks navigate to site url.
            // Subtract 1 from position % MAX_TILES to account for "all apps" taking up one space.
            PropertyModel item =
                    mModel.get(SUGGESTIONS_KEY).get(getModelPositionFromAdapterPosition(position));
            // Only update the icon for icon updates.
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

    @Override
    public void onPropertyChanged(
            PropertyObservable<PropertyKey> source, @Nullable PropertyKey propertyKey) {
        if (propertyKey == CURRENT_INDEX_KEY) {
            // When the current index changes, we want to scroll to position and update the title.
            int position = mModel.get(CURRENT_INDEX_KEY);
            mLayoutManager.scrollToPosition(position);
            if (isAllAppsPosition(position)) {
                mTitleView.setText(R.string.ntp_all_apps);
            } else {
                mTitleView.setText(mModel.get(SUGGESTIONS_KEY)
                                           .get(getModelPositionFromAdapterPosition(position))
                                           .get(SiteSuggestionModel.TITLE_KEY));
            }
        }
    }

    @Override
    public void notifyItemRangeInserted(int index, int count) {
        if (mModel.get(SUGGESTIONS_KEY).size() == 1) {
            // When we just added something for the first time,
            // we would change from non-scrolling to infinite-scrolling.
            super.notifyItemRangeInserted(1, Integer.MAX_VALUE - 1);
        } else {
            // Otherwise we are already infinite-scrolling, so just tell recyclerview
            // that everything changed.
            super.notifyItemRangeChanged(0, Integer.MAX_VALUE, null);
        }
    }

    @Override
    public void notifyItemRangeRemoved(int index, int count) {
        if (mModel.get(SUGGESTIONS_KEY).size() == 0) {
            // When we removed the last item in the model, we would go from infinite scroll
            // back to non-scrolling. Notify RecyclerView to remove everything.
            super.notifyItemRangeRemoved(1, Integer.MAX_VALUE - 1);
        } else {
            // Otherwise we are already infinite-scrolling, so just tell recyclerview that
            // everything has changed.
            super.notifyItemRangeChanged(0, Integer.MAX_VALUE, null);
        }
    }

    @Override
    public void notifyItemRangeChanged(int index, int count, @Nullable PropertyKey payload) {
        // When something has changed, assume everything has changed.
        super.notifyItemRangeChanged(0, Integer.MAX_VALUE, payload);
    }

    public void destroy() {
        mModel.removeObserver(this);
        mModel.get(SUGGESTIONS_KEY).removeObserver(this);
    }

    private int getModelPositionFromAdapterPosition(int adapterPosition) {
        return adapterPosition % mModel.get(ITEM_COUNT_KEY) - 1;
    }

    private boolean isAllAppsPosition(int adapterPosition) {
        return mModel.get(ITEM_COUNT_KEY) == 1
                || getModelPositionFromAdapterPosition(adapterPosition) < 0;
    }
}
