// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.ui;

import static junit.framework.Assert.assertEquals;
import static junit.framework.Assert.assertNull;

import android.content.ComponentName;

import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.download.DownloadItem;
import org.chromium.chrome.browser.download.ui.BackendProvider.DownloadDelegate;
import org.chromium.chrome.browser.download.ui.BackendProvider.OfflinePageDelegate;
import org.chromium.chrome.browser.offlinepages.downloads.OfflinePageDownloadBridge;
import org.chromium.chrome.browser.offlinepages.downloads.OfflinePageDownloadItem;
import org.chromium.chrome.browser.widget.selection.SelectionDelegate;
import org.chromium.content.browser.test.util.CallbackHelper;

import java.util.ArrayList;
import java.util.List;

/** Stubs out backends used by the Download Home UI. */
public class StubbedProvider implements BackendProvider {

    /** Stubs out the DownloadManagerService. */
    public static class StubbedDownloadDelegate implements DownloadDelegate {
        public final CallbackHelper addCallback = new CallbackHelper();
        public final CallbackHelper removeCallback = new CallbackHelper();
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
            ThreadUtils.runOnUiThread(new Runnable() {
                @Override
                public void run() {
                    mAdapter.onAllDownloadsRetrieved(
                            isOffTheRecord ? offTheRecordItems : regularItems, isOffTheRecord);
                }
            });
        }
    }

    /** Stubs out the OfflinePageDownloadBridge. */
    public static class StubbedOfflinePageDelegate implements OfflinePageDelegate {
        public final CallbackHelper addCallback = new CallbackHelper();
        public final CallbackHelper removeCallback = new CallbackHelper();
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

        @Override public void openItem(String guid, ComponentName componentName) { }
        @Override public void deleteItem(String guid) { }
        @Override public void destroy() { }
    }

    private StubbedDownloadDelegate mDownloadDelegate;
    private StubbedOfflinePageDelegate mOfflineDelegate;
    private SelectionDelegate<DownloadHistoryItemWrapper> mSelectionDelegate;

    public StubbedProvider() {
        mDownloadDelegate = new StubbedDownloadDelegate();
        mOfflineDelegate = new StubbedOfflinePageDelegate();
        mSelectionDelegate = new SelectionDelegate<>();
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
}
