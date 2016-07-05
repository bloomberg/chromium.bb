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
import org.chromium.chrome.browser.ntp.snippets.DisabledReason;
import org.chromium.chrome.browser.ntp.snippets.SnippetArticle;
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

    /**
     * Position of the first card in the adapter. This is always going to be a valid position,
     * occupied either by a card showing content or by a status card.
     */
    private static final int FIRST_CARD_POSITION = 2;

    private final NewTabPageManager mNewTabPageManager;
    private final NewTabPageLayout mNewTabPageLayout;
    private final AboveTheFoldListItem mAboveTheFoldListItem;
    private final SnippetHeaderListItem mHeaderListItem;
    private StatusListItem mStatusListItem;
    private final List<NewTabPageListItem> mNewTabPageListItems;
    private final ItemTouchCallbacks mItemTouchCallbacks;
    private NewTabPageRecyclerView mRecyclerView;
    private boolean mWantsSnippets;

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
     */
    public NewTabPageAdapter(NewTabPageManager manager, NewTabPageLayout newTabPageLayout,
            SnippetsBridge snippetsBridge) {
        mNewTabPageManager = manager;
        mNewTabPageLayout = newTabPageLayout;
        mAboveTheFoldListItem = new AboveTheFoldListItem();
        mHeaderListItem = new SnippetHeaderListItem();
        mItemTouchCallbacks = new ItemTouchCallbacks();
        mNewTabPageListItems = new ArrayList<NewTabPageListItem>();
        mWantsSnippets = true;
        mSnippetsBridge = snippetsBridge;
        mStatusListItem = StatusListItem.create(snippetsBridge.getDisabledReason(), this, manager);

        loadSnippets(new ArrayList<SnippetArticle>());
        mSnippetsBridge.setObserver(this);
    }

    /** Returns callbacks to configure the interactions with the RecyclerView's items. */
    public ItemTouchHelper.Callback getItemTouchCallbacks() {
        return mItemTouchCallbacks;
    }

    @Override
    public void onSnippetsReceived(List<SnippetArticle> listSnippets) {
        if (!mWantsSnippets) return;

        int newSnippetCount = listSnippets.size();
        Log.d(TAG, "Received %d new snippets.", newSnippetCount);

        // At first, there might be no snippets available, we wait until they have been fetched.
        if (newSnippetCount == 0) return;

        loadSnippets(listSnippets);

        // We don't want to get notified of other changes.
        mWantsSnippets = false;
        NewTabPageUma.recordSnippetAction(NewTabPageUma.SNIPPETS_ACTION_SHOWN);
    }

    @Override
    public void onDisabledReasonChanged(int disabledReason) {
        // Observers should not be registered for that state
        assert disabledReason != DisabledReason.EXPLICITLY_DISABLED;

        mStatusListItem = StatusListItem.create(disabledReason, this, mNewTabPageManager);
        if (getItemCount() > 4 /* above-the-fold + header + card + spacing */) {
            // We had many items, implies that the service was previously enabled and just
            // transitioned. to a disabled state. We now clear it.
            loadSnippets(new ArrayList<SnippetArticle>());
        } else {
            mNewTabPageListItems.set(FIRST_CARD_POSITION, mStatusListItem);
            notifyItemRangeChanged(FIRST_CARD_POSITION, 2); // Update both the first card and the
            // spacing item coming after it.
        }

        if (disabledReason == DisabledReason.NONE) mWantsSnippets = true;
    }

    @Override
    @NewTabPageListItem.ViewType
    public int getItemViewType(int position) {
        return mNewTabPageListItems.get(position).getType();
    }

    @Override
    public NewTabPageViewHolder onCreateViewHolder(ViewGroup parent, int viewType) {
        assert parent == mRecyclerView;

        if (viewType == NewTabPageListItem.VIEW_TYPE_ABOVE_THE_FOLD) {
            return new NewTabPageViewHolder(mNewTabPageLayout);
        }

        if (viewType == NewTabPageListItem.VIEW_TYPE_HEADER) {
            return new SnippetHeaderViewHolder(
                    SnippetHeaderListItem.createView(parent), mRecyclerView);
        }

        if (viewType == NewTabPageListItem.VIEW_TYPE_SNIPPET) {
            return new SnippetArticleViewHolder(mRecyclerView, mNewTabPageManager, mSnippetsBridge);
        }

        if (viewType == NewTabPageListItem.VIEW_TYPE_SPACING) {
            return new NewTabPageViewHolder(SpacingListItem.createView(parent));
        }

        if (viewType == NewTabPageListItem.VIEW_TYPE_STATUS) {
            return new StatusListItem.ViewHolder(mRecyclerView);
        }

        return null;
    }

    @Override
    public void onBindViewHolder(NewTabPageViewHolder holder, final int position) {
        holder.onBindViewHolder(mNewTabPageListItems.get(position));
    }

    @Override
    public int getItemCount() {
        return mNewTabPageListItems.size();
    }

    /** Start a request for new snippets. */
    public void reloadSnippets() {
        mWantsSnippets = true;
        SnippetsBridge.fetchSnippets();
    }

    private void loadSnippets(List<SnippetArticle> listSnippets) {
        // Copy thumbnails over
        for (SnippetArticle newSnippet : listSnippets) {
            int existingSnippetIdx = mNewTabPageListItems.indexOf(newSnippet);
            if (existingSnippetIdx == -1) continue;

            newSnippet.setThumbnailBitmap(
                    ((SnippetArticle) mNewTabPageListItems.get(existingSnippetIdx))
                            .getThumbnailBitmap());
        }

        boolean hasContentToShow = !listSnippets.isEmpty();
        mHeaderListItem.setVisible(hasContentToShow);

        mNewTabPageListItems.clear();
        mNewTabPageListItems.add(mAboveTheFoldListItem);
        mNewTabPageListItems.add(mHeaderListItem);

        if (hasContentToShow) {
            mNewTabPageListItems.addAll(listSnippets);
        } else {
            mNewTabPageListItems.add(mStatusListItem);
        }

        mNewTabPageListItems.add(new SpacingListItem());

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
        SnippetArticle dismissedSnippet = (SnippetArticle) mNewTabPageListItems.get(position);

        mSnippetsBridge.getSnippedVisited(dismissedSnippet, new Callback<Boolean>() {
            @Override
            public void onResult(Boolean result) {
                NewTabPageUma.recordSnippetAction(result
                                ? NewTabPageUma.SNIPPETS_ACTION_DISMISSED_VISITED
                                : NewTabPageUma.SNIPPETS_ACTION_DISMISSED_UNVISITED);
            }
        });

        mSnippetsBridge.discardSnippet(dismissedSnippet);
        mNewTabPageListItems.remove(position);
        notifyItemRemoved(position);
    }

    private void addStatusCardIfNecessary() {
        if (mNewTabPageListItems.size() == 3 /* above-the-fold + header + spacing */) {
            // TODO(dgn) hack until we refactor the entire class with sections, etc.
            // (see https://crbug.com/616090)
            mNewTabPageListItems.add(FIRST_CARD_POSITION, mStatusListItem);

            // We also want to refresh the header and the bottom padding.
            mHeaderListItem.setVisible(false);
            notifyDataSetChanged();
        }
    }

    List<NewTabPageListItem> getItemsForTesting() {
        return mNewTabPageListItems;
    }
}
