// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.list;

import org.chromium.chrome.browser.download.home.list.ListItem.ViewListItem;
import org.chromium.chrome.browser.modelutil.ListObservable;
import org.chromium.chrome.browser.modelutil.ListObservable.ListObserver;

class DecoratedListItemModel extends ListObservable implements ListObserver {
    private final ListItemModel mModel;

    private ViewListItem mHeaderItem;

    /** */
    public DecoratedListItemModel(ListItemModel model) {
        mModel = model;
        mModel.addObserver(this);
    }

    /**
     *
     * @param item
     */
    public void setHeader(ViewListItem item) {
        if (mHeaderItem == item) return;

        ViewListItem oldHeaderItem = mHeaderItem;
        mHeaderItem = item;

        if (oldHeaderItem != null && item == null) {
            notifyItemRangeRemoved(0, 1);
        } else if (oldHeaderItem == null && item != null) {
            notifyItemRangeInserted(0, 1);
        } else {
            notifyItemRangeChanged(0, 1, null);
        }
    }

    /**
     *
     * @param index
     * @return
     */
    public ListItem getItemAt(int index) {
        if (index == 0 && mHeaderItem != null) return mHeaderItem;
        return mModel.getItemAt(convertIndexForSource(index));
    }

    // ListObserver implementation.
    @Override
    public void onItemRangeInserted(ListObservable source, int index, int count) {
        notifyItemRangeInserted(convertIndexFromSource(index), count);
    }

    @Override
    public void onItemRangeRemoved(ListObservable source, int index, int count) {
        notifyItemRangeRemoved(convertIndexFromSource(index), count);
    }

    @Override
    public void onItemRangeChanged(ListObservable source, int index, int count, Object payload) {
        notifyItemRangeChanged(convertIndexFromSource(index), count, payload);
    }

    // ListObservable implementation.
    @Override
    public int getItemCount() {
        return mModel.getItemCount() + (mHeaderItem == null ? 0 : 1);
    }

    private int convertIndexForSource(int index) {
        return mHeaderItem == null ? index : index - 1;
    }

    private int convertIndexFromSource(int index) {
        return mHeaderItem == null ? index : index + 1;
    }
}