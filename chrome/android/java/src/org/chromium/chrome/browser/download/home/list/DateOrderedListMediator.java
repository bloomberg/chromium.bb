// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.list;

import android.content.Intent;
import android.os.Handler;
import android.support.annotation.Nullable;
import android.support.v4.util.Pair;

import org.chromium.base.CollectionUtil;
import org.chromium.base.ContextUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.ChromeApplication;
import org.chromium.chrome.browser.download.home.JustNowProvider;
import org.chromium.chrome.browser.download.home.OfflineItemSource;
import org.chromium.chrome.browser.download.home.filter.DeleteUndoOfflineItemFilter;
import org.chromium.chrome.browser.download.home.filter.Filters.FilterType;
import org.chromium.chrome.browser.download.home.filter.InvalidStateOfflineItemFilter;
import org.chromium.chrome.browser.download.home.filter.OffTheRecordOfflineItemFilter;
import org.chromium.chrome.browser.download.home.filter.OfflineItemFilter;
import org.chromium.chrome.browser.download.home.filter.OfflineItemFilterObserver;
import org.chromium.chrome.browser.download.home.filter.OfflineItemFilterSource;
import org.chromium.chrome.browser.download.home.filter.SearchOfflineItemFilter;
import org.chromium.chrome.browser.download.home.filter.TypeOfflineItemFilter;
import org.chromium.chrome.browser.download.home.glue.OfflineContentProviderGlue;
import org.chromium.chrome.browser.download.home.glue.ThumbnailRequestGlue;
import org.chromium.chrome.browser.download.home.list.DateOrderedListCoordinator.DateOrderedListObserver;
import org.chromium.chrome.browser.download.home.list.DateOrderedListCoordinator.DeleteController;
import org.chromium.chrome.browser.widget.ThumbnailProvider;
import org.chromium.chrome.browser.widget.ThumbnailProvider.ThumbnailRequest;
import org.chromium.chrome.browser.widget.ThumbnailProviderImpl;
import org.chromium.chrome.browser.widget.selection.SelectionDelegate;
import org.chromium.components.offline_items_collection.OfflineContentProvider;
import org.chromium.components.offline_items_collection.OfflineItem;
import org.chromium.components.offline_items_collection.OfflineItemShareInfo;
import org.chromium.components.offline_items_collection.OfflineItemState;
import org.chromium.components.offline_items_collection.VisualsCallback;

import java.io.Closeable;
import java.util.ArrayList;
import java.util.Collection;
import java.util.List;

/**
 * A Mediator responsible for converting an OfflineContentProvider to a list of items in downloads
 * home.  This includes support for filtering, deleting, etc..
 */
class DateOrderedListMediator {
    /** Helper interface for handling share requests by the UI. */
    @FunctionalInterface
    public interface ShareController {
        /**
         * Will be called whenever {@link OfflineItem}s are being requested to be shared by the UI.
         * @param intent The {@link Intent} representing the share action to broadcast to Android.
         */
        void share(Intent intent);
    }

    private final Handler mHandler = new Handler();

    private final OfflineContentProviderGlue mProvider;
    private final ShareController mShareController;
    private final ListItemModel mModel;
    private final DeleteController mDeleteController;

    private final OfflineItemSource mSource;
    private final DateOrderedListMutator mListMutator;
    private final ThumbnailProvider mThumbnailProvider;
    private final MediatorSelectionObserver mSelectionObserver;
    private final SelectionDelegate<ListItem> mSelectionDelegate;

    private final OffTheRecordOfflineItemFilter mOffTheRecordFilter;
    private final InvalidStateOfflineItemFilter mInvalidStateFilter;
    private final DeleteUndoOfflineItemFilter mDeleteUndoFilter;
    private final TypeOfflineItemFilter mTypeFilter;
    private final SearchOfflineItemFilter mSearchFilter;

