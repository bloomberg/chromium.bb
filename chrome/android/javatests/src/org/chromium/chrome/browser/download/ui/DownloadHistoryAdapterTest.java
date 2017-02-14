// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.ui;

import static org.chromium.chrome.browser.widget.DateDividedAdapter.TYPE_DATE;
import static org.chromium.chrome.browser.widget.DateDividedAdapter.TYPE_NORMAL;

import android.support.test.filters.SmallTest;
import android.support.v7.widget.RecyclerView;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.chrome.browser.download.DownloadItem;
import org.chromium.chrome.browser.download.ui.StubbedProvider.StubbedDownloadDelegate;
import org.chromium.chrome.browser.download.ui.StubbedProvider.StubbedOfflinePageDelegate;
import org.chromium.chrome.browser.offlinepages.downloads.OfflinePageDownloadItem;
import org.chromium.content.browser.test.NativeLibraryTestBase;
import org.chromium.content_public.browser.DownloadState;

import java.util.Set;

/**
 * Tests a DownloadHistoryAdapter that is isolated from the real bridges.
 */
public class DownloadHistoryAdapterTest extends NativeLibraryTestBase {

    private static class Observer extends RecyclerView.AdapterDataObserver
            implements DownloadHistoryAdapter.TestObserver {
        public CallbackHelper onChangedCallback = new CallbackHelper();
        public CallbackHelper onDownloadItemCreatedCallback = new CallbackHelper();
        public CallbackHelper onDownloadItemUpdatedCallback = new CallbackHelper();

        public DownloadItem createdItem;
        public DownloadItem updatedItem;

        @Override
        public void onChanged() {
            onChangedCallback.notifyCalled();
        }

        @Override
        public void onDownloadItemCreated(DownloadItem item) {
            createdItem = item;
            onDownloadItemCreatedCallback.notifyCalled();
        }

        @Override
        public void onDownloadItemUpdated(DownloadItem item) {
            updatedItem = item;
            onDownloadItemUpdatedCallback.notifyCalled();
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
        mAdapter.registerObserverForTest(mObserver);

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
        mObserver.onChangedCallback.waitForCallback(callCount, 1);
    }

    /** Nothing downloaded, nothing shown. */
    @SmallTest
    public void testInitialize_Empty() throws Exception {
        initializeAdapter(false);
        assertEquals(0, mAdapter.getItemCount());
        assertEquals(0, mAdapter.getTotalDownloadSize());

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mAdapter.onManagerDestroyed();
            }
        });

        mDownloadDelegate.removeCallback.waitForCallback(0);
    }

    /** One downloaded item should show the item and a date header. */
    @SmallTest
    public void testInitialize_SingleItem() throws Exception {
        DownloadItem item = StubbedProvider.createDownloadItem(0, "19840116 12:00");
        mDownloadDelegate.regularItems.add(item);
        initializeAdapter(false);
        checkAdapterContents(null, item);
        assertEquals(1, mAdapter.getTotalDownloadSize());
    }

    /** Two items downloaded on the same day should end up in the same group, in recency order. */
    @SmallTest
    public void testInitialize_TwoItemsOneDate() throws Exception {
        DownloadItem item0 = StubbedProvider.createDownloadItem(0, "19840116 12:00");
        DownloadItem item1 = StubbedProvider.createDownloadItem(1, "19840116 12:01");
        mDownloadDelegate.regularItems.add(item0);
        mDownloadDelegate.regularItems.add(item1);
        initializeAdapter(false);
        checkAdapterContents(null, item1, item0);
        assertEquals(11, mAdapter.getTotalDownloadSize());
    }

    /** Two items downloaded on different days should end up in different date groups. */
    @SmallTest
    public void testInitialize_TwoItemsTwoDates() throws Exception {
        DownloadItem item0 = StubbedProvider.createDownloadItem(0, "19840116 12:00");
        DownloadItem item1 = StubbedProvider.createDownloadItem(1, "19840117 12:00");
        mDownloadDelegate.regularItems.add(item0);
        mDownloadDelegate.regularItems.add(item1);
        initializeAdapter(false);
        checkAdapterContents(null, item1, null, item0);
        assertEquals(11, mAdapter.getTotalDownloadSize());
    }

    /** Off the record downloads are ignored if the DownloadHistoryAdapter isn't watching them. */
    @SmallTest
    public void testInitialize_OffTheRecord_Ignored() throws Exception {
        DownloadItem item0 = StubbedProvider.createDownloadItem(0, "19840116 12:00");
        DownloadItem item1 = StubbedProvider.createDownloadItem(1, "19840116 12:01");
        mDownloadDelegate.regularItems.add(item0);
        mDownloadDelegate.offTheRecordItems.add(item1);
        initializeAdapter(false);
        checkAdapterContents(null, item0);
        assertEquals(1, mAdapter.getTotalDownloadSize());
    }

    /** A regular and a off the record item with the same date are bucketed together. */
    @SmallTest
    public void testInitialize_OffTheRecord_TwoItemsOneDate() throws Exception {
        DownloadItem item0 = StubbedProvider.createDownloadItem(0, "19840116 18:00");
        DownloadItem item1 = StubbedProvider.createDownloadItem(1, "19840116 12:00");
        mDownloadDelegate.regularItems.add(item0);
        mDownloadDelegate.offTheRecordItems.add(item1);
        initializeAdapter(true);
        checkAdapterContents(null, item0, item1);
        assertEquals(11, mAdapter.getTotalDownloadSize());
    }

    /** Test that all the download item types intermingle correctly. */
    @SmallTest
    public void testInitialize_ThreeItemsDifferentKinds() throws Exception {
        DownloadItem item0 = StubbedProvider.createDownloadItem(0, "19840116 18:00");
        DownloadItem item1 = StubbedProvider.createDownloadItem(1, "19840116 12:00");
        OfflinePageDownloadItem item2 = StubbedProvider.createOfflineItem(2, "19840117 6:00");
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
        assertEquals(1, mObserver.onChangedCallback.getCallCount());
        DownloadItem item0 = StubbedProvider.createDownloadItem(0, "19840116 12:00");
        mAdapter.onDownloadItemCreated(item0);
        mObserver.onChangedCallback.waitForCallback(1);
        checkAdapterContents(null, item0);
        assertEquals(1, mAdapter.getTotalDownloadSize());

        // Add a second item with a different date.
        assertEquals(2, mObserver.onChangedCallback.getCallCount());
        DownloadItem item1 = StubbedProvider.createDownloadItem(1, "19840117 12:00");
        mAdapter.onDownloadItemCreated(item1);
        mObserver.onChangedCallback.waitForCallback(2);
        checkAdapterContents(null, item1, null, item0);
        assertEquals(11, mAdapter.getTotalDownloadSize());

        // Add a third item with the same date as the second item.
        assertEquals(2, mObserver.onDownloadItemCreatedCallback.getCallCount());
        DownloadItem item2 = StubbedProvider.createDownloadItem(
                2, "19840117 18:00", false, DownloadState.IN_PROGRESS, 0);
        mAdapter.onDownloadItemCreated(item2);
        mObserver.onDownloadItemCreatedCallback.waitForCallback(2);
        assertEquals(mObserver.createdItem, item2);
        checkAdapterContents(null, item2, item1, null, item0);
        assertEquals(11, mAdapter.getTotalDownloadSize());

        // An item with the same download ID as the second item should just update the old one,
        // but it should now be visible.
        int callCount = mObserver.onDownloadItemUpdatedCallback.getCallCount();
        DownloadItem item3 = StubbedProvider.createDownloadItem(
                2, "19840117 18:00", false, DownloadState.COMPLETE, 100);
        mAdapter.onDownloadItemUpdated(item3);
        mObserver.onDownloadItemUpdatedCallback.waitForCallback(callCount);
        assertEquals(mObserver.updatedItem, item3);
        checkAdapterContents(null, item3, item1, null, item0);
        assertEquals(111, mAdapter.getTotalDownloadSize());

        // Throw on a new OfflinePageItem.
        callCount = mObserver.onChangedCallback.getCallCount();
        OfflinePageDownloadItem item4 = StubbedProvider.createOfflineItem(0, "19840117 19:00");
        mOfflineDelegate.observer.onItemAdded(item4);
        mObserver.onChangedCallback.waitForCallback(callCount);
        checkAdapterContents(null, item4, item3, item1, null, item0);

        // Update the existing OfflinePageItem.
        callCount = mObserver.onChangedCallback.getCallCount();
        OfflinePageDownloadItem item5 = StubbedProvider.createOfflineItem(0, "19840117 19:00");
        mOfflineDelegate.observer.onItemUpdated(item5);
        mObserver.onChangedCallback.waitForCallback(callCount);
        checkAdapterContents(null, item5, item3, item1, null, item0);
    }

    /** Test removal of items. */
    @SmallTest
    public void testRemove_ThreeItemsTwoDates() throws Exception {
        // Initialize the DownloadHistoryAdapter with three items in two date buckets.
        DownloadItem regularItem = StubbedProvider.createDownloadItem(0, "19840116 18:00");
        DownloadItem offTheRecordItem = StubbedProvider.createDownloadItem(
                1, "19840116 12:00", true, DownloadState.COMPLETE, 100);
        OfflinePageDownloadItem offlineItem =
                StubbedProvider.createOfflineItem(2, "19840117 12:01");
        mDownloadDelegate.regularItems.add(regularItem);
        mDownloadDelegate.offTheRecordItems.add(offTheRecordItem);
        mOfflineDelegate.items.add(offlineItem);
        initializeAdapter(true);
        checkAdapterContents(null, offlineItem, null, regularItem, offTheRecordItem);
        assertEquals(100011, mAdapter.getTotalDownloadSize());

        // Remove an item from the date bucket with two items.
        assertEquals(1, mObserver.onChangedCallback.getCallCount());
        mAdapter.onDownloadItemRemoved(offTheRecordItem.getId(), true);
        mObserver.onChangedCallback.waitForCallback(1);
        checkAdapterContents(null, offlineItem, null, regularItem);
        assertEquals(100001, mAdapter.getTotalDownloadSize());

        // Remove an item from the second bucket, which removes the bucket entirely.
        assertEquals(2, mObserver.onChangedCallback.getCallCount());
        mOfflineDelegate.observer.onItemDeleted(offlineItem.getGuid());
        mObserver.onChangedCallback.waitForCallback(2);
        checkAdapterContents(null, regularItem);
        assertEquals(1, mAdapter.getTotalDownloadSize());

        // Remove the last item in the list.
        assertEquals(3, mObserver.onChangedCallback.getCallCount());
        mAdapter.onDownloadItemRemoved(regularItem.getId(), false);
        mObserver.onChangedCallback.waitForCallback(3);
        assertEquals(0, mAdapter.getItemCount());
        assertEquals(0, mAdapter.getTotalDownloadSize());
    }

    /** Test filtering of items. */
    @SmallTest
    public void testFilter_SevenItems() throws Exception {
        DownloadItem item0 = StubbedProvider.createDownloadItem(0, "19840116 12:00");
        DownloadItem item1 = StubbedProvider.createDownloadItem(1, "19840116 12:01");
        DownloadItem item2 = StubbedProvider.createDownloadItem(2, "19840117 12:00");
        DownloadItem item3 = StubbedProvider.createDownloadItem(3, "19840117 12:01");
        DownloadItem item4 = StubbedProvider.createDownloadItem(4, "19840118 12:00");
        DownloadItem item5 = StubbedProvider.createDownloadItem(5, "19840118 12:01");
        OfflinePageDownloadItem item6 = StubbedProvider.createOfflineItem(0, "19840118 6:00");
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
        OfflinePageDownloadItem item0 = StubbedProvider.createOfflineItem(0, "19840116 6:00");
        OfflinePageDownloadItem item1 = StubbedProvider.createOfflineItem(1, "19840116 12:00");
        OfflinePageDownloadItem item2 = StubbedProvider.createOfflineItem(2, "19840120 6:00");
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

    @SmallTest
    public void testInProgress_FilePathMapAccurate() throws Exception {
        Set<DownloadHistoryItemWrapper> toDelete;

        initializeAdapter(false);
        assertEquals(0, mAdapter.getItemCount());
        assertEquals(0, mAdapter.getTotalDownloadSize());

        // Simulate the creation of a new item by providing a DownloadItem without a path.
        DownloadItem itemCreated = StubbedProvider.createDownloadItem(
                9, "19840118 12:01", false, DownloadState.IN_PROGRESS, 0);
        mAdapter.onDownloadItemCreated(itemCreated);
        mObserver.onDownloadItemCreatedCallback.waitForCallback(0);
        assertEquals(mObserver.createdItem, itemCreated);

        checkAdapterContents();
        toDelete = mAdapter.getItemsForFilePath(itemCreated.getDownloadInfo().getFilePath());
        assertNull(toDelete);

        // Update the Adapter with new information about the item.
        int callCount = mObserver.onDownloadItemUpdatedCallback.getCallCount();
        DownloadItem itemUpdated = StubbedProvider.createDownloadItem(
                10, "19840118 12:01", false, DownloadState.IN_PROGRESS, 50);
        mAdapter.onDownloadItemUpdated(itemUpdated);
        mObserver.onDownloadItemUpdatedCallback.waitForCallback(callCount);
        assertEquals(mObserver.updatedItem, itemUpdated);

        checkAdapterContents(null, itemUpdated);
        toDelete = mAdapter.getItemsForFilePath(itemUpdated.getDownloadInfo().getFilePath());
        assertNull(toDelete);

        // Tell the Adapter that the item has finished downloading.
        callCount = mObserver.onDownloadItemUpdatedCallback.getCallCount();
        DownloadItem itemCompleted = StubbedProvider.createDownloadItem(
                10, "19840118 12:01", false, DownloadState.COMPLETE, 100);
        mAdapter.onDownloadItemUpdated(itemCompleted);
        mObserver.onDownloadItemUpdatedCallback.waitForCallback(callCount);
        assertEquals(mObserver.updatedItem, itemCompleted);
        checkAdapterContents(null, itemCompleted);

        // Confirm that the file now shows up when trying to delete it.
        toDelete = mAdapter.getItemsForFilePath(itemCompleted.getDownloadInfo().getFilePath());
        assertEquals(1, toDelete.size());
        assertEquals(itemCompleted.getId(), toDelete.iterator().next().getId());
    }

    @SmallTest
    public void testSearch_NoFilter() throws Exception {
        DownloadItem item0 = StubbedProvider.createDownloadItem(0, "19840116 12:00");
        DownloadItem item1 = StubbedProvider.createDownloadItem(1, "19840116 12:01");
        DownloadItem item2 = StubbedProvider.createDownloadItem(2, "19840117 12:00");
        DownloadItem item3 = StubbedProvider.createDownloadItem(3, "19840117 12:01");
        DownloadItem item4 = StubbedProvider.createDownloadItem(4, "19840118 12:00");
        DownloadItem item5 = StubbedProvider.createDownloadItem(5, "19840118 12:01");
        OfflinePageDownloadItem item6 = StubbedProvider.createOfflineItem(0, "19840118 6:00");
        mDownloadDelegate.regularItems.add(item0);
        mDownloadDelegate.offTheRecordItems.add(item1);
        mDownloadDelegate.regularItems.add(item2);
        mDownloadDelegate.regularItems.add(item3);
        mDownloadDelegate.offTheRecordItems.add(item4);
        mDownloadDelegate.regularItems.add(item5);
        mOfflineDelegate.items.add(item6);
        initializeAdapter(true);
        checkAdapterContents(null, item5, item4, item6, null, item3, item2, null, item1, item0);

        // Perform a search that matches the file name for a few downloads.
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mAdapter.search("FiLe");
            }
        });

        // Only items matching the query should be shown.
        checkAdapterContents(null, item2, null, item1, item0);

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mAdapter.onEndSearch();
            }
        });

        // All items should be shown again after the search is ended.
        checkAdapterContents(null, item5, item4, item6, null, item3, item2, null, item1, item0);

        // Perform a search that matches the hostname for a couple downloads.
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mAdapter.search("oNE");
            }
        });

        checkAdapterContents(null, item4, null, item1);
    }

    @SmallTest
    public void testSearch_WithFilter() throws Exception {
        DownloadItem item0 = StubbedProvider.createDownloadItem(0, "19840116 12:00");
        DownloadItem item1 = StubbedProvider.createDownloadItem(1, "19840116 12:01");
        DownloadItem item2 = StubbedProvider.createDownloadItem(2, "19840117 12:00");
        DownloadItem item3 = StubbedProvider.createDownloadItem(3, "19840117 12:01");
        DownloadItem item4 = StubbedProvider.createDownloadItem(4, "19840118 12:00");
        DownloadItem item5 = StubbedProvider.createDownloadItem(5, "19840118 12:01");
        OfflinePageDownloadItem item6 = StubbedProvider.createOfflineItem(0, "19840118 6:00");
        mDownloadDelegate.regularItems.add(item0);
        mDownloadDelegate.offTheRecordItems.add(item1);
        mDownloadDelegate.regularItems.add(item2);
        mDownloadDelegate.regularItems.add(item3);
        mDownloadDelegate.offTheRecordItems.add(item4);
        mDownloadDelegate.regularItems.add(item5);
        mOfflineDelegate.items.add(item6);
        initializeAdapter(true);
        checkAdapterContents(null, item5, item4, item6, null, item3, item2, null, item1, item0);

        // Change the filter
        mAdapter.onFilterChanged(DownloadFilter.FILTER_IMAGE);
        checkAdapterContents(null, item1, item0);

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mAdapter.search("FiRSt");
            }
        });

        // Only items matching both the filter and the search query should be shown.
        checkAdapterContents(null, item0);

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mAdapter.onEndSearch();
            }
        });

        // All items matching the filter should be shown after the search is ended.
        checkAdapterContents(null, item1, item0);
    }

    @SmallTest
    public void testSearch_RemoveItem() throws Exception {
        DownloadItem item0 = StubbedProvider.createDownloadItem(0, "19840116 12:00");
        DownloadItem item1 = StubbedProvider.createDownloadItem(1, "19840116 12:01");
        DownloadItem item2 = StubbedProvider.createDownloadItem(2, "19840117 12:00");
        DownloadItem item3 = StubbedProvider.createDownloadItem(3, "19840117 12:01");
        DownloadItem item4 = StubbedProvider.createDownloadItem(4, "19840118 12:00");
        DownloadItem item5 = StubbedProvider.createDownloadItem(5, "19840118 12:01");
        OfflinePageDownloadItem item6 = StubbedProvider.createOfflineItem(0, "19840118 6:00");
        mDownloadDelegate.regularItems.add(item0);
        mDownloadDelegate.offTheRecordItems.add(item1);
        mDownloadDelegate.regularItems.add(item2);
        mDownloadDelegate.regularItems.add(item3);
        mDownloadDelegate.offTheRecordItems.add(item4);
        mDownloadDelegate.regularItems.add(item5);
        mOfflineDelegate.items.add(item6);
        initializeAdapter(true);
        checkAdapterContents(null, item5, item4, item6, null, item3, item2, null, item1, item0);

        // Perform a search that matches the file name for a few downloads.
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mAdapter.search("FiLe");
            }
        });

        // Only items matching the query should be shown.
        checkAdapterContents(null, item2, null, item1, item0);

        mAdapter.onDownloadItemRemoved(item1.getId(), false);

        checkAdapterContents(null, item2, null, item0);
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

}
