// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.list;

import android.support.annotation.CallSuper;
import android.support.annotation.DrawableRes;
import android.support.annotation.Nullable;
import android.support.v7.widget.AppCompatTextView;
import android.support.v7.widget.RecyclerView.ViewHolder;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.ProgressBar;
import android.widget.TextView;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.download.DownloadUtils;
import org.chromium.chrome.browser.download.home.list.ListItem.DateListItem;
import org.chromium.chrome.browser.download.home.list.ListItem.OfflineItemListItem;
import org.chromium.chrome.browser.download.home.list.ListItem.ViewListItem;
import org.chromium.chrome.browser.download.home.view.GenericListItemView;
import org.chromium.chrome.browser.download.home.view.ImageListItemView;
import org.chromium.chrome.browser.download.home.view.ListItemView;
import org.chromium.chrome.browser.widget.ListMenuButton;
import org.chromium.chrome.browser.widget.ListMenuButton.Item;
import org.chromium.chrome.browser.widget.TintedImageButton;
import org.chromium.components.offline_items_collection.ContentId;
import org.chromium.components.offline_items_collection.OfflineItem;
import org.chromium.components.offline_items_collection.OfflineItemState;
import org.chromium.components.offline_items_collection.OfflineItemVisuals;
import org.chromium.components.offline_items_collection.VisualsCallback;

/**
 * A {@link ViewHolder} responsible for building and setting properties on the underlying Android
 * {@link View}s for the Download Manager list.
 */
abstract class ListItemViewHolder extends ViewHolder {
    private static final int INVALID_ID = -1;

    /** Creates an instance of a {@link ListItemViewHolder}. */
    protected ListItemViewHolder(View itemView) {
        super(itemView);
    }

    /**
     * Used as a method reference for ViewHolderFactory.
     * @see
     * org.chromium.chrome.browser.modelutil.RecyclerViewAdapter.ViewHolderFactory#createViewHolder
     */
    public static ListItemViewHolder create(ViewGroup parent, @ListUtils.ViewType int viewType) {
        switch (viewType) {
            case ListUtils.ViewType.DATE:
                return DateViewHolder.create(parent);
            case ListUtils.ViewType.IN_PROGRESS:
                return InProgressViewHolder.create(parent);
            case ListUtils.ViewType.GENERIC:
                return GenericViewHolder.create(parent);
            case ListUtils.ViewType.VIDEO:
                return new VideoViewHolder(parent);
            case ListUtils.ViewType.IMAGE:
                return ImageViewHolder.create(parent);
            case ListUtils.ViewType.CUSTOM_VIEW:
                return new CustomViewHolder(parent);
            case ListUtils.ViewType.PREFETCH:
                return PrefetchViewHolder.create(parent);
        }

        assert false;
        return null;
    }

    /**
     * Binds the currently held {@link View} to {@code item}.
     * @param properties The shared {@link ListPropertyModel} all items can access.
     * @param item       The {@link ListItem} to visually represent in this {@link ViewHolder}.
     */
    public abstract void bind(ListPropertyModel properties, ListItem item);

    /** A {@link ViewHolder} that holds a {@link View} that is opaque to the holder. */
    public static class CustomViewHolder extends ListItemViewHolder {
        /** Creates a new {@link CustomViewHolder} instance. */
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
        /** Creates a new {@link DateViewHolder} instance. */
        public static DateViewHolder create(ViewGroup parent) {
            View view = LayoutInflater.from(parent.getContext())
                                .inflate(R.layout.download_manager_date_item, null);
            return new DateViewHolder(view);
        }

