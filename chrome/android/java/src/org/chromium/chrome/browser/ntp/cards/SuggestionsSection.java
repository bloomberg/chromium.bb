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
import java.util.List;
import java.util.Map;

/**
 * A group of suggestions, with a header, a status card, and a progress indicator. This is
 * responsible for tracking whether its suggestions have been saved offline.
 */
public class SuggestionsSection extends InnerNode {
    private static final String TAG = "NtpCards";

    private final List<TreeNode> mChildren = new ArrayList<>();
    private final List<SnippetArticle> mSuggestions = new ArrayList<>();
    private final SectionHeader mHeader;
    private final TreeNode mSuggestionsList = new SuggestionsList(this);
    private final StatusItem mStatus;
    private final ProgressItem mProgressIndicator = new ProgressItem();
    private final ActionItem mMoreButton;
    private final SuggestionsCategoryInfo mCategoryInfo;
    private final OfflinePageDownloadBridge mOfflinePageDownloadBridge;

    public SuggestionsSection(NodeParent parent, SuggestionsCategoryInfo info,
            NewTabPageManager manager, OfflinePageDownloadBridge offlinePageDownloadBridge) {
        super(parent);
        mHeader = new SectionHeader(info.getTitle());
        mCategoryInfo = info;
        mMoreButton = new ActionItem(info, this);
        mStatus = StatusItem.createNoSuggestionsItem(info);
        resetChildren();

        mOfflinePageDownloadBridge = offlinePageDownloadBridge;
        // We need to setup a listener because the OfflinePageDownloadBridge won't be available
        // when Chrome is freshly opened.
        setupOfflinePageDownloadBridgeObserver(manager);
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

    private class SuggestionsList extends ChildNode {
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
    }

    @Override
    protected List<TreeNode> getChildren() {
        return mChildren;
    }

    private void resetChildren() {
        mChildren.clear();
        mChildren.add(mHeader);
        mChildren.add(mSuggestionsList);

        if (mSuggestions.isEmpty()) mChildren.add(mStatus);
        mChildren.add(mMoreButton);
        if (mSuggestions.isEmpty()) mChildren.add(mProgressIndicator);
    }

    public void removeSuggestion(SnippetArticle suggestion) {
        int removedIndex = mSuggestions.indexOf(suggestion);
        if (removedIndex == -1) return;

        mSuggestions.remove(removedIndex);

        resetChildren();

        // Note: Keep this coherent with getItems()
        int globalRemovedIndex = removedIndex + 1; // Header has index 0 in the section.
        notifyItemRemoved(globalRemovedIndex);

        // If we still have some suggestions, we are done. Otherwise, we'll have to notify about the
        // status-related items (status card, action card, and progress indicator) that are
        // now present.
        if (!hasSuggestions()) notifyItemRangeInserted(globalRemovedIndex, 3);
    }

    public void removeSuggestionById(String idWithinCategory) {
        for (SnippetArticle suggestion : mSuggestions) {
            if (suggestion.mIdWithinCategory.equals(idWithinCategory)) {
                removeSuggestion(suggestion);
                return;
            }
        }
    }

    public boolean hasSuggestions() {
        return !mSuggestions.isEmpty();
    }

    public int getSuggestionsCount() {
        return mSuggestions.size();
    }

    public String[] getDisplayedSuggestionIds() {
        String[] suggestionIds = new String[mSuggestions.size()];
        for (int i = 0; i < mSuggestions.size(); ++i) {
            suggestionIds[i] = mSuggestions.get(i).mIdWithinCategory;
        }
        return suggestionIds;
    }

    public void addSuggestions(List<SnippetArticle> suggestions, @CategoryStatusEnum int status) {
        int itemCountBefore = getItemCount();
        setStatusInternal(status);

        Log.d(TAG, "addSuggestions: current number of suggestions: %d", mSuggestions.size());

        int sizeBefore = suggestions.size();

        // TODO(dgn): remove once the backend stops sending duplicates.
        if (suggestions.removeAll(mSuggestions)) {
            Log.d(TAG, "addSuggestions: Removed duplicates from incoming suggestions. "
                            + "Count changed from %d to %d",
                    sizeBefore, suggestions.size());
        }

        mSuggestions.addAll(suggestions);

        markSnippetsAvailableOffline();

        resetChildren();
        // TODO(dgn): change to handle only adding new items, or handling no modifications.
        notifySectionChanged(itemCountBefore);
    }


    /** Checks which SnippetArticles are available offline, and updates them accordingly. */
    private void markSnippetsAvailableOffline() {
        Map<String, String> urlToOfflineGuid = new HashMap<>();

        for (OfflinePageDownloadItem item : mOfflinePageDownloadBridge.getAllItems()) {
            urlToOfflineGuid.put(item.getUrl(), item.getGuid());
        }

        for (final SnippetArticle article : mSuggestions) {
            String guid = urlToOfflineGuid.get(article.mUrl);
            guid = guid != null ? guid : urlToOfflineGuid.get(article.mAmpUrl);
            article.setOfflinePageDownloadGuid(guid);
        }
    }

    /** Sets the status for the section. Some statuses can cause the suggestions to be cleared. */
    public void setStatus(@CategoryStatusEnum int status) {
        int itemCountBefore = getItemCount();
        setStatusInternal(status);
        resetChildren();
        notifySectionChanged(itemCountBefore);
    }

    private void setStatusInternal(@CategoryStatusEnum int status) {
        if (!SnippetsBridge.isCategoryStatusAvailable(status)) mSuggestions.clear();

        mProgressIndicator.setVisible(SnippetsBridge.isCategoryLoading(status));
    }

    @CategoryInt
    public int getCategory() {
        return mCategoryInfo.getCategory();
    }

    @Override
    public int getDismissSiblingPosDelta(int position) {
        // The only dismiss siblings we have so far are the More button and the status card.
        // Exit early if there is no More button.
        if (!mMoreButton.isShown()) return 0;

        // When there are suggestions we won't have contiguous status and action items.
        if (hasSuggestions()) return 0;

        TreeNode child = getChildForPosition(position);

        // The sibling of the more button is the status card, that should be right above.
        if (child == mMoreButton) return -1;

        // The sibling of the status card is the more button when it exists, should be right below.
        if (child == mStatus) return 1;

        return 0;
    }

    private void notifySectionChanged(int itemCountBefore) {
        int itemCountAfter = getItemCount();

        // The header is stable in sections. Don't notify about it.
        final int startPos = 1;
        itemCountBefore--;
        itemCountAfter--;

        notifyItemRangeChanged(startPos, Math.min(itemCountBefore, itemCountAfter));
        if (itemCountBefore < itemCountAfter) {
            notifyItemRangeInserted(startPos + itemCountBefore, itemCountAfter - itemCountBefore);
        } else if (itemCountBefore > itemCountAfter) {
            notifyItemRangeRemoved(startPos + itemCountAfter, itemCountBefore - itemCountAfter);
        }
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
