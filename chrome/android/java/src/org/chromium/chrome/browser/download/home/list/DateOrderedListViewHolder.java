// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.list;

import android.support.v7.widget.AppCompatTextView;
import android.support.v7.widget.RecyclerView.ViewHolder;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import org.chromium.chrome.browser.download.home.list.DateOrderedListModel.ListItem;

/**
 * A {@link ViewHolder} responsible for building and setting properties on the underlying Android
 * {@link View}s for the Download Manager list.
 */
abstract class DateOrderedListViewHolder extends ViewHolder {
    /** Creates an instance of a {@link DateOrderedListViewHolder}. */
    public DateOrderedListViewHolder(View itemView) {
        super(itemView);
    }

    /**
     * Binds the currently held {@link View} to {@code item}.
     * @param item The {@link ListItem} to visually represent in this {@link ViewHolder}.
     */
    public abstract void bind(ListItem item);

    /** A {@link ViewHolder} specifically meant to display a date header. */
    public static class DateViewHolder extends DateOrderedListViewHolder {
        public DateViewHolder(ViewGroup parent) {
            super(new AppCompatTextView(parent.getContext()));
        }

        // DateOrderedListViewHolder implementation.
        @Override
        public void bind(ListItem item) {
            ((TextView) itemView).setText(UiUtils.dateToHeaderString(item.date));
        }
    }

    /** A {@link ViewHolder} specifically meant to display an in-progress {@code OfflineItem}. */
    public static class InProgressViewHolder extends DateOrderedListViewHolder {
        public InProgressViewHolder(ViewGroup parent) {
            super(new AppCompatTextView(parent.getContext()));
        }

        // DateOrderedListViewHolder implementation.
        @Override
        public void bind(ListItem item) {
            ((TextView) itemView).setText(item.item.title);
        }
    }

    /** A {@link ViewHolder} specifically meant to display a generic {@code OfflineItem}. */
    public static class GenericViewHolder extends DateOrderedListViewHolder {
        public GenericViewHolder(ViewGroup parent) {
            super(new AppCompatTextView(parent.getContext()));
        }

        // DateOrderedListViewHolder implementation.
        @Override
        public void bind(ListItem item) {
            ((TextView) itemView).setText(item.item.title);
        }
    }

    /** A {@link ViewHolder} specifically meant to display a video {@code OfflineItem}. */
    public static class VideoViewHolder extends DateOrderedListViewHolder {
        public VideoViewHolder(ViewGroup parent) {
            super(new AppCompatTextView(parent.getContext()));
        }

        // DateOrderedListViewHolder implementation.
        @Override
        public void bind(ListItem item) {
            ((TextView) itemView).setText(item.item.title);
        }
    }

    /** A {@link ViewHolder} specifically meant to display an image {@code OfflineItem}. */
    public static class ImageViewHolder extends DateOrderedListViewHolder {
        public ImageViewHolder(ViewGroup parent) {
            super(new AppCompatTextView(parent.getContext()));
        }

        // DateOrderedListViewHolder implementation.
        @Override
        public void bind(ListItem item) {
            ((TextView) itemView).setText(item.item.title);
        }
    }
}