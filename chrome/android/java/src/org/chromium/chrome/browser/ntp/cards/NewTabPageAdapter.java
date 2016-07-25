// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.cards;

import android.graphics.Canvas;
import android.support.v7.widget.RecyclerView;
import android.support.v7.widget.RecyclerView.Adapter;
import android.support.v7.widget.RecyclerView.ViewHolder;
import android.support.v7.widget.helper.ItemTouchHelper;
import android.view.ViewGroup;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.chrome.browser.ntp.NewTabPageLayout;
import org.chromium.chrome.browser.ntp.NewTabPageUma;
import org.chromium.chrome.browser.ntp.NewTabPageView.NewTabPageManager;
import org.chromium.chrome.browser.ntp.UiConfig;
import org.chromium.chrome.browser.ntp.snippets.ContentSuggestionsCategoryStatus;
import org.chromium.chrome.browser.ntp.snippets.SnippetArticleListItem;
import org.chromium.chrome.browser.ntp.snippets.SnippetArticleViewHolder;
import org.chromium.chrome.browser.ntp.snippets.SnippetHeaderListItem;
import org.chromium.chrome.browser.ntp.snippets.SnippetHeaderViewHolder;
import org.chromium.chrome.browser.ntp.snippets.SnippetsBridge;
import org.chromium.chrome.browser.ntp.snippets.SnippetsBridge.SnippetsObserver;

import java.util.ArrayList;
import java.util.List;

/**
 * A class that handles merging above the fold elements and below the fold cards into an adapter
 * that will be used to back the NTP RecyclerView. The first element in the adapter should always be
 * the above-the-fold view (containing the logo, search box, and most visited tiles) and subsequent
 * elements will be the cards shown to the user
 */
public class NewTabPageAdapter extends Adapter<NewTabPageViewHolder> implements SnippetsObserver {
    private static final String TAG = "Ntp";

    private final NewTabPageManager mNewTabPageManager;
    private final NewTabPageLayout mNewTabPageLayout;
    private final AboveTheFoldListItem mAboveTheFold;
    private final SnippetHeaderListItem mHeader;
    private final UiConfig mUiConfig;
    private StatusListItem mStatusCard;
    private final SpacingListItem mBottomSpacer;
    private final List<NewTabPageListItem> mItems;
    private final ItemTouchCallbacks mItemTouchCallbacks;
    private NewTabPageRecyclerView mRecyclerView;
    private int mProviderStatus;

    private SnippetsBridge mSnippetsBridge;

    private class ItemTouchCallbacks extends ItemTouchHelper.Callback {
        @Override
        public void onSwiped(ViewHolder viewHolder, int direction) {
            mRecyclerView.onItemDismissStarted(viewHolder.itemView);

            NewTabPageAdapter.this.dismissItem(viewHolder);
            addStatusCardIfNecessary();
        }

        @Override
        public void clearView(RecyclerView recyclerView, ViewHolder viewHolder) {
            // clearView() is called when an interaction with the item is finished, which does
            // not mean that the user went all the way and dismissed the item before releasing it.
            // We need to check that the item has been removed.
            if (viewHolder.getAdapterPosition() == RecyclerView.NO_POSITION) {
                mRecyclerView.onItemDismissFinished(viewHolder.itemView);
            }

            super.clearView(recyclerView, viewHolder);
        }

        @Override
        public boolean onMove(RecyclerView recyclerView, ViewHolder viewHolder, ViewHolder target) {
            assert false; // Drag and drop not supported, the method will never be called.
            return false;
        }

        @Override
        public int getMovementFlags(RecyclerView recyclerView, ViewHolder viewHolder) {
            assert viewHolder instanceof NewTabPageViewHolder;

            int swipeFlags = 0;
            if (((NewTabPageViewHolder) viewHolder).isDismissable()) {
                swipeFlags = ItemTouchHelper.START | ItemTouchHelper.END;
            }

            return makeMovementFlags(0 /* dragFlags */, swipeFlags);
        }

        @Override
        public void onChildDraw(Canvas c, RecyclerView recyclerView, ViewHolder viewHolder,
                float dX, float dY, int actionState, boolean isCurrentlyActive) {
            assert viewHolder instanceof NewTabPageViewHolder;

            ((NewTabPageViewHolder) viewHolder).updateViewStateForDismiss(dX);
            super.onChildDraw(c, recyclerView, viewHolder, dX, dY, actionState, isCurrentlyActive);
        }
    }

    /**
     * Constructor to create the manager for all the cards to display on the NTP
     *
     * @param manager the NewTabPageManager to use to interact with the rest of the system.
     * @param newTabPageLayout the layout encapsulating all the above-the-fold elements
     *                         (logo, search box, most visited tiles)
     * @param snippetsBridge the bridge to interact with the snippets service.
     * @param uiConfig the NTP UI configuration, to be passed to created views.
     */
    public NewTabPageAdapter(NewTabPageManager manager, NewTabPageLayout newTabPageLayout,
            SnippetsBridge snippetsBridge, UiConfig uiConfig) {
        mNewTabPageManager = manager;
        mNewTabPageLayout = newTabPageLayout;
        mAboveTheFold = new AboveTheFoldListItem();
        mHeader = new SnippetHeaderListItem();
        mBottomSpacer = new SpacingListItem();
        mItemTouchCallbacks = new ItemTouchCallbacks();
        mItems = new ArrayList<>();
        mProviderStatus = ContentSuggestionsCategoryStatus.INITIALIZING;
        mSnippetsBridge = snippetsBridge;
        mUiConfig = uiConfig;
        mStatusCard = StatusListItem.create(snippetsBridge.getCategoryStatus(), this, manager);

        loadSnippets(new ArrayList<SnippetArticleListItem>());
        mSnippetsBridge.setObserver(this);
    }

    /** Returns callbacks to configure the interactions with the RecyclerView's items. */
    public ItemTouchHelper.Callback getItemTouchCallbacks() {
        return mItemTouchCallbacks;
    }

    @Override
    public void onSnippetsReceived(List<SnippetArticleListItem> snippets) {
        // We never want to refresh the suggestions if we already have some content.
        if (hasSuggestions()) return;

        if (!SnippetsBridge.isCategoryStatusInitOrAvailable(mProviderStatus)) {
            return;
        }

        Log.d(TAG, "Received %d new snippets.", snippets.size());

        // At first, there might be no snippets available, we wait until they have been fetched.
        if (snippets.isEmpty()) return;

        loadSnippets(snippets);

        NewTabPageUma.recordSnippetAction(NewTabPageUma.SNIPPETS_ACTION_SHOWN);
    }

    @Override
    public void onCategoryStatusChanged(int categoryStatus) {
        // Observers should not be registered for that state
        assert categoryStatus
                != ContentSuggestionsCategoryStatus.ALL_SUGGESTIONS_EXPLICITLY_DISABLED;

        mProviderStatus = categoryStatus;
        mStatusCard = StatusListItem.create(mProviderStatus, this, mNewTabPageManager);

        // We had suggestions but we just got notified about the provider being enabled. Nothing to
        // do then.
        if (SnippetsBridge.isCategoryStatusAvailable(mProviderStatus) && hasSuggestions()) return;

        if (hasSuggestions()) {
            // We have suggestions, this implies that the service was previously enabled and just
            // transitioned to a disabled state. Clear them.
            loadSnippets(new ArrayList<SnippetArticleListItem>());
        } else {
            // If there are no suggestions there is an old status card that must be replaced.
            int firstCardPosition = getFirstCardPosition();
            mItems.set(firstCardPosition, mStatusCard);
            // Update both the status card and the spacer after it.
            notifyItemRangeChanged(firstCardPosition, 2);
        }
    }

    @Override
    @NewTabPageListItem.ViewType
    public int getItemViewType(int position) {
        return mItems.get(position).getType();
    }

    @Override
    public NewTabPageViewHolder onCreateViewHolder(ViewGroup parent, int viewType) {
        assert parent == mRecyclerView;

        if (viewType == NewTabPageListItem.VIEW_TYPE_ABOVE_THE_FOLD) {
            return new NewTabPageViewHolder(mNewTabPageLayout);
        }

        if (viewType == NewTabPageListItem.VIEW_TYPE_HEADER) {
            return new SnippetHeaderViewHolder(mRecyclerView, mUiConfig);
        }

        if (viewType == NewTabPageListItem.VIEW_TYPE_SNIPPET) {
            return new SnippetArticleViewHolder(
                    mRecyclerView, mNewTabPageManager, mSnippetsBridge, mUiConfig);
        }

        if (viewType == NewTabPageListItem.VIEW_TYPE_SPACING) {
            return new NewTabPageViewHolder(SpacingListItem.createView(parent));
        }

        if (viewType == NewTabPageListItem.VIEW_TYPE_STATUS) {
            return new StatusListItem.ViewHolder(mRecyclerView, mUiConfig);
        }

        return null;
    }

