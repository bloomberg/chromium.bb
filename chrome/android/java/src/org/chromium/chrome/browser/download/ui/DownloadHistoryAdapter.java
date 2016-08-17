// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.ui;

import android.content.ComponentName;
import android.content.Context;
import android.content.res.Resources;
import android.graphics.Paint;
import android.support.v7.widget.RecyclerView;
import android.support.v7.widget.RecyclerView.ViewHolder;
import android.text.TextUtils;
import android.text.format.Formatter;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.TextView;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ContextUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.download.DownloadItem;
import org.chromium.chrome.browser.download.DownloadManagerService;
import org.chromium.chrome.browser.download.ui.DownloadHistoryItemWrapper.DownloadItemWrapper;
import org.chromium.chrome.browser.download.ui.DownloadHistoryItemWrapper.OfflinePageItemWrapper;
import org.chromium.chrome.browser.download.ui.DownloadManagerUi.DownloadUiObserver;
import org.chromium.chrome.browser.offlinepages.downloads.OfflinePageDownloadBridge;
import org.chromium.chrome.browser.offlinepages.downloads.OfflinePageDownloadItem;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.util.UrlUtilities;
import org.chromium.chrome.browser.widget.DateDividedAdapter;

import java.util.ArrayList;
import java.util.List;

/** Bridges the user's download history and the UI used to display it. */
public class DownloadHistoryAdapter extends DateDividedAdapter implements DownloadUiObserver {

    /** Holds onto a View that displays information about a downloaded file. */
    static class ItemViewHolder extends RecyclerView.ViewHolder {
        public DownloadItemView mItemView;
        public ImageView mIconView;
        public TextView mFilenameView;
        public TextView mHostnameView;
        public TextView mFilesizeView;

        public ItemViewHolder(View itemView) {
            super(itemView);

            assert itemView instanceof DownloadItemView;
            mItemView = (DownloadItemView) itemView;

            mIconView = (ImageView) itemView.findViewById(R.id.icon_view);
            mFilenameView = (TextView) itemView.findViewById(R.id.filename_view);
            mHostnameView = (TextView) itemView.findViewById(R.id.hostname_view);
            mFilesizeView = (TextView) itemView.findViewById(R.id.filesize_view);
        }
    }

    /** See {@link #findItemIndex}. */
    private static final int INVALID_INDEX = -1;

    private final List<DownloadItemWrapper> mDownloadItems = new ArrayList<>();
    private final List<DownloadItemWrapper> mDownloadOffTheRecordItems = new ArrayList<>();
    private final List<OfflinePageItemWrapper> mOfflinePageItems = new ArrayList<>();
    private final List<DownloadHistoryItemWrapper> mFilteredItems = new ArrayList<>();
    private final ComponentName mParentComponent;
    private final boolean mShowOffTheRecord;

    private int mFilter = DownloadFilter.FILTER_ALL;
    private DownloadManagerUi mManager;
    private OfflinePageDownloadBridge mOfflinePageBridge;
    private int mFilenameViewTextColor;

    DownloadHistoryAdapter(boolean showOffTheRecord, ComponentName parentComponent) {
        mShowOffTheRecord = showOffTheRecord;
        mParentComponent = parentComponent;
    }

    @Override
    public void initialize(DownloadManagerUi manager) {
        manager.addObserver(this);
        mManager = manager;

        // Get all regular and (if necessary) off the record downloads.
        getDownloadManagerService().addDownloadHistoryAdapter(this);
        getDownloadManagerService().getAllDownloads(false);
        if (mShowOffTheRecord) getDownloadManagerService().getAllDownloads(true);

        initializeOfflinePageBridge();
    }

    /** Called when the user's download history has been gathered. */
    public void onAllDownloadsRetrieved(List<DownloadItem> result, boolean isOffTheRecord) {
        if (isOffTheRecord && !mShowOffTheRecord) return;

        List<DownloadItemWrapper> list = getDownloadItemList(isOffTheRecord);
        list.clear();
        for (DownloadItem item : result) list.add(new DownloadItemWrapper(item, isOffTheRecord));
        filter(DownloadFilter.FILTER_ALL);
    }

    /** Called when the user's offline page history has been gathered. */
    private void onAllOfflinePagesRetrieved(List<OfflinePageDownloadItem> result) {
        mOfflinePageItems.clear();
        for (OfflinePageDownloadItem item : result) {
            mOfflinePageItems.add(
                    new OfflinePageItemWrapper(item, mOfflinePageBridge, mParentComponent));
        }

        // TODO(ianwen): Implement a loading screen to prevent filter-changing wonkiness.
        filter(DownloadFilter.FILTER_ALL);
    }

