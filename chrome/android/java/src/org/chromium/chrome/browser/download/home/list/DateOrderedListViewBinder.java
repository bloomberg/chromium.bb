// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.list;

import android.support.annotation.IntDef;
import android.view.ViewGroup;

import org.chromium.chrome.browser.modelutil.RecyclerViewAdapter.ViewBinder;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * A {@link ViewBinder} responsible for connecting a {@link DateOrderedListModel} with a underlying
 * {@link ViewHolder}.
 */
class DateOrderedListViewBinder implements ViewBinder<DecoratedListItemModel, ListItemViewHolder> {
    /** The potential types of list items that could be displayed. */
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({DATE, IN_PROGRESS, GENERIC, VIDEO, IMAGE, CUSTOM_VIEW})
    public @interface ViewType {}
    public static final int DATE = 0;
    public static final int IN_PROGRESS = 1;
    public static final int GENERIC = 2;
    public static final int VIDEO = 3;
    public static final int IMAGE = 4;
    public static final int CUSTOM_VIEW = 5;

    // ViewBinder implementation.
    @Override
    public ListItemViewHolder onCreateViewHolder(ViewGroup parent, @ViewType int viewType) {
        switch (viewType) {
            case DATE:
                return new ListItemViewHolder.DateViewHolder(parent);
            case IN_PROGRESS:
                return new ListItemViewHolder.InProgressViewHolder(parent);
            case GENERIC:
                return new ListItemViewHolder.GenericViewHolder(parent);
            case VIDEO:
                return new ListItemViewHolder.VideoViewHolder(parent);
            case IMAGE:
                return new ListItemViewHolder.ImageViewHolder(parent);
            case CUSTOM_VIEW:
                return new ListItemViewHolder.CustomViewHolder(parent);
        }

        assert false;
        return null;
    }

    @Override
    public void onBindViewHolder(
            DecoratedListItemModel model, ListItemViewHolder holder, int position) {
        holder.bind(model.getItemAt(position));
    }
}