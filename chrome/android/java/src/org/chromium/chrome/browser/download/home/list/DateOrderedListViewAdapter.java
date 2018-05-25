// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.list;

import android.support.v7.widget.RecyclerView;
import android.support.v7.widget.RecyclerView.ViewHolder;

import org.chromium.chrome.browser.download.home.list.DateOrderedListModel.ListItem;
import org.chromium.chrome.browser.download.home.list.DateOrderedListViewBinder.ViewType;
import org.chromium.chrome.browser.modelutil.RecyclerViewAdapter;
import org.chromium.components.offline_items_collection.OfflineItemFilter;
import org.chromium.components.offline_items_collection.OfflineItemState;

/**
 * A helper {@link RecyclerView.Adapter} implementation meant to glue a {@link DateOrderedListModel}
 * to the right {@link ViewHolder} and {@link ViewBinder}.
 */
class DateOrderedListViewAdapter extends RecyclerViewAdapter<DateOrderedListModel, ViewHolder> {
    /** Creates an instance of a {@link DateOrderedListViewAdapter}. */
    public DateOrderedListViewAdapter(
            DateOrderedListModel model, ViewBinder<DateOrderedListModel, ViewHolder> viewBinder) {
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

        if (item.item == null) return DateOrderedListViewBinder.DATE;

        if (item.item.state == OfflineItemState.IN_PROGRESS) {
            return DateOrderedListViewBinder.IN_PROGRESS;
        }

        switch (item.item.filter) {
            case OfflineItemFilter.FILTER_VIDEO:
                return DateOrderedListViewBinder.VIDEO;
            case OfflineItemFilter.FILTER_IMAGE:
                return DateOrderedListViewBinder.IMAGE;
            case OfflineItemFilter.FILTER_ALL:
            case OfflineItemFilter.FILTER_PAGE:
            case OfflineItemFilter.FILTER_AUDIO:
            case OfflineItemFilter.FILTER_OTHER:
            case OfflineItemFilter.FILTER_DOCUMENT:
            default:
                return DateOrderedListViewBinder.GENERIC;
        }
    }
}