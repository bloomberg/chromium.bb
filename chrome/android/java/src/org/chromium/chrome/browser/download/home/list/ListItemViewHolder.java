// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.list;

import android.graphics.Bitmap;
import android.support.annotation.CallSuper;
import android.support.v7.widget.AppCompatTextView;
import android.support.v7.widget.RecyclerView.ViewHolder;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.TextView;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.download.home.list.ListItem.DateListItem;
import org.chromium.chrome.browser.download.home.list.ListItem.OfflineItemListItem;
import org.chromium.chrome.browser.download.home.list.ListItem.ViewListItem;
import org.chromium.chrome.browser.widget.ListMenuButton;
import org.chromium.chrome.browser.widget.ListMenuButton.Item;
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
        public static GenericViewHolder create(ViewGroup parent) {
            View view = new AppCompatTextView(parent.getContext());
            int imageSize = parent.getContext().getResources().getDimensionPixelSize(
                    R.dimen.download_manager_generic_thumbnail_size);
            return new GenericViewHolder(view, imageSize);
        }

        private GenericViewHolder(View view, int thumbnailSizePx) {
            super(view, thumbnailSizePx, thumbnailSizePx);
        }

        // ListItemViewHolder implementation.
        @Override
        public void bind(ListPropertyModel properties, ListItem item) {
            super.bind(properties, item);
            OfflineItemListItem offlineItem = (OfflineItemListItem) item;
            ((TextView) itemView).setText(offlineItem.item.title);
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
    public static class ImageViewHolder extends ListItemViewHolder {
        public ImageViewHolder(ViewGroup parent) {
            super(new AppCompatTextView(parent.getContext()));
        }

        // ListItemViewHolder implementation.
        @Override
        public void bind(ListPropertyModel properties, ListItem item) {
            OfflineItemListItem offlineItem = (OfflineItemListItem) item;
            ((TextView) itemView).setText(offlineItem.item.title);
        }
    }

    /** A {@link ViewHolder} specifically meant to display a prefetch item. */
    public static class PrefetchViewHolder extends ThumbnailAwareViewHolder {
        private final TextView mTitle;
        private final TextView mCaption;
        private final TextView mTimestamp;

        /** Creates a new instance of a {@link PrefetchViewHolder}. */
        public static PrefetchViewHolder create(ViewGroup parent) {
            View view = LayoutInflater.from(parent.getContext())
                                .inflate(R.layout.download_manager_prefetch_item, null);
            int imageSize = parent.getContext().getResources().getDimensionPixelSize(
                    R.dimen.download_manager_prefetch_thumbnail_size);
            return new PrefetchViewHolder(view, imageSize);
        }

        private PrefetchViewHolder(View view, int thumbnailSizePx) {
            super(view, thumbnailSizePx, thumbnailSizePx);
            mTitle = (TextView) itemView.findViewById(R.id.title);
            mCaption = (TextView) itemView.findViewById(R.id.caption);
            mTimestamp = (TextView) itemView.findViewById(R.id.timestamp);
        }

        // ThumbnailAwareViewHolder implementation.
        @Override
        public void bind(ListPropertyModel properties, ListItem item) {
            super.bind(properties, item);
            OfflineItemListItem offlineItem = (OfflineItemListItem) item;

            mTitle.setText(offlineItem.item.title);
            mCaption.setText(UiUtils.generatePrefetchCaption(offlineItem.item));
            mTimestamp.setText(UiUtils.generatePrefetchTimestamp(offlineItem.date));

            itemView.setOnClickListener(
                    v -> properties.getOpenCallback().onResult(offlineItem.item));
        }
    }

    /** Helper {@link ViewHolder} that handles showing a 3-dot menu with preset actions. */
    private static class MoreButtonViewHolder
            extends ListItemViewHolder implements ListMenuButton.Delegate {
        private final ListMenuButton mMore;

        private Runnable mShareCallback;
        private Runnable mDeleteCallback;

        /** Creates a new instance of a {@link MoreButtonViewHolder}. */
        public MoreButtonViewHolder(View view) {
            super(view);
            mMore = (ListMenuButton) view.findViewById(R.id.more);
            if (mMore != null) mMore.setDelegate(this);
        }

        // ListItemViewHolder implementation.
        @CallSuper
        @Override
        public void bind(ListPropertyModel properties, ListItem item) {
            OfflineItemListItem offlineItem = (OfflineItemListItem) item;
            mShareCallback = () -> properties.getShareCallback().onResult(offlineItem.item);
            mDeleteCallback = () -> properties.getRemoveCallback().onResult(offlineItem.item);
        }

        // ListMenuButton.Delegate implementation.
        @Override
        public Item[] getItems() {
            return new Item[] {new Item(itemView.getContext(), R.string.share, true),
                    new Item(itemView.getContext(), R.string.delete, true)};
        }

        @Override
        public void onItemSelected(Item item) {
            if (item.getTextId() == R.string.share) {
                if (mShareCallback != null) mShareCallback.run();
            } else if (item.getTextId() == R.string.delete) {
                if (mDeleteCallback != null) mDeleteCallback.run();
            }
        }
    }

    /** Helper {@link ViewHolder} that handles querying for thumbnails if necessary. */
    private abstract static class ThumbnailAwareViewHolder
            extends MoreButtonViewHolder implements VisualsCallback {
        private final int mThumbnailWidthPx;
        private final int mThumbnailHeightPx;
        private final ImageView mThumbnail;

        /** Track whether or not the result of a query came back while we were making it. */
        private boolean mQueryFinished;
        private ContentId mQueryId;
        private Runnable mQueryCancelRunnable;

        /**
         * Creates a new instance of a {@link ThumbnailAwareViewHolder}.
         * @param view              The root {@link View} for this holder.
         * @param thumbnailWidthPx  The desired width of the thumbnail that will be retrieved.
         * @param thumbnailHeightPx The desired height of the thumbnail that will be retrieved.
         */
        public ThumbnailAwareViewHolder(View view, int thumbnailWidthPx, int thumbnailHeightPx) {
            super(view);

            mThumbnailWidthPx = thumbnailWidthPx;
            mThumbnailHeightPx = thumbnailHeightPx;
            mThumbnail = (ImageView) view.findViewById(R.id.thumbnail);
        }

        // MoreButtonViewHolder implementation.
        @Override
        @CallSuper
        public void bind(ListPropertyModel properties, ListItem item) {
            super.bind(properties, item);
            OfflineItemListItem offlineItem = (OfflineItemListItem) item;

            if (mThumbnail == null) return;
            if (offlineItem.item.id.equals(mQueryId)) return;

            if (mQueryId != null) onVisualsChanged(null);
            if (mQueryCancelRunnable != null) mQueryCancelRunnable.run();

            mQueryId = offlineItem.item.id;
            mQueryFinished = false;
            mQueryCancelRunnable = properties.getVisualsProvider().getVisuals(
                    offlineItem.item, mThumbnailWidthPx, mThumbnailHeightPx, this);

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

        private void onVisualsChanged(OfflineItemVisuals visuals) {
            Bitmap bitmap = null;
            if (visuals != null && visuals.icon != null) bitmap = visuals.icon;
            mThumbnail.setImageBitmap(bitmap);
        }
    }
}