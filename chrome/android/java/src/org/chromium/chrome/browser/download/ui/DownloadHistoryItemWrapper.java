// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.ui;

import android.text.TextUtils;

import org.chromium.chrome.browser.download.DownloadItem;
import org.chromium.chrome.browser.offlinepages.downloads.OfflinePageDownloadItem;
import org.chromium.chrome.browser.widget.DateDividedAdapter.TimedItem;

import java.io.File;
import java.util.Locale;

/** Wraps different classes that contain information about downloads. */
abstract class DownloadHistoryItemWrapper implements TimedItem {
    /** @return Item that is being wrapped. */
    abstract Object getItem();

    /** @return ID representing the download. */
    abstract String getId();

    /** @return String showing where the download resides. */
    abstract String getFilePath();

    /** @return String to display for the file. */
    abstract String getDisplayFileName();

    /** @return Size of the file. */
    abstract long getFileSize();

    /** @return URL the file was downloaded from. */
    abstract String getUrl();

    /** @return {@link DownloadFilter} that represents the file type. */
    abstract int getFilterType();

    /** Called when the item has been clicked on. */
    abstract void onClicked(DownloadManagerUi manager);

    /** Wraps a {@link DownloadItem}. */
    static class DownloadItemWrapper extends DownloadHistoryItemWrapper {
        private static final String MIMETYPE_VIDEO = "video";
        private static final String MIMETYPE_AUDIO = "audio";
        private static final String MIMETYPE_IMAGE = "image";
        private static final String MIMETYPE_DOCUMENT = "text";

        private final DownloadItem mItem;

        DownloadItemWrapper(DownloadItem item) {
            mItem = item;
        }

        @Override
        public DownloadItem getItem() {
            return mItem;
        }

        @Override
        public String getId() {
            return mItem.getId();
        }

        @Override
        public long getStartTime() {
            return mItem.getStartTime();
        }

        @Override
        public String getFilePath() {
            return mItem.getDownloadInfo().getFilePath();
        }

        @Override
        public String getDisplayFileName() {
            return mItem.getDownloadInfo().getFileName();
        }

        @Override
        public long getFileSize() {
            return mItem.getDownloadInfo().getContentLength();
        }

        @Override
        public String getUrl() {
            return mItem.getDownloadInfo().getUrl();
        }

        @Override
        public int getFilterType() {
            return convertMimeTypeToFilterType(mItem.getDownloadInfo().getMimeType());
        }

        @Override
        public void onClicked(DownloadManagerUi manager) {
            manager.onDownloadItemClicked(mItem);
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

    /** Wraps a {@link OfflinePageDownloadItem}. */
    static class OfflinePageItemWrapper extends DownloadHistoryItemWrapper {
        private final OfflinePageDownloadItem mItem;

        OfflinePageItemWrapper(OfflinePageDownloadItem item) {
            mItem = item;
        }

        @Override
        public OfflinePageDownloadItem getItem() {
            return mItem;
        }

        @Override
        public String getId() {
            return mItem.getGuid();
        }

        @Override
        public long getStartTime() {
            return mItem.getStartTimeMs();
        }

        @Override
        public String getFilePath() {
            return mItem.getTargetPath();
        }

        @Override
        public String getDisplayFileName() {
            File path = new File(getFilePath());
            return path.getName();
        }

        @Override
        public long getFileSize() {
            return mItem.getTotalBytes();
        }

        @Override
        public String getUrl() {
            return mItem.getUrl();
        }

        @Override
        public int getFilterType() {
            return DownloadFilter.FILTER_PAGE;
        }

        @Override
        public void onClicked(DownloadManagerUi manager) {
            // TODO(dfalcantara): Figure out what to do here.
        }
    }
}
