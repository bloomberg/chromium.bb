// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.ui;

import static junit.framework.Assert.assertEquals;
import static junit.framework.Assert.assertNull;

import android.content.ComponentName;
import android.graphics.Bitmap;
import android.os.Handler;
import android.os.Looper;
import android.text.TextUtils;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.chrome.browser.download.DownloadInfo;
import org.chromium.chrome.browser.download.DownloadItem;
import org.chromium.chrome.browser.offlinepages.downloads.OfflinePageDownloadBridge;
import org.chromium.chrome.browser.offlinepages.downloads.OfflinePageDownloadItem;
import org.chromium.chrome.browser.widget.selection.SelectionDelegate;

import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;

/** Stubs out backends used by the Download Home UI. */
public class StubbedProvider implements BackendProvider {

    /** Stubs out the DownloadManagerService. */
    public class StubbedDownloadDelegate implements DownloadDelegate {
        public final CallbackHelper addCallback = new CallbackHelper();
        public final CallbackHelper removeCallback = new CallbackHelper();
        public final CallbackHelper checkExternalCallback = new CallbackHelper();
        public final CallbackHelper removeDownloadCallback = new CallbackHelper();

        public final List<DownloadItem> regularItems = new ArrayList<>();
        public final List<DownloadItem> offTheRecordItems = new ArrayList<>();
        private DownloadHistoryAdapter mAdapter;

        @Override
        public void addDownloadHistoryAdapter(DownloadHistoryAdapter adapter) {
            addCallback.notifyCalled();
            assertNull(mAdapter);
            mAdapter = adapter;
        }

        @Override
        public void removeDownloadHistoryAdapter(DownloadHistoryAdapter adapter) {
            removeCallback.notifyCalled();
            mAdapter = null;
        }

        @Override
        public void getAllDownloads(final boolean isOffTheRecord) {
            mHandler.post(new Runnable() {
                @Override
                public void run() {
                    mAdapter.onAllDownloadsRetrieved(
                            isOffTheRecord ? offTheRecordItems : regularItems, isOffTheRecord);
                }
            });
        }

        @Override
        public void checkForExternallyRemovedDownloads(boolean isOffTheRecord) {
            checkExternalCallback.notifyCalled();
        }

        @Override
        public void removeDownload(final String guid, final boolean isOffTheRecord) {
            mHandler.post(new Runnable() {
                @Override
                public void run() {
                    mAdapter.onDownloadItemRemoved(guid, isOffTheRecord);
                    removeDownloadCallback.notifyCalled();
                }
            });
        }

        @Override
        public boolean isDownloadOpenableInBrowser(
                String guid, boolean isOffTheRecord, String mimeType) {
            return false;
        }
    }

    /** Stubs out the OfflinePageDownloadBridge. */
    public class StubbedOfflinePageDelegate implements OfflinePageDelegate {
        public final CallbackHelper addCallback = new CallbackHelper();
        public final CallbackHelper removeCallback = new CallbackHelper();
        public final CallbackHelper deleteItemCallback = new CallbackHelper();
        public final List<OfflinePageDownloadItem> items = new ArrayList<>();
        public OfflinePageDownloadBridge.Observer observer;

        @Override
        public void addObserver(OfflinePageDownloadBridge.Observer addedObserver) {
            // Immediately indicate that the delegate has loaded.
            observer = addedObserver;
            addCallback.notifyCalled();

            ThreadUtils.runOnUiThread(new Runnable() {
                @Override
                public void run() {
                    observer.onItemsLoaded();
                }
            });
        }

        @Override
        public void removeObserver(OfflinePageDownloadBridge.Observer removedObserver) {
            assertEquals(observer, removedObserver);
            observer = null;
            removeCallback.notifyCalled();
        }

        @Override
        public List<OfflinePageDownloadItem> getAllItems() {
            return items;
        }

        @Override
        public void deleteItem(final String guid) {
            for (OfflinePageDownloadItem item : items) {
                if (TextUtils.equals(item.getGuid(), guid)) {
                    items.remove(item);
                    break;
                }
            }

            mHandler.post(new Runnable() {
                @Override
                public void run() {
                    observer.onItemDeleted(guid);
                    deleteItemCallback.notifyCalled();
                }
            });
        }

        @Override public void openItem(String guid, ComponentName componentName) { }
        @Override public void destroy() { }
    }

    /** Stubs out all attempts to get thumbnails for files. */
    public static class StubbedThumbnailProvider implements ThumbnailProvider {
        @Override
        public void destroy() {}

        @Override
        public Bitmap getThumbnail(ThumbnailRequest request) {
            return null;
        }

        @Override
        public void cancelRetrieval(ThumbnailRequest request) {}
    }

    private static final long ONE_GIGABYTE = 1024L * 1024L * 1024L;

    private final Handler mHandler;
    private final StubbedDownloadDelegate mDownloadDelegate;
    private final StubbedOfflinePageDelegate mOfflineDelegate;
    private final SelectionDelegate<DownloadHistoryItemWrapper> mSelectionDelegate;
    private final StubbedThumbnailProvider mStubbedThumbnailProvider;

    public StubbedProvider() {
        mHandler = new Handler(Looper.getMainLooper());
        mDownloadDelegate = new StubbedDownloadDelegate();
        mOfflineDelegate = new StubbedOfflinePageDelegate();
        mSelectionDelegate = new SelectionDelegate<>();
        mStubbedThumbnailProvider = new StubbedThumbnailProvider();
    }

    @Override
    public StubbedDownloadDelegate getDownloadDelegate() {
        return mDownloadDelegate;
    }

