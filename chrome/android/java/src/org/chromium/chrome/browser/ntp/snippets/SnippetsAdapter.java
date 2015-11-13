// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.snippets;

import android.support.v7.widget.RecyclerView.Adapter;
import android.view.ViewGroup;

import org.chromium.chrome.browser.ntp.snippets.SnippetsManager.SnippetListItem;

import java.util.Collections;
import java.util.List;

/**
 * A class that represents the adapter backing the snippets RecyclerView.
 */
class SnippetsAdapter extends Adapter<SnippetListItemViewHolder> {
    private List<SnippetListItem> mSnippetListItems;
    private final SnippetsManager mManager;

    /**
     * Constructs a SnippetsAdapter object that backs the Snippets RecyclerView on the NTP
     *
     * @param snippetsManager SnippetsManager object used to open an article
     */
    public SnippetsAdapter(SnippetsManager snippetsManager) {
        mManager = snippetsManager;
        mSnippetListItems = Collections.emptyList();
    }

    /**
     * Set the list of items to display in the snippets RecyclerView
     *
     * @param listItems the new list of items to display
     */
    public void setSnippetListItems(List<SnippetListItem> listItems) {
        mSnippetListItems = listItems;
        notifyDataSetChanged();
    }

    @Override
    public int getItemViewType(int position) {
        return mSnippetListItems.get(position).getType();
    }

    @Override
    public SnippetListItemViewHolder onCreateViewHolder(ViewGroup parent, int viewType) {
        if (viewType == SnippetsManager.SNIPPET_ITEM_TYPE_HEADER) {
            return new SnippetHeaderItemViewHolder(
                    SnippetHeaderItemViewHolder.createView(parent), mManager);
        }

        if (viewType == SnippetsManager.SNIPPET_ITEM_TYPE_SNIPPET) {
            return new SnippetCardItemViewHolder(
                    SnippetCardItemViewHolder.createView(parent), mManager);
        }

        return null;
    }

    @Override
    public void onBindViewHolder(SnippetListItemViewHolder holder, final int position) {
        holder.onBindViewHolder(mSnippetListItems.get(position));
    }

    @Override
    public int getItemCount() {
        return mSnippetListItems.size();
    }
}
