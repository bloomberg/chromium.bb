// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import static org.chromium.chrome.browser.widget.DateDividedAdapter.TYPE_DATE;
import static org.chromium.chrome.browser.widget.DateDividedAdapter.TYPE_HEADER;
import static org.chromium.chrome.browser.widget.DateDividedAdapter.TYPE_NORMAL;

import android.support.test.filters.SmallTest;
import android.test.InstrumentationTestCase;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.widget.selection.SelectionDelegate;

import java.util.Date;
import java.util.concurrent.TimeUnit;

/**
 * Tests for the {@link HistoryAdapter}.
 */
public class HistoryAdapterTest extends InstrumentationTestCase {
    private StubbedHistoryProvider mHistoryProvider;
    private HistoryAdapter mAdapter;

    @Override
    public void setUp() throws Exception {
        super.setUp();
        mHistoryProvider = new StubbedHistoryProvider();
        mAdapter = new HistoryAdapter(new SelectionDelegate<HistoryItem>(), null, mHistoryProvider);
    }

    private void initializeAdapter() {
        ThreadUtils.runOnUiThreadBlocking(new Runnable(){
            @Override
            public void run() {
                mAdapter.initialize();
            }
        });
    }

    @SmallTest
    public void testInitialize_Empty() {
        initializeAdapter();
        checkAdapterContents(false);
    }

    @SmallTest
    public void testInitialize_SingleItem() {
        Date today = new Date();
        long[] timestamps = {today.getTime(), today.getTime() - 100};
        HistoryItem item1 = StubbedHistoryProvider.createHistoryItem(0, timestamps);
        mHistoryProvider.addItem(item1);

        initializeAdapter();

        // There should be three items - the header, a date and the history item.
        checkAdapterContents(true, null, null, item1);
    }

    @SmallTest
    public void testRemove_TwoItemsOneDate() {
        Date today = new Date();
        long[] timestamps = {today.getTime()};
        HistoryItem item1 = StubbedHistoryProvider.createHistoryItem(0, timestamps);
        mHistoryProvider.addItem(item1);

        HistoryItem item2 = StubbedHistoryProvider.createHistoryItem(1, timestamps);
        mHistoryProvider.addItem(item2);

        initializeAdapter();

        // There should be four items - the list header, a date header and two history items.
        checkAdapterContents(true, null, null, item1, item2);

        mAdapter.markItemForRemoval(item1);

        // Check that one item was removed.
        checkAdapterContents(true, null, null, item2);
        assertEquals(1, mHistoryProvider.markItemForRemovalCallback.getCallCount());
        assertEquals(0, mHistoryProvider.removeItemsCallback.getCallCount());

        mAdapter.markItemForRemoval(item2);

        // There should no longer be any items in the adapter.
        checkAdapterContents(false);
        assertEquals(2, mHistoryProvider.markItemForRemovalCallback.getCallCount());
        assertEquals(0, mHistoryProvider.removeItemsCallback.getCallCount());

        mAdapter.removeItems();
        assertEquals(1, mHistoryProvider.removeItemsCallback.getCallCount());
    }

    @SmallTest
    public void testRemove_TwoItemsTwoDates() {
        Date today = new Date();
        long[] timestamps = {today.getTime()};
        HistoryItem item1 = StubbedHistoryProvider.createHistoryItem(0, timestamps);
        mHistoryProvider.addItem(item1);

        long[] timestamps2 = {today.getTime() - TimeUnit.DAYS.toMillis(3)};
        HistoryItem item2 = StubbedHistoryProvider.createHistoryItem(1, timestamps2);
        mHistoryProvider.addItem(item2);

        initializeAdapter();

        // There should be five items - the list header, a date header, a history item, another
        // date header and another history item.
        checkAdapterContents(true, null, null, item1, null, item2);

        mAdapter.markItemForRemoval(item1);

        // Check that the first item and date header were removed.
        checkAdapterContents(true, null, null, item2);
        assertEquals(1, mHistoryProvider.markItemForRemovalCallback.getCallCount());
        assertEquals(0, mHistoryProvider.removeItemsCallback.getCallCount());

        mAdapter.markItemForRemoval(item2);

        // There should no longer be any items in the adapter.
        checkAdapterContents(false);
        assertEquals(2, mHistoryProvider.markItemForRemovalCallback.getCallCount());
        assertEquals(0, mHistoryProvider.removeItemsCallback.getCallCount());

        mAdapter.removeItems();
        assertEquals(1, mHistoryProvider.removeItemsCallback.getCallCount());
    }

