// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.list;

import android.support.v7.widget.RecyclerView;

import org.chromium.chrome.browser.download.home.list.ListUtils.ViewType;
import org.chromium.chrome.browser.modelutil.RecyclerViewAdapter;

/**
 * A helper {@link RecyclerView.Adapter} implementation meant to glue a {@link ListItemModel}
 * to the right {@link ViewHolder} and {@link ViewBinder}.
 */
class DateOrderedListViewAdapter
        extends RecyclerViewAdapter<DecoratedListItemModel, ListItemViewHolder> {
    /** Creates an instance of a {@link DateOrderedListViewAdapter}. */
    public DateOrderedListViewAdapter(DecoratedListItemModel model,
            ViewBinder<DecoratedListItemModel, ListItemViewHolder> viewBinder) {
        super(model, viewBinder);
        setHasStableIds(true);
    }

    @Override
    public long getItemId(int position) {
        if (!hasStableIds()) return RecyclerView.NO_ID;
        return mModel.getItemAt(position).stableId;
    }

    @Override
    public @ViewType int getItemViewType(int position) {
        ListItem item = mModel.getItemAt(position);
        return ListUtils.getViewTypeForItem(item);
    }
}