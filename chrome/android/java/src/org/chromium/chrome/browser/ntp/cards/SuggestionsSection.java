// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.cards;

import org.chromium.base.Log;
import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.browser.ntp.NewTabPage.DestructionObserver;
import org.chromium.chrome.browser.ntp.NewTabPageView.NewTabPageManager;
import org.chromium.chrome.browser.ntp.snippets.CategoryInt;
import org.chromium.chrome.browser.ntp.snippets.CategoryStatus.CategoryStatusEnum;
import org.chromium.chrome.browser.ntp.snippets.SectionHeader;
import org.chromium.chrome.browser.ntp.snippets.SnippetArticle;
import org.chromium.chrome.browser.ntp.snippets.SnippetArticleViewHolder;
import org.chromium.chrome.browser.ntp.snippets.SnippetsBridge;
import org.chromium.chrome.browser.offlinepages.downloads.OfflinePageDownloadBridge;
import org.chromium.chrome.browser.offlinepages.downloads.OfflinePageDownloadItem;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.Iterator;
import java.util.List;
import java.util.Map;

/**
 * A group of suggestions, with a header, a status card, and a progress indicator. This is
 * responsible for tracking whether its suggestions have been saved offline.
 */
public class SuggestionsSection extends InnerNode {
    private static final String TAG = "NtpCards";

    private final SuggestionsCategoryInfo mCategoryInfo;
    private final OfflinePageDownloadBridge mOfflinePageDownloadBridge;
    private final List<TreeNode> mChildren = new ArrayList<>();

    // Children
    private final SectionHeader mHeader;
    private final SuggestionsList mSuggestionsList;
    private final StatusItem mStatus;
    private final ProgressItem mProgressIndicator;
    private final ActionItem mMoreButton;

    public SuggestionsSection(NodeParent parent, SuggestionsCategoryInfo info,
            NewTabPageManager manager, OfflinePageDownloadBridge offlinePageDownloadBridge) {
        super(parent);
        mCategoryInfo = info;
        mOfflinePageDownloadBridge = offlinePageDownloadBridge;

        mHeader = new SectionHeader(info.getTitle());
        mSuggestionsList = new SuggestionsList(this);
        mStatus = StatusItem.createNoSuggestionsItem(this);
        mMoreButton = new ActionItem(this);
        mProgressIndicator = new ProgressItem(this);
        initializeChildren();

        // We need to setup a listener because the OfflinePageDownloadBridge won't be available
        // when Chrome is freshly opened.
        setupOfflinePageDownloadBridgeObserver(manager);
    }

    private static class SuggestionsList extends ChildNode implements Iterable<SnippetArticle> {
        private final List<SnippetArticle> mSuggestions = new ArrayList<>();

        public SuggestionsList(NodeParent parent) {
            super(parent);
        }

        @Override
        public int getItemCount() {
            return mSuggestions.size();
        }

        @Override
        @ItemViewType
        public int getItemViewType(int position) {
            return ItemViewType.SNIPPET;
        }

        @Override
        public void onBindViewHolder(NewTabPageViewHolder holder, int position) {
            assert holder instanceof SnippetArticleViewHolder;
            ((SnippetArticleViewHolder) holder).onBindViewHolder(getSuggestionAt(position));
        }

        @Override
        public SnippetArticle getSuggestionAt(int position) {
            return mSuggestions.get(position);
        }

        @Override
        public int getDismissSiblingPosDelta(int position) {
            return 0;
        }

        public void remove(SnippetArticle suggestion) {
            int removedIndex = mSuggestions.indexOf(suggestion);
            if (removedIndex == -1) throw new IndexOutOfBoundsException();

            mSuggestions.remove(removedIndex);
            notifyItemRemoved(removedIndex);
        }

        public void clear() {
            int itemCount = mSuggestions.size();
            if (itemCount == 0) return;

            mSuggestions.clear();
            notifyItemRangeRemoved(0, itemCount);
        }

        public void addAll(List<SnippetArticle> suggestions) {
            if (suggestions.isEmpty()) return;

            int insertionPointIndex = mSuggestions.size();
            mSuggestions.addAll(suggestions);
            notifyItemRangeInserted(insertionPointIndex, suggestions.size());
        }

        @Override
        public Iterator<SnippetArticle> iterator() {
            return mSuggestions.iterator();
        }
    }

    @Override
    protected List<TreeNode> getChildren() {
        return mChildren;
    }

    private void initializeChildren() {
        mChildren.add(mHeader);
        mChildren.add(mSuggestionsList);

        // Optional leaves.
        mChildren.add(mStatus); // Needs to be refreshed when the status changes.

        mChildren.add(mMoreButton); // Needs to be refreshed when the suggestions change.
        mChildren.add(mProgressIndicator); // Needs to be refreshed when the suggestions change.
        refreshChildrenVisibility();
    }

