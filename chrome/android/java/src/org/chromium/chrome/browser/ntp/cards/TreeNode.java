// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.cards;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.ntp.snippets.SnippetArticle;

import java.util.List;

/**
 * A tree interface to allow the New Tab Page RecyclerView to delegate to other components.
 */
interface TreeNode {
    /**
     * Initialize the node (and any children underneath it). This method should be called after the
     * node has been added to the tree, i.e. when it is in the list of its parent's children.
     * The node may notify its parent about changes that happen during initialization.
     */
    void setParent(NodeParent parent);

    /**
     * Returns the number of items under this subtree. This method may be called
     * before initialization.
     *
     * @return The number of items under this subtree.
     * @see android.support.v7.widget.RecyclerView.Adapter#getItemCount()
     */
    int getItemCount();

    /**
     * @param position The position to query
     * @return The view type of the item at {@code position} under this subtree.
     * @see android.support.v7.widget.RecyclerView.Adapter#getItemViewType
     */
    @ItemViewType
    int getItemViewType(int position);

    /**
     * Display the data at {@code position} under this subtree, making a partial update based on
     * the {@code payload} data.
     * @param holder The view holder that should be updated.
     * @param position The position of the item under this subtree.
     * @see android.support.v7.widget.RecyclerView.Adapter#onBindViewHolder(ViewHolder, int, List)
     */
    void onBindViewHolder(NewTabPageViewHolder holder, int position, List<Object> payloads);

    /**
     * @param position The position to query.
     * @return The article at {@code position} under this subtree, or null if the item is not
     * an article.
     */
    SnippetArticle getSuggestionAt(int position);

    /**
     * Dismiss the item at the given {@code position}.
     * @param position The position of the item to be dismissed.
     * @param itemRemovedCallback Should be called with the title of the dismissed item, to announce
     * it for accessibility purposes.
     */
    public void dismissItem(int position, Callback<String> itemRemovedCallback);

    /**
     * The dismiss sibling is an item that should be dismissed at the same time as the provided
     * one. For example, if we want to dismiss a status card that has a More button attached, the
     * button is the card's dismiss sibling. This function returns the adapter position delta to
     * apply to get to the sibling from the provided item. For the previous example, it would return
     * {@code +1}, as the button comes right after the status card.
     *
     * @return a position delta to apply to the position of the provided item to get the adapter
     * position of the item to animate. Returns {@code 0} if there is no dismiss sibling.
     */
    int getDismissSiblingPosDelta(int position);
}
