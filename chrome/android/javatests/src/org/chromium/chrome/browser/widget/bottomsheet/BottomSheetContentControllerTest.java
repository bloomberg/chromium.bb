// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.widget.bottomsheet;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import android.support.test.filters.SmallTest;
import android.view.View;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.bookmarks.BookmarkSheetContent;
import org.chromium.chrome.browser.bookmarks.BookmarkUtils;
import org.chromium.chrome.browser.download.DownloadSheetContent;
import org.chromium.chrome.browser.download.DownloadUtils;
import org.chromium.chrome.browser.history.HistoryManagerUtils;
import org.chromium.chrome.browser.history.HistorySheetContent;
import org.chromium.chrome.test.BottomSheetTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.ui.test.util.UiRestriction;

import java.util.concurrent.TimeoutException;

/** This class tests the functionality of the {@link BottomSheetContentController}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Restriction(UiRestriction.RESTRICTION_TYPE_PHONE) // ChromeHome is only enabled on phones
public class BottomSheetContentControllerTest {
    private BottomSheetTestRule.Observer mObserver;
    private BottomSheet mBottomSheet;
    private BottomSheetContentController mBottomSheetContentController;

    @Rule
    public BottomSheetTestRule mBottomSheetTestRule = new BottomSheetTestRule();

    @Before
    public void setUp() throws Exception {
        mBottomSheetTestRule.startMainActivityOnBlankPage();
        mBottomSheetTestRule.setSheetState(BottomSheet.SHEET_STATE_PEEK, false);
        mObserver = mBottomSheetTestRule.getObserver();
        mBottomSheet = mBottomSheetTestRule.getBottomSheet();
        mBottomSheetContentController = mBottomSheetTestRule.getBottomSheetContentController();
    }

    @Test
    @SmallTest
    @CommandLineFlags.Add({
            "disable-features=" + ChromeFeatureList.CHROME_HOME_DROP_ALL_BUT_FIRST_THUMBNAIL,
            "enable-features=" + ChromeFeatureList.CHROME_HOME_DESTROY_SUGGESTIONS})
    public void testSelectContent_DestroyContents() throws InterruptedException, TimeoutException {
        int contentChangedCount = mObserver.mContentChangedCallbackHelper.getCallCount();
        int openedCount = mObserver.mOpenedCallbackHelper.getCallCount();
        int closedCount = mObserver.mClosedCallbackHelper.getCallCount();

        mBottomSheetTestRule.setSheetState(BottomSheet.SHEET_STATE_HALF, false);
        mObserver.mOpenedCallbackHelper.waitForCallback(openedCount++, 1);
        mObserver.mContentChangedCallbackHelper.waitForCallback(contentChangedCount++, 1);
        assertEquals(closedCount, mObserver.mClosedCallbackHelper.getCallCount());

        mBottomSheetTestRule.selectBottomSheetContent(R.id.action_history);
        mObserver.mContentChangedCallbackHelper.waitForCallback(contentChangedCount++, 1);
        assertEquals(openedCount, mObserver.mOpenedCallbackHelper.getCallCount());
        assertEquals(closedCount, mObserver.mClosedCallbackHelper.getCallCount());
        assertTrue(mBottomSheet.getCurrentSheetContent() instanceof HistorySheetContent);
        assertEquals(
                R.id.action_history, mBottomSheetContentController.getSelectedItemIdForTests());
        assertEquals(View.INVISIBLE, mBottomSheet.getDefaultToolbarView().getVisibility());

        mBottomSheetTestRule.setSheetState(BottomSheet.SHEET_STATE_PEEK, false);
        mObserver.mClosedCallbackHelper.waitForCallback(closedCount, 1);
        mObserver.mContentChangedCallbackHelper.waitForCallback(contentChangedCount++, 1);
        assertEquals(openedCount, mObserver.mOpenedCallbackHelper.getCallCount());
        assertEquals(null, mBottomSheet.getCurrentSheetContent());
        assertEquals(0, mBottomSheetContentController.getSelectedItemIdForTests());
        assertEquals(View.VISIBLE, mBottomSheet.getDefaultToolbarView().getVisibility());
    }

    @Test
    @SmallTest
    @CommandLineFlags.Add({
            "enable-features=" + ChromeFeatureList.CHROME_HOME_DROP_ALL_BUT_FIRST_THUMBNAIL,
            "disable-features=" + ChromeFeatureList.CHROME_HOME_DESTROY_SUGGESTIONS})
    public void testSelectContent_DropAllButFirstThumbnail()
            throws InterruptedException, TimeoutException {
        int contentChangedCount = mObserver.mContentChangedCallbackHelper.getCallCount();
        int openedCount = mObserver.mOpenedCallbackHelper.getCallCount();
        int closedCount = mObserver.mClosedCallbackHelper.getCallCount();

        mBottomSheetTestRule.setSheetState(BottomSheet.SHEET_STATE_HALF, false);
        mObserver.mOpenedCallbackHelper.waitForCallback(openedCount++, 1);
        assertEquals(closedCount, mObserver.mClosedCallbackHelper.getCallCount());

        mBottomSheetTestRule.selectBottomSheetContent(R.id.action_history);
        mObserver.mContentChangedCallbackHelper.waitForCallback(contentChangedCount++, 1);
        assertEquals(openedCount, mObserver.mOpenedCallbackHelper.getCallCount());
        assertEquals(closedCount, mObserver.mClosedCallbackHelper.getCallCount());
        assertTrue(mBottomSheet.getCurrentSheetContent() instanceof HistorySheetContent);
        assertEquals(
                R.id.action_history, mBottomSheetContentController.getSelectedItemIdForTests());
        assertEquals(View.INVISIBLE, mBottomSheet.getDefaultToolbarView().getVisibility());

        mBottomSheetTestRule.setSheetState(BottomSheet.SHEET_STATE_PEEK, false);
        mObserver.mClosedCallbackHelper.waitForCallback(closedCount, 1);
        mObserver.mContentChangedCallbackHelper.waitForCallback(contentChangedCount++, 1);
        assertEquals(openedCount, mObserver.mOpenedCallbackHelper.getCallCount());
        assertEquals(View.VISIBLE, mBottomSheet.getDefaultToolbarView().getVisibility());
    }

    @Test
    @SmallTest
    public void testShowContentAndOpenSheet_Bookmarks()
            throws InterruptedException, TimeoutException {
        int initialContentChangedCount = mObserver.mContentChangedCallbackHelper.getCallCount();
        int initialOpenedCount = mObserver.mOpenedCallbackHelper.getCallCount();

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                BookmarkUtils.showBookmarkManager(mBottomSheetTestRule.getActivity());
            }
        });

        mObserver.mContentChangedCallbackHelper.waitForCallback(initialContentChangedCount, 1);
        mObserver.mOpenedCallbackHelper.waitForCallback(initialOpenedCount, 1);

        assertTrue(mBottomSheet.getCurrentSheetContent() instanceof BookmarkSheetContent);
        assertEquals(
                R.id.action_bookmarks, mBottomSheetContentController.getSelectedItemIdForTests());
    }

    @Test
    @SmallTest
    public void testShowContentAndOpenSheet_Downloads()
            throws InterruptedException, TimeoutException {
        int initialContentChangedCount = mObserver.mContentChangedCallbackHelper.getCallCount();
        int initialOpenedCount = mObserver.mOpenedCallbackHelper.getCallCount();

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                ChromeTabbedActivity activity = mBottomSheetTestRule.getActivity();
                DownloadUtils.showDownloadManager(activity, activity.getActivityTab());
            }
        });

        mObserver.mContentChangedCallbackHelper.waitForCallback(initialContentChangedCount, 1);
        mObserver.mOpenedCallbackHelper.waitForCallback(initialOpenedCount, 1);

        assertTrue(mBottomSheet.getCurrentSheetContent() instanceof DownloadSheetContent);
        assertEquals(
                R.id.action_downloads, mBottomSheetContentController.getSelectedItemIdForTests());
    }

    @Test
    @SmallTest
    public void testShowContentAndOpenSheet_History()
            throws InterruptedException, TimeoutException {
        int initialContentChangedCount = mObserver.mContentChangedCallbackHelper.getCallCount();
        int initialOpenedCount = mObserver.mOpenedCallbackHelper.getCallCount();

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                ChromeTabbedActivity activity = mBottomSheetTestRule.getActivity();
                HistoryManagerUtils.showHistoryManager(activity, activity.getActivityTab());
            }
        });

        mObserver.mContentChangedCallbackHelper.waitForCallback(initialContentChangedCount, 1);
        mObserver.mOpenedCallbackHelper.waitForCallback(initialOpenedCount, 1);

        assertTrue(mBottomSheet.getCurrentSheetContent() instanceof HistorySheetContent);
        assertEquals(
                R.id.action_history, mBottomSheetContentController.getSelectedItemIdForTests());
    }
}
