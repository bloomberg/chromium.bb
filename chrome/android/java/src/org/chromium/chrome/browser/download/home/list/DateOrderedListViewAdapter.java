// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.list;

import android.support.v7.widget.RecyclerView;

import org.chromium.chrome.browser.download.home.list.DateOrderedListViewBinder.ViewType;
import org.chromium.chrome.browser.download.home.list.ListItem.DateListItem;
import org.chromium.chrome.browser.download.home.list.ListItem.OfflineItemListItem;
import org.chromium.chrome.browser.download.home.list.ListItem.ViewListItem;
import org.chromium.chrome.browser.modelutil.RecyclerViewAdapter;
import org.chromium.components.offline_items_collection.OfflineItemFilter;
import org.chromium.components.offline_items_collection.OfflineItemState;

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

        if (item instanceof ViewListItem) return DateOrderedListViewBinder.CUSTOM_VIEW;
        if (item instanceof DateListItem) {
            if (item instanceof OfflineItemListItem) {
                OfflineItemListItem offlineItem = (OfflineItemListItem) item;

                if (offlineItem.item.state == OfflineItemState.IN_PROGRESS) {
                    return DateOrderedListViewBinder.IN_PROGRESS;
                }

                switch (offlineItem.item.filter) {
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
            } else {
                return DateOrderedListViewBinder.DATE;
            }
        }

        assert false;
        return DateOrderedListViewBinder.GENERIC;
    }
}