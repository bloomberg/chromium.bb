// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.os.SystemClock;
import android.test.InstrumentationTestCase;
import android.test.suitebuilder.annotation.LargeTest;
import android.test.suitebuilder.annotation.SmallTest;
import android.view.MotionEvent;
import android.view.ViewConfiguration;

import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.ScalableTimeout;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

/**
 * Test suite for LongPressDetector.
 */
public class LongPressDetectorTest extends InstrumentationTestCase {
    private static final int FAKE_COORD_X = 42;
    private static final int FAKE_COORD_Y = 24;
    private LongPressDetector mLongPressDetector;

    private MotionEvent motionEvent(int action, long downTime, long eventTime) {
        return MotionEvent.obtain(downTime, eventTime, action, FAKE_COORD_X, FAKE_COORD_Y, 0);
    }

    @Override
    public void setUp() {
        mLongPressDetector = new LongPressDetector(getInstrumentation().getTargetContext(), null);
    }

    /**
     * Verify a DOWN without a corresponding UP will have a pending DOWN.
     *
     * @throws Exception
     */
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testGestureSimpleLongPress() throws Exception {
        final long downTime = SystemClock.uptimeMillis();
        final long eventTime = SystemClock.uptimeMillis();

        MotionEvent event = motionEvent(MotionEvent.ACTION_DOWN, downTime, eventTime);
        mLongPressDetector.startLongPressTimerIfNeeded(event);

        assertTrue("Should have a pending LONG_PRESS", mLongPressDetector.hasPendingMessage());
    }

    private void gestureNoLongPressTestHelper(int cancelActionType) throws Exception {
        final long downTime = SystemClock.uptimeMillis();
        final long eventTime = SystemClock.uptimeMillis();

        MotionEvent event = motionEvent(MotionEvent.ACTION_DOWN, downTime, eventTime);
        mLongPressDetector.startLongPressTimerIfNeeded(event);

        assertTrue("Should have a pending LONG_PRESS", mLongPressDetector.hasPendingMessage());

        event = motionEvent(cancelActionType, downTime, eventTime + 10);
        mLongPressDetector.cancelLongPressIfNeeded(event);
        assertTrue("Should not have a pending LONG_PRESS", !mLongPressDetector.hasPendingMessage());
    }

    /**
     * Verify a DOWN with a corresponding UP will not have a pending Gesture.
     *
     * @throws Exception
     */
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testGestureNoLongPressOnUp() throws Exception {
        gestureNoLongPressTestHelper(MotionEvent.ACTION_UP);
    }

    /**
     * Verify a DOWN with a corresponding CANCEL will not have a pending Gesture.
     *
     * @throws Exception
     */
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testGestureNoLongPressOnCancel() throws Exception {
        gestureNoLongPressTestHelper(MotionEvent.ACTION_CANCEL);
    }

    /**
     * Verify that a DOWN followed by an UP after the long press timer would
     * detect a long press (that is, the UP will not trigger a tap or cancel the
     * long press).
     *
     * @throws Exception
     */
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testGestureLongWithDelayedUp() throws Exception {
        final long downTime = SystemClock.uptimeMillis();
        final long eventTime = SystemClock.uptimeMillis();

        MotionEvent event = motionEvent(MotionEvent.ACTION_DOWN, downTime, eventTime);
        mLongPressDetector.startLongPressTimerIfNeeded(event);

        assertTrue("Should have a pending LONG_PRESS", mLongPressDetector.hasPendingMessage());

        // Event time must be larger than LONG_PRESS_TIMEOUT.
        event = motionEvent(MotionEvent.ACTION_UP, downTime, eventTime + 1000);
        mLongPressDetector.cancelLongPressIfNeeded(event);
        assertTrue("Should still have a pending gesture", mLongPressDetector.hasPendingMessage());
    }

    /**
     * Verify that the touch move threshold (slop) is working for events offered to native.
     */
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testConfirmOfferMoveEventToNative() {
        final int slop = ViewConfiguration.get(getInstrumentation().getTargetContext())
                .getScaledTouchSlop();

        long eventTime = SystemClock.uptimeMillis();
        final MotionEvent downEvent = MotionEvent.obtain(
                eventTime, eventTime, MotionEvent.ACTION_DOWN, FAKE_COORD_X, FAKE_COORD_Y, 0);

        // Test a small move, where confirmOfferMoveEventToNative should return false.
        mLongPressDetector.onOfferTouchEventToJavaScript(downEvent);
        eventTime = SystemClock.uptimeMillis();
        final MotionEvent smallMove = MotionEvent.obtain(
                eventTime, eventTime, MotionEvent.ACTION_MOVE,
                FAKE_COORD_X + slop / 2, FAKE_COORD_Y + slop / 2, 0);
        assertFalse(mLongPressDetector.confirmOfferMoveEventToJavaScript(smallMove));

        // Test a big move, where confirmOfferMoveEventToNative should return true.
        mLongPressDetector.onOfferTouchEventToJavaScript(downEvent);
        eventTime = SystemClock.uptimeMillis();
        final MotionEvent largeMove = MotionEvent.obtain(
                eventTime, eventTime, MotionEvent.ACTION_MOVE,
                FAKE_COORD_X + slop * 2, FAKE_COORD_Y + slop * 2, 0);
        assertTrue(mLongPressDetector.confirmOfferMoveEventToJavaScript(largeMove));
    }

    /**
     * This is an example of a large test running delayed messages.
     * It exercises GestureDetector itself, and expects the onLongPress to be called.
     * Note that GestureDetector creates a Handler and posts message to it for detecting
     * long press. It needs to be created on the Main thread.
     *
     * @throws Exception
     */
    @LargeTest
    @Feature({"AndroidWebView"})
    public void testGestureLongPressDetected() throws Exception {
        final CountDownLatch longPressCalled = new CountDownLatch(1);
        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                LongPressDetector longPressDetector = new LongPressDetector(
                        getInstrumentation().getTargetContext(),
                        new LongPressDetector.LongPressDelegate() {
                            @Override
                            public void onLongPress(MotionEvent event) {
                                longPressCalled.countDown();
                            }
                });

                final long downTime = SystemClock.uptimeMillis();
                final long eventTime = SystemClock.uptimeMillis();
                MotionEvent event = motionEvent(MotionEvent.ACTION_DOWN, downTime, eventTime);
                longPressDetector.startLongPressTimerIfNeeded(event);
            }
        });
        assertTrue(longPressCalled.await(
                ScalableTimeout.ScaleTimeout(1000), TimeUnit.MILLISECONDS));
    }
}
