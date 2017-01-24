// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.cards;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.browser.ntp.NewTabPage.DestructionObserver;
import org.chromium.chrome.browser.ntp.NewTabPageUma;
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
import org.chromium.chrome.browser.suggestions.PartialUpdateId;
import org.chromium.chrome.browser.suggestions.SuggestionsRanker;
import org.chromium.chrome.browser.suggestions.SuggestionsUiDelegate;

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

    // Keep track of how many suggestions have been seen by the user so that we replace only
    // suggestions that have not been seen, yet.
    private int mNumberOfSuggestionsSeen;

    /**
     * Delegate interface that allows dismissing this section without introducing
     * a circular dependency.
     */
    public interface Delegate {
        void dismissSection(SuggestionsSection section);
    }

    public SuggestionsSection(Delegate delegate, SuggestionsUiDelegate uiDelegate,
            SuggestionsRanker ranker, OfflinePageBridge offlinePageBridge,
            SuggestionsCategoryInfo info) {
        mDelegate = delegate;
        mCategoryInfo = info;
        mOfflinePageBridge = offlinePageBridge;

        mHeader = new SectionHeader(info.getTitle());
        mSuggestionsList = new SuggestionsList(uiDelegate, ranker, info);
        mStatus = StatusItem.createNoSuggestionsItem(info);
        mMoreButton = new ActionItem(this, ranker);
        mProgressIndicator = new ProgressItem();
        addChildren(mHeader, mSuggestionsList, mStatus, mMoreButton, mProgressIndicator);

        setupOfflinePageBridgeObserver(uiDelegate);
        refreshChildrenVisibility();
    }

    private static class SuggestionsList extends ChildNode implements Iterable<SnippetArticle> {
        private final List<SnippetArticle> mSuggestions = new ArrayList<>();

        // TODO(crbug.com/677672): Replace by SuggestionSource when it handles destruction.
        private final SuggestionsUiDelegate mUiDelegate;
        private final SuggestionsRanker mSuggestionsRanker;
        private final SuggestionsCategoryInfo mCategoryInfo;

        public SuggestionsList(SuggestionsUiDelegate uiDelegate, SuggestionsRanker ranker,
                SuggestionsCategoryInfo categoryInfo) {
            mUiDelegate = uiDelegate;
            mSuggestionsRanker = ranker;
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
        public void onBindViewHolder(NewTabPageViewHolder holder, int position) {
            checkIndex(position);
            assert holder instanceof SnippetArticleViewHolder;
            SnippetArticle suggestion = getSuggestionAt(position);
            mSuggestionsRanker.rankSuggestion(suggestion);
            ((SnippetArticleViewHolder) holder).onBindViewHolder(suggestion, mCategoryInfo);
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

        /**
         * Clears all suggestions except for the first {@code n} suggestions.
         */
        private void clearAllButFirstN(int n) {
            int itemCount = mSuggestions.size();
            if (itemCount > n) {
                mSuggestions.subList(n, itemCount).clear();
                notifyItemRangeRemoved(n, itemCount - 1);
            }
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
            SuggestionsSource suggestionsSource = mUiDelegate.getSuggestionsSource();
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
            int index = mSuggestions.indexOf(article);
            // The suggestions could have been removed / replaced in the meantime.
            if (index == -1) return;

            Long oldId = article.getOfflinePageOfflineId();
            article.setOfflinePageOfflineId(newId);

            if ((oldId == null) == (newId == null)) return;
            notifyItemChanged(index, PartialUpdateId.OFFLINE_BADGE);
        }
    }

    private void setupOfflinePageBridgeObserver(SuggestionsUiDelegate uiDelegate) {
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

        uiDelegate.addDestructionObserver(new DestructionObserver() {
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

    @Override
    public void onBindViewHolder(NewTabPageViewHolder holder, int position) {
        super.onBindViewHolder(holder, position);
        childSeen(position);
    }

    /**
     * Sets the child at position {@code position} as being seen by the user.
     * @param position Position in the list being shown (the first suggestion being at index 1,
     * as at index 0, there is a non-suggestion).
     */
    private void childSeen(int position) {
        Log.d(TAG, "childSeen: position %d in category %d", position, mCategoryInfo.getCategory());
        assert getStartingOffsetForChild(mSuggestionsList) == 1;
        // We assume all non-snippet cards come after all cards of type SNIPPET.
        if (getItemViewType(position) == ItemViewType.SNIPPET) {
            // As asserted above, first suggestion has position 1, etc., so the position of this
            // child coincides with the number of suggestions above this child (including this one).
            mNumberOfSuggestionsSeen =
                    Math.max(mNumberOfSuggestionsSeen, position);
        }
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

    /**
     * Puts {@code suggestions} into this section. It can either replace all existing suggestions
     * with the new ones or append the new suggestions at the end of the list. This call may have no
     * or only partial effect if changing the list of suggestions is not allowed (e.g. because the
     * user has already seen the suggestions).
     * @param suggestions The new list of suggestions for the given category.
     * @param status The new category status.
     * @param replaceExisting If true, {@code suggestions} replace the current list of suggestions.
     * If false, {@code suggestions} are appended to current list of suggestions.
     */
    public void setSuggestions(List<SnippetArticle> suggestions, @CategoryStatusEnum int status,
            boolean replaceExisting) {
        Log.d(TAG, "setSuggestions: previous number of suggestions: %d; replace existing: %b",
                mSuggestionsList.getItemCount(), replaceExisting);
        if (!SnippetsBridge.isCategoryStatusAvailable(status)) mSuggestionsList.clear();

        // Remove suggestions to be replaced.
        if (replaceExisting && hasSuggestions()) {
            if (CardsVariationParameters.ignoreUpdatesForExistingSuggestions()) {
                Log.d(TAG, "setSuggestions: replacing existing suggestion disabled");
                NewTabPageUma.recordUIUpdateResult(NewTabPageUma.UI_UPDATE_FAIL_DISABLED);
                return;
            }

            if (mNumberOfSuggestionsSeen >= getSuggestionsCount()) {
                Log.d(TAG, "setSuggestions: replacing existing suggestion not possible, all seen");
                NewTabPageUma.recordUIUpdateResult(NewTabPageUma.UI_UPDATE_FAIL_ALL_SEEN);
                return;
            }

            Log.d(TAG, "setSuggestions: keeping the first %d suggestion",
                        mNumberOfSuggestionsSeen);
            mSuggestionsList.clearAllButFirstN(mNumberOfSuggestionsSeen);

            if (mNumberOfSuggestionsSeen > 0) {
                // Make sure that mSuggestionsList will contain as many elements as newly provided
                // in suggestions. Remove the kept first element from the new collection, if it
                // repeats there. Otherwise, remove the last element of the new collection.
                int targetCountToAppend =
                        Math.max(0, suggestions.size() - mNumberOfSuggestionsSeen);
                for (SnippetArticle suggestion : mSuggestionsList) {
                    suggestions.remove(suggestion);
                }
                if (suggestions.size() > targetCountToAppend) {
                    Log.d(TAG, "setSuggestions: removing %d excess elements from the end",
                            suggestions.size() - targetCountToAppend);
                    suggestions.subList(targetCountToAppend, suggestions.size()).clear();
                }
            }
            NewTabPageUma.recordNumberOfSuggestionsSeenBeforeUIUpdateSuccess(
                    mNumberOfSuggestionsSeen);
            NewTabPageUma.recordUIUpdateResult(NewTabPageUma.UI_UPDATE_SUCCESS_REPLACED);
        } else {
            NewTabPageUma.recordUIUpdateResult(NewTabPageUma.UI_UPDATE_SUCCESS_APPENDED);
        }

        mProgressIndicator.setVisible(SnippetsBridge.isCategoryLoading(status));

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