    private void setupOfflinePageDownloadBridgeObserver(NewTabPageManager manager) {
        // TODO(peconn): Update logic to listen to specific events, not just recalculate every time.
        final OfflinePageDownloadBridge.Observer observer =
                new OfflinePageDownloadBridge.Observer() {
                    @Override
                    public void onItemsLoaded() {
                        markSnippetsAvailableOffline();
                    }

                    @Override
                    public void onItemAdded(OfflinePageDownloadItem item) {
                        markSnippetsAvailableOffline();
                    }

                    @Override
                    public void onItemDeleted(String guid) {
                        markSnippetsAvailableOffline();
                    }

                    @Override
                    public void onItemUpdated(OfflinePageDownloadItem item) {
                        markSnippetsAvailableOffline();
                    }
                };

        mOfflinePageDownloadBridge.addObserver(observer);

        manager.addDestructionObserver(new DestructionObserver() {
            @Override
            public void onDestroy() {
                mOfflinePageDownloadBridge.removeObserver(observer);
            }
        });
    }

    private void refreshChildrenVisibility() {
        mStatus.setVisible(!hasSuggestions());
        mMoreButton.refreshVisibility();
    }

    public void removeSuggestion(SnippetArticle suggestion) {
        mSuggestionsList.remove(suggestion);
        refreshChildrenVisibility();
    }

    public void removeSuggestionById(String idWithinCategory) {
        for (SnippetArticle suggestion : mSuggestionsList) {
            if (suggestion.mIdWithinCategory.equals(idWithinCategory)) {
                removeSuggestion(suggestion);
                return;
            }
        }
    }

    public boolean hasSuggestions() {
        return mSuggestionsList.getItemCount() != 0;
    }

    public int getSuggestionsCount() {
        return mSuggestionsList.getItemCount();
    }

    public String[] getDisplayedSuggestionIds() {
        String[] suggestionIds = new String[mSuggestionsList.getItemCount()];
        for (int i = 0; i < mSuggestionsList.getItemCount(); ++i) {
            suggestionIds[i] = mSuggestionsList.getSuggestionAt(i).mIdWithinCategory;
        }
        return suggestionIds;
    }

    public void addSuggestions(List<SnippetArticle> suggestions, @CategoryStatusEnum int status) {
        if (!SnippetsBridge.isCategoryStatusAvailable(status)) mSuggestionsList.clear();
        mProgressIndicator.setVisible(SnippetsBridge.isCategoryLoading(status));

        Log.d(TAG, "addSuggestions: current number of suggestions: %d",
                mSuggestionsList.getItemCount());

        int sizeBefore = suggestions.size();

        // TODO(dgn): remove once the backend stops sending duplicates.
        if (suggestions.removeAll(mSuggestionsList.mSuggestions)) {
            Log.d(TAG, "addSuggestions: Removed duplicates from incoming suggestions. "
                            + "Count changed from %d to %d",
                    sizeBefore, suggestions.size());
        }

        mSuggestionsList.addAll(suggestions);
        markSnippetsAvailableOffline();

        refreshChildrenVisibility();
    }


    /** Checks which SnippetArticles are available offline, and updates them accordingly. */
    private void markSnippetsAvailableOffline() {
        Map<String, String> urlToOfflineGuid = new HashMap<>();

        for (OfflinePageDownloadItem item : mOfflinePageDownloadBridge.getAllItems()) {
            urlToOfflineGuid.put(item.getUrl(), item.getGuid());
        }

        for (final SnippetArticle article : mSuggestionsList) {
            String guid = urlToOfflineGuid.get(article.mUrl);
            guid = guid != null ? guid : urlToOfflineGuid.get(article.mAmpUrl);
            article.setOfflinePageDownloadGuid(guid);
        }
    }

    /** Lets the {@link SuggestionsSection} know when the FetchMore action has been triggered. */
    public void onFetchMore() {
        mProgressIndicator.setVisible(true);
    }

    /** Sets the status for the section. Some statuses can cause the suggestions to be cleared. */
    public void setStatus(@CategoryStatusEnum int status) {
        if (!SnippetsBridge.isCategoryStatusAvailable(status)) mSuggestionsList.clear();
        mProgressIndicator.setVisible(SnippetsBridge.isCategoryLoading(status));
        refreshChildrenVisibility();
    }

    @CategoryInt
    public int getCategory() {
        return mCategoryInfo.getCategory();
    }

    @Override
    public int getDismissSiblingPosDelta(int position) {
        // The only dismiss siblings we have so far are the More button and the status card.
        // Exit early if there is no More button.
        if (!mMoreButton.isVisible()) return 0;

        // When there are suggestions we won't have contiguous status and action items.
        if (hasSuggestions()) return 0;

        TreeNode child = getChildForPosition(position);

        // The sibling of the more button is the status card, that should be right above.
        if (child == mMoreButton) return -1;

        // The sibling of the status card is the more button when it exists, should be right below.
        if (child == mStatus) return 1;

        return 0;
    }

    public SuggestionsCategoryInfo getCategoryInfo() {
        return mCategoryInfo;
    }

    public String getHeaderText() {
        return mHeader.getHeaderText();
    }

    /**
     * @return The progress indicator.
     */
    @VisibleForTesting
    ProgressItem getProgressItemForTesting() {
        return mProgressIndicator;
    }

    @VisibleForTesting
    ActionItem getActionItem() {
        return mMoreButton;
    }

    @VisibleForTesting
    StatusItem getStatusItem() {
        return mStatus;
    }
}