    @Override
    public StubbedOfflinePageDelegate getOfflinePageBridge() {
        return mOfflineDelegate;
    }

    @Override
    public SelectionDelegate<DownloadHistoryItemWrapper> getSelectionDelegate() {
        return mSelectionDelegate;
    }

    @Override
    public StubbedThumbnailProvider getThumbnailProvider() {
        return mStubbedThumbnailProvider;
    }

    @Override
    public void destroy() {}

    /** Creates a new DownloadItem with pre-defined values. */
    public static DownloadItem createDownloadItem(int which, String date) throws Exception {
        DownloadItem item = null;
        if (which == 0) {
            item = new DownloadItem(false, new DownloadInfo.Builder()
                    .setUrl("https://google.com")
                    .setContentLength(1)
                    .setFileName("first_file.jpg")
                    .setFilePath("/storage/fake_path/Downloads/first_file.jpg")
                    .setDownloadGuid("first_guid")
                    .setMimeType("image/jpeg")
                    .build());
        } else if (which == 1) {
            item = new DownloadItem(false, new DownloadInfo.Builder()
                    .setUrl("https://one.com")
                    .setContentLength(10)
                    .setFileName("second_file.gif")
                    .setFilePath("/storage/fake_path/Downloads/second_file.gif")
                    .setDownloadGuid("second_guid")
                    .setMimeType("image/gif")
                    .build());
        } else if (which == 2) {
            item = new DownloadItem(false, new DownloadInfo.Builder()
                    .setUrl("https://is.com")
                    .setContentLength(100)
                    .setFileName("third_file")
                    .setFilePath("/storage/fake_path/Downloads/third_file")
                    .setDownloadGuid("third_guid")
                    .setMimeType("text/plain")
                    .build());
        } else if (which == 3) {
            item = new DownloadItem(false, new DownloadInfo.Builder()
                    .setUrl("https://the.com")
                    .setContentLength(5)
                    .setFileName("four.webm")
                    .setFilePath("/storage/fake_path/Downloads/four.webm")
                    .setDownloadGuid("fourth_guid")
                    .setMimeType("video/webm")
                    .build());
        } else if (which == 4) {
            item = new DownloadItem(false, new DownloadInfo.Builder()
                    .setUrl("https://loneliest.com")
                    .setContentLength(50)
                    .setFileName("five.mp3")
                    .setFilePath("/storage/fake_path/Downloads/five.mp3")
                    .setDownloadGuid("fifth_guid")
                    .setMimeType("audio/mp3")
                    .build());
        } else if (which == 5) {
            item = new DownloadItem(false, new DownloadInfo.Builder()
                    .setUrl("https://number.com")
                    .setContentLength(500)
                    .setFileName("six.mp3")
                    .setFilePath("/storage/fake_path/Downloads/six.mp3")
                    .setDownloadGuid("sixth_guid")
                    .setMimeType("audio/mp3")
                    .build());
        } else if (which == 6) {
            item = new DownloadItem(false, new DownloadInfo.Builder()
                    .setUrl("https://sigh.com")
                    .setContentLength(ONE_GIGABYTE)
                    .setFileName("huge_image.png")
                    .setFilePath("/storage/fake_path/Downloads/huge_image.png")
                    .setDownloadGuid("seventh_guid")
                    .setMimeType("image/png")
                    .build());
        } else if (which == 7) {
            item = new DownloadItem(false, new DownloadInfo.Builder()
                    .setUrl("https://sleepy.com")
                    .setContentLength(ONE_GIGABYTE / 2)
                    .setFileName("sleep.pdf")
                    .setFilePath("/storage/fake_path/Downloads/sleep.pdf")
                    .setDownloadGuid("eighth_guid")
                    .setMimeType("application/pdf")
                    .build());
        } else if (which == 8) {
            // This is a duplicate of item 7 above.
            item = new DownloadItem(false, new DownloadInfo.Builder()
                    .setUrl("https://sleepy.com")
                    .setContentLength(ONE_GIGABYTE / 2)
                    .setFileName("sleep.pdf")
                    .setFilePath("/storage/fake_path/Downloads/sleep.pdf")
                    .setDownloadGuid("ninth_guid")
                    .setMimeType("application/pdf")
                    .build());
        } else {
            return null;
        }

        item.setStartTime(dateToEpoch(date));
        return item;
    }

    /** Creates a new OfflinePageDownloadItem with pre-defined values. */
    public static OfflinePageDownloadItem createOfflineItem(int which, String date)
            throws Exception {
        long startTime = dateToEpoch(date);
        if (which == 0) {
            return new OfflinePageDownloadItem("offline_guid_1", "https://url.com",
                    "page 1", "/data/fake_path/Downloads/first_file", startTime, 1000);
        } else if (which == 1) {
            return new OfflinePageDownloadItem("offline_guid_2", "http://stuff_and_things.com",
                    "page 2", "/data/fake_path/Downloads/file_two", startTime, 10000);
        } else if (which == 2) {
            return new OfflinePageDownloadItem("offline_guid_3", "https://url.com",
                    "page 3", "/data/fake_path/Downloads/3_file", startTime, 100000);
        } else if (which == 3) {
            return new OfflinePageDownloadItem("offline_guid_4", "https://thangs.com",
                    "page 4", "/data/fake_path/Downloads/4", startTime, ONE_GIGABYTE * 5L);
        } else {
            return null;
        }
    }

    /** Converts a date string to a timestamp. */
    private static long dateToEpoch(String dateStr) throws Exception {
        return new SimpleDateFormat("yyyyMMdd HH:mm", Locale.getDefault()).parse(dateStr).getTime();
    }

}
