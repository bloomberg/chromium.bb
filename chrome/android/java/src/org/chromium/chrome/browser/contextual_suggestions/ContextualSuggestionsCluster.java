// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextual_suggestions;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.ntp.cards.ChildNode;
import org.chromium.chrome.browser.ntp.cards.InnerNode;
import org.chromium.chrome.browser.ntp.cards.ItemViewType;
import org.chromium.chrome.browser.ntp.cards.NewTabPageViewHolder;
import org.chromium.chrome.browser.ntp.cards.NodeVisitor;
import org.chromium.chrome.browser.ntp.cards.SuggestionsCategoryInfo;
import org.chromium.chrome.browser.ntp.snippets.ContentSuggestionsCardLayout;
import org.chromium.chrome.browser.ntp.snippets.KnownCategories;
import org.chromium.chrome.browser.ntp.snippets.SectionHeader;
import org.chromium.chrome.browser.ntp.snippets.SnippetArticle;
import org.chromium.chrome.browser.suggestions.ContentSuggestionsAdditionalAction;

import java.util.ArrayList;
import java.util.List;
import java.util.Set;

/** A node in a tree that groups contextual suggestions in a cluster of related items. */
public class ContextualSuggestionsCluster extends InnerNode {
    private final String mTitle;
    private final List<SnippetArticle> mSuggestions = new ArrayList<>();

    private SectionHeader mHeader;
    private boolean mShouldShowTitle = true;

    /** Creates a new contextual suggestions cluster with provided title. */
    public ContextualSuggestionsCluster(String title) {
        mTitle = title;
    }

    /** @return A title related to this cluster */
    public String getTitle() {
        return mTitle;
    }

    /** @return A list of suggestions in this cluster */
    public List<SnippetArticle> getSuggestions() {
        return mSuggestions;
    }

    /** @param showTitle Whether the cluster title should be shown. */
    public void setShouldShowTitle(boolean showTitle) {
        mShouldShowTitle = showTitle;
    }

    /**
     * Called to build the tree node's children. Should be called after all suggestions have been
     * added.
     */
    public void buildChildren() {
        if (mShouldShowTitle) {
            mHeader = new SectionHeader(mTitle);
            addChild(mHeader);
        }

        SuggestionsList suggestionsList = new SuggestionsList();
        suggestionsList.addAll(mSuggestions);
        addChild(suggestionsList);
    }

    /** A tree node that holds a list of suggestions. */
    private static class SuggestionsList extends ChildNode {
        private final List<SnippetArticle> mSuggestions = new ArrayList<>();

        private final SuggestionsCategoryInfo mCategoryInfo;

        public SuggestionsList() {
            mCategoryInfo = new SuggestionsCategoryInfo(KnownCategories.CONTEXTUAL, "",
                    ContentSuggestionsCardLayout.FULL_CARD, ContentSuggestionsAdditionalAction.NONE,
                    false, "");
        }

        @Override
        protected int getItemCountForDebugging() {
            return mSuggestions.size();
        }

        @Override
        @ItemViewType
        public int getItemViewType(int position) {
            checkIndex(position);
            return ItemViewType.SNIPPET;
        }

        @Override
        public void onBindViewHolder(NewTabPageViewHolder holder, int position) {
            checkIndex(position);
            SnippetArticle suggestion = getSuggestionAt(position);
            ((ContextualSuggestionCardViewHolder) holder)
                    .onBindViewHolder(suggestion, mCategoryInfo);
        }

        private SnippetArticle getSuggestionAt(int position) {
            return mSuggestions.get(position);
        }

        public void addAll(List<SnippetArticle> suggestions) {
            if (suggestions.isEmpty()) return;

            int insertionPointIndex = mSuggestions.size();
            mSuggestions.addAll(suggestions);
            notifyItemRangeInserted(insertionPointIndex, suggestions.size());
        }

        @Override
        public void visitItems(NodeVisitor visitor) {
            for (SnippetArticle suggestion : mSuggestions) {
                visitor.visitSuggestion(suggestion);
            }
        }

        @Override
        public Set<Integer> getItemDismissalGroup(int position) {
            // Contextual suggestions are not dismissible.
            assert false;

            return null;
        }

        @Override
        public void dismissItem(int position, Callback<String> itemRemovedCallback) {
            // Contextual suggestions are not dismissible.
            assert false;
        }
    }
}
