// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.list;

import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.support.annotation.CallSuper;
import android.support.v7.widget.AppCompatTextView;
import android.support.v7.widget.RecyclerView.ViewHolder;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;
import android.widget.FrameLayout;
import android.widget.TextView;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.browser.download.home.list.ListItem.DateListItem;
import org.chromium.chrome.browser.download.home.list.ListItem.OfflineItemListItem;
import org.chromium.chrome.browser.download.home.list.ListItem.ViewListItem;
import org.chromium.components.offline_items_collection.ContentId;
import org.chromium.components.offline_items_collection.OfflineItemVisuals;
import org.chromium.components.offline_items_collection.VisualsCallback;

/**
 * A {@link ViewHolder} responsible for building and setting properties on the underlying Android
 * {@link View}s for the Download Manager list.
 */
abstract class ListItemViewHolder extends ViewHolder {
    /** Creates an instance of a {@link ListItemViewHolder}. */
    public ListItemViewHolder(View itemView) {
        super(itemView);
    }

    /**
     * Binds the currently held {@link View} to {@code item}.
     * @param properties The shared {@link ListPropertyModel} all items can access.
     * @param item       The {@link ListItem} to visually represent in this {@link ViewHolder}.
     */
    public abstract void bind(ListPropertyModel properties, ListItem item);

    public static class CustomViewHolder extends ListItemViewHolder {
        public CustomViewHolder(ViewGroup parent) {
            super(new FrameLayout(parent.getContext()));
            itemView.setLayoutParams(
                    new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT));
        }

        // ListItemViewHolder implemenation.
        @Override
        public void bind(ListPropertyModel properties, ListItem item) {
            ViewListItem viewItem = (ViewListItem) item;
            ViewGroup viewGroup = (ViewGroup) itemView;

            if (viewGroup.getChildCount() > 0 && viewGroup.getChildAt(0) == viewItem.customView) {
                return;
            }

            viewGroup.removeAllViews();
            viewGroup.addView(viewItem.customView,
                    new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT));
        }
    }

    /** A {@link ViewHolder} specifically meant to display a date header. */
    public static class DateViewHolder extends ListItemViewHolder {
        public DateViewHolder(ViewGroup parent) {
            super(new AppCompatTextView(parent.getContext()));
        }

        // ListItemViewHolder implementation.
        @Override
        public void bind(ListPropertyModel properties, ListItem item) {
            DateListItem dateItem = (DateListItem) item;
            ((TextView) itemView).setText(UiUtils.dateToHeaderString(dateItem.date));
        }
    }

    /** A {@link ViewHolder} specifically meant to display an in-progress {@code OfflineItem}. */
    public static class InProgressViewHolder extends ListItemViewHolder {
        public InProgressViewHolder(ViewGroup parent) {
            super(new AppCompatTextView(parent.getContext()));
        }

        // ListItemViewHolder implementation.
        @Override
        public void bind(ListPropertyModel properties, ListItem item) {
            OfflineItemListItem offlineItem = (OfflineItemListItem) item;
            ((TextView) itemView).setText(offlineItem.item.title);
        }
    }

    /** A {@link ViewHolder} specifically meant to display a generic {@code OfflineItem}. */
    public static class GenericViewHolder extends ThumbnailAwareViewHolder {
        public GenericViewHolder(ViewGroup parent) {
            super(new AppCompatTextView(parent.getContext()));
        }

        // ListItemViewHolder implementation.
        @Override
        public void bind(ListPropertyModel properties, ListItem item) {
            super.bind(properties, item);
            OfflineItemListItem offlineItem = (OfflineItemListItem) item;
            ((TextView) itemView).setText(offlineItem.item.title);
        }

        @Override
        void onVisualsChanged(OfflineItemVisuals visuals) {
            Drawable drawable = null;
            if (visuals != null && visuals.icon != null) {
                drawable = new BitmapDrawable(itemView.getResources(), visuals.icon);
            }

            ApiCompatibilityUtils.setCompoundDrawablesRelativeWithIntrinsicBounds(
                    (TextView) itemView, drawable, null, null, null);
        }
    }

    /** A {@link ViewHolder} specifically meant to display a video {@code OfflineItem}. */
    public static class VideoViewHolder extends ListItemViewHolder {
        public VideoViewHolder(ViewGroup parent) {
            super(new AppCompatTextView(parent.getContext()));
        }

        // ListItemViewHolder implementation.
        @Override
        public void bind(ListPropertyModel properties, ListItem item) {
            OfflineItemListItem offlineItem = (OfflineItemListItem) item;
            ((TextView) itemView).setText(offlineItem.item.title);
        }
    }

    /** A {@link ViewHolder} specifically meant to display an image {@code OfflineItem}. */
    public static class ImageViewHolder extends ThumbnailAwareViewHolder {
        public ImageViewHolder(ViewGroup parent) {
            super(new AppCompatTextView(parent.getContext()));
        }

        // ThumbnailAwareViewHolder implementation.
        @Override
        public void bind(ListPropertyModel properties, ListItem item) {
            super.bind(properties, item);
            OfflineItemListItem offlineItem = (OfflineItemListItem) item;
            ((TextView) itemView).setText(offlineItem.item.title);
        }

        @Override
        void onVisualsChanged(OfflineItemVisuals visuals) {
            Drawable drawable = null;
            if (visuals != null && visuals.icon != null) {
                drawable = new BitmapDrawable(itemView.getResources(), visuals.icon);
            }

            ApiCompatibilityUtils.setCompoundDrawablesRelativeWithIntrinsicBounds(
                    (TextView) itemView, drawable, null, null, null);
        }
    }

    /** Helper {@link ViewHolder} that handles querying for thumbnails if necessary. */
    private abstract static class ThumbnailAwareViewHolder
            extends ListItemViewHolder implements VisualsCallback {
        private static final int TEST_IMAGE_SIZE_PX = 100;
        /** Track whether or not the result of a query came back while we were making it. */
        private boolean mQueryFinished;
        private ContentId mQueryId;
        private Runnable mQueryCancelRunnable;

        public ThumbnailAwareViewHolder(View view) {
            super(view);
        }

        abstract void onVisualsChanged(OfflineItemVisuals visuals);

        // ListItemViewHolder implementation.
        @Override
        @CallSuper
        public void bind(ListPropertyModel properties, ListItem item) {
            OfflineItemListItem offlineItem = (OfflineItemListItem) item;
            if (offlineItem.item.id.equals(mQueryId)) return;

            if (mQueryId != null) onVisualsChanged(null);
            if (mQueryCancelRunnable != null) mQueryCancelRunnable.run();

            mQueryId = offlineItem.item.id;
            mQueryFinished = false;
            mQueryCancelRunnable = properties.getVisualsProvider().getVisuals(
                    offlineItem.item, TEST_IMAGE_SIZE_PX, TEST_IMAGE_SIZE_PX, this);

            // Handle reentrancy case.
            if (mQueryFinished) mQueryCancelRunnable = null;
        }

        // VisualsCallback implementation.
        @Override
        public void onVisualsAvailable(ContentId id, OfflineItemVisuals visuals) {
            if (!id.equals(mQueryId)) return;
            mQueryCancelRunnable = null;
            mQueryFinished = true;
            onVisualsChanged(visuals);
        }
    }
}