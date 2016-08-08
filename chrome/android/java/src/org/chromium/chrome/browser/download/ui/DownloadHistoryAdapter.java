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

import org.chromium.chrome.R;
import org.chromium.chrome.browser.download.DownloadItem;
import org.chromium.chrome.browser.download.ui.DownloadManagerUi.DownloadUiObserver;
import org.chromium.chrome.browser.util.UrlUtilities;
import org.chromium.chrome.browser.widget.DateDividedAdapter;

import java.util.ArrayList;
import java.util.List;
import java.util.Locale;

/** Bridges the user's download history and the UI used to display it. */
public class DownloadHistoryAdapter extends DateDividedAdapter implements DownloadUiObserver {

    /** Holds onto a View that displays information about a downloaded file. */
    static class ItemViewHolder extends RecyclerView.ViewHolder {
        public ImageView mIconView;
        public TextView mFilenameView;
        public TextView mHostnameView;
        public TextView mFilesizeView;

        public ItemViewHolder(View itemView) {
            super(itemView);
            mIconView = (ImageView) itemView.findViewById(R.id.icon_view);
            mFilenameView = (TextView) itemView.findViewById(R.id.filename_view);
            mHostnameView = (TextView) itemView.findViewById(R.id.hostname_view);
            mFilesizeView = (TextView) itemView.findViewById(R.id.filesize_view);
        }
    }

    private static final String MIMETYPE_VIDEO = "video";
    private static final String MIMETYPE_AUDIO = "audio";
    private static final String MIMETYPE_IMAGE = "image";
    private static final String MIMETYPE_DOCUMENT = "text";

    private final List<DownloadItem> mAllItems = new ArrayList<>();
    private final List<DownloadItem> mFilteredItems = new ArrayList<>();

    private int mFilter = DownloadFilter.FILTER_ALL;

    @Override
    public void initialize(DownloadManagerUi manager) {
        manager.addObserver(this);
    }

    /** Called when the user's download history has been gathered into a List of DownloadItems. */
    public void onAllDownloadsRetrieved(List<DownloadItem> list) {
        mAllItems.clear();
        mAllItems.addAll(list);
        filter(DownloadFilter.FILTER_ALL);
    }

    /** Returns the total size of all the downloaded items. */
    public long getTotalDownloadSize() {
        long totalSize = 0;
        for (int i = 0; i < mAllItems.size(); i++) {
            totalSize += mAllItems.get(i).getDownloadInfo().getContentLength();
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
        return new ItemViewHolder(v);
    }

    @Override
    public void bindViewHolderForTimedItem(ViewHolder current, TimedItem timedItem) {
        ItemViewHolder holder = (ItemViewHolder) current;
        Context context = holder.mFilesizeView.getContext();
        DownloadItem item = (DownloadItem) timedItem;
        holder.mFilenameView.setText(item.getDownloadInfo().getFileName());
        holder.mHostnameView.setText(
                UrlUtilities.formatUrlForSecurityDisplay(item.getDownloadInfo().getUrl(), false));
        holder.mFilesizeView.setText(
                Formatter.formatFileSize(context, item.getDownloadInfo().getContentLength()));

        // Pick what icon to display for the item.
        int fileType = convertMimeTypeToFilterType(item.getDownloadInfo().getMimeType());
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
        for (int i = 0; i < mAllItems.size() && !isFound; i++) {
            if (TextUtils.equals(mAllItems.get(i).getId(), item.getId())) {
                mAllItems.set(i, item);
                isFound = true;
            }
        }

        // Add a new entry if one doesn't already exist.
        if (!isFound) mAllItems.add(item);
        filter(mFilter);
    }

    @Override
    public void onFilterChanged(int filter) {
        filter(filter);
    }

    @Override
    public void onDestroy(DownloadManagerUi manager) {
        manager.removeObserver(this);
    }

    /** Filters the list of downloads to show only files of a specific type. */
    private void filter(int filterType) {
        mFilter = filterType;
        mFilteredItems.clear();
        if (filterType == DownloadFilter.FILTER_ALL) {
            mFilteredItems.addAll(mAllItems);
        } else {
            for (int i = 0; i < mAllItems.size(); i++) {
                int currentFiletype = convertMimeTypeToFilterType(
                        mAllItems.get(i).getDownloadInfo().getMimeType());
                if (currentFiletype == filterType) mFilteredItems.add(mAllItems.get(i));
            }
        }

        loadItems(mFilteredItems);
    }

    /** Identifies the type of file represented by the given MIME type string. */
    private static int convertMimeTypeToFilterType(String mimeType) {
        if (TextUtils.isEmpty(mimeType)) return DownloadFilter.FILTER_OTHER;

        String[] pieces = mimeType.toLowerCase(Locale.getDefault()).split("/");
        if (pieces.length != 2) return DownloadFilter.FILTER_OTHER;

        if (MIMETYPE_VIDEO.equals(pieces[0])) {
            return DownloadFilter.FILTER_VIDEO;
        } else if (MIMETYPE_AUDIO.equals(pieces[0])) {
            return DownloadFilter.FILTER_AUDIO;
        } else if (MIMETYPE_IMAGE.equals(pieces[0])) {
            return DownloadFilter.FILTER_IMAGE;
        } else if (MIMETYPE_DOCUMENT.equals(pieces[0])) {
            return DownloadFilter.FILTER_DOCUMENT;
        } else {
            return DownloadFilter.FILTER_OTHER;
        }
    }
}