    /**
     * A selection observer that correctly updates the selection state for each item in the list.
     */
    private class MediatorSelectionObserver
            implements SelectionDelegate.SelectionObserver<ListItem> {
        private final SelectionDelegate<ListItem> mSelectionDelegate;

        public MediatorSelectionObserver(SelectionDelegate<ListItem> delegate) {
            mSelectionDelegate = delegate;
            mSelectionDelegate.addObserver(this);
        }

        @Override
        public void onSelectionStateChange(List<ListItem> selectedItems) {
            for (int i = 0; i < mModel.size(); i++) {
                ListItem item = mModel.get(i);
                boolean selected = mSelectionDelegate.isItemSelected(item);
                item.showSelectedAnimation = selected && !item.selected;
                item.selected = selected;
                mModel.update(i, item);
            }
            mModel.dispatchLastEvent();
            mModel.getProperties().set(
                    ListProperties.SELECTION_MODE_ACTIVE, mSelectionDelegate.isSelectionEnabled());
        }
    }

    /**
     * Creates an instance of a DateOrderedListMediator that will push {@code provider} into
     * {@code model}.
     * @param offTheRecord            Whether or not to include off the record items.
     * @param provider                The {@link OfflineContentProvider} to visually represent.
     * @param deleteController        A class to manage whether or not items can be deleted.
     * @param shareController         A class responsible for sharing downloaded item {@link
     *                                Intent}s.
     * @param selectionDelegate       A class responsible for handling list item selection.
     * @param dateOrderedListObserver An observer of the list and recycler view.
     * @param model                   The {@link ListItemModel} to push {@code provider} into.
     */
    public DateOrderedListMediator(boolean offTheRecord, OfflineContentProvider provider,
            ShareController shareController, DeleteController deleteController,
            SelectionDelegate<ListItem> selectionDelegate,
            DateOrderedListObserver dateOrderedListObserver, ListItemModel model) {
        // Build a chain from the data source to the model.  The chain will look like:
        // [OfflineContentProvider] ->
        //     [OfflineItemSource] ->
        //         [OffTheRecordOfflineItemFilter] ->
        //             [InvalidStateOfflineItemFilter] ->
        //                 [DeleteUndoOfflineItemFilter] ->
        //                     [SearchOfflineItemFitler] ->
        //                         [TypeOfflineItemFilter] ->
        //                             [DateOrderedListMutator] ->
        //                                 [ListItemModel]

        mProvider = new OfflineContentProviderGlue(provider, offTheRecord);
        mShareController = shareController;
        mModel = model;
        mDeleteController = deleteController;
        mSelectionDelegate = selectionDelegate;

        mSource = new OfflineItemSource(mProvider);
        mOffTheRecordFilter = new OffTheRecordOfflineItemFilter(offTheRecord, mSource);
        mInvalidStateFilter = new InvalidStateOfflineItemFilter(mOffTheRecordFilter);
        mDeleteUndoFilter = new DeleteUndoOfflineItemFilter(mInvalidStateFilter);
        mSearchFilter = new SearchOfflineItemFilter(mDeleteUndoFilter);
        mTypeFilter = new TypeOfflineItemFilter(mSearchFilter);
        mListMutator = new DateOrderedListMutator(mTypeFilter, mModel, new JustNowProvider());

        mSearchFilter.addObserver(new EmptyStateObserver(mSearchFilter, dateOrderedListObserver));
        mThumbnailProvider = new ThumbnailProviderImpl(
                ((ChromeApplication) ContextUtils.getApplicationContext()).getReferencePool());
        mSelectionObserver = new MediatorSelectionObserver(selectionDelegate);

        mModel.getProperties().set(ListProperties.ENABLE_ITEM_ANIMATIONS, true);
        mModel.getProperties().set(ListProperties.CALLBACK_OPEN, mProvider::openItem);
        mModel.getProperties().set(ListProperties.CALLBACK_PAUSE, mProvider::pauseDownload);
        mModel.getProperties().set(
                ListProperties.CALLBACK_RESUME, item -> mProvider.resumeDownload(item, true));
        mModel.getProperties().set(ListProperties.CALLBACK_CANCEL, mProvider::cancelDownload);
        mModel.getProperties().set(ListProperties.CALLBACK_SHARE, this ::onShareItem);
        mModel.getProperties().set(ListProperties.CALLBACK_SHARE_ALL, this ::onShareItems);
        mModel.getProperties().set(ListProperties.CALLBACK_REMOVE, this ::onDeleteItem);
        mModel.getProperties().set(ListProperties.CALLBACK_REMOVE_ALL, this ::onDeleteItems);
        mModel.getProperties().set(ListProperties.PROVIDER_VISUALS, this ::getVisuals);
        mModel.getProperties().set(ListProperties.CALLBACK_SELECTION, this ::onSelection);
    }