    @SmallTest
    public void testSearch() {
        Date today = new Date();
        long[] timestamps = {today.getTime()};
        HistoryItem item1 = StubbedHistoryProvider.createHistoryItem(0, timestamps);
        mHistoryProvider.addItem(item1);

        long[] timestamps2 = {today.getTime() - TimeUnit.DAYS.toMillis(3)};
        HistoryItem item2 = StubbedHistoryProvider.createHistoryItem(1, timestamps2);
        mHistoryProvider.addItem(item2);

        initializeAdapter();
        checkAdapterContents(true, null, null, item1, null, item2);

        mAdapter.search("google");

        // The header should be hidden during the search.
        checkAdapterContents(false, null, item1);

        mAdapter.onEndSearch();

        // The header should be shown again after the search.
        checkAdapterContents(true, null, null, item1, null, item2);
    }

    @SmallTest
    public void testLoadMoreItems() {
        Date today = new Date();
        long[] timestamps = {today.getTime()};
        HistoryItem item1 = StubbedHistoryProvider.createHistoryItem(0, timestamps);
        mHistoryProvider.addItem(item1);

        HistoryItem item2 = StubbedHistoryProvider.createHistoryItem(1, timestamps);
        mHistoryProvider.addItem(item2);

        HistoryItem item3 = StubbedHistoryProvider.createHistoryItem(2, timestamps);
        mHistoryProvider.addItem(item3);

        HistoryItem item4 = StubbedHistoryProvider.createHistoryItem(3, timestamps);
        mHistoryProvider.addItem(item4);

        long[] timestamps2 = {today.getTime() - TimeUnit.DAYS.toMillis(2)};
        HistoryItem item5 = StubbedHistoryProvider.createHistoryItem(4, timestamps2);
        mHistoryProvider.addItem(item5);

        HistoryItem item6 = StubbedHistoryProvider.createHistoryItem(0, timestamps2);
        mHistoryProvider.addItem(item6);

        long[] timestamps3 = {today.getTime() - TimeUnit.DAYS.toMillis(4)};
        HistoryItem item7 = StubbedHistoryProvider.createHistoryItem(1, timestamps3);
        mHistoryProvider.addItem(item7);

        initializeAdapter();

        // Only the first five of the seven items should be loaded.
        checkAdapterContents(true, null, null, item1, item2, item3, item4, null, item5);
        assertTrue(mAdapter.canLoadMoreItems());

        mAdapter.loadMoreItems();

        // All items should now be loaded.
        checkAdapterContents(true, null, null, item1, item2, item3, item4, null, item5, item6,
                null, item7);
        assertFalse(mAdapter.canLoadMoreItems());
    }

    @SmallTest
    public void testOnHistoryDeleted() throws Exception {
        Date today = new Date();
        long[] timestamps = {today.getTime()};
        HistoryItem item1 = StubbedHistoryProvider.createHistoryItem(0, timestamps);
        mHistoryProvider.addItem(item1);

        initializeAdapter();

        checkAdapterContents(true, null, null, item1);

        mHistoryProvider.removeItem(item1);

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mAdapter.onHistoryDeleted();
            }
        });

        checkAdapterContents(false);
    }

    @SmallTest
    public void testBlockedSite() {
        Date today = new Date();
        long[] timestamps = {today.getTime()};
        HistoryItem item1 = StubbedHistoryProvider.createHistoryItem(0, timestamps);
        mHistoryProvider.addItem(item1);

        HistoryItem item2 = StubbedHistoryProvider.createHistoryItem(5, timestamps);
        mHistoryProvider.addItem(item2);

        initializeAdapter();

        checkAdapterContents(true, null, null, item1, item2);
        assertEquals(ContextUtils.getApplicationContext().getString(
                R.string.android_history_blocked_site), item2.getTitle());
        assertTrue(item2.wasBlockedVisit());
    }

    private void checkAdapterContents(boolean hasHeader, Object... expectedItems) {
        assertEquals(expectedItems.length, mAdapter.getItemCount());
        assertEquals(hasHeader, mAdapter.hasListHeader());

        for (int i = 0; i < expectedItems.length; i++) {
            if (i == 0 && hasHeader) {
                assertEquals(TYPE_HEADER, mAdapter.getItemViewType(i));
                continue;
            }

            if (expectedItems[i] == null) {
                // TODO(twellington): Check what date header is showing.
                assertEquals(TYPE_DATE, mAdapter.getItemViewType(i));
            } else {
                assertEquals(TYPE_NORMAL, mAdapter.getItemViewType(i));
                assertEquals(expectedItems[i], mAdapter.getItemAt(i).second);
            }
        }
    }
}
