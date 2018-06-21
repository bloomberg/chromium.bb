// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.cards;

import org.chromium.base.Callback;

import java.util.Set;

/**
 * A tree interface to allow the New Tab Page RecyclerView to delegate to other components.
 */
public interface TreeNode {
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
     * Display the data at {@code position} under this subtree.
     * @param holder The view holder that should be updated.
     * @param position The position of the item under this subtree.
     * @see android.support.v7.widget.RecyclerView.Adapter#onBindViewHolder
     */
    void onBindViewHolder(NewTabPageViewHolder holder, int position);

    /**
     * @param position the position of an item to be dismissed.
     * @return the set of item positions that should be dismissed simultaneously when dismissing the
     *         item at the given {@code position} (including the position itself), or an empty set
     *         if the item can't be dismissed.
     * @see NewTabPageAdapter#getItemDismissalGroup
     */
    Set<Integer> getItemDismissalGroup(int position);

    /**
     * Dismiss the item at the given {@code position}.
     * @param position The position of the item to be dismissed.
     * @param itemRemovedCallback Should be called with the title of the dismissed item, to announce
     * it for accessibility purposes.
     * @see NewTabPageAdapter#dismissItem
     */
    void dismissItem(int position, Callback<String> itemRemovedCallback);

    /**
     * Describe the item at the given {@code position}. As the description is used in tests for
     * dumping state and equality checks, different items should have distinct descriptions,
     * but for items that are unique or don't have interesting state it can be sufficient to return
     * e.g. a string that describes the type of the item.
     * @param position The position of the item to be described.
     * @return A string description of the item.
     */
    String describeItemForTesting(int position);
}
