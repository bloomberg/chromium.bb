// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import android.support.v7.widget.RecyclerView.ViewHolder;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.history.BrowsingHistoryBridge.BrowsingHistoryObserver;
import org.chromium.chrome.browser.widget.DateDividedAdapter;
import org.chromium.chrome.browser.widget.selection.SelectableItemViewHolder;
import org.chromium.chrome.browser.widget.selection.SelectionDelegate;

import java.util.List;

/**
 * Bridges the user's browsing history and the UI used to display it.
 */
public class HistoryAdapter extends DateDividedAdapter implements BrowsingHistoryObserver {
    private static final String EMPTY_QUERY = "";

    private final SelectionDelegate<HistoryItem> mSelectionDelegate;
    private final BrowsingHistoryBridge mBridge;
    private final HistoryManager mManager;

    private boolean mDestroyed;

    public HistoryAdapter(SelectionDelegate<HistoryItem> delegate, HistoryManager manager) {
        setHasStableIds(true);
        mSelectionDelegate = delegate;
        mBridge = new BrowsingHistoryBridge(this);
        mManager = manager;
    }

    /**
     * Called when the activity/native page is destroyed.
     */
    public void onDestroyed() {
        mBridge.destroy();
        mDestroyed = true;
    }

    /**
     * Initializes the HistoryAdapter and loads the first set of browsing history items.
     */
    public void initialize() {
        mBridge.queryHistory(EMPTY_QUERY, 0);
    }

    /**
     * Adds the HistoryItem to the list of items being removed and removes it from the adapter. The
     * removal will not be committed until #removeItems() is called.
     * @param item The item to mark for removal.
     */
    public void markItemForRemoval(HistoryItem item) {
        ItemGroup group = getGroupAt(item.getPosition()).first;
        group.removeItem(item);
        // Remove the group if only the date header is left.
        if (group.size() == 1) removeGroup(group);
        notifyDataSetChanged();

        mBridge.markItemForRemoval(item);
    }

    /**
     * Removes all items that have been marked for removal through #markItemForRemoval().
     */
    public void removeItems() {
        mBridge.removeItems();
    }

    @Override
    protected ViewHolder createViewHolder(ViewGroup parent) {
        View v = LayoutInflater.from(parent.getContext()).inflate(
                R.layout.history_item_view, parent, false);
        return new SelectableItemViewHolder<HistoryItem>(v, mSelectionDelegate);
    }

    @Override
    protected void bindViewHolderForTimedItem(ViewHolder current, TimedItem timedItem) {
        final HistoryItem item = (HistoryItem) timedItem;
        @SuppressWarnings("unchecked")
        SelectableItemViewHolder<HistoryItem> holder =
                (SelectableItemViewHolder<HistoryItem>) current;
        holder.displayItem(item);
        item.setHistoryManager(mManager);
    }

    @Override
    protected int getTimedItemViewResId() {
        return R.layout.date_view;
    }

    @Override
    public void onQueryHistoryComplete(List<HistoryItem> items) {
        // Return early if the results are returned after the activity/native page is
        // destroyed to avoid unnecessary work.
        if (mDestroyed) return;

        loadItems(items);
    }

    @Override
    public void onHistoryDeleted() {
        mSelectionDelegate.clearSelection();
        // TODO(twellington): Account for items that have been paged in due to infinite scroll.
        //                    This currently removes all items and re-issues a query.
        initialize();
    }
}