    /** Tears down this mediator. */
    public void destroy() {
        mSource.destroy();
        mProvider.destroy();
        mThumbnailProvider.destroy();
    }

    /**
     * To be called when this mediator should filter its content based on {@code filter}.
     * @see TypeOfflineItemFilter#onFilterSelected(int)
     */
    public void onFilterTypeSelected(@FilterType int filter) {
        mListMutator.onFilterTypeSelected(filter);
        try (AnimationDisableClosable closeable = new AnimationDisableClosable()) {
            mTypeFilter.onFilterSelected(filter);
        }
    }

    /**
     * To be called when this mediator should filter its content based on {@code filter}.
     * @see SearchOfflineItemFilter#onQueryChanged(String)
     */
    public void onFilterStringChanged(String filter) {
        try (AnimationDisableClosable closeable = new AnimationDisableClosable()) {
            mSearchFilter.onQueryChanged(filter);
        }
    }

    /** Called to delete the list of currently selected items. */
    public void deleteSelectedItems() {
        RecordHistogram.recordCount100Histogram("Android.DownloadManager.Menu.Delete.SelectedCount",
                mSelectionDelegate.getSelectedItems().size());

        onDeleteItems(ListUtils.toOfflineItems(mSelectionDelegate.getSelectedItems()));
        mSelectionDelegate.clearSelection();
    }

    /** Called to share the list of currently selected items. */
    public void shareSelectedItems() {
        RecordHistogram.recordCount100Histogram("Android.DownloadManager.Menu.Share.SelectedCount",
                mSelectionDelegate.getSelectedItems().size());

        onShareItems(ListUtils.toOfflineItems(mSelectionDelegate.getSelectedItems()));
        mSelectionDelegate.clearSelection();
    }

    /** Called to handle a back press event. */
    public boolean handleBackPressed() {
        if (mSelectionDelegate.isSelectionEnabled()) {
            mSelectionDelegate.clearSelection();
            return true;
        }
        return false;
    }

    /**
     * @return The {@link OfflineItemFilterSource} that should be used to determine which filter
     *         options are available.
     */
    public OfflineItemFilterSource getFilterSource() {
        return mSearchFilter;
    }

    /**
     * @return The {@link OfflineItemFilterSource} that should be used to determine whether there
     * are no items and empty view should be shown.
     */
    public OfflineItemFilterSource getEmptySource() {
        return mTypeFilter;
    }

    private void onSelection(@Nullable ListItem item) {
        if (item == null) {
            mSelectionDelegate.setSelectionModeEnabledForZeroItems(true);
        } else {
            mSelectionDelegate.toggleSelectionForItem(item);
        }
    }

    private void onDeleteItem(OfflineItem item) {
        onDeleteItems(CollectionUtil.newArrayList(item));
    }

