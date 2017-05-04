// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.widget.bottomsheet;

import android.support.test.filters.MediumTest;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.bookmarks.BookmarkSheetContent;
import org.chromium.chrome.browser.download.DownloadSheetContent;
import org.chromium.chrome.browser.history.HistorySheetContent;
import org.chromium.chrome.browser.suggestions.SuggestionsBottomSheetContent;
import org.chromium.chrome.browser.util.MathUtils;
import org.chromium.chrome.test.BottomSheetTestCaseBase;

import java.util.concurrent.TimeoutException;

/** This class tests the functionality of the {@link BottomSheetObserver}. */
public class BottomSheetObserverTest extends BottomSheetTestCaseBase {
    /**
     * Test that the onSheetClosed event is triggered if the sheet is closed without animation.
     */
    @MediumTest
    public void testCloseEventCalledNoAnimation() throws InterruptedException, TimeoutException {
        setSheetState(BottomSheet.SHEET_STATE_FULL, false);

        CallbackHelper closedCallbackHelper = mObserver.mClosedCallbackHelper;

        int initialOpenedCount = mObserver.mOpenedCallbackHelper.getCallCount();

        int closedCallbackCount = closedCallbackHelper.getCallCount();
        setSheetState(BottomSheet.SHEET_STATE_PEEK, false);
        closedCallbackHelper.waitForCallback(closedCallbackCount, 1);

        assertEquals(initialOpenedCount, mObserver.mOpenedCallbackHelper.getCallCount());
    }

    /**
     * Test that the onSheetClosed event is triggered if the sheet is closed with animation.
     */
    @MediumTest
    public void testCloseEventCalledWithAnimation() throws InterruptedException, TimeoutException {
        setSheetState(BottomSheet.SHEET_STATE_FULL, false);

        CallbackHelper closedCallbackHelper = mObserver.mClosedCallbackHelper;

        int initialOpenedCount = mObserver.mOpenedCallbackHelper.getCallCount();

        int closedCallbackCount = closedCallbackHelper.getCallCount();
        setSheetState(BottomSheet.SHEET_STATE_PEEK, true);
        closedCallbackHelper.waitForCallback(closedCallbackCount, 1);

        assertEquals(initialOpenedCount, mObserver.mOpenedCallbackHelper.getCallCount());
    }

    /**
     * Test that the onSheetOpened event is triggered if the sheet is opened without animation.
     */
    @MediumTest
    public void testOpenedEventCalledNoAnimation() throws InterruptedException, TimeoutException {
        setSheetState(BottomSheet.SHEET_STATE_PEEK, false);

        CallbackHelper openedCallbackHelper = mObserver.mOpenedCallbackHelper;

        int initialClosedCount = mObserver.mClosedCallbackHelper.getCallCount();

        int openedCallbackCount = openedCallbackHelper.getCallCount();
        setSheetState(BottomSheet.SHEET_STATE_FULL, false);
        openedCallbackHelper.waitForCallback(openedCallbackCount, 1);

        assertEquals(initialClosedCount, mObserver.mClosedCallbackHelper.getCallCount());
    }

    /**
     * Test that the onSheetOpened event is triggered if the sheet is opened with animation.
     */
    @MediumTest
    public void testOpenedEventCalledWithAnimation() throws InterruptedException, TimeoutException {
        setSheetState(BottomSheet.SHEET_STATE_PEEK, false);

        CallbackHelper openedCallbackHelper = mObserver.mOpenedCallbackHelper;

        int initialClosedCount = mObserver.mClosedCallbackHelper.getCallCount();

        int openedCallbackCount = openedCallbackHelper.getCallCount();
        setSheetState(BottomSheet.SHEET_STATE_FULL, true);
        openedCallbackHelper.waitForCallback(openedCallbackCount, 1);

        assertEquals(initialClosedCount, mObserver.mClosedCallbackHelper.getCallCount());
    }

