// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.widget;

import android.support.test.filters.MediumTest;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.chrome.browser.util.MathUtils;
import org.chromium.chrome.test.BottomSheetTestCaseBase;

import java.util.concurrent.TimeoutException;

/** This class tests the functionality of the {@link BottomSheetObserver}. */
public class BottomSheetObserverTest extends BottomSheetTestCaseBase {
    /** A handle to the sheet's observer. */
    private TestBottomSheetObserver mObserver;

    /** An observer used to record events that occur with respect to the bottom sheet. */
    private static class TestBottomSheetObserver implements BottomSheetObserver {
        /** A {@link CallbackHelper} that can wait for the bottom sheet to be closed. */
        private final CallbackHelper mClosedCallbackHelper = new CallbackHelper();

        /** A {@link CallbackHelper} that can wait for the bottom sheet to be opened. */
        private final CallbackHelper mOpenedCallbackHelper = new CallbackHelper();

        /** A {@link CallbackHelper} that can wait for the onTransitionPeekToHalf event. */
        private final CallbackHelper mPeekToHalfCallbackHelper = new CallbackHelper();

        /** A {@link CallbackHelper} that can wait for the onOffsetChanged event. */
        private final CallbackHelper mOffsetChangedCallbackHelper = new CallbackHelper();

        /** The last value that the onTransitionPeekToHalf event sent. */
        private float mLastPeekToHalfValue;

        /** The last value that the onOffsetChanged event sent. */
        private float mLastOffsetChangedValue;

        @Override
        public void onTransitionPeekToHalf(float fraction) {
            mLastPeekToHalfValue = fraction;
            mPeekToHalfCallbackHelper.notifyCalled();
        }

        @Override
        public void onSheetOffsetChanged(float heightFraction) {
            mLastOffsetChangedValue = heightFraction;
            mOffsetChangedCallbackHelper.notifyCalled();
        }

        @Override
        public void onSheetOpened() {
            mOpenedCallbackHelper.notifyCalled();
        }

        @Override
        public void onSheetClosed() {
            mClosedCallbackHelper.notifyCalled();
        }

        @Override
        public void onLoadUrl(String url) {}
    }

    @Override
    protected void setUp() throws Exception {
        super.setUp();

        mObserver = new TestBottomSheetObserver();
        mBottomSheet.addObserver(mObserver);
    }

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
        assertEquals(0f, mObserver.mLastOffsetChangedValue, MathUtils.EPSILON);

        // When in the full state, the transition value should be 1.
        callbackCount = callbackHelper.getCallCount();
        setSheetOffsetFromBottom(fullHeight);
        callbackHelper.waitForCallback(callbackCount, 1);
        assertEquals(1f, mObserver.mLastOffsetChangedValue, MathUtils.EPSILON);

        // Halfway between peek and full should send 0.5.
        callbackCount = callbackHelper.getCallCount();
        setSheetOffsetFromBottom(midPeekFull);
        callbackHelper.waitForCallback(callbackCount, 1);
        assertEquals(0.5f, mObserver.mLastOffsetChangedValue, MathUtils.EPSILON);
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
        assertEquals(0f, mObserver.mLastPeekToHalfValue, MathUtils.EPSILON);

        // When in between peek and half states, the transition value should be 0.5.
        callbackCount = callbackHelper.getCallCount();
        setSheetOffsetFromBottom(midPeekHalf);
        callbackHelper.waitForCallback(callbackCount, 1);
        assertEquals(0.5f, mObserver.mLastPeekToHalfValue, MathUtils.EPSILON);

        // After jumping to the full state (skipping the half state), the event should have
        // triggered once more with a max value of 1.
        callbackCount = callbackHelper.getCallCount();
        setSheetOffsetFromBottom(fullHeight);
        callbackHelper.waitForCallback(callbackCount, 1);
        assertEquals(1f, mObserver.mLastPeekToHalfValue, MathUtils.EPSILON);

        // Moving from full to somewhere between half and full should not trigger the event.
        callbackCount = callbackHelper.getCallCount();
        setSheetOffsetFromBottom(midHalfFull);
        assertEquals(callbackCount, callbackHelper.getCallCount());

        // Reset the sheet to be between peek and half states.
        callbackCount = callbackHelper.getCallCount();
        setSheetOffsetFromBottom(midPeekHalf);
        callbackHelper.waitForCallback(callbackCount, 1);
        assertEquals(0.5f, mObserver.mLastPeekToHalfValue, MathUtils.EPSILON);

        // At the half state the event should send 1.
        callbackCount = callbackHelper.getCallCount();
        setSheetOffsetFromBottom(halfHeight);
        callbackHelper.waitForCallback(callbackCount, 1);
        assertEquals(1f, mObserver.mLastPeekToHalfValue, MathUtils.EPSILON);
    }
}
