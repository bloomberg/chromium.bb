// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.ui;

import static org.chromium.chrome.browser.widget.DateDividedAdapter.TYPE_DATE;
import static org.chromium.chrome.browser.widget.DateDividedAdapter.TYPE_NORMAL;

import android.support.v7.widget.RecyclerView;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.download.DownloadInfo;
import org.chromium.chrome.browser.download.DownloadItem;
import org.chromium.chrome.browser.download.ui.StubbedProvider.StubbedDownloadDelegate;
import org.chromium.chrome.browser.download.ui.StubbedProvider.StubbedOfflinePageDelegate;
import org.chromium.chrome.browser.offlinepages.downloads.OfflinePageDownloadItem;
import org.chromium.content.browser.test.NativeLibraryTestBase;
import org.chromium.content.browser.test.util.CallbackHelper;

import java.text.SimpleDateFormat;
import java.util.Locale;

/**
 * Tests a DownloadHistoryAdapter that is isolated from the real bridges.
 */
public class DownloadHistoryAdapterTest extends NativeLibraryTestBase {

    private static class Observer extends RecyclerView.AdapterDataObserver {
        public CallbackHelper onChangedCallback = new CallbackHelper();

        @Override
        public void onChanged() {
            onChangedCallback.notifyCalled();
        }
    }

    private DownloadHistoryAdapter mAdapter;
    private Observer mObserver;
    private StubbedDownloadDelegate mDownloadDelegate;
    private StubbedOfflinePageDelegate mOfflineDelegate;
    private StubbedProvider mBackendProvider;

    @Override
    public void setUp() throws Exception {
        super.setUp();
        loadNativeLibraryAndInitBrowserProcess();
        mBackendProvider = new StubbedProvider();
        mDownloadDelegate = mBackendProvider.getDownloadDelegate();
        mOfflineDelegate = mBackendProvider.getOfflinePageBridge();
    }

    private void initializeAdapter(boolean showOffTheRecord) throws Exception {
        mObserver = new Observer();
        mAdapter = new DownloadHistoryAdapter(showOffTheRecord, null);
        mAdapter.registerAdapterDataObserver(mObserver);

        // Initialize the Adapter with all the DownloadItems and OfflinePageDownloadItems.
        int callCount = mObserver.onChangedCallback.getCallCount();
        assertEquals(0, callCount);
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mAdapter.initialize(mBackendProvider);
            }
        });
        mDownloadDelegate.addCallback.waitForCallback(0);
        mObserver.onChangedCallback.waitForCallback(callCount, showOffTheRecord ? 3 : 2);
    }

    /** Nothing downloaded, nothing shown. */
    @SmallTest
    public void testInitialize_Empty() throws Exception {
        initializeAdapter(false);
        assertEquals(0, mAdapter.getItemCount());
        assertEquals(0, mAdapter.getTotalDownloadSize());
        mAdapter.onManagerDestroyed();
        mDownloadDelegate.removeCallback.waitForCallback(0);
    }

    /** One downloaded item should show the item and a date header. */
    @SmallTest
    public void testInitialize_SingleItem() throws Exception {
        DownloadItem item = createDownloadItem(0, "19840116 12:00");
        mDownloadDelegate.regularItems.add(item);
        initializeAdapter(false);
        checkAdapterContents(null, item);
        assertEquals(1, mAdapter.getTotalDownloadSize());
    }

    /** Two items downloaded on the same day should end up in the same group, in recency order. */
    @SmallTest
    public void testInitialize_TwoItemsOneDate() throws Exception {
        DownloadItem item0 = createDownloadItem(0, "19840116 12:00");
        DownloadItem item1 = createDownloadItem(1, "19840116 12:01");
        mDownloadDelegate.regularItems.add(item0);
        mDownloadDelegate.regularItems.add(item1);
        initializeAdapter(false);
        checkAdapterContents(null, item1, item0);
        assertEquals(11, mAdapter.getTotalDownloadSize());
    }

    /** Two items downloaded on different days should end up in different date groups. */
    @SmallTest
    public void testInitialize_TwoItemsTwoDates() throws Exception {
        DownloadItem item0 = createDownloadItem(0, "19840116 12:00");
        DownloadItem item1 = createDownloadItem(1, "19840117 12:00");
        mDownloadDelegate.regularItems.add(item0);
        mDownloadDelegate.regularItems.add(item1);
        initializeAdapter(false);
        checkAdapterContents(null, item1, null, item0);
        assertEquals(11, mAdapter.getTotalDownloadSize());
    }

    /** Off the record downloads are ignored if the DownloadHistoryAdapter isn't watching them. */
    @SmallTest
    public void testInitialize_OffTheRecord_Ignored() throws Exception {
        DownloadItem item0 = createDownloadItem(0, "19840116 12:00");
        DownloadItem item1 = createDownloadItem(1, "19840116 12:01");
        mDownloadDelegate.regularItems.add(item0);
        mDownloadDelegate.offTheRecordItems.add(item1);
        initializeAdapter(false);
        checkAdapterContents(null, item0);
        assertEquals(1, mAdapter.getTotalDownloadSize());
    }

    /** A regular and a off the record item with the same date are bucketed together. */
    @SmallTest
    public void testInitialize_OffTheRecord_TwoItemsOneDate() throws Exception {
        DownloadItem item0 = createDownloadItem(0, "19840116 18:00");
        DownloadItem item1 = createDownloadItem(1, "19840116 12:00");
        mDownloadDelegate.regularItems.add(item0);
        mDownloadDelegate.offTheRecordItems.add(item1);
        initializeAdapter(true);
        checkAdapterContents(null, item0, item1);
        assertEquals(11, mAdapter.getTotalDownloadSize());
    }

    /** Test that all the download item types intermingle correctly. */
    @SmallTest
    public void testInitialize_ThreeItemsDifferentKinds() throws Exception {
        DownloadItem item0 = createDownloadItem(0, "19840116 18:00");
        DownloadItem item1 = createDownloadItem(1, "19840116 12:00");
        OfflinePageDownloadItem item2 = createOfflineItem(2, "19840117 6:00");
        mDownloadDelegate.regularItems.add(item0);
        mDownloadDelegate.offTheRecordItems.add(item1);
        mOfflineDelegate.items.add(item2);
        initializeAdapter(true);
        checkAdapterContents(null, item2, null, item0, item1);
        assertEquals(100011, mAdapter.getTotalDownloadSize());
    }

    /** Adding and updating new items should bucket them into the proper dates. */
    @SmallTest
    public void testUpdate_UpdateItems() throws Exception {
        // Start with an empty Adapter.
        initializeAdapter(false);
        assertEquals(0, mAdapter.getItemCount());
        assertEquals(0, mAdapter.getTotalDownloadSize());

        // Add the first item.
        assertEquals(2, mObserver.onChangedCallback.getCallCount());
        DownloadItem item0 = createDownloadItem(0, "19840116 12:00");
        mAdapter.onDownloadItemUpdated(item0, false);
        mObserver.onChangedCallback.waitForCallback(2);
        checkAdapterContents(null, item0);
        assertEquals(1, mAdapter.getTotalDownloadSize());

        // Add a second item with a different date.
        assertEquals(3, mObserver.onChangedCallback.getCallCount());
        DownloadItem item1 = createDownloadItem(1, "19840117 12:00");
        mAdapter.onDownloadItemUpdated(item1, false);
        mObserver.onChangedCallback.waitForCallback(3);
        checkAdapterContents(null, item1, null, item0);
        assertEquals(11, mAdapter.getTotalDownloadSize());

        // Add a third item with the same date as the second item.
        assertEquals(4, mObserver.onChangedCallback.getCallCount());
        DownloadItem item2 = createDownloadItem(2, "19840117 18:00");
        mAdapter.onDownloadItemUpdated(item2, false);
        mObserver.onChangedCallback.waitForCallback(4);
        checkAdapterContents(null, item2, item1, null, item0);
        assertEquals(111, mAdapter.getTotalDownloadSize());

        // An item with the same download ID as the second item should just update the old one.
        assertEquals(5, mObserver.onChangedCallback.getCallCount());
        DownloadItem item3 = createDownloadItem(2, "19840117 18:00");
        mAdapter.onDownloadItemUpdated(item3, false);
        mObserver.onChangedCallback.waitForCallback(5);
        checkAdapterContents(null, item3, item1, null, item0);
        assertEquals(111, mAdapter.getTotalDownloadSize());

        // Throw on a new OfflinePageItem.
        assertEquals(6, mObserver.onChangedCallback.getCallCount());
        OfflinePageDownloadItem item4 = createOfflineItem(0, "19840117 19:00");
        mOfflineDelegate.observer.onItemAdded(item4);
        mObserver.onChangedCallback.waitForCallback(6);
        checkAdapterContents(null, item4, item3, item1, null, item0);

        // Update the existing OfflinePageItem.
        assertEquals(7, mObserver.onChangedCallback.getCallCount());
        OfflinePageDownloadItem item5 = createOfflineItem(0, "19840117 19:00");
        mOfflineDelegate.observer.onItemUpdated(item5);
        mObserver.onChangedCallback.waitForCallback(7);
        checkAdapterContents(null, item5, item3, item1, null, item0);
    }

    /** Test removal of items. */
    @SmallTest
    public void testRemove_ThreeItemsTwoDates() throws Exception {
        // Initialize the DownloadHistoryAdapter with three items in two date buckets.
        DownloadItem regularItem = createDownloadItem(0, "19840116 18:00");
        DownloadItem offTheRecordItem = createDownloadItem(1, "19840116 12:00");
        OfflinePageDownloadItem offlineItem = createOfflineItem(2, "19840117 12:01");
        mDownloadDelegate.regularItems.add(regularItem);
        mDownloadDelegate.offTheRecordItems.add(offTheRecordItem);
        mOfflineDelegate.items.add(offlineItem);
        initializeAdapter(true);
        checkAdapterContents(null, offlineItem, null, regularItem, offTheRecordItem);
        assertEquals(100011, mAdapter.getTotalDownloadSize());

        // Remove an item from the date bucket with two items.
        assertEquals(3, mObserver.onChangedCallback.getCallCount());
        mAdapter.onDownloadItemRemoved(offTheRecordItem.getId(), true);
        mObserver.onChangedCallback.waitForCallback(3);
        checkAdapterContents(null, offlineItem, null, regularItem);
        assertEquals(100001, mAdapter.getTotalDownloadSize());

        // Remove an item from the second bucket, which removes the bucket entirely.
        assertEquals(4, mObserver.onChangedCallback.getCallCount());
        mOfflineDelegate.observer.onItemDeleted(offlineItem.getGuid());
        mObserver.onChangedCallback.waitForCallback(4);
        checkAdapterContents(null, regularItem);
        assertEquals(1, mAdapter.getTotalDownloadSize());

        // Remove the last item in the list.
        assertEquals(5, mObserver.onChangedCallback.getCallCount());
        mAdapter.onDownloadItemRemoved(regularItem.getId(), false);
        mObserver.onChangedCallback.waitForCallback(5);
        assertEquals(0, mAdapter.getItemCount());
        assertEquals(0, mAdapter.getTotalDownloadSize());
    }

    /** Test filtering of items. */
    @SmallTest
    public void testFilter_SevenItems() throws Exception {
        DownloadItem item0 = createDownloadItem(0, "19840116 12:00");
        DownloadItem item1 = createDownloadItem(1, "19840116 12:01");
        DownloadItem item2 = createDownloadItem(2, "19840117 12:00");
        DownloadItem item3 = createDownloadItem(3, "19840117 12:01");
        DownloadItem item4 = createDownloadItem(4, "19840118 12:00");
        DownloadItem item5 = createDownloadItem(5, "19840118 12:01");
        OfflinePageDownloadItem item6 = createOfflineItem(0, "19840118 6:00");
        mDownloadDelegate.regularItems.add(item0);
        mDownloadDelegate.offTheRecordItems.add(item1);
        mDownloadDelegate.regularItems.add(item2);
        mDownloadDelegate.regularItems.add(item3);
        mDownloadDelegate.offTheRecordItems.add(item4);
        mDownloadDelegate.regularItems.add(item5);
        mOfflineDelegate.items.add(item6);
        initializeAdapter(true);
        checkAdapterContents(null, item5, item4, item6, null, item3, item2, null, item1, item0);
        assertEquals(1666, mAdapter.getTotalDownloadSize());

        mAdapter.onFilterChanged(DownloadFilter.FILTER_AUDIO);
        checkAdapterContents(null, item5, item4);
        assertEquals(1666, mAdapter.getTotalDownloadSize());  // Total size ignores filters.

        mAdapter.onFilterChanged(DownloadFilter.FILTER_VIDEO);
        checkAdapterContents(null, item3);

        mAdapter.onFilterChanged(DownloadFilter.FILTER_IMAGE);
        checkAdapterContents(null, item1, item0);

        mAdapter.onFilterChanged(DownloadFilter.FILTER_PAGE);
        checkAdapterContents(null, item6);

        mAdapter.onFilterChanged(DownloadFilter.FILTER_ALL);
        checkAdapterContents(null, item5, item4, item6, null, item3, item2, null, item1, item0);
        assertEquals(1666, mAdapter.getTotalDownloadSize());
    }

    /** Tests that the list is updated appropriately when Offline Pages are deleted. */
    @SmallTest
    public void testFilter_AfterOfflineDeletions() throws Exception {
        OfflinePageDownloadItem item0 = createOfflineItem(0, "19840116 6:00");
        OfflinePageDownloadItem item1 = createOfflineItem(1, "19840116 12:00");
        OfflinePageDownloadItem item2 = createOfflineItem(2, "19840120 6:00");
        mOfflineDelegate.items.add(item0);
        mOfflineDelegate.items.add(item1);
        mOfflineDelegate.items.add(item2);
        initializeAdapter(false);
        checkAdapterContents(null, item2, null, item1, item0);
        assertEquals(111000, mAdapter.getTotalDownloadSize());

        // Filter shows everything.
        mOfflineDelegate.observer.onItemDeleted(item1.getGuid());
        checkAdapterContents(null, item2, null, item0);

        // Filter shows nothing when the item is deleted because it's a different kind of item.
        mAdapter.onFilterChanged(DownloadFilter.FILTER_AUDIO);
        assertEquals(0, mAdapter.getItemCount());
        mOfflineDelegate.observer.onItemDeleted(item0.getGuid());
        assertEquals(0, mAdapter.getItemCount());

        // Filter shows just pages.
        mAdapter.onFilterChanged(DownloadFilter.FILTER_PAGE);
        checkAdapterContents(null, item2);
        mOfflineDelegate.observer.onItemDeleted(item2.getGuid());
        assertEquals(0, mAdapter.getItemCount());
    }

    /** Checks that the adapter has the correct items in the right places. */
    private void checkAdapterContents(Object... expectedItems) {
        assertEquals(expectedItems.length, mAdapter.getItemCount());
        for (int i = 0; i < expectedItems.length; i++) {
            if (expectedItems[i] == null) {
                // Expect a date.
                // TODO(dfalcantara): Check what date the header is showing.
                assertEquals(TYPE_DATE, mAdapter.getItemViewType(i));
            } else {
                // Expect a particular item.
                assertEquals(TYPE_NORMAL, mAdapter.getItemViewType(i));
                assertEquals(expectedItems[i],
                        ((DownloadHistoryItemWrapper) mAdapter.getItemAt(i).second).getItem());
            }
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
        } else {
            return null;
        }

        item.setStartTime(dateToEpoch(date));
        return item;
    }

    /** Creates a new OfflinePageDownloadItem with pre-defined values. */
    private OfflinePageDownloadItem createOfflineItem(int which, String date)
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
        } else {
            return null;
        }
    }

}