    /** Returns the total size of all the downloaded items. */
    public long getTotalDownloadSize() {
        long totalSize = 0;
        for (DownloadHistoryItemWrapper wrapper : mDownloadItems) {
            totalSize += wrapper.getFileSize();
        }
        for (DownloadHistoryItemWrapper wrapper : mDownloadOffTheRecordItems) {
            totalSize += wrapper.getFileSize();
        }
        for (DownloadHistoryItemWrapper wrapper : mOfflinePageItems) {
            totalSize += wrapper.getFileSize();
        }
        return totalSize;
    }

    @Override
    protected int getTimedItemViewResId() {
        return R.layout.download_date_view;
    }

    @Override
    public ViewHolder createViewHolder(ViewGroup parent) {
        View v = LayoutInflater.from(parent.getContext()).inflate(
                R.layout.download_item_view, parent, false);
        ((DownloadItemView) v).setSelectionDelegate(mManager.getSelectionDelegate());
        return new ItemViewHolder(v);
    }

    @Override
    public void bindViewHolderForTimedItem(ViewHolder current, TimedItem timedItem) {
        final DownloadHistoryItemWrapper item = (DownloadHistoryItemWrapper) timedItem;

        ItemViewHolder holder = (ItemViewHolder) current;
        DownloadHistoryItemWrapper previousItem = holder.mItemView.mItem;
        Context context = holder.mFilesizeView.getContext();
        holder.mFilenameView.setText(item.getDisplayFileName());
        holder.mHostnameView.setText(
                UrlUtilities.formatUrlForSecurityDisplay(item.getUrl(), false));
        holder.mFilesizeView.setText(
                Formatter.formatFileSize(context, item.getFileSize()));
        holder.mItemView.initialize(item);

        // Pick what icon to display for the item.
        int fileType = item.getFilterType();
        int iconResource = R.drawable.ic_drive_file_white_24dp;
        switch (fileType) {
            case DownloadFilter.FILTER_PAGE:
                iconResource = R.drawable.ic_drive_site_white_24dp;
                break;
            case DownloadFilter.FILTER_VIDEO:
                iconResource = R.drawable.ic_play_arrow_white_24dp;
                break;
            case DownloadFilter.FILTER_AUDIO:
                iconResource = R.drawable.ic_music_note_white_24dp;
                break;
            case DownloadFilter.FILTER_IMAGE:
                iconResource = R.drawable.ic_image_white_24dp;
                break;
            case DownloadFilter.FILTER_DOCUMENT:
                iconResource = R.drawable.ic_drive_text_white_24dp;
                break;
            default:
        }

        holder.mIconView.setImageResource(iconResource);

        // Externally removed items have a different style. Update the item's style if necessary.
        if (previousItem == null
                || previousItem.hasBeenExternallyRemoved() != item.hasBeenExternallyRemoved()) {
            setItemViewStyle(holder, item);
        }
    }

    /**
     * Updates the list when new information about a download comes in.
     */
    public void onDownloadItemUpdated(DownloadItem item, boolean isOffTheRecord) {
        if (isOffTheRecord && !mShowOffTheRecord) return;

        List<DownloadItemWrapper> list = getDownloadItemList(isOffTheRecord);
        int index = findItemIndex(list, item.getId());
        if (index == INVALID_INDEX) {
            // Add a new entry.
            list.add(new DownloadItemWrapper(item, isOffTheRecord));
        } else {
            // Update the old one.
            list.set(index, new DownloadItemWrapper(item, isOffTheRecord));
        }

        filter(mFilter);
    }

    /**
     * Removes the DownloadItem with the given ID.
     * @param guid           ID of the DownloadItem that has been removed.
     * @param isOffTheRecord True if off the record, false otherwise.
     */
    public void onDownloadItemRemoved(String guid, boolean isOffTheRecord) {
        if (isOffTheRecord && !mShowOffTheRecord) return;

        List<DownloadItemWrapper> list = getDownloadItemList(isOffTheRecord);
        int index = findItemIndex(list, guid);
        if (index != INVALID_INDEX) {
            DownloadItemWrapper wrapper = list.remove(index);
            if (mManager.getSelectionDelegate().isItemSelected(wrapper)) {
                mManager.getSelectionDelegate().toggleSelectionForItem(wrapper);
            }
            filter(mFilter);
        }
    }

    @Override
    public void onFilterChanged(int filter) {
        filter(filter);
    }

    @Override
    public void onManagerDestroyed(DownloadManagerUi manager) {
        getDownloadManagerService().removeDownloadHistoryAdapter(this);

        if (mOfflinePageBridge != null) {
            mOfflinePageBridge.destroy();
            mOfflinePageBridge = null;
        }

        mManager = null;
    }