        private DateViewHolder(View view) {
            super(view);
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
        private final ProgressBar mProgressBar;
        private final TextView mTitle;
        private final TextView mCaption;
        private final TintedImageButton mPauseResumeButton;
        private final TintedImageButton mCancelButton;

        /**
         * Creates a new {@link InProgressViewHolder} instance.
         */
        public static InProgressViewHolder create(ViewGroup parent) {
            View view = LayoutInflater.from(parent.getContext())
                                .inflate(R.layout.download_manager_in_progress_item, null);
            return new InProgressViewHolder(view);
        }

        /**
         * Creates a new {@link InProgressViewHolder} instance.
         */
        public InProgressViewHolder(View view) {
            super(view);
            mProgressBar = view.findViewById(R.id.progress_bar);
            mTitle = view.findViewById(R.id.title);
            mCaption = view.findViewById(R.id.caption);
            mPauseResumeButton = view.findViewById(R.id.pause_button);
            mCancelButton = view.findViewById(R.id.cancel_button);
        }

        // ListItemViewHolder implementation.
        @Override
        public void bind(ListPropertyModel properties, ListItem item) {
            OfflineItemListItem offlineItem = (OfflineItemListItem) item;
            mTitle.setText(offlineItem.item.title);
            mCancelButton.setOnClickListener(
                    v -> properties.getCancelCallback().onResult(offlineItem.item));

            if (offlineItem.item.state == OfflineItemState.PAUSED) {
                mPauseResumeButton.setImageResource(R.drawable.ic_play_arrow_white_24dp);
                mPauseResumeButton.setContentDescription(itemView.getContext().getString(
                        R.string.download_notification_resume_button));
            } else {
                mPauseResumeButton.setImageResource(R.drawable.ic_pause_white_24dp);
                mPauseResumeButton.setContentDescription(itemView.getContext().getString(
                        R.string.download_notification_pause_button));
            }

            // TODO(shaktisahu): Create status string for the new specs.
            mCaption.setText(
                    DownloadUtils.getProgressTextForNotification(offlineItem.item.progress));
            mPauseResumeButton.setOnClickListener(view -> {
                if (offlineItem.item.state == OfflineItemState.PAUSED) {
                    properties.getResumeCallback().onResult(offlineItem.item);
                } else {
                    properties.getPauseCallback().onResult(offlineItem.item);
                }
            });

            boolean showIndeterminate = offlineItem.item.progress.isIndeterminate()
                    && offlineItem.item.state != OfflineItemState.PAUSED
                    && offlineItem.item.state != OfflineItemState.PENDING;
            if (showIndeterminate) {
                mProgressBar.setIndeterminate(true);
                mProgressBar.setIndeterminateDrawable(
                        itemView.getContext().getResources().getDrawable(
                                R.drawable.download_circular_progress_bar));
            } else {
                mProgressBar.setIndeterminate(false);
            }

            if (!offlineItem.item.progress.isIndeterminate()) {
                mProgressBar.setProgress(offlineItem.item.progress.getPercentage());
            }
        }
    }

    /** A {@link ViewHolder} specifically meant to display a generic {@code OfflineItem}. */
    public static class GenericViewHolder extends ThumbnailAwareViewHolder {
        private final TextView mTitle;
        private final TextView mCaption;

        /**
         * Whether or not we are currently showing an icon.  This determines whether or not we
         * udpate the icon on rebind.
         */
        private boolean mDrawingIcon;

        /** The icon to use when there is no thumbnail. */
        private @DrawableRes int mIconId = INVALID_ID;

        /** Creates a new {@link GenericViewHolder} instance. */
        public static GenericViewHolder create(ViewGroup parent) {
            View view = LayoutInflater.from(parent.getContext())
                                .inflate(R.layout.download_manager_generic_item, null);
            int imageSize = parent.getContext().getResources().getDimensionPixelSize(
                    R.dimen.download_manager_generic_thumbnail_size);
            return new GenericViewHolder(view, imageSize);
        }

        private GenericViewHolder(View view, int thumbnailSizePx) {
            super(view, thumbnailSizePx, thumbnailSizePx);

            mTitle = (TextView) itemView.findViewById(R.id.title);
            mCaption = (TextView) itemView.findViewById(R.id.caption);
        }

        // ListItemViewHolder implementation.
        @Override
        public void bind(ListPropertyModel properties, ListItem item) {
            super.bind(properties, item);
            OfflineItemListItem offlineItem = (OfflineItemListItem) item;

            mTitle.setText(offlineItem.item.title);
            mCaption.setText(UiUtils.generateGenericCaption(offlineItem.item));

            mIconId = UiUtils.getIconForItem(offlineItem.item);
        }

        @Override
        void onVisualsChanged(ImageView view, OfflineItemVisuals visuals) {
            mDrawingIcon = visuals == null || visuals.icon == null;

            GenericListItemView selectableView = (GenericListItemView) itemView;
            if (mDrawingIcon) {
                if (mIconId != INVALID_ID) {
                    selectableView.setThumbnailResource(mIconId);
                }
            } else {
                selectableView.setThumbnail(visuals.icon);
            }
        }
    }

    /**
     * A {@link ViewHolder} specifically meant to display a video {@code OfflineItem}.
     */
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
        public static ImageViewHolder create(ViewGroup parent) {
            View view = LayoutInflater.from(parent.getContext())
                                .inflate(R.layout.download_manager_image_item, null);
            int imageSize = parent.getContext().getResources().getDimensionPixelSize(
                    R.dimen.download_manager_image_width);
            return new ImageViewHolder(view, imageSize);
        }

        public ImageViewHolder(View view, int thumbnailSizePx) {
            super(view, thumbnailSizePx, thumbnailSizePx);
        }

        // ThumbnailAwareViewHolder implementation.
        @Override
        public void bind(ListPropertyModel properties, ListItem item) {
            super.bind(properties, item);
            OfflineItemListItem offlineItem = (OfflineItemListItem) item;
            View imageView = itemView.findViewById(R.id.thumbnail);
            imageView.setContentDescription(offlineItem.item.title);

            ImageListItemView view = (ImageListItemView) itemView;
            view.setItem(item);
        }

