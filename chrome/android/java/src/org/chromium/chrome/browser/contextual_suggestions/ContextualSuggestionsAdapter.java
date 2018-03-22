// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextual_suggestions;

import android.content.Context;
import android.support.v7.widget.RecyclerView;
import android.view.ViewGroup;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.modelutil.RecyclerViewAdapter;
import org.chromium.chrome.browser.ntp.cards.ItemViewType;
import org.chromium.chrome.browser.ntp.cards.SuggestionsCategoryInfo;
import org.chromium.chrome.browser.ntp.snippets.ContentSuggestionsCardLayout;
import org.chromium.chrome.browser.ntp.snippets.KnownCategories;
import org.chromium.chrome.browser.ntp.snippets.SnippetArticle;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.suggestions.ContentSuggestionsAdditionalAction;
import org.chromium.chrome.browser.suggestions.SuggestionsRecyclerView;
import org.chromium.chrome.browser.suggestions.SuggestionsUiDelegate;
import org.chromium.chrome.browser.widget.displaystyle.UiConfig;

/**
 * An adapter that contains the view binder for the content component.
 */
class ContextualSuggestionsAdapter
        extends RecyclerViewAdapter<SnippetArticle, ContextualSuggestionCardViewHolder> {
    private class ContextualSuggestionsViewBinder
            implements ViewBinder<SnippetArticle, ContextualSuggestionCardViewHolder> {
        @Override
        public ContextualSuggestionCardViewHolder onCreateViewHolder(
                ViewGroup parent, int viewType) {
            assert viewType == ItemViewType.SNIPPET;

            // TODO(twellington): Hook up ContextMenuManager.
            return new ContextualSuggestionCardViewHolder(mRecyclerView, null, mUiDelegate,
                    mUiConfig, OfflinePageBridge.getForProfile(mProfile));
        }

        @Override
        public void onBindViewHolder(
                ContextualSuggestionCardViewHolder holder, SnippetArticle item) {
            holder.onBindViewHolder(item, mCategoryInfo);
        }
    }

    private final Profile mProfile;
    private final UiConfig mUiConfig;
    private final SuggestionsUiDelegate mUiDelegate;
    private final SuggestionsCategoryInfo mCategoryInfo;

    private SuggestionsRecyclerView mRecyclerView;

    /**
     * Construct a new {@link ContextualSuggestionsAdapter}.
     * @param context The {@link Context} used to retrieve resources.
     * @param profile The regular {@link Profile}.
     * @param uiConfig The {@link UiConfig} used to adjust view display.
     * @param uiDelegate The {@link SuggestionsUiDelegate} used to help construct items in the
     *                   content view.
     * @param model The {@link ContextualSuggestionsModel} for the component.
     */
    ContextualSuggestionsAdapter(Context context, Profile profile, UiConfig uiConfig,
            SuggestionsUiDelegate uiDelegate, ContextualSuggestionsModel model) {
        super(model.mSuggestionsList);

        setViewBinder(new ContextualSuggestionsViewBinder());

        mProfile = profile;
        mUiConfig = uiConfig;
        mUiDelegate = uiDelegate;

        mCategoryInfo = new SuggestionsCategoryInfo(KnownCategories.CONTEXTUAL,
                context.getString(R.string.contextual_suggestions_title),
                ContentSuggestionsCardLayout.FULL_CARD, ContentSuggestionsAdditionalAction.NONE,
                false, "");
    }

    @Override
    @ItemViewType
    public int getItemViewType(int position) {
        return ItemViewType.SNIPPET;
    }

    @Override
    public void onAttachedToRecyclerView(RecyclerView recyclerView) {
        mRecyclerView = (SuggestionsRecyclerView) recyclerView;
    }

    @Override
    public void onDetachedFromRecyclerView(RecyclerView recyclerView) {
        mRecyclerView = null;
    }

    @Override
    public void onViewRecycled(ContextualSuggestionCardViewHolder holder) {
        holder.recycle();
    }
}
