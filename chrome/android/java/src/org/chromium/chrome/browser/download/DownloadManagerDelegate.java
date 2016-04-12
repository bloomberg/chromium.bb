// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import android.app.DownloadManager;
import android.content.Context;
import android.database.Cursor;
import android.os.AsyncTask;

/**
 * A wrapper for Android DownloadManager to provide utility functions.
 */
public class DownloadManagerDelegate {
    protected final Context mContext;

    public DownloadManagerDelegate(Context context) {
        mContext = context;
    }

    /**
     * @see android.app.DownloadManager#addCompletedDownload(String, String, boolean, String,
     * String, long, boolean)
     */
    protected long addCompletedDownload(String fileName, String description, String mimeType,
            String path, long length, String originalUrl, String referer) {
        DownloadManager manager =
                (DownloadManager) mContext.getSystemService(Context.DOWNLOAD_SERVICE);
        return manager.addCompletedDownload(fileName, description, true, mimeType, path, length,
                false);
    }

    /**
     * Interface for returning the query result when it completes.
     */
    public interface DownloadQueryCallback {
        /**
         * Callback function to return query result.
         * @param result Query result from android DownloadManager.
         * @param showNotifications Whether to show status notifications.
         */
        public void onQueryCompleted(DownloadQueryResult result, boolean showNotifications);
    }

    /**
     * Result for querying the Android DownloadManager.
     */
    static class DownloadQueryResult {
        public final DownloadItem item;
        public final int downloadStatus;
        public final long downloadTimeInMilliseconds;
        public final long bytesDownloaded;
        public final boolean canResolve;
        public final int failureReason;

        DownloadQueryResult(DownloadItem item, int downloadStatus, long downloadTimeInMilliseconds,
                long bytesDownloaded, boolean canResolve, int failureReason) {
            this.item = item;
            this.downloadStatus = downloadStatus;
            this.downloadTimeInMilliseconds = downloadTimeInMilliseconds;
            this.canResolve = canResolve;
            this.bytesDownloaded = bytesDownloaded;
            this.failureReason = failureReason;
        }
    }

    /**
     * Query the Android DownloadManager for download status.
     * @param downloadItem Download item to query.
     * @param showNotifications Whether to show status notifications.
     * @param callback Callback to be notified when query completes.
     */
    void queryDownloadResult(
            DownloadItem downloadItem, boolean showNotifications, DownloadQueryCallback callback) {
        DownloadQueryTask task = new DownloadQueryTask(downloadItem, showNotifications, callback);
        task.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
    }

    /**
     * Async task to query download status from Android DownloadManager
     */
    private class DownloadQueryTask extends AsyncTask<Void, Void, DownloadQueryResult> {
        private final DownloadItem mDownloadItem;
        private final boolean mShowNotifications;
        private final DownloadQueryCallback mCallback;

        public DownloadQueryTask(DownloadItem downloadItem, boolean showNotifications,
                DownloadQueryCallback callback) {
            mDownloadItem = downloadItem;
            mShowNotifications = showNotifications;
            mCallback = callback;
        }

        @Override
        public DownloadQueryResult doInBackground(Void... voids) {
            DownloadManager manager =
                    (DownloadManager) mContext.getSystemService(Context.DOWNLOAD_SERVICE);
            Cursor c = manager.query(
                    new DownloadManager.Query().setFilterById(mDownloadItem.getSystemDownloadId()));
            long bytesDownloaded = 0;
            boolean canResolve = false;
            int downloadStatus = DownloadManagerService.DOWNLOAD_STATUS_IN_PROGRESS;
            int failureReason = 0;
            long lastModifiedTime = 0;
            if (c.moveToNext()) {
                int statusIndex = c.getColumnIndex(DownloadManager.COLUMN_STATUS);
                int status = c.getInt(c.getColumnIndex(DownloadManager.COLUMN_STATUS));
                if (status == DownloadManager.STATUS_SUCCESSFUL) {
                    downloadStatus = DownloadManagerService.DOWNLOAD_STATUS_COMPLETE;
                    if (mShowNotifications) {
                        canResolve = DownloadManagerService.isOMADownloadDescription(
                                mDownloadItem.getDownloadInfo())
                                || DownloadManagerService.canResolveDownloadItem(
                                        mContext, mDownloadItem);
                    }
                } else if (status == DownloadManager.STATUS_FAILED) {
                    downloadStatus = DownloadManagerService.DOWNLOAD_STATUS_FAILED;
                    failureReason = c.getInt(c.getColumnIndex(DownloadManager.COLUMN_REASON));
                }
                lastModifiedTime =
                        c.getLong(c.getColumnIndex(DownloadManager.COLUMN_LAST_MODIFIED_TIMESTAMP));
                bytesDownloaded =
                        c.getLong(c.getColumnIndex(DownloadManager.COLUMN_BYTES_DOWNLOADED_SO_FAR));
            } else {
                downloadStatus = DownloadManagerService.DOWNLOAD_STATUS_CANCELLED;
            }
            c.close();
            long totalTime = Math.max(0, lastModifiedTime - mDownloadItem.getStartTime());
            return new DownloadQueryResult(mDownloadItem, downloadStatus, totalTime,
                    bytesDownloaded, canResolve, failureReason);
        }

        @Override
        protected void onPostExecute(DownloadQueryResult result) {
            mCallback.onQueryCompleted(result, mShowNotifications);
        }
    }
}