        @Override
        void onVisualsChanged(ImageView view, OfflineItemVisuals visuals) {
            view.setImageBitmap(visuals == null ? null : visuals.icon);
        }
    }

    /**
     * A {@link ViewHolder} specifically meant to display a prefetch item.
     */
    public static class PrefetchViewHolder extends ThumbnailAwareViewHolder {
        private final TextView mTitle;
        private final TextView mCaption;
        private final TextView mTimestamp;

        /**
         * Creates a new instance of a {@link PrefetchViewHolder}.
         */
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
        }

        @Override
        void onVisualsChanged(ImageView view, OfflineItemVisuals visuals) {
            view.setImageBitmap(visuals == null ? null : visuals.icon);
        }
    }

    /**
     * Helper {@link ViewHolder} that handles showing a 3-dot menu with preset actions.
     */
    private static class MoreButtonViewHolder
            extends ListItemViewHolder implements ListMenuButton.Delegate {
        private final ListMenuButton mMore;

        private Runnable mShareCallback;
        private Runnable mDeleteCallback;

        /**
         * Creates a new instance of a {@link MoreButtonViewHolder}.
         */
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

    /**
     * Helper {@link ViewHolder} that handles querying for thumbnails if necessary.
     */
    private abstract static class ThumbnailAwareViewHolder
            extends MoreButtonViewHolder implements VisualsCallback {
        private final ImageView mThumbnail;

        /**
         * The {@link ContentId} of the associated thumbnail/request if any.
         */
        private @Nullable ContentId mId;

        /**
         * A {@link Runnable} to cancel an outstanding thumbnail request if any.
         */
        private @Nullable Runnable mCancellable;

        /**
         * Whether or not a request is outstanding to support synchronous responses.
         */
        private boolean mIsRequesting;

        /**
         * The ideal width of the queried thumbnail.
         */
        private int mWidthPx;

        /**
         * The ideal height of the queried thumbnail.
         */
        private int mHeightPx;

        /**
         * Creates a new instance of a {@link ThumbnailAwareViewHolder}.
         * @param view              The root {@link View} for this holder.
         * @param thumbnailWidthPx  The desired width of the thumbnail that will be retrieved.
         * @param thumbnailHeightPx The desired height of the thumbnail that will be retrieved.
         */
        public ThumbnailAwareViewHolder(View view, int thumbnailWidthPx, int thumbnailHeightPx) {
            super(view);

            mThumbnail = (ImageView) view.findViewById(R.id.thumbnail);
            mWidthPx = thumbnailWidthPx;
            mHeightPx = thumbnailHeightPx;
        }

        // MoreButtonViewHolder implementation.
        @Override
        @CallSuper
        public void bind(ListPropertyModel properties, ListItem item) {
            super.bind(properties, item);
            // If we have no thumbnail to show just return early.
            if (mThumbnail == null) return;

            OfflineItem offlineItem = ((OfflineItemListItem) item).item;

            // If we're rebinding the same item, ignore the bind.
            if (offlineItem.id.equals(mId)) return;

            ListItemView selectableView = (ListItemView) itemView;
            selectableView.setSelectionDelegate(properties.getSelectionDelegate());
            selectableView.setItem(item);
            selectableView.setClickCallback(
                    () -> properties.getOpenCallback().onResult(offlineItem));

            // Clear any associated bitmap from the thumbnail.
            if (mId != null) onVisualsChanged(mThumbnail, null);

            // Clear out any outstanding thumbnail request.
            if (mCancellable != null) mCancellable.run();

            // Start the new request.
            mId = offlineItem.id;
            mCancellable = properties.getVisualsProvider().getVisuals(
                    offlineItem, mWidthPx, mHeightPx, this);

            // Make sure to update our state properly if we got a synchronous response.
            if (!mIsRequesting) mCancellable = null;
        }

        // VisualsCallback implementation.
        @Override
        public void onVisualsAvailable(ContentId id, OfflineItemVisuals visuals) {
            // Quit early if the request is not for our currently bound item.
            if (!id.equals(mId)) return;

            // Clear out the request state.
            mCancellable = null;
            mIsRequesting = false;

            // Notify of the new visuals (if any).
            onVisualsChanged(mThumbnail, visuals);
        }

        /**
         * Called when the contents of the thumbnail should be changed to due an event (either this
         * {@link ViewHolder} being rebound to another {@link ListItem} or a thumbnail query
         * returning results.
         * @param view    The {@link ImageView} that the thumbnail should be set on.
         * @param visuals The {@link OfflineItemVisuals} that were returned by the backend if any.
         */
        abstract void onVisualsChanged(ImageView view, @Nullable OfflineItemVisuals visuals);
    }
}
