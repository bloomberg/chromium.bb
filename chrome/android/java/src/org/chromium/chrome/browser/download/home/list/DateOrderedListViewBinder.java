// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.list;

import android.support.annotation.IntDef;
import android.support.v7.widget.RecyclerView.ViewHolder;
import android.view.ViewGroup;

import org.chromium.chrome.browser.modelutil.RecyclerViewAdapter.ViewBinder;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * A {@link ViewBinder} responsible for connecting a {@link DateOrderedListModel} with a underlying
 * {@link ViewHolder}.
 */
class DateOrderedListViewBinder implements ViewBinder<DateOrderedListModel, ViewHolder> {
    /** The potential types of list items that could be displayed. */
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({DATE, IN_PROGRESS, GENERIC, VIDEO, IMAGE})
    public @interface ViewType {}
    public static final int DATE = 0;
    public static final int IN_PROGRESS = 1;
    public static final int GENERIC = 2;
    public static final int VIDEO = 3;
    public static final int IMAGE = 4;

    // ViewBinder implementation.
    @Override
    public ViewHolder onCreateViewHolder(ViewGroup parent, @ViewType int viewType) {
        switch (viewType) {
            case DATE:
                return new DateOrderedListViewHolder.DateViewHolder(parent);
            case IN_PROGRESS:
                return new DateOrderedListViewHolder.InProgressViewHolder(parent);
            case GENERIC:
                return new DateOrderedListViewHolder.GenericViewHolder(parent);
            case VIDEO:
                return new DateOrderedListViewHolder.VideoViewHolder(parent);
            case IMAGE:
                return new DateOrderedListViewHolder.ImageViewHolder(parent);
        }

        assert false;
        return null;
    }

    @Override
    public void onBindViewHolder(DateOrderedListModel model, ViewHolder holder, int position) {
        if (holder instanceof DateOrderedListViewHolder) {
            ((DateOrderedListViewHolder) holder).bind(model.getItemAt(position));
        }
    }
}