// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.ui;

import android.app.PendingIntent;
import android.content.ActivityNotFoundException;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Color;
import android.net.Uri;
import android.provider.Browser;
import android.support.customtabs.CustomTabsIntent;
import android.text.TextUtils;

import org.chromium.base.ContextUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.UrlConstants;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider;
import org.chromium.chrome.browser.download.DownloadItem;
import org.chromium.chrome.browser.download.DownloadUtils;
import org.chromium.chrome.browser.offlinepages.downloads.OfflinePageDownloadItem;
import org.chromium.chrome.browser.widget.DateDividedAdapter.TimedItem;
import org.chromium.ui.widget.Toast;

import java.io.File;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;

/** Wraps different classes that contain information about downloads. */
public abstract class DownloadHistoryItemWrapper implements TimedItem {
    private static final String TAG = "download_ui";

    protected final BackendProvider mBackendProvider;
    protected final ComponentName mComponentName;
    private Long mStableId;

    private DownloadHistoryItemWrapper(BackendProvider provider, ComponentName component) {
        mBackendProvider = provider;
        mComponentName = component;
    }

    @Override
    public long getStableId() {
        if (mStableId == null) {
            // Generate a stable ID that combines the timestamp and the download ID.
            mStableId = (long) getId().hashCode();
            mStableId = (mStableId << 32) + (getTimestamp() & 0x0FFFFFFFF);
        }
        return mStableId;
    }

    /** @return Item that is being wrapped. */
    abstract Object getItem();

    /** @return ID representing the download. */
    abstract String getId();

    /** @return String showing where the download resides. */
    abstract String getFilePath();

    /** @return The file where the download resides. */
    public abstract File getFile();

    /** @return String to display for the file. */
    abstract String getDisplayFileName();

    /** @return Size of the file. */
    abstract long getFileSize();

    /** @return URL the file was downloaded from. */
    public abstract String getUrl();

    /** @return {@link DownloadFilter} that represents the file type. */
    public abstract int getFilterType();

    /** @return The mime type or null if the item doesn't have one. */
    public abstract String getMimeType();

    /** Called when the user wants to open the file. */
    abstract void open();

    /**
     * Called when the user wants to remove the download from the backend. May also delete the file
     * associated with the download item.
     * @return Whether the file associated with the download item was deleted.
     */
    abstract boolean remove();

    /**
     * @return Whether the file associated with this item has been removed through an external
     *         action.
     */
    abstract boolean hasBeenExternallyRemoved();

    /**
     * @return Whether this download is associated with the off the record profile.
     */
    abstract boolean isOffTheRecord();

    protected void recordOpenSuccess() {
        RecordHistogram.recordEnumeratedHistogram("Android.DownloadManager.Item.OpenSucceeded",
                getFilterType(), DownloadFilter.FILTER_BOUNDARY);
    }

    protected void recordOpenFailure() {
        RecordHistogram.recordEnumeratedHistogram("Android.DownloadManager.Item.OpenFailed",
                getFilterType(), DownloadFilter.FILTER_BOUNDARY);
    }

    /** Wraps a {@link DownloadItem}. */
    static class DownloadItemWrapper extends DownloadHistoryItemWrapper {
        private static final String MIMETYPE_VIDEO = "video";
        private static final String MIMETYPE_AUDIO = "audio";
        private static final String MIMETYPE_IMAGE = "image";
        private static final String MIMETYPE_DOCUMENT = "text";

        private final DownloadItem mItem;
        private final boolean mIsOffTheRecord;
        private File mFile;

        DownloadItemWrapper(DownloadItem item, boolean isOffTheRecord, BackendProvider provider,
                ComponentName component) {
            super(provider, component);
            mItem = item;
            mIsOffTheRecord = isOffTheRecord;
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
        public long getTimestamp() {
            return mItem.getStartTime();
        }

        @Override
        public String getFilePath() {
            return mItem.getDownloadInfo().getFilePath();
        }

        @Override
        public File getFile() {
            if (mFile == null) mFile = new File(getFilePath());
            return mFile;
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
            return convertMimeTypeToFilterType(getMimeType());
        }

        @Override
        public String getMimeType() {
            return mItem.getDownloadInfo().getMimeType();
        }

        @Override
        public void open() {
            Context context = ContextUtils.getApplicationContext();
            Intent viewIntent = DownloadUtils.createViewIntentForDownloadItem(mItem, getFile());

            if (mItem.hasBeenExternallyRemoved()) {
                Toast.makeText(context, context.getString(R.string.download_cant_open_file),
                        Toast.LENGTH_SHORT).show();
                return;
            }

            // Check if Chrome should open the file itself.
            if (mBackendProvider.getDownloadDelegate().isDownloadOpenableInBrowser(
                    mItem.getId(), mIsOffTheRecord)) {
                CustomTabsIntent.Builder builder = new CustomTabsIntent.Builder();
                builder.setToolbarColor(Color.BLACK);
                builder.setShowTitle(true);

                // Create a PendingIntent that can be used to view the file externally.
                PendingIntent pendingViewIntent =
                        PendingIntent.getActivity(context, 0, viewIntent, 0);
                builder.addMenuItem(context.getString(R.string.download_manager_view_externally),
                        pendingViewIntent);

                // Create a PendingIntent that shares the file with external apps.
                List<DownloadHistoryItemWrapper> items = new ArrayList<>();
                items.add(this);
                PendingIntent pendingShareIntent = PendingIntent.getActivity(
                        context, 0, DownloadUtils.createShareIntent(items), 0);
                Bitmap bitmap = BitmapFactory.decodeResource(
                        context.getResources(), R.drawable.ic_share_white_24dp);
                builder.setActionButton(
                        bitmap, context.getString(R.string.share), pendingShareIntent, true);

                // Build up the Intent further.
                Intent intent = builder.build().intent;
                intent.setAction(Intent.ACTION_VIEW);
                intent.setData(Uri.parse(UrlConstants.FILE_SCHEME + getFilePath()));
                intent.putExtra(CustomTabIntentDataProvider.EXTRA_IS_MEDIA_VIEWER, true);
                intent.putExtra(Browser.EXTRA_APPLICATION_ID, context.getPackageName());

                intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                intent.setClass(context, CustomTabActivity.class);
                IntentHandler.startActivityForTrustedIntent(intent, context);
                return;
            }

            // Check if any apps can open the file.
            try {
                context.startActivity(viewIntent);
                recordOpenSuccess();
            } catch (ActivityNotFoundException e) {
                // Can't launch the Intent.
                Toast.makeText(context, context.getString(R.string.download_cant_open_file),
                        Toast.LENGTH_SHORT).show();
                recordOpenFailure();
            }
        }

        @Override
        public boolean remove() {
            // Tell the DownloadManager to remove the file from history.
            mBackendProvider.getDownloadDelegate().removeDownload(getId(), mIsOffTheRecord);
            return false;
        }

        @Override
        boolean hasBeenExternallyRemoved() {
            return mItem.hasBeenExternallyRemoved();
        }

        @Override
        boolean isOffTheRecord() {
            return mIsOffTheRecord;
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
    public static class OfflinePageItemWrapper extends DownloadHistoryItemWrapper {
        private final OfflinePageDownloadItem mItem;
        private File mFile;

        OfflinePageItemWrapper(OfflinePageDownloadItem item, BackendProvider provider,
                ComponentName component) {
            super(provider, component);
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
        public long getTimestamp() {
            return mItem.getStartTimeMs();
        }

        @Override
        public String getFilePath() {
            return mItem.getTargetPath();
        }

        @Override
        public File getFile() {
            if (mFile == null) mFile = new File(getFilePath());
            return mFile;
        }

        @Override
        public String getDisplayFileName() {
            String title = mItem.getTitle();
            if (TextUtils.isEmpty(title)) {
                File path = new File(getFilePath());
                return path.getName();
            } else {
                return title;
            }
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
        public String getMimeType() {
            return "text/plain";
        }

        @Override
        public void open() {
            mBackendProvider.getOfflinePageBridge().openItem(getId(), mComponentName);
            recordOpenSuccess();
        }

        @Override
        public boolean remove() {
            mBackendProvider.getOfflinePageBridge().deleteItem(getId());
            return true;
        }

        @Override
        boolean hasBeenExternallyRemoved() {
            // We don't currently detect when offline pages have been removed externally.
            return false;
        }

        @Override
        boolean isOffTheRecord() {
            return false;
        }
    }
}
