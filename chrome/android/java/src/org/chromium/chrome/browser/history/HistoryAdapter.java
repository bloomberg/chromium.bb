// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import android.support.v7.widget.RecyclerView.ViewHolder;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import org.chromium.base.Callback;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.widget.DateDividedAdapter;
import org.chromium.chrome.browser.widget.selection.SelectableItemViewHolder;
import org.chromium.chrome.browser.widget.selection.SelectionDelegate;

import java.util.List;

/**
 * Bridges the user's browsing history and the UI used to display it.
 */
public class HistoryAdapter extends DateDividedAdapter {
    private static final String EMPTY_QUERY = "";

    private final SelectionDelegate<HistoryItem> mSelectionDelegate;
    private final BrowsingHistoryBridge mBridge;

    private boolean mDestroyed;

    public HistoryAdapter(SelectionDelegate<HistoryItem> delegate) {
        setHasStableIds(true);
        mSelectionDelegate = delegate;
        mBridge = new BrowsingHistoryBridge();
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
        mBridge.queryHistory(new Callback<List<HistoryItem>>() {
            @Override
            public void onResult(List<HistoryItem> result) {
                // Return early if the results are returned after the activity/native page is
                // destroyed to avoid unnecessary work.
                if (mDestroyed) return;

                loadItems(result);
            }
        }, EMPTY_QUERY, 0);
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
    }

    @Override
    protected int getTimedItemViewResId() {
        return R.layout.date_view;
    }

}
