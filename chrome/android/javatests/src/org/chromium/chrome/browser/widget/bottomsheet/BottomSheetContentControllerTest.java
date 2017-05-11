// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.widget.bottomsheet;

import android.support.test.filters.SmallTest;

import org.chromium.base.ThreadUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.bookmarks.BookmarkSheetContent;
import org.chromium.chrome.browser.bookmarks.BookmarkUtils;
import org.chromium.chrome.browser.download.DownloadSheetContent;
import org.chromium.chrome.browser.download.DownloadUtils;
import org.chromium.chrome.browser.history.HistoryManagerUtils;
import org.chromium.chrome.browser.history.HistorySheetContent;
import org.chromium.chrome.browser.suggestions.SuggestionsBottomSheetContent;
import org.chromium.chrome.test.BottomSheetTestCaseBase;

import java.util.concurrent.TimeoutException;

/** This class tests the functionality of the {@link BottomSheetContentController}. */
public class BottomSheetContentControllerTest extends BottomSheetTestCaseBase {
    @Override
    protected void setUp() throws Exception {
        super.setUp();
        setSheetState(BottomSheet.SHEET_STATE_PEEK, false);
    }

    @SmallTest
    public void testSelectContent() throws InterruptedException, TimeoutException {
        int contentChangedCount = mObserver.mContentChangedCallbackHelper.getCallCount();
        int openedCount = mObserver.mOpenedCallbackHelper.getCallCount();
        int closedCount = mObserver.mClosedCallbackHelper.getCallCount();

        setSheetState(BottomSheet.SHEET_STATE_HALF, false);
        mObserver.mOpenedCallbackHelper.waitForCallback(openedCount, 1);
        openedCount++;
        assertEquals(contentChangedCount, mObserver.mContentChangedCallbackHelper.getCallCount());
        assertEquals(closedCount, mObserver.mClosedCallbackHelper.getCallCount());

        selectBottomSheetContent(R.id.action_history);
        mObserver.mContentChangedCallbackHelper.waitForCallback(contentChangedCount, 1);
        contentChangedCount++;
        assertEquals(openedCount, mObserver.mOpenedCallbackHelper.getCallCount());
        assertEquals(closedCount, mObserver.mClosedCallbackHelper.getCallCount());
        assertTrue(mBottomSheet.getCurrentSheetContent() instanceof HistorySheetContent);
        assertEquals(
                R.id.action_history, mBottomSheetContentController.getSelectedItemIdForTests());

        setSheetState(BottomSheet.SHEET_STATE_PEEK, false);
        mObserver.mClosedCallbackHelper.waitForCallback(closedCount, 1);
        mObserver.mContentChangedCallbackHelper.waitForCallback(contentChangedCount, 1);
        assertEquals(openedCount, mObserver.mOpenedCallbackHelper.getCallCount());
        assertTrue(mBottomSheet.getCurrentSheetContent() instanceof SuggestionsBottomSheetContent);
        assertEquals(R.id.action_home, mBottomSheetContentController.getSelectedItemIdForTests());
    }

    @SmallTest
    public void testShowContentAndOpenSheet_Bookmarks()
            throws InterruptedException, TimeoutException {
        int initialContentChangedCount = mObserver.mContentChangedCallbackHelper.getCallCount();
        int initialOpenedCount = mObserver.mOpenedCallbackHelper.getCallCount();

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                BookmarkUtils.showBookmarkManager(getActivity());
            }
        });

        mObserver.mContentChangedCallbackHelper.waitForCallback(initialContentChangedCount, 1);
        mObserver.mOpenedCallbackHelper.waitForCallback(initialOpenedCount, 1);

        assertTrue(mBottomSheet.getCurrentSheetContent() instanceof BookmarkSheetContent);
        assertEquals(
                R.id.action_bookmarks, mBottomSheetContentController.getSelectedItemIdForTests());
    }

    @SmallTest
    public void testShowContentAndOpenSheet_Downloads()
            throws InterruptedException, TimeoutException {
        int initialContentChangedCount = mObserver.mContentChangedCallbackHelper.getCallCount();
        int initialOpenedCount = mObserver.mOpenedCallbackHelper.getCallCount();

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                DownloadUtils.showDownloadManager(getActivity(), getActivity().getActivityTab());
            }
        });

        mObserver.mContentChangedCallbackHelper.waitForCallback(initialContentChangedCount, 1);
        mObserver.mOpenedCallbackHelper.waitForCallback(initialOpenedCount, 1);

        assertTrue(mBottomSheet.getCurrentSheetContent() instanceof DownloadSheetContent);
        assertEquals(
                R.id.action_downloads, mBottomSheetContentController.getSelectedItemIdForTests());
    }

    @SmallTest
    public void testShowContentAndOpenSheet_History()
            throws InterruptedException, TimeoutException {
        int initialContentChangedCount = mObserver.mContentChangedCallbackHelper.getCallCount();
        int initialOpenedCount = mObserver.mOpenedCallbackHelper.getCallCount();

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                HistoryManagerUtils.showHistoryManager(
                        getActivity(), getActivity().getActivityTab());
            }
        });

        mObserver.mContentChangedCallbackHelper.waitForCallback(initialContentChangedCount, 1);
        mObserver.mOpenedCallbackHelper.waitForCallback(initialOpenedCount, 1);

        assertTrue(mBottomSheet.getCurrentSheetContent() instanceof HistorySheetContent);
        assertEquals(
                R.id.action_history, mBottomSheetContentController.getSelectedItemIdForTests());
    }
}
