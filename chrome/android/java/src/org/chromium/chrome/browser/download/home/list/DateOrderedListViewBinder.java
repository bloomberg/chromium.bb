// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.list;

import android.view.ViewGroup;

import org.chromium.chrome.browser.download.home.list.ListUtils.ViewType;
import org.chromium.chrome.browser.modelutil.RecyclerViewAdapter.ViewBinder;

/**
 * A {@link ViewBinder} responsible for connecting a {@link DateOrderedListModel} with a underlying
 * {@link ViewHolder}.
 */
class DateOrderedListViewBinder implements ViewBinder<DecoratedListItemModel, ListItemViewHolder> {
    // ViewBinder implementation.
    @Override
    public ListItemViewHolder onCreateViewHolder(ViewGroup parent, @ViewType int viewType) {
        switch (viewType) {
            case ListUtils.DATE:
                return ListItemViewHolder.DateViewHolder.create(parent);
            case ListUtils.IN_PROGRESS:
                return new ListItemViewHolder.InProgressViewHolder(parent);
            case ListUtils.GENERIC:
                return ListItemViewHolder.GenericViewHolder.create(parent);
            case ListUtils.VIDEO:
                return new ListItemViewHolder.VideoViewHolder(parent);
            case ListUtils.IMAGE:
                return new ListItemViewHolder.ImageViewHolder(parent);
            case ListUtils.CUSTOM_VIEW:
                return new ListItemViewHolder.CustomViewHolder(parent);
            case ListUtils.PREFETCH:
                return ListItemViewHolder.PrefetchViewHolder.create(parent);
        }

        assert false;
        return null;
    }

    @Override
    public void onBindViewHolder(
            DecoratedListItemModel model, ListItemViewHolder holder, int position) {
        holder.bind(model.getProperties(), model.getItemAt(position));
    }
}