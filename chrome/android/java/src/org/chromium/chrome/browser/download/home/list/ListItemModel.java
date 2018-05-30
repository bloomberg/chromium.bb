// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.list;

import org.chromium.chrome.browser.modelutil.ListObservable;

import java.util.ArrayList;
import java.util.List;

/**
 * A simple {@link ListObservable} that acts as a backing store for the output of
 * {@link DateOrderedListMutator}.  This will be updated in the near future to support large batch
 * updates to minimize the hit on {@link ListObserver}s.
 */
class ListItemModel extends BatchListObservable {
    private final List<ListItem> mItems = new ArrayList<>();

    /** Adds {@code item} to this list at {@code index}. */
    public void addItem(int index, ListItem item) {
        mItems.add(index, item);
        notifyItemRangeInserted(index, 1);
    }

    /** Removes the {@link ListItem} at {@code index}. */
    public void removeItem(int index) {
        mItems.remove(index);
        notifyItemRangeRemoved(index, 1);
    }

    /** Sets the {@link ListItem} at {@code index} to {@code item}. */
    public void setItem(int index, ListItem item) {
        mItems.set(index, item);
        notifyItemRangeChanged(index, 1, null);
    }

    /** @return The {@link ListItem} at {@code index}. */
    public ListItem getItemAt(int index) {
        return mItems.get(index);
    }

    // ListObservable implementation.
    @Override
    public int getItemCount() {
        return mItems.size();
    }
}