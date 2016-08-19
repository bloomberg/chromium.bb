// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.ui;

import static org.chromium.chrome.browser.widget.DateDividedAdapter.TYPE_DATE;
import static org.chromium.chrome.browser.widget.DateDividedAdapter.TYPE_NORMAL;

import android.support.v7.widget.RecyclerView;
import android.test.InstrumentationTestCase;
import android.test.UiThreadTest;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.chrome.browser.download.DownloadInfo;
import org.chromium.chrome.browser.download.DownloadItem;
import org.chromium.chrome.browser.widget.selection.SelectionDelegate;
import org.chromium.content.browser.test.util.CallbackHelper;

import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;

/**
 * Tests a DownloadHistoryAdapter that is isolated from the real bridges.
 */
public class DownloadHistoryAdapterTest extends InstrumentationTestCase {

    private static class StubbedAdapter extends DownloadHistoryAdapter {
        private final SelectionDelegate<DownloadItem> mDelegate = new SelectionDelegate<>();

        StubbedAdapter(boolean showOffTheRecord) {
            super(showOffTheRecord, null);
        }

        @Override
        protected SelectionDelegate getSelectionDelegate(DownloadManagerUi manager) {
            return mDelegate;
        }

        @Override
        protected void initializeDownloadBridge() {
        }

        @Override
        protected void initializeOfflinePageBridge() {
        }
    }

    private static class Observer extends RecyclerView.AdapterDataObserver {
        public CallbackHelper onChangedCallback = new CallbackHelper();

        @Override
        public void onChanged() {
            onChangedCallback.notifyCalled();
        }
    }

    private DownloadHistoryAdapter mAdapter;
    private Observer mObserver;

    private void initializeAdapter(boolean showOffTheRecord) {
        mObserver = new Observer();
        mAdapter = new StubbedAdapter(showOffTheRecord);
        mAdapter.initialize(null);
        mAdapter.registerAdapterDataObserver(mObserver);
    }

    /** Nothing downloaded, nothing shown. */
    @SmallTest
    public void testInitialize_Empty() throws Exception {
        initializeAdapter(false);

        List<DownloadItem> regularItems = new ArrayList<DownloadItem>();
        int callCount = mObserver.onChangedCallback.getCallCount();
        assertEquals(0, callCount);
        mAdapter.onAllDownloadsRetrieved(regularItems, false);
        mObserver.onChangedCallback.waitForCallback(callCount, 1);

        assertEquals(0, mAdapter.getItemCount());
        assertEquals(0, mAdapter.getTotalDownloadSize());
    }

    /** One downloaded item should show the item and a date header. */
    @UiThreadTest
    @SmallTest
    public void testInitialize_SingleItem() throws Exception {
        initializeAdapter(false);

        List<DownloadItem> regularItems = new ArrayList<DownloadItem>();
        DownloadItem firstItem = createDownloadItem(0, "19840116 12:00");
        regularItems.add(firstItem);

        int callCount = mObserver.onChangedCallback.getCallCount();
        assertEquals(0, callCount);
        mAdapter.onAllDownloadsRetrieved(regularItems, false);
        mObserver.onChangedCallback.waitForCallback(callCount, 1);
        checkAdapterTypes(TYPE_DATE, TYPE_NORMAL);
        assertEquals(1, mAdapter.getTotalDownloadSize());
    }

    /** Two items downloaded on the same day should end up in the same date group. */
    @UiThreadTest
    @SmallTest
    public void testInitialize_TwoItemsOneDate() throws Exception {
        initializeAdapter(false);

        List<DownloadItem> regularItems = new ArrayList<DownloadItem>();
        regularItems.add(createDownloadItem(0, "19840116 12:00"));
        regularItems.add(createDownloadItem(1, "19840116 12:01"));

        int callCount = mObserver.onChangedCallback.getCallCount();
        assertEquals(0, callCount);
        mAdapter.onAllDownloadsRetrieved(regularItems, false);
        mObserver.onChangedCallback.waitForCallback(callCount, 1);
        checkAdapterTypes(TYPE_DATE, TYPE_NORMAL, TYPE_NORMAL);
        assertEquals(11, mAdapter.getTotalDownloadSize());
    }

    /** Two items downloaded on different days should end up in different date groups. */
    @UiThreadTest
    @SmallTest
    public void testInitialize_TwoItemsTwoDates() throws Exception {
        initializeAdapter(false);

        List<DownloadItem> regularItems = new ArrayList<DownloadItem>();
        regularItems.add(createDownloadItem(0, "19840116 12:00"));
        regularItems.add(createDownloadItem(1, "19840117 12:00"));

        int callCount = mObserver.onChangedCallback.getCallCount();
        assertEquals(0, callCount);
        mAdapter.onAllDownloadsRetrieved(regularItems, false);
        mObserver.onChangedCallback.waitForCallback(callCount, 1);
        checkAdapterTypes(TYPE_DATE, TYPE_NORMAL, TYPE_DATE, TYPE_NORMAL);
        assertEquals(11, mAdapter.getTotalDownloadSize());
    }

    /** Off the record downloads are ignored if the DownloadHistoryAdapter isn't watching them. */
    @UiThreadTest
    @SmallTest
    public void testInitialize_OffTheRecord_Ignored() throws Exception {
        initializeAdapter(false);

        List<DownloadItem> regularItems = new ArrayList<DownloadItem>();
        regularItems.add(createDownloadItem(0, "19840116 12:00"));

        List<DownloadItem> offTheRecordItems = new ArrayList<DownloadItem>();
        offTheRecordItems.add(createDownloadItem(1, "19840116 12:00"));

        int callCount = mObserver.onChangedCallback.getCallCount();
        assertEquals(0, callCount);
        mAdapter.onAllDownloadsRetrieved(offTheRecordItems, true);
        assertEquals(0, callCount);
        mAdapter.onAllDownloadsRetrieved(regularItems, false);
        mObserver.onChangedCallback.waitForCallback(callCount, 1);

        checkAdapterTypes(TYPE_DATE, TYPE_NORMAL);
        assertEquals(1, mAdapter.getTotalDownloadSize());
    }

    /** A regular and a off the record item with the same date are bucketed together. */
    @UiThreadTest
    @SmallTest
    public void testInitialize_OffTheRecord_TwoItemsOneDate() throws Exception {
        initializeAdapter(true);

        List<DownloadItem> regularItems = new ArrayList<DownloadItem>();
        regularItems.add(createDownloadItem(0, "19840116 12:00"));

        List<DownloadItem> offTheRecordItems = new ArrayList<DownloadItem>();
        offTheRecordItems.add(createDownloadItem(1, "19840116 12:00"));

        int callCount = mObserver.onChangedCallback.getCallCount();
        assertEquals(0, callCount);
        mAdapter.onAllDownloadsRetrieved(offTheRecordItems, true);
        mObserver.onChangedCallback.waitForCallback(callCount, 1);
        mAdapter.onAllDownloadsRetrieved(regularItems, false);
        mObserver.onChangedCallback.waitForCallback(callCount, 2);

        checkAdapterTypes(TYPE_DATE, TYPE_NORMAL, TYPE_NORMAL);
        assertEquals(11, mAdapter.getTotalDownloadSize());
    }

    /** Adding new items should bucket them into the proper dates. */
    @UiThreadTest
    @SmallTest
    public void testUpdate_UpdateItems() throws Exception {
        initializeAdapter(false);

        // Start with an empty Adapter.
        List<DownloadItem> regularItems = new ArrayList<DownloadItem>();
        assertEquals(0, mObserver.onChangedCallback.getCallCount());
        mAdapter.onAllDownloadsRetrieved(regularItems, false);
        mObserver.onChangedCallback.waitForCallback(0, 1);
        assertEquals(0, mAdapter.getItemCount());
        assertEquals(0, mAdapter.getTotalDownloadSize());

        // Add the first item.
        mAdapter.onDownloadItemUpdated(createDownloadItem(0, "19840116 12:00"), false);
        mObserver.onChangedCallback.waitForCallback(1, 1);
        checkAdapterTypes(TYPE_DATE, TYPE_NORMAL);
        assertEquals(1, mAdapter.getTotalDownloadSize());

        // Add a second item with a different date.
        DownloadItem secondItem = createDownloadItem(1, "19840117 12:00");
        assertEquals(2, mObserver.onChangedCallback.getCallCount());
        mAdapter.onDownloadItemUpdated(secondItem, false);
        mObserver.onChangedCallback.waitForCallback(2, 1);
        checkAdapterTypes(TYPE_DATE, TYPE_NORMAL, TYPE_DATE, TYPE_NORMAL);
        assertEquals(11, mAdapter.getTotalDownloadSize());

        // Add a third item with the same date as the second item.
        assertEquals(3, mObserver.onChangedCallback.getCallCount());
        mAdapter.onDownloadItemUpdated(createDownloadItem(2, "19840117 12:00"), false);
        mObserver.onChangedCallback.waitForCallback(3, 1);
        checkAdapterTypes(TYPE_DATE, TYPE_NORMAL, TYPE_NORMAL, TYPE_DATE, TYPE_NORMAL);
        assertEquals(111, mAdapter.getTotalDownloadSize());

        // An item with the same values as the second item should just update the old one.
        assertEquals(4, mObserver.onChangedCallback.getCallCount());
        mAdapter.onDownloadItemUpdated(createDownloadItem(2, "19840117 12:00"), false);
        mObserver.onChangedCallback.waitForCallback(4, 1);
        checkAdapterTypes(TYPE_DATE, TYPE_NORMAL, TYPE_NORMAL, TYPE_DATE, TYPE_NORMAL);
        assertEquals(111, mAdapter.getTotalDownloadSize());
    }

    /** Test removal of items. */
    @UiThreadTest
    @SmallTest
    public void testRemove_ThreeItemsTwoDates() throws Exception {
        initializeAdapter(false);

        // Initialize the DownloadHistoryAdapter with three items in two date buckets.
        DownloadItem item0 = createDownloadItem(0, "19840116 12:00");
        DownloadItem item1 = createDownloadItem(1, "19840116 12:01");
        DownloadItem item2 = createDownloadItem(2, "19840117 12:01");

        List<DownloadItem> regularItems = new ArrayList<DownloadItem>();
        regularItems.add(item0);
        regularItems.add(item1);
        regularItems.add(item2);

        assertEquals(0, mObserver.onChangedCallback.getCallCount());
        mAdapter.onAllDownloadsRetrieved(regularItems, false);
        mObserver.onChangedCallback.waitForCallback(0, 1);
        checkAdapterTypes(TYPE_DATE, TYPE_NORMAL, TYPE_DATE, TYPE_NORMAL, TYPE_NORMAL);
        assertEquals(111, mAdapter.getTotalDownloadSize());

        // Remove an item from the date bucket with two items.
        assertEquals(1, mObserver.onChangedCallback.getCallCount());
        mAdapter.onDownloadItemRemoved(item1.getId(), false);
        mObserver.onChangedCallback.waitForCallback(1, 1);
        checkAdapterTypes(TYPE_DATE, TYPE_NORMAL, TYPE_DATE, TYPE_NORMAL);

        // Remove an item from the second bucket, which removes it entirely.
        assertEquals(2, mObserver.onChangedCallback.getCallCount());
        mAdapter.onDownloadItemRemoved(item2.getId(), false);
        mObserver.onChangedCallback.waitForCallback(2, 1);
        checkAdapterTypes(TYPE_DATE, TYPE_NORMAL);
    }

    /** Checks that the adapter has the correct types of items in the right places. */
    private void checkAdapterTypes(Integer... expectedTypes) {
        assertEquals(expectedTypes.length, mAdapter.getItemCount());
        for (int i = 0; i < expectedTypes.length; i++) {
            assertEquals(expectedTypes[i].intValue(), mAdapter.getItemViewType(i));
        }
    }

    /** Converts a date string to a timestamp. */
    private long dateToEpoch(String dateStr) throws Exception {
        return new SimpleDateFormat("yyyyMMdd HH:mm", Locale.getDefault()).parse(dateStr).getTime();
    }

    /** Creates a new DownloadItem with pre-defined values. */
    private DownloadItem createDownloadItem(int which, String date)
            throws Exception {
        DownloadItem item = null;
        if (which == 0) {
            item = new DownloadItem(false, new DownloadInfo.Builder()
                    .setUrl("https://google.com")
                    .setContentLength(1)
                    .setFileName("first_file")
                    .setFilePath("/storage/fake_path/Downloads/first_file")
                    .setDownloadGuid("first_guid")
                    .setMimeType("image/gif")
                    .build());
        } else if (which == 1) {
            item = new DownloadItem(false, new DownloadInfo.Builder()
                    .setUrl("https://ugh.com")
                    .setContentLength(10)
                    .setFileName("second_file")
                    .setFilePath("/storage/fake_path/Downloads/second_file")
                    .setDownloadGuid("second_guid")
                    .setMimeType("image/gif")
                    .build());
        } else if (which == 2) {
            item = new DownloadItem(false, new DownloadInfo.Builder()
                    .setUrl("https://ugh.com")
                    .setContentLength(100)
                    .setFileName("third_file")
                    .setFilePath("/storage/fake_path/Downloads/third_file")
                    .setDownloadGuid("third_guid")
                    .setMimeType("text/plain")
                    .build());
        } else {
            return null;
        }

        item.setStartTime(dateToEpoch(date));
        return item;
    }

}