    /**
     * Test the onOffsetChanged event.
     */
    @MediumTest
    public void testOffsetChangedEvent() throws InterruptedException, TimeoutException {
        CallbackHelper callbackHelper = mObserver.mOffsetChangedCallbackHelper;

        float peekHeight = mBottomSheet.getPeekRatio() * mBottomSheet.getSheetContainerHeight();
        float fullHeight = mBottomSheet.getFullRatio() * mBottomSheet.getSheetContainerHeight();

        // The sheet's half state is not necessarily 50% of the way to the top.
        float midPeekFull = (peekHeight + fullHeight) / 2f;

        // When in the peeking state, the transition value should be 0.
        int callbackCount = callbackHelper.getCallCount();
        setSheetOffsetFromBottom(peekHeight);
        callbackHelper.waitForCallback(callbackCount, 1);
        assertEquals(0f, mObserver.getLastOffsetChangedValue(), MathUtils.EPSILON);

        // When in the full state, the transition value should be 1.
        callbackCount = callbackHelper.getCallCount();
        setSheetOffsetFromBottom(fullHeight);
        callbackHelper.waitForCallback(callbackCount, 1);
        assertEquals(1f, mObserver.getLastOffsetChangedValue(), MathUtils.EPSILON);

        // Halfway between peek and full should send 0.5.
        callbackCount = callbackHelper.getCallCount();
        setSheetOffsetFromBottom(midPeekFull);
        callbackHelper.waitForCallback(callbackCount, 1);
        assertEquals(0.5f, mObserver.getLastOffsetChangedValue(), MathUtils.EPSILON);
    }

    /**
     * Test the onTransitionPeekToHalf event.
     */
    @MediumTest
    public void testPeekToHalfTransition() throws InterruptedException, TimeoutException {
        CallbackHelper callbackHelper = mObserver.mPeekToHalfCallbackHelper;

        float peekHeight = mBottomSheet.getPeekRatio() * mBottomSheet.getSheetContainerHeight();
        float halfHeight = mBottomSheet.getHalfRatio() * mBottomSheet.getSheetContainerHeight();
        float fullHeight = mBottomSheet.getFullRatio() * mBottomSheet.getSheetContainerHeight();

        float midPeekHalf = (peekHeight + halfHeight) / 2f;
        float midHalfFull = (halfHeight + fullHeight) / 2f;

        // When in the peeking state, the transition value should be 0.
        int callbackCount = callbackHelper.getCallCount();
        setSheetOffsetFromBottom(peekHeight);
        callbackHelper.waitForCallback(callbackCount, 1);
        assertEquals(0f, mObserver.getLastPeekToHalfValue(), MathUtils.EPSILON);

        // When in between peek and half states, the transition value should be 0.5.
        callbackCount = callbackHelper.getCallCount();
        setSheetOffsetFromBottom(midPeekHalf);
        callbackHelper.waitForCallback(callbackCount, 1);
        assertEquals(0.5f, mObserver.getLastPeekToHalfValue(), MathUtils.EPSILON);

        // After jumping to the full state (skipping the half state), the event should have
        // triggered once more with a max value of 1.
        callbackCount = callbackHelper.getCallCount();
        setSheetOffsetFromBottom(fullHeight);
        callbackHelper.waitForCallback(callbackCount, 1);
        assertEquals(1f, mObserver.getLastPeekToHalfValue(), MathUtils.EPSILON);

        // Moving from full to somewhere between half and full should not trigger the event.
        callbackCount = callbackHelper.getCallCount();
        setSheetOffsetFromBottom(midHalfFull);
        assertEquals(callbackCount, callbackHelper.getCallCount());

        // Reset the sheet to be between peek and half states.
        callbackCount = callbackHelper.getCallCount();
        setSheetOffsetFromBottom(midPeekHalf);
        callbackHelper.waitForCallback(callbackCount, 1);
        assertEquals(0.5f, mObserver.getLastPeekToHalfValue(), MathUtils.EPSILON);

        // At the half state the event should send 1.
        callbackCount = callbackHelper.getCallCount();
        setSheetOffsetFromBottom(halfHeight);
        callbackHelper.waitForCallback(callbackCount, 1);
        assertEquals(1f, mObserver.getLastPeekToHalfValue(), MathUtils.EPSILON);
    }

    /**
     * Test the onSheetContentChanged event.
     */
    @MediumTest
    public void testSheetContentChanged() throws InterruptedException, TimeoutException {
        CallbackHelper callbackHelper = mObserver.mContentChangedCallbackHelper;

        int callbackCount = callbackHelper.getCallCount();
        selectBottomSheetContent(R.id.action_bookmarks);
        callbackHelper.waitForCallback(callbackCount, 1);
        assertTrue(getBottomSheetContent() instanceof BookmarkSheetContent);

        callbackCount++;
        selectBottomSheetContent(R.id.action_history);
        callbackHelper.waitForCallback(callbackCount, 1);
        assertTrue(getBottomSheetContent() instanceof HistorySheetContent);

        callbackCount++;
        selectBottomSheetContent(R.id.action_downloads);
        callbackHelper.waitForCallback(callbackCount, 1);
        assertTrue(getBottomSheetContent() instanceof DownloadSheetContent);

        callbackCount++;
        selectBottomSheetContent(R.id.action_home);
        callbackHelper.waitForCallback(callbackCount, 1);
        assertTrue(getBottomSheetContent() instanceof SuggestionsBottomSheetContent);
    }
}
