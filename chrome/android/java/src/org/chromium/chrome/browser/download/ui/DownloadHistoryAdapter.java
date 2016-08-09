// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.ui;

import android.content.Context;
import android.support.v7.widget.RecyclerView;
import android.support.v7.widget.RecyclerView.ViewHolder;
import android.text.TextUtils;
import android.text.format.Formatter;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.TextView;

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

    private final List<DownloadItemWrapper> mDownloadItems = new ArrayList<>();
    private final List<OfflinePageItemWrapper> mOfflinePageItems = new ArrayList<>();
    private final List<DownloadHistoryItemWrapper> mFilteredItems = new ArrayList<>();

    private int mFilter = DownloadFilter.FILTER_ALL;
    private DownloadManagerUi mManager;
    private OfflinePageDownloadBridge mOfflinePageBridge;

    @Override
    public void initialize(DownloadManagerUi manager) {
        manager.addObserver(this);
        mManager = manager;

        getDownloadManagerService().addDownloadHistoryAdapter(this);
        getDownloadManagerService().getAllDownloads();

        initializeOfflinePageBridge();
    }

    /** Called when the user's download history has been gathered. */
    public void onAllDownloadsRetrieved(List<DownloadItem> result) {
        mDownloadItems.clear();
        for (DownloadItem item : result) {
            mDownloadItems.add(new DownloadItemWrapper(item));
        }
        filter(DownloadFilter.FILTER_ALL);
    }

    /** Called when the user's offline page history has been gathered. */
    private void onAllOfflinePagesRetrieved(List<OfflinePageDownloadItem> result) {
        mOfflinePageItems.clear();
        for (OfflinePageDownloadItem item : result) {
            mOfflinePageItems.add(new OfflinePageItemWrapper(item));
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
        Context context = holder.mFilesizeView.getContext();
        holder.mFilenameView.setText(item.getDisplayFileName());
        holder.mHostnameView.setText(
                UrlUtilities.formatUrlForSecurityDisplay(item.getUrl(), false));
        holder.mFilesizeView.setText(
                Formatter.formatFileSize(context, item.getFileSize()));
        holder.mItemView.initialize(mManager, item);

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
    }

    /**
     * Updates the list when new information about a download comes in.
     */
    public void updateDownloadItem(DownloadItem item) {
        boolean isFound = false;

        // Search for an existing entry representing the DownloadItem.
        for (int i = 0; i < mDownloadItems.size() && !isFound; i++) {
            if (TextUtils.equals(mDownloadItems.get(i).getId(), item.getId())) {
                mDownloadItems.set(i, new DownloadItemWrapper(item));
                isFound = true;
            }
        }

        // Add a new entry if one doesn't already exist.
        if (!isFound) mDownloadItems.add(new DownloadItemWrapper(item));
        filter(mFilter);
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
            mFilteredItems.addAll(mOfflinePageItems);
        } else {
            for (DownloadHistoryItemWrapper item : mDownloadItems) {
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
                mOfflinePageItems.add(new OfflinePageItemWrapper(item));
                updateFilter();
            }

            @Override
            public void onItemDeleted(String guid) {
                for (int i = 0; i < mOfflinePageItems.size(); i++) {
                    if (TextUtils.equals(mOfflinePageItems.get(i).getId(), guid)) {
                        mOfflinePageItems.remove(mOfflinePageItems.get(i));
                        updateFilter();
                        return;
                    }
                }
            }

            @Override
            public void onItemUpdated(OfflinePageDownloadItem item) {
                for (int i = 0; i < mOfflinePageItems.size(); i++) {
                    if (TextUtils.equals(mOfflinePageItems.get(i).getId(), item.getGuid())) {
                        mOfflinePageItems.set(i, new OfflinePageItemWrapper(item));
                        updateFilter();
                        return;
                    }
                }
            }

            private void updateFilter() {
                // Re-filter the items if needed.
                if (mFilter == DownloadFilter.FILTER_PAGE) filter(mFilter);
            }
        });
    }

    private static DownloadManagerService getDownloadManagerService() {
        return DownloadManagerService.getDownloadManagerService(
                ContextUtils.getApplicationContext());
    }

}
