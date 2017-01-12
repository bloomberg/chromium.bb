// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.cards;

import org.chromium.base.Callback;
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
import org.chromium.chrome.browser.ntp.snippets.SuggestionsSource;
import org.chromium.chrome.browser.offlinepages.ClientId;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge;
import org.chromium.chrome.browser.offlinepages.OfflinePageItem;

import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;

/**
 * A group of suggestions, with a header, a status card, and a progress indicator. This is
 * responsible for tracking whether its suggestions have been saved offline.
 */
public class SuggestionsSection extends InnerNode {
    private static final String TAG = "NtpCards";

    private final Delegate mDelegate;
    private final SuggestionsCategoryInfo mCategoryInfo;
    private final OfflinePageBridge mOfflinePageBridge;

    // Children
    private final SectionHeader mHeader;
    private final SuggestionsList mSuggestionsList;
    private final StatusItem mStatus;
    private final ActionItem mMoreButton;
    private final ProgressItem mProgressIndicator;

    private boolean mIsNtpDestroyed;

    /**
     * Delegate interface that allows dismissing this section without introducing
     * a circular dependency.
     */
    public interface Delegate {
        void dismissSection(SuggestionsSection section);
    }

    public SuggestionsSection(Delegate delegate, NewTabPageManager manager,
            OfflinePageBridge offlinePageBridge, SuggestionsCategoryInfo info) {
        mDelegate = delegate;
        mCategoryInfo = info;
        mOfflinePageBridge = offlinePageBridge;

        mHeader = new SectionHeader(info.getTitle());
        mSuggestionsList = new SuggestionsList(manager, info);
        mStatus = StatusItem.createNoSuggestionsItem(info);
        mMoreButton = new ActionItem(this);
        mProgressIndicator = new ProgressItem();
        addChildren(mHeader, mSuggestionsList, mStatus, mMoreButton, mProgressIndicator);

        setupOfflinePageBridgeObserver(manager);
        refreshChildrenVisibility();
    }

    private static class SuggestionsList extends ChildNode implements Iterable<SnippetArticle> {
        private final List<SnippetArticle> mSuggestions = new ArrayList<>();
        private final NewTabPageManager mNewTabPageManager;
        private final SuggestionsCategoryInfo mCategoryInfo;

        public SuggestionsList(NewTabPageManager newTabPageManager,
                SuggestionsCategoryInfo categoryInfo) {
            mNewTabPageManager = newTabPageManager;
            mCategoryInfo = categoryInfo;
        }

        @Override
        public int getItemCount() {
            return mSuggestions.size();
        }

        @Override
        @ItemViewType
        public int getItemViewType(int position) {
            checkIndex(position);
            return ItemViewType.SNIPPET;
        }

        @Override
        public void onBindViewHolder(
                NewTabPageViewHolder holder, int position, List<Object> payloads) {
            checkIndex(position);
            assert holder instanceof SnippetArticleViewHolder;
            ((SnippetArticleViewHolder) holder)
                    .onBindViewHolder(getSuggestionAt(position), mCategoryInfo, payloads);
        }

        @Override
        public SnippetArticle getSuggestionAt(int position) {
            return mSuggestions.get(position);
        }

        @Override
        public int getDismissSiblingPosDelta(int position) {
            checkIndex(position);
            return 0;
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

        public SnippetArticle remove(int position) {
            SnippetArticle suggestion = mSuggestions.remove(position);
            notifyItemRemoved(position);
            return suggestion;
        }

        @Override
        public Iterator<SnippetArticle> iterator() {
            return mSuggestions.iterator();
        }

        @Override
        public void dismissItem(int position, Callback<String> itemRemovedCallback) {
            checkIndex(position);
            SuggestionsSource suggestionsSource = mNewTabPageManager.getSuggestionsSource();
            if (suggestionsSource == null) {
                // It is possible for this method to be called after the NewTabPage has had
                // destroy() called. This can happen when
                // NewTabPageRecyclerView.dismissWithAnimation() is called and the animation ends
                // after the user has navigated away. In this case we cannot inform the native side
                // that the snippet has been dismissed (http://crbug.com/649299).
                return;
            }

            SnippetArticle suggestion = remove(position);
            suggestionsSource.dismissSuggestion(suggestion);
            itemRemovedCallback.onResult(suggestion.mTitle);
        }

        public void updateSuggestionOfflineId(SnippetArticle article, Long newId) {
            Long oldId = article.getOfflinePageOfflineId();
            article.setOfflinePageOfflineId(newId);

            if ((oldId == null) == (newId == null)) return;
            notifyItemChanged(mSuggestions.indexOf(article),
                    SnippetArticleViewHolder.PARTIAL_UPDATE_OFFLINE_ID);
        }
    }