    /**
     * Deletes a given list of items. If the items are not completed yet, they would be cancelled.
     * @param items The list of items to delete.
     */
    private void onDeleteItems(List<OfflineItem> items) {
        // Calculate the real offline items we are going to remove here.
        final Collection<OfflineItem> itemsToDelete =
                ItemUtils.findItemsWithSameFilePath(items, mSource.getItems());

        mDeleteUndoFilter.addPendingDeletions(itemsToDelete);
        mDeleteController.canDelete(items, delete -> {
            if (delete) {
                for (OfflineItem item : itemsToDelete) {
                    if (item.state != OfflineItemState.COMPLETE) {
                        mProvider.cancelDownload(item);
                    } else {
                        mProvider.removeItem(item);
                    }

                    // Remove and have a single decision path for cleaning up thumbnails when the
                    // glue layer is no longer needed.
                    mProvider.removeVisualsForItem(mThumbnailProvider, item.id);
                }
            } else {
                mDeleteUndoFilter.removePendingDeletions(itemsToDelete);
            }
        });
    }

    private void onShareItem(OfflineItem item) {
        onShareItems(CollectionUtil.newHashSet(item));
    }

    private void onShareItems(Collection<OfflineItem> items) {
        final Collection<Pair<OfflineItem, OfflineItemShareInfo>> shareInfo = new ArrayList<>();

        for (OfflineItem item : items) {
            mProvider.getShareInfoForItem(item, (id, info) -> {
                shareInfo.add(Pair.create(item, info));

                // When we've gotten callbacks for all items, create and share the intent.
                if (shareInfo.size() == items.size()) {
                    Intent intent = ShareUtils.createIntent(shareInfo);
                    if (intent != null) mShareController.share(intent);
                }
            });
        }
    }

    private Runnable getVisuals(
            OfflineItem item, int iconWidthPx, int iconHeightPx, VisualsCallback callback) {
        if (!UiUtils.canHaveThumbnails(item) || iconWidthPx == 0 || iconHeightPx == 0) {
            mHandler.post(() -> callback.onVisualsAvailable(item.id, null));
            return () -> {};
        }

        ThumbnailRequest request =
                new ThumbnailRequestGlue(mProvider, item, iconWidthPx, iconHeightPx, callback);
        mThumbnailProvider.getThumbnail(request);
        return () -> mThumbnailProvider.cancelRetrieval(request);
    }

    /** Helper class to disable animations for certain list changes. */
    private class AnimationDisableClosable implements Closeable {
        AnimationDisableClosable() {
            mModel.getProperties().set(ListProperties.ENABLE_ITEM_ANIMATIONS, false);
        }

        // Closeable implementation.
        @Override
        public void close() {
            mHandler.post(() -> {
                mModel.getProperties().set(ListProperties.ENABLE_ITEM_ANIMATIONS, true);
            });
        }
    }

    /**
     * A helper class to observe the list content and notify the given observer when the list state
     * changes between empty and non-empty.
     */
    private static class EmptyStateObserver implements OfflineItemFilterObserver {
        private boolean mIsEmpty;
        private final DateOrderedListObserver mDateOrderedListObserver;
        private final OfflineItemFilter mOfflineItemFilter;

        public EmptyStateObserver(OfflineItemFilter offlineItemFilter,
                DateOrderedListObserver dateOrderedListObserver) {
            mOfflineItemFilter = offlineItemFilter;
            mDateOrderedListObserver = dateOrderedListObserver;
            calculateEmptyState();
        }

        @Override
        public void onItemsAvailable() {
            calculateEmptyState();
        }

        @Override
        public void onItemsAdded(Collection<OfflineItem> items) {
            calculateEmptyState();
        }

        @Override
        public void onItemsRemoved(Collection<OfflineItem> items) {
            calculateEmptyState();
        }

        @Override
        public void onItemUpdated(OfflineItem oldItem, OfflineItem item) {
            calculateEmptyState();
        }

        private void calculateEmptyState() {
            boolean isEmpty = mOfflineItemFilter.getItems().isEmpty();
            if (mIsEmpty == isEmpty) return;

            mIsEmpty = isEmpty;
            mDateOrderedListObserver.onEmptyStateChanged(mIsEmpty);
        }
    }
}