    /** Filters the list of downloads to show only files of a specific type. */
    private void filter(int filterType) {
        mFilter = filterType;
        mFilteredItems.clear();
        if (filterType == DownloadFilter.FILTER_ALL) {
            mFilteredItems.addAll(mDownloadItems);
            mFilteredItems.addAll(mDownloadOffTheRecordItems);
            mFilteredItems.addAll(mOfflinePageItems);
        } else {
            for (DownloadHistoryItemWrapper item : mDownloadItems) {
                if (item.getFilterType() == filterType) mFilteredItems.add(item);
            }

            for (DownloadHistoryItemWrapper item : mDownloadOffTheRecordItems) {
                if (item.getFilterType() == filterType) mFilteredItems.add(item);
            }

            if (filterType == DownloadFilter.FILTER_PAGE) {
                for (DownloadHistoryItemWrapper item : mOfflinePageItems) mFilteredItems.add(item);
            }
        }

        loadItems(mFilteredItems);
    }

    private void initializeOfflinePageBridge() {
        mOfflinePageBridge = new OfflinePageDownloadBridge(
                Profile.getLastUsedProfile().getOriginalProfile());

        mOfflinePageBridge.addObserver(new OfflinePageDownloadBridge.Observer() {
            @Override
            public void onItemsLoaded() {
                onAllOfflinePagesRetrieved(mOfflinePageBridge.getAllItems());
            }

            @Override
            public void onItemAdded(OfflinePageDownloadItem item) {
                mOfflinePageItems.add(
                        new OfflinePageItemWrapper(item, mOfflinePageBridge, mParentComponent));
                updateFilter();
            }

            @Override
            public void onItemDeleted(String guid) {
                int index = findItemIndex(mOfflinePageItems, guid);
                if (index != INVALID_INDEX) {
                    DownloadHistoryItemWrapper wrapper = mOfflinePageItems.remove(index);
                    if (mManager.getSelectionDelegate().isItemSelected(wrapper)) {
                        mManager.getSelectionDelegate().toggleSelectionForItem(wrapper);
                    }
                    updateFilter();
                }
            }

            @Override
            public void onItemUpdated(OfflinePageDownloadItem item) {
                int index = findItemIndex(mOfflinePageItems, item.getGuid());
                if (index != INVALID_INDEX) {
                    mOfflinePageItems.set(index,
                            new OfflinePageItemWrapper(item, mOfflinePageBridge, mParentComponent));
                    updateFilter();
                }
            }

            /** Re-filter the items if needed. */
            private void updateFilter() {
                if (mFilter == DownloadFilter.FILTER_ALL || mFilter == DownloadFilter.FILTER_PAGE) {
                    filter(mFilter);
                }
            }
        });
    }

    private List<DownloadItemWrapper> getDownloadItemList(boolean isOffTheRecord) {
        return isOffTheRecord ? mDownloadOffTheRecordItems : mDownloadItems;
    }

    /**
     * Search for an existing entry for the {@link DownloadHistoryItemWrapper} with the given ID.
     * @param list List to search through.
     * @param guid GUID of the entry.
     * @return The index of the item, or INVALID_INDEX if it couldn't be found.
     */
    private <T extends DownloadHistoryItemWrapper> int findItemIndex(List<T> list, String guid) {
        for (int i = 0; i < list.size(); i++) {
            if (TextUtils.equals(list.get(i).getId(), guid)) return i;
        }
        return INVALID_INDEX;
    }

    private static DownloadManagerService getDownloadManagerService() {
        return DownloadManagerService.getDownloadManagerService(
                ContextUtils.getApplicationContext());
    }

    private void setItemViewStyle(ItemViewHolder holder, DownloadHistoryItemWrapper item) {
        if (mFilenameViewTextColor == 0) {
            // The color is not explicitly set in the XML. Programmatically retrieve the original
            // color so that the color can be reset if it's changed due to the contained item
            // being externally removed.
            mFilenameViewTextColor = holder.mFilenameView.getTextColors().getDefaultColor();
        }

        Context context = holder.itemView.getContext();
        Resources res = context.getResources();
        if (item.hasBeenExternallyRemoved()) {
            int disabledColor = ApiCompatibilityUtils.getColor(res, R.color.google_grey_300);

            holder.mHostnameView.setTextColor(disabledColor);
            holder.mIconView.setBackgroundColor(disabledColor);
            holder.mFilenameView.setTextColor(disabledColor);
            holder.mFilenameView.setPaintFlags(holder.mFilenameView.getPaintFlags()
                    | Paint.STRIKE_THRU_TEXT_FLAG);
            holder.mFilesizeView.setText(context.getString(R.string.download_manager_ui_deleted));
        } else {
            holder.mHostnameView.setTextColor(mFilenameViewTextColor);
            holder.mIconView.setBackgroundColor(ApiCompatibilityUtils.getColor(res,
                    R.color.light_active_color));
            holder.mFilenameView.setTextColor(ApiCompatibilityUtils.getColor(res,
                    R.color.default_text_color));
            holder.mFilenameView.setPaintFlags(holder.mFilenameView.getPaintFlags()
                    & ~Paint.STRIKE_THRU_TEXT_FLAG);
        }
    }

}