    private void setupOfflinePageBridgeObserver(NewTabPageManager manager) {
        final OfflinePageBridge.OfflinePageModelObserver observer =
                new OfflinePageBridge.OfflinePageModelObserver() {
                    @Override
                    public void offlinePageModelLoaded() {
                        updateAllSnippetOfflineAvailability();
                    }

                    @Override
                    public void offlinePageAdded(OfflinePageItem addedPage) {
                        updateAllSnippetOfflineAvailability();
                    }

                    @Override
                    public void offlinePageDeleted(long offlineId, ClientId clientId) {
                        for (SnippetArticle article : mSuggestionsList) {
                            if (article.requiresExactOfflinePage()) continue;
                            Long articleOfflineId = article.getOfflinePageOfflineId();
                            if (articleOfflineId == null) continue;
                            if (articleOfflineId.longValue() != offlineId) continue;
                            // The old value cannot be simply removed without a request to the
                            // model, because there may be an older offline page for the same
                            // URL.
                            updateSnippetOfflineAvailability(article);
                        }
                    }
                };

        mOfflinePageBridge.addObserver(observer);

        manager.addDestructionObserver(new DestructionObserver() {
            @Override
            public void onDestroy() {
                mIsNtpDestroyed = true;
                mOfflinePageBridge.removeObserver(observer);
            }
        });
    }

    private void refreshChildrenVisibility() {
        mStatus.setVisible(!hasSuggestions());
        mMoreButton.refreshVisibility();
    }

    @Override
    public void dismissItem(int position, Callback<String> itemRemovedCallback) {
        if (!hasSuggestions()) {
            mDelegate.dismissSection(this);
            itemRemovedCallback.onResult(getHeaderText());
            return;
        }

        super.dismissItem(position, itemRemovedCallback);
    }

    @Override
    public void onItemRangeRemoved(TreeNode child, int index, int count) {
        super.onItemRangeRemoved(child, index, count);
        if (child == mSuggestionsList) refreshChildrenVisibility();
    }

    /**
     * Removes a suggestion. Does nothing if the ID is unknown.
     * @param idWithinCategory The ID of the suggestion to remove.
     */
    public void removeSuggestionById(String idWithinCategory) {
        int i = 0;
        for (SnippetArticle suggestion : mSuggestionsList) {
            if (suggestion.mIdWithinCategory.equals(idWithinCategory)) {
                mSuggestionsList.remove(i);
                return;
            }
            i++;
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
        for (SnippetArticle article : suggestions) {
            if (!article.requiresExactOfflinePage()) {
                updateSnippetOfflineAvailability(article);
            }
        }

        refreshChildrenVisibility();
    }

    private void updateSnippetOfflineAvailability(final SnippetArticle article) {
        // This method is not applicable to articles for which the exact offline id must specified.
        assert !article.requiresExactOfflinePage();
        if (!mOfflinePageBridge.isOfflinePageModelLoaded()) return;
        // TabId is relevant only for recent tab offline pages, which we do not handle here, so we
        // do not care about tab id.
        mOfflinePageBridge.selectPageForOnlineUrl(
                article.mUrl, /*tabId=*/0, new Callback<OfflinePageItem>() {
                    @Override
                    public void onResult(OfflinePageItem item) {
                        if (mIsNtpDestroyed) return;
                        mSuggestionsList.updateSuggestionOfflineId(
                                article, item == null ? null : item.getOfflineId());
                    }
                });
    }

    /**
     * Checks which SnippetArticles are available offline and updates them with offline id of the
     * matched offline page.
     */
    private void updateAllSnippetOfflineAvailability() {
        for (final SnippetArticle article : mSuggestionsList) {
            if (article.requiresExactOfflinePage()) continue;
            updateSnippetOfflineAvailability(article);
        }
    }

    /** Lets the {@link SuggestionsSection} know when a suggestion fetch has been started. */
    public void onFetchStarted() {
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
