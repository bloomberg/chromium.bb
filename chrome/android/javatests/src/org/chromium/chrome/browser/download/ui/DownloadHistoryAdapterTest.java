// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.ui;

import static org.chromium.chrome.browser.widget.DateDividedAdapter.TYPE_DATE;
import static org.chromium.chrome.browser.widget.DateDividedAdapter.TYPE_HEADER;
import static org.chromium.chrome.browser.widget.DateDividedAdapter.TYPE_NORMAL;

import android.content.SharedPreferences.Editor;
import android.support.test.filters.SmallTest;
import android.support.v7.widget.RecyclerView;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.chrome.browser.download.DownloadItem;
import org.chromium.chrome.browser.download.ui.StubbedProvider.StubbedDownloadDelegate;
import org.chromium.chrome.browser.download.ui.StubbedProvider.StubbedOfflinePageDelegate;
import org.chromium.chrome.browser.offlinepages.downloads.OfflinePageDownloadItem;
import org.chromium.chrome.browser.test.ChromeBrowserTestRule;
import org.chromium.content_public.browser.DownloadState;

import java.util.Set;

/**
 * Tests a DownloadHistoryAdapter that is isolated from the real bridges.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class DownloadHistoryAdapterTest {
    @Rule
    public final ChromeBrowserTestRule mBrowserTestRule = new ChromeBrowserTestRule();

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

    /**
     * Object for use in {@link #checkAdapterContents(Object...)} that corresponds to
     * {@link #TYPE_HEADER}.
     */
    private static final Integer HEADER = -1;

    private static final String PREF_SHOW_STORAGE_INFO_HEADER =
            "download_home_show_storage_info_header";

    private DownloadHistoryAdapter mAdapter;
    private Observer mObserver;
    private StubbedDownloadDelegate mDownloadDelegate;
    private StubbedOfflinePageDelegate mOfflineDelegate;
    private StubbedProvider mBackendProvider;

    @Before
    public void setUp() throws Exception {
        mBackendProvider = new StubbedProvider();
        mDownloadDelegate = mBackendProvider.getDownloadDelegate();
        mOfflineDelegate = mBackendProvider.getOfflinePageBridge();
        Editor editor = ContextUtils.getAppSharedPreferences().edit();
        editor.putBoolean(PREF_SHOW_STORAGE_INFO_HEADER, true).apply();
    }

    private void initializeAdapter(boolean showOffTheRecord) throws Exception {
        mObserver = new Observer();
        mAdapter = new DownloadHistoryAdapter(showOffTheRecord, null);
        mAdapter.registerAdapterDataObserver(mObserver);
        mAdapter.registerObserverForTest(mObserver);

        // Initialize the Adapter with all the DownloadItems and OfflinePageDownloadItems.
        int callCount = mObserver.onChangedCallback.getCallCount();
        Assert.assertEquals(0, callCount);
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mAdapter.initialize(mBackendProvider, null);
            }
        });
        mDownloadDelegate.addCallback.waitForCallback(0);
        mObserver.onChangedCallback.waitForCallback(callCount, 1);
    }

    /** Nothing downloaded, nothing shown. */
    @Test
    @SmallTest
    public void testInitialize_Empty() throws Exception {
        initializeAdapter(false);
        Assert.assertEquals(0, mAdapter.getItemCount());
        Assert.assertEquals(0, mAdapter.getTotalDownloadSize());

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mAdapter.onManagerDestroyed();
            }
        });

        mDownloadDelegate.removeCallback.waitForCallback(0);
    }

    /** One downloaded item should show the item and a date header. */
    @Test
    @SmallTest
    public void testInitialize_SingleItem() throws Exception {
        DownloadItem item = StubbedProvider.createDownloadItem(0, "19840116 12:00");
        mDownloadDelegate.regularItems.add(item);
        initializeAdapter(false);
        checkAdapterContents(HEADER, null, item);
        Assert.assertEquals(1, mAdapter.getTotalDownloadSize());
    }

    /** Two items downloaded on the same day should end up in the same group, in recency order. */
    @Test
    @SmallTest
    public void testInitialize_TwoItemsOneDate() throws Exception {
        DownloadItem item0 = StubbedProvider.createDownloadItem(0, "19840116 12:00");
        DownloadItem item1 = StubbedProvider.createDownloadItem(1, "19840116 12:01");
        mDownloadDelegate.regularItems.add(item0);
        mDownloadDelegate.regularItems.add(item1);
        initializeAdapter(false);
        checkAdapterContents(HEADER, null, item1, item0);
        Assert.assertEquals(11, mAdapter.getTotalDownloadSize());
    }

    /** Two items downloaded on different days should end up in different date groups. */
    @Test
    @SmallTest
    public void testInitialize_TwoItemsTwoDates() throws Exception {
        DownloadItem item0 = StubbedProvider.createDownloadItem(0, "19840116 12:00");
        DownloadItem item1 = StubbedProvider.createDownloadItem(1, "19840117 12:00");
        mDownloadDelegate.regularItems.add(item0);
        mDownloadDelegate.regularItems.add(item1);
        initializeAdapter(false);
        checkAdapterContents(HEADER, null, item1, null, item0);
        Assert.assertEquals(11, mAdapter.getTotalDownloadSize());
    }

    /** Storage header shouldn't show up if user has already turned it off. */
    @Test
    @SmallTest
    public void testInitialize_SingleItemNoStorageHeader() throws Exception {
        Editor editor = ContextUtils.getAppSharedPreferences().edit();
        editor.putBoolean(PREF_SHOW_STORAGE_INFO_HEADER, false).apply();
        DownloadItem item = StubbedProvider.createDownloadItem(0, "19840116 12:00");
        mDownloadDelegate.regularItems.add(item);
        initializeAdapter(false);
        checkAdapterContents(null, item);
        Assert.assertEquals(1, mAdapter.getTotalDownloadSize());
    }

    /** Toggle the info button. Storage header should turn off/on accordingly. */
    @Test
    @SmallTest
    public void testToggleStorageHeader() throws Exception {
        DownloadItem item0 = StubbedProvider.createDownloadItem(0, "19840116 12:00");
        DownloadItem item1 = StubbedProvider.createDownloadItem(1, "19840116 12:01");
        mDownloadDelegate.regularItems.add(item0);
        mDownloadDelegate.regularItems.add(item1);
        initializeAdapter(false);
        checkAdapterContents(HEADER, null, item1, item0);
        Assert.assertEquals(11, mAdapter.getTotalDownloadSize());

        // Turn off info and check that header is gone.
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mAdapter.setShowStorageInfoHeader(false);
            }
        });
        checkAdapterContents(null, item1, item0);

        // Turn on info and check that header is back again.
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mAdapter.setShowStorageInfoHeader(true);
            }
        });
        checkAdapterContents(HEADER, null, item1, item0);
    }

    /** Off the record downloads are ignored if the DownloadHistoryAdapter isn't watching them. */
    @Test
    @SmallTest
    public void testInitialize_OffTheRecord_Ignored() throws Exception {
        DownloadItem item0 = StubbedProvider.createDownloadItem(0, "19840116 12:00");
        DownloadItem item1 = StubbedProvider.createDownloadItem(1, "19840116 12:01");
        mDownloadDelegate.regularItems.add(item0);
        mDownloadDelegate.offTheRecordItems.add(item1);
        initializeAdapter(false);
        checkAdapterContents(HEADER, null, item0);
        Assert.assertEquals(1, mAdapter.getTotalDownloadSize());
    }

    /** A regular and a off the record item with the same date are bucketed together. */
    @Test
    @SmallTest
    public void testInitialize_OffTheRecord_TwoItemsOneDate() throws Exception {
        DownloadItem item0 = StubbedProvider.createDownloadItem(0, "19840116 18:00");
        DownloadItem item1 = StubbedProvider.createDownloadItem(1, "19840116 12:00");
        mDownloadDelegate.regularItems.add(item0);
        mDownloadDelegate.offTheRecordItems.add(item1);
        initializeAdapter(true);
        checkAdapterContents(HEADER, null, item0, item1);
        Assert.assertEquals(11, mAdapter.getTotalDownloadSize());
    }

    /** Test that all the download item types intermingle correctly. */
    @Test
    @SmallTest
    public void testInitialize_ThreeItemsDifferentKinds() throws Exception {
        DownloadItem item0 = StubbedProvider.createDownloadItem(0, "19840116 18:00");
        DownloadItem item1 = StubbedProvider.createDownloadItem(1, "19840116 12:00");
        OfflinePageDownloadItem item2 = StubbedProvider.createOfflineItem(2, "19840117 6:00");
        mDownloadDelegate.regularItems.add(item0);
        mDownloadDelegate.offTheRecordItems.add(item1);
        mOfflineDelegate.items.add(item2);
        initializeAdapter(true);
        checkAdapterContents(HEADER, null, item2, null, item0, item1);
        Assert.assertEquals(100011, mAdapter.getTotalDownloadSize());
    }

    /** Adding and updating new items should bucket them into the proper dates. */
    @Test
    @SmallTest
    public void testUpdate_UpdateItems() throws Exception {
        // Start with an empty Adapter.
        initializeAdapter(false);
        Assert.assertEquals(0, mAdapter.getItemCount());
        Assert.assertEquals(0, mAdapter.getTotalDownloadSize());

        // Add the first item.
        Assert.assertEquals(1, mObserver.onChangedCallback.getCallCount());
        DownloadItem item0 = StubbedProvider.createDownloadItem(0, "19840116 12:00");
        mAdapter.onDownloadItemCreated(item0);
        mObserver.onChangedCallback.waitForCallback(1);
        checkAdapterContents(HEADER, null, item0);
        Assert.assertEquals(1, mAdapter.getTotalDownloadSize());

        // Add a second item with a different date.
        Assert.assertEquals(2, mObserver.onChangedCallback.getCallCount());
        DownloadItem item1 = StubbedProvider.createDownloadItem(1, "19840117 12:00");
        mAdapter.onDownloadItemCreated(item1);
        mObserver.onChangedCallback.waitForCallback(2);
        checkAdapterContents(HEADER, null, item1, null, item0);
        Assert.assertEquals(11, mAdapter.getTotalDownloadSize());

        // Add a third item with the same date as the second item.
        Assert.assertEquals(2, mObserver.onDownloadItemCreatedCallback.getCallCount());
        DownloadItem item2 = StubbedProvider.createDownloadItem(
                2, "19840117 18:00", false, DownloadState.IN_PROGRESS, 0);
        mAdapter.onDownloadItemCreated(item2);
        mObserver.onDownloadItemCreatedCallback.waitForCallback(2);
        Assert.assertEquals(mObserver.createdItem, item2);
        checkAdapterContents(HEADER, null, item2, item1, null, item0);
        Assert.assertEquals(11, mAdapter.getTotalDownloadSize());

        // An item with the same download ID as the second item should just update the old one,
        // but it should now be visible.
        int callCount = mObserver.onDownloadItemUpdatedCallback.getCallCount();
        DownloadItem item3 = StubbedProvider.createDownloadItem(
                2, "19840117 18:00", false, DownloadState.COMPLETE, 100);
        mAdapter.onDownloadItemUpdated(item3);
        mObserver.onDownloadItemUpdatedCallback.waitForCallback(callCount);
        Assert.assertEquals(mObserver.updatedItem, item3);
        checkAdapterContents(HEADER, null, item3, item1, null, item0);
        Assert.assertEquals(111, mAdapter.getTotalDownloadSize());

        // Throw on a new OfflinePageItem.
        callCount = mObserver.onChangedCallback.getCallCount();
        OfflinePageDownloadItem item4 = StubbedProvider.createOfflineItem(0, "19840117 19:00");
        mOfflineDelegate.observer.onItemAdded(item4);
        mObserver.onChangedCallback.waitForCallback(callCount);
        checkAdapterContents(HEADER, null, item4, item3, item1, null, item0);

        // Update the existing OfflinePageItem.
        callCount = mObserver.onChangedCallback.getCallCount();
        OfflinePageDownloadItem item5 = StubbedProvider.createOfflineItem(0, "19840117 19:00");
        mOfflineDelegate.observer.onItemUpdated(item5);
        mObserver.onChangedCallback.waitForCallback(callCount);
        checkAdapterContents(HEADER, null, item5, item3, item1, null, item0);
    }

    /** Test removal of items. */
    @Test
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
        checkAdapterContents(HEADER, null, offlineItem, null, regularItem, offTheRecordItem);
        Assert.assertEquals(100011, mAdapter.getTotalDownloadSize());

        // Remove an item from the date bucket with two items.
        Assert.assertEquals(1, mObserver.onChangedCallback.getCallCount());
        mAdapter.onDownloadItemRemoved(offTheRecordItem.getId(), true);
        mObserver.onChangedCallback.waitForCallback(1);
        checkAdapterContents(HEADER, null, offlineItem, null, regularItem);
        Assert.assertEquals(100001, mAdapter.getTotalDownloadSize());

        // Remove an item from the second bucket, which removes the bucket entirely.
        Assert.assertEquals(2, mObserver.onChangedCallback.getCallCount());
        mOfflineDelegate.observer.onItemDeleted(offlineItem.getGuid());
        mObserver.onChangedCallback.waitForCallback(2);
        checkAdapterContents(HEADER, null, regularItem);
        Assert.assertEquals(1, mAdapter.getTotalDownloadSize());

        // Remove the last item in the list.
        Assert.assertEquals(3, mObserver.onChangedCallback.getCallCount());
        mAdapter.onDownloadItemRemoved(regularItem.getId(), false);
        mObserver.onChangedCallback.waitForCallback(3);
        Assert.assertEquals(0, mAdapter.getItemCount());
        Assert.assertEquals(0, mAdapter.getTotalDownloadSize());
    }

    /** Test filtering of items. */
    @Test
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
        checkAdapterContents(
                HEADER, null, item5, item4, item6, null, item3, item2, null, item1, item0);
        Assert.assertEquals(1666, mAdapter.getTotalDownloadSize());

        mAdapter.onFilterChanged(DownloadFilter.FILTER_AUDIO);
        checkAdapterContents(HEADER, null, item5, item4);
        Assert.assertEquals(1666, mAdapter.getTotalDownloadSize()); // Total size ignores filters.

        mAdapter.onFilterChanged(DownloadFilter.FILTER_VIDEO);
        checkAdapterContents(HEADER, null, item3);

        mAdapter.onFilterChanged(DownloadFilter.FILTER_IMAGE);
        checkAdapterContents(HEADER, null, item1, item0);

        mAdapter.onFilterChanged(DownloadFilter.FILTER_PAGE);
        checkAdapterContents(HEADER, null, item6);

        mAdapter.onFilterChanged(DownloadFilter.FILTER_ALL);
        checkAdapterContents(
                HEADER, null, item5, item4, item6, null, item3, item2, null, item1, item0);
        Assert.assertEquals(1666, mAdapter.getTotalDownloadSize());
    }

    /** Tests that the list is updated appropriately when Offline Pages are deleted. */
    @Test
    @SmallTest
    public void testFilter_AfterOfflineDeletions() throws Exception {
        OfflinePageDownloadItem item0 = StubbedProvider.createOfflineItem(0, "19840116 6:00");
        OfflinePageDownloadItem item1 = StubbedProvider.createOfflineItem(1, "19840116 12:00");
        OfflinePageDownloadItem item2 = StubbedProvider.createOfflineItem(2, "19840120 6:00");
        mOfflineDelegate.items.add(item0);
        mOfflineDelegate.items.add(item1);
        mOfflineDelegate.items.add(item2);
        initializeAdapter(false);
        checkAdapterContents(HEADER, null, item2, null, item1, item0);
        Assert.assertEquals(111000, mAdapter.getTotalDownloadSize());

        // Filter shows everything.
        mOfflineDelegate.observer.onItemDeleted(item1.getGuid());
        checkAdapterContents(HEADER, null, item2, null, item0);

        // Filter shows nothing when the item is deleted because it's a different kind of item.
        mAdapter.onFilterChanged(DownloadFilter.FILTER_AUDIO);
        Assert.assertEquals(0, mAdapter.getItemCount());
        mOfflineDelegate.observer.onItemDeleted(item0.getGuid());
        Assert.assertEquals(0, mAdapter.getItemCount());

        // Filter shows just pages.
        mAdapter.onFilterChanged(DownloadFilter.FILTER_PAGE);
        checkAdapterContents(HEADER, null, item2);
        mOfflineDelegate.observer.onItemDeleted(item2.getGuid());
        Assert.assertEquals(0, mAdapter.getItemCount());
    }

    @Test
    @SmallTest
    public void testInProgress_FilePathMapAccurate() throws Exception {
        Set<DownloadHistoryItemWrapper> toDelete;

        initializeAdapter(false);
        Assert.assertEquals(0, mAdapter.getItemCount());
        Assert.assertEquals(0, mAdapter.getTotalDownloadSize());

        // Simulate the creation of a new item by providing a DownloadItem without a path.
        DownloadItem itemCreated = StubbedProvider.createDownloadItem(
                9, "19840118 12:01", false, DownloadState.IN_PROGRESS, 0);
        mAdapter.onDownloadItemCreated(itemCreated);
        mObserver.onDownloadItemCreatedCallback.waitForCallback(0);
        Assert.assertEquals(mObserver.createdItem, itemCreated);

        checkAdapterContents();
        toDelete = mAdapter.getItemsForFilePath(itemCreated.getDownloadInfo().getFilePath());
        Assert.assertNull(toDelete);

        // Update the Adapter with new information about the item.
        int callCount = mObserver.onDownloadItemUpdatedCallback.getCallCount();
        DownloadItem itemUpdated = StubbedProvider.createDownloadItem(
                10, "19840118 12:01", false, DownloadState.IN_PROGRESS, 50);
        mAdapter.onDownloadItemUpdated(itemUpdated);
        mObserver.onDownloadItemUpdatedCallback.waitForCallback(callCount);
        Assert.assertEquals(mObserver.updatedItem, itemUpdated);

        checkAdapterContents(HEADER, null, itemUpdated);
        toDelete = mAdapter.getItemsForFilePath(itemUpdated.getDownloadInfo().getFilePath());
        Assert.assertNull(toDelete);

        // Tell the Adapter that the item has finished downloading.
        callCount = mObserver.onDownloadItemUpdatedCallback.getCallCount();
        DownloadItem itemCompleted = StubbedProvider.createDownloadItem(
                10, "19840118 12:01", false, DownloadState.COMPLETE, 100);
        mAdapter.onDownloadItemUpdated(itemCompleted);
        mObserver.onDownloadItemUpdatedCallback.waitForCallback(callCount);
        Assert.assertEquals(mObserver.updatedItem, itemCompleted);
        checkAdapterContents(HEADER, null, itemCompleted);

        // Confirm that the file now shows up when trying to delete it.
        toDelete = mAdapter.getItemsForFilePath(itemCompleted.getDownloadInfo().getFilePath());
        Assert.assertEquals(1, toDelete.size());
        Assert.assertEquals(itemCompleted.getId(), toDelete.iterator().next().getId());
    }

    @Test
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
        checkAdapterContents(
                HEADER, null, item5, item4, item6, null, item3, item2, null, item1, item0);

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
        checkAdapterContents(
                HEADER, null, item5, item4, item6, null, item3, item2, null, item1, item0);

        // Perform a search that matches the hostname for a couple downloads.
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mAdapter.search("oNE");
            }
        });

        checkAdapterContents(null, item4, null, item1);
    }

    @Test
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
        checkAdapterContents(
                HEADER, null, item5, item4, item6, null, item3, item2, null, item1, item0);

        // Change the filter
        mAdapter.onFilterChanged(DownloadFilter.FILTER_IMAGE);
        checkAdapterContents(HEADER, null, item1, item0);

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
        checkAdapterContents(HEADER, null, item1, item0);
    }

    @Test
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
        checkAdapterContents(
                HEADER, null, item5, item4, item6, null, item3, item2, null, item1, item0);

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
        if (expectedItems.length == 0) {
            Assert.assertEquals(0, mAdapter.getItemCount());
            return;
        }

        Assert.assertEquals(expectedItems.length, mAdapter.getItemCount());

        for (int i = 0; i < expectedItems.length; i++) {
            if (HEADER.equals(expectedItems[i])) {
                Assert.assertEquals("The header should be the first item in the adapter.", 0, i);
                Assert.assertEquals(TYPE_HEADER, mAdapter.getItemViewType(i));
            } else if (expectedItems[i] == null) {
                // Expect a date.
                // TODO(dfalcantara): Check what date the header is showing.
                Assert.assertEquals(TYPE_DATE, mAdapter.getItemViewType(i));
            } else {
                // Expect a particular item.
                Assert.assertEquals(TYPE_NORMAL, mAdapter.getItemViewType(i));
                Assert.assertEquals(expectedItems[i],
                        ((DownloadHistoryItemWrapper) mAdapter.getItemAt(i).second).getItem());
            }
        }
    }

}
