// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.widget.selection;

import android.support.v7.widget.RecyclerView;
import android.view.View;

/**
 * An ViewHolder for a {@link SelectableItemView}.
 * @param <E> The type of the item associated with the SelectableItemView.
 */
public class SelectableItemViewHolder<E> extends RecyclerView.ViewHolder {
    private SelectableItemView<E> mItemView;

    /**
     * @param itemView The SelectableItemView to be held by this ViewHolder.
     * @param delegate The SelectionDelegate for the itemView.
     */
    @SuppressWarnings("unchecked")
    public SelectableItemViewHolder(View itemView, SelectionDelegate<E> delegate) {
        super(itemView);
        mItemView = (SelectableItemView<E>) itemView;
        mItemView.setSelectionDelegate(delegate);
    }

    /**
     * @param item The item to display in the SelectableItemView held by this ViewHolder.
     */
    public void displayItem(E item) {
        mItemView.setItem(item);
    }
}