    @Override
    public void onBindViewHolder(NewTabPageViewHolder holder, final int position) {
        holder.onBindViewHolder(mItems.get(position));
    }

    @Override
    public int getItemCount() {
        return mItems.size();
    }

    public int getHeaderPosition() {
        return mItems.indexOf(mHeader);
    }

    public int getFirstCardPosition() {
        return getHeaderPosition() + 1;
    }

    public int getLastCardPosition() {
        return getBottomSpacerPosition() - 1;
    }

    public int getBottomSpacerPosition() {
        return mItems.indexOf(mBottomSpacer);
    }

    /** Start a request for new snippets. */
    public void reloadSnippets() {
        SnippetsBridge.fetchSnippets();
    }

    private void loadSnippets(List<SnippetArticleListItem> snippets) {
        // Copy thumbnails over
        for (SnippetArticleListItem snippet : snippets) {
            int existingSnippetIdx = mItems.indexOf(snippet);
            if (existingSnippetIdx == -1) continue;

            snippet.setThumbnailBitmap(
                    ((SnippetArticleListItem) mItems.get(existingSnippetIdx)).getThumbnailBitmap());
        }

        boolean hasContentToShow = !snippets.isEmpty();

        // TODO(mvanouwerkerk): Make it so that the header does not need to be manipulated
        // separately from the cards to which it belongs - crbug.com/616090.
        mHeader.setVisible(hasContentToShow);

        mItems.clear();
        mItems.add(mAboveTheFold);
        mItems.add(mHeader);
        if (hasContentToShow) {
            mItems.addAll(snippets);
        } else {
            mItems.add(mStatusCard);
        }

        mItems.add(mBottomSpacer);

        notifyDataSetChanged();
    }

    @Override
    public void onAttachedToRecyclerView(RecyclerView recyclerView) {
        super.onAttachedToRecyclerView(recyclerView);

        // We are assuming for now that the adapter is used with a single RecyclerView.
        // Getting the reference as we are doing here is going to be broken if that changes.
        assert mRecyclerView == null;

        // FindBugs chokes on the cast below when not checked, raising BC_UNCONFIRMED_CAST
        assert recyclerView instanceof NewTabPageRecyclerView;

        mRecyclerView = (NewTabPageRecyclerView) recyclerView;
    }

    private void dismissItem(ViewHolder itemViewHolder) {
        assert itemViewHolder.getItemViewType() == NewTabPageListItem.VIEW_TYPE_SNIPPET;

        int position = itemViewHolder.getAdapterPosition();
        SnippetArticleListItem dismissedSnippet = (SnippetArticleListItem) mItems.get(position);

        mSnippetsBridge.getSnippedVisited(dismissedSnippet, new Callback<Boolean>() {
            @Override
            public void onResult(Boolean result) {
                NewTabPageUma.recordSnippetAction(result
                                ? NewTabPageUma.SNIPPETS_ACTION_DISMISSED_VISITED
                                : NewTabPageUma.SNIPPETS_ACTION_DISMISSED_UNVISITED);
            }
        });

        mSnippetsBridge.discardSnippet(dismissedSnippet);
        mItems.remove(position);
        notifyItemRemoved(position);
    }

    private void addStatusCardIfNecessary() {
        if (!hasSuggestions() && !mItems.contains(mStatusCard)) {
            mItems.add(getFirstCardPosition(), mStatusCard);

            // We also want to refresh the header and the bottom padding.
            mHeader.setVisible(false);
            notifyDataSetChanged();
        }
    }

    /** Returns whether we have some suggested content to display. */
    private boolean hasSuggestions() {
        for (NewTabPageListItem item : mItems) {
            if (item instanceof SnippetArticleListItem) return true;
        }
        return false;
    }

    List<NewTabPageListItem> getItemsForTesting() {
        return mItems;
    }
}
