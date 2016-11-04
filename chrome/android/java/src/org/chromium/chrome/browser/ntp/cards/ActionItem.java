// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.cards;

import android.view.View;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.ntp.NewTabPageView.NewTabPageManager;
import org.chromium.chrome.browser.ntp.UiConfig;
import org.chromium.chrome.browser.ntp.snippets.SnippetsConfig;

/**
 * Item that allows the user to perform an action on the NTP.
 */
class ActionItem extends OptionalLeaf {
    private final SuggestionsCategoryInfo mCategoryInfo;
    private final SuggestionsSection mParentSection;

    private boolean mImpressionTracked = false;

    public ActionItem(SuggestionsCategoryInfo categoryInfo, SuggestionsSection section) {
        super(section);
        mCategoryInfo = categoryInfo;
        mParentSection = section;
    }

    @Override
    public int getItemViewType() {
        return ItemViewType.ACTION;
    }

    @Override
    protected void onBindViewHolder(NewTabPageViewHolder holder) {
        assert holder instanceof ViewHolder;
        ((ViewHolder) holder).onBindViewHolder(this);
    }

    @Override
    public boolean isShown() {
        return mCategoryInfo.hasMoreButton(mParentSection.hasSuggestions());
    }

    private int getPosition() {
        // TODO(dgn): looks dodgy. Confirm that's what we want.
        return mParentSection.getSuggestionsCount();
    }

    private void performAction(NewTabPageManager manager, NewTabPageAdapter adapter) {
        manager.trackSnippetCategoryActionClick(mCategoryInfo.getCategory(), getPosition());

        if (mCategoryInfo.hasViewAllAction()) {
            mCategoryInfo.performViewAllAction(manager);
            return;
        }

        if (mCategoryInfo.hasMoreAction() && mParentSection.hasSuggestions()) {
            manager.getSuggestionsSource().fetchSuggestions(
                    mCategoryInfo.getCategory(), mParentSection.getDisplayedSuggestionIds());
            return;
        }

        if (mCategoryInfo.hasReloadAction()) {
            // TODO(dgn): reload only the current section. https://crbug.com/634892
            adapter.reloadSnippets();
        }

        // Should not be reached. Otherwise the action item was shown at an inappropriate moment.
        assert false;
    }

    public static class ViewHolder extends CardViewHolder {
        private ActionItem mActionListItem;

        public ViewHolder(final NewTabPageRecyclerView recyclerView,
                final NewTabPageManager manager, UiConfig uiConfig) {
            super(R.layout.new_tab_page_action_card, recyclerView, uiConfig);

            itemView.findViewById(R.id.action_button)
                    .setOnClickListener(new View.OnClickListener() {
                        @Override
                        public void onClick(View v) {
                            mActionListItem.performAction(
                                    manager, recyclerView.getNewTabPageAdapter());
                        }
                    });

            new ImpressionTracker(itemView, new ImpressionTracker.Listener() {
                @Override
                public void onImpression() {
                    if (mActionListItem != null && !mActionListItem.mImpressionTracked) {
                        mActionListItem.mImpressionTracked = true;
                        manager.trackSnippetCategoryActionImpression(
                                mActionListItem.mCategoryInfo.getCategory(),
                                mActionListItem.getPosition());
                    }
                }
            });
        }

        @Override
        public boolean isDismissable() {
            return SnippetsConfig.isSectionDismissalEnabled()
                    && !mActionListItem.mParentSection.hasSuggestions();
        }

        public void onBindViewHolder(ActionItem item) {
            super.onBindViewHolder();
            mActionListItem = item;
        }
    }
}
