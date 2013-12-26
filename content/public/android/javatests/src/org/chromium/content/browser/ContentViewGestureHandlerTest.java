// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.content.Context;
import android.os.Bundle;
import android.os.SystemClock;
import android.test.InstrumentationTestCase;
import android.test.suitebuilder.annotation.SmallTest;
import android.util.Log;
import android.view.MotionEvent;
import android.view.MotionEvent.PointerCoords;
import android.view.MotionEvent.PointerProperties;
import android.view.ViewConfiguration;

import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.ScalableTimeout;
import org.chromium.content.browser.ContentViewGestureHandler.MotionEventDelegate;
import org.chromium.content.browser.third_party.GestureDetector;

import java.util.ArrayList;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

/**
 * Test suite for ContentViewGestureHandler.
 */
public class ContentViewGestureHandlerTest extends InstrumentationTestCase {
    private static final int FAKE_COORD_X = 42;
    private static final int FAKE_COORD_Y = 24;

    private static final String TAG = "ContentViewGestureHandler";
    private MockListener mMockListener;
    private MockMotionEventDelegate mMockMotionEventDelegate;
    private MockGestureDetector mMockGestureDetector;
    private MockZoomManager mMockZoomManager;
    private ContentViewGestureHandler mGestureHandler;
    private LongPressDetector mLongPressDetector;

    static class MockListener extends GestureDetector.SimpleOnGestureListener {
        MotionEvent mLastLongPress;
        MotionEvent mLastShowPress;
        MotionEvent mLastSingleTap;
        MotionEvent mLastFling1;
        CountDownLatch mLongPressCalled;
        CountDownLatch mShowPressCalled;

        public MockListener() {
            mLongPressCalled = new CountDownLatch(1);
            mShowPressCalled = new CountDownLatch(1);
        }

        @Override
        public void onLongPress(MotionEvent e) {
            mLastLongPress = MotionEvent.obtain(e);
            mLongPressCalled.countDown();
        }

        @Override
        public void onShowPress(MotionEvent e) {
            mLastShowPress = MotionEvent.obtain(e);
            mShowPressCalled.countDown();
            Log.e("Overscroll", "OnShowPress");
        }

        @Override
        public boolean onSingleTapConfirmed(MotionEvent e) {
            mLastSingleTap = e;
            return true;
        }

        @Override
        public boolean onSingleTapUp(MotionEvent e) {
            mLastSingleTap = e;
            return true;
        }

        @Override
        public boolean onScroll(MotionEvent e1, MotionEvent e2, float distanceX, float distanceY) {
            return true;
        }

        @Override
        public boolean onFling(MotionEvent e1, MotionEvent e2, float velocityX, float velocityY) {
            mLastFling1 = e1;
            return true;
        }
    }

    static class MockGestureDetector extends GestureDetector {
        MotionEvent mLastEvent;
        public MockGestureDetector(Context context, OnGestureListener listener) {
            super(context, listener);
        }

        @Override
        public boolean onTouchEvent(MotionEvent ev) {
            mLastEvent = MotionEvent.obtain(ev);
            return super.onTouchEvent(ev);
        }
    }

    static class MockMotionEventDelegate implements MotionEventDelegate {
        private ContentViewGestureHandler mSynchronousConfirmTarget;
        private int mSynchronousConfirmAckResult;

        public int mLastTouchAction;
        public int mLastGestureType;
        public int mTotalSentGestureCount;

        @Override
        public boolean sendTouchEvent(long timeMs, int action, TouchPoint[] pts) {
            mLastTouchAction = action;
            if (mSynchronousConfirmTarget != null) {
                mSynchronousConfirmTarget.confirmTouchEvent(mSynchronousConfirmAckResult);
            }
            return true;
        }

        @Override
        public boolean sendGesture(int type, long timeMs, int x, int y, Bundle extraParams) {
            Log.i(TAG,"Gesture event received with type id " + type);
            mLastGestureType = type;
            mTotalSentGestureCount++;
            return true;
        }

        @Override
        public void sendSingleTapUMA(int type) {
            // Not implemented.
        }

        @Override
        public void sendActionAfterDoubleTapUMA(int type,
                boolean clickDelayEnabled) {
            // Not implemented.
        }

        @Override
        public void invokeZoomPicker() {
            // Not implemented.
        }

        public void enableSynchronousConfirmTouchEvent(
                ContentViewGestureHandler handler, int ackResult) {
            mSynchronousConfirmTarget = handler;
            mSynchronousConfirmAckResult = ackResult;
        }

        public void disableSynchronousConfirmTouchEvent() {
            mSynchronousConfirmTarget = null;
        }
    }

    static class MockZoomManager extends ZoomManager {
        private ContentViewGestureHandler mHandlerForMoveEvents;

        MockZoomManager(Context context, ContentViewCore contentViewCore) {
            super(context, contentViewCore);
        }

        public void pinchOnMoveEvents(ContentViewGestureHandler handler) {
            mHandlerForMoveEvents = handler;
        }

        @Override
        public boolean processTouchEvent(MotionEvent event) {
            if (event.getActionMasked() == MotionEvent.ACTION_MOVE
                    && mHandlerForMoveEvents != null) {
                mHandlerForMoveEvents.pinchBy(event.getEventTime(), 1, 1, 1.1f);
                return true;
            }
            return false;
        }
    }

    private static MotionEvent motionEvent(int action, long downTime, long eventTime) {
        return MotionEvent.obtain(downTime, eventTime, action, FAKE_COORD_X, FAKE_COORD_Y, 0);
    }

    @Override
    public void setUp() {
        mMockListener = new MockListener();
        mMockGestureDetector = new MockGestureDetector(
                getInstrumentation().getTargetContext(), mMockListener);
        mMockMotionEventDelegate = new MockMotionEventDelegate();
        mMockZoomManager = new MockZoomManager(getInstrumentation().getTargetContext(), null);
        mGestureHandler = new ContentViewGestureHandler(
                getInstrumentation().getTargetContext(), mMockMotionEventDelegate,
                mMockZoomManager);
        mLongPressDetector = new LongPressDetector(
                getInstrumentation().getTargetContext(), mGestureHandler);
        mGestureHandler.setTestDependencies(
                mLongPressDetector, mMockGestureDetector, mMockListener);
        TouchPoint.initializeConstantsForTesting();
    }

    /**
     * Verify that a DOWN followed shortly by an UP will trigger a single tap.
     *
     * @throws Exception
     */
    @SmallTest
    @Feature({"Gestures"})
    public void testGestureSingleClick() throws Exception {
        final long downTime = SystemClock.uptimeMillis();
        final long eventTime = SystemClock.uptimeMillis();

        MotionEvent event = motionEvent(MotionEvent.ACTION_DOWN, downTime, downTime);

        assertFalse(mGestureHandler.onTouchEvent(event));
        assertTrue("Should have a pending gesture", mMockGestureDetector.mLastEvent != null);
        assertTrue("Should have a pending LONG_PRESS", mLongPressDetector.hasPendingMessage());

        event = motionEvent(MotionEvent.ACTION_UP, downTime, eventTime + 10);
        mLongPressDetector.cancelLongPressIfNeeded(event);
        assertTrue("Should not have a pending LONG_PRESS", !mLongPressDetector.hasPendingMessage());
        assertTrue(mGestureHandler.onTouchEvent(event));
        // Synchronous, no need to wait.
        assertTrue("Should have a single tap", mMockListener.mLastSingleTap != null);
    }

    /**
     * Verify that when a touch event handler is registered the touch events are queued
     * and sent in order.
     * @throws Exception
     */
    @SmallTest
    @Feature({"Gestures"})
    public void testFlingOnTouchHandler() throws Exception {
        final long downTime = SystemClock.uptimeMillis();
        final long eventTime = SystemClock.uptimeMillis();

        MotionEvent event = motionEvent(MotionEvent.ACTION_DOWN, downTime, downTime);

        mGestureHandler.hasTouchEventHandlers(true);

        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals(1, mGestureHandler.getNumberOfPendingMotionEventsForTesting());
        assertTrue("Should not have a pending gesture", mMockGestureDetector.mLastEvent == null);
        assertFalse("Should not have a pending LONG_PRESS", mLongPressDetector.hasPendingMessage());

        event = MotionEvent.obtain(
                downTime, eventTime + 5, MotionEvent.ACTION_MOVE,
                FAKE_COORD_X * 5, FAKE_COORD_Y * 5, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals(2, mGestureHandler.getNumberOfPendingMotionEventsForTesting());

        event = MotionEvent.obtain(
                downTime, eventTime + 10, MotionEvent.ACTION_MOVE,
                FAKE_COORD_X * 10, FAKE_COORD_Y * 10, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals("We should have coalesced move events into one"
                , 2, mGestureHandler.getNumberOfPendingMotionEventsForTesting());

        event = MotionEvent.obtain(
                downTime, eventTime + 15, MotionEvent.ACTION_UP,
                FAKE_COORD_X * 10, FAKE_COORD_Y * 10, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals(3, mGestureHandler.getNumberOfPendingMotionEventsForTesting());

        mGestureHandler.confirmTouchEvent(
                ContentViewGestureHandler.INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
        assertEquals(2, mGestureHandler.getNumberOfPendingMotionEventsForTesting());
        assertEquals(MotionEvent.ACTION_MOVE,
                mGestureHandler.peekFirstInPendingMotionEventsForTesting().getActionMasked());
        assertFalse("Pending LONG_PRESS should have been canceled",
                mLongPressDetector.hasPendingMessage());

        mGestureHandler.confirmTouchEvent(ContentViewGestureHandler.INPUT_EVENT_ACK_STATE_CONSUMED);
        assertEquals(1, mGestureHandler.getNumberOfPendingMotionEventsForTesting());
        assertEquals(MotionEvent.ACTION_UP,
                mGestureHandler.peekFirstInPendingMotionEventsForTesting().getActionMasked());

        mGestureHandler.confirmTouchEvent(ContentViewGestureHandler.INPUT_EVENT_ACK_STATE_CONSUMED);
        assertEquals(0, mGestureHandler.getNumberOfPendingMotionEventsForTesting());

        // Synchronous, no need to wait.
        assertTrue("Should not have a fling", mMockListener.mLastFling1 == null);
        assertTrue("Should not have a long press", mMockListener.mLastLongPress == null);
    }

    /**
     * Verify that after a touch event handlers starts handling a gesture, even though some event
     * in the middle of the gesture returns with NOT_CONSUMED, we don't send that to the gesture
     * detector to avoid falling to a faulty state.
     * @throws Exception
     */
    @SmallTest
    @Feature({"Gestures"})
    public void testFlingOnTouchHandlerWithOneEventNotConsumed() throws Exception {
        final long downTime = SystemClock.uptimeMillis();
        final long eventTime = SystemClock.uptimeMillis();

        MotionEvent event = motionEvent(MotionEvent.ACTION_DOWN, downTime, downTime);

        mGestureHandler.hasTouchEventHandlers(true);

        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals(1, mGestureHandler.getNumberOfPendingMotionEventsForTesting());
        assertTrue("Should not have a pending gesture", mMockGestureDetector.mLastEvent == null);
        assertFalse("Should not have a pending LONG_PRESS", mLongPressDetector.hasPendingMessage());

        event = MotionEvent.obtain(
                downTime, eventTime + 5, MotionEvent.ACTION_MOVE,
                FAKE_COORD_X * 5, FAKE_COORD_Y * 5, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals(2, mGestureHandler.getNumberOfPendingMotionEventsForTesting());

        event = MotionEvent.obtain(
                downTime, eventTime + 10, MotionEvent.ACTION_MOVE,
                FAKE_COORD_X * 10, FAKE_COORD_Y * 10, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals("We should have coalesced move events into one"
                , 2, mGestureHandler.getNumberOfPendingMotionEventsForTesting());

        event = MotionEvent.obtain(
                downTime, eventTime + 15, MotionEvent.ACTION_UP,
                FAKE_COORD_X * 10, FAKE_COORD_Y * 10, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals(3, mGestureHandler.getNumberOfPendingMotionEventsForTesting());

        mGestureHandler.confirmTouchEvent(
                ContentViewGestureHandler.INPUT_EVENT_ACK_STATE_CONSUMED);
        assertEquals(2, mGestureHandler.getNumberOfPendingMotionEventsForTesting());
        assertEquals(MotionEvent.ACTION_MOVE,
                mGestureHandler.peekFirstInPendingMotionEventsForTesting().getActionMasked());

        mGestureHandler.confirmTouchEvent(
                ContentViewGestureHandler.INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
        assertEquals(1, mGestureHandler.getNumberOfPendingMotionEventsForTesting());
        assertEquals(MotionEvent.ACTION_UP,
                mGestureHandler.peekFirstInPendingMotionEventsForTesting().getActionMasked());
        assertTrue("Even though the last event was not consumed by JavaScript," +
                "it shouldn't have been sent to the Gesture Detector",
                        mMockGestureDetector.mLastEvent == null);

        mGestureHandler.confirmTouchEvent(ContentViewGestureHandler.INPUT_EVENT_ACK_STATE_CONSUMED);
        assertEquals(0, mGestureHandler.getNumberOfPendingMotionEventsForTesting());

        // Synchronous, no need to wait.
        assertTrue("Should not have a fling", mMockListener.mLastFling1 == null);
        assertTrue("Should not have a long press", mMockListener.mLastLongPress == null);
    }

    /**
     * Verify that when a registered touch event handler return NO_CONSUMER_EXISTS for down event
     * all queue is drained until next down.
     * @throws Exception
     */
    @SmallTest
    @Feature({"Gestures"})
    public void testDrainWithFlingAndClickOutofTouchHandler() throws Exception {
        final long downTime = SystemClock.uptimeMillis();
        final long eventTime = SystemClock.uptimeMillis();

        mGestureHandler = new ContentViewGestureHandler(
                getInstrumentation().getTargetContext(), new MockMotionEventDelegate(),
                mMockZoomManager);
        mLongPressDetector = new LongPressDetector(
                getInstrumentation().getTargetContext(), mGestureHandler);

        mGestureHandler.hasTouchEventHandlers(true);

        MotionEvent event = motionEvent(MotionEvent.ACTION_DOWN, downTime, downTime);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals(1, mGestureHandler.getNumberOfPendingMotionEventsForTesting());
        assertFalse("Should not have a pending LONG_PRESS", mLongPressDetector.hasPendingMessage());

        event = MotionEvent.obtain(
                downTime, eventTime + 5, MotionEvent.ACTION_MOVE,
                FAKE_COORD_X * 5, FAKE_COORD_Y * 5, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals(2, mGestureHandler.getNumberOfPendingMotionEventsForTesting());

        event = MotionEvent.obtain(
                downTime, eventTime + 10, MotionEvent.ACTION_MOVE,
                FAKE_COORD_X * 10, FAKE_COORD_Y * 10, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals(2, mGestureHandler.getNumberOfPendingMotionEventsForTesting());

        event = MotionEvent.obtain(
                downTime, eventTime + 15, MotionEvent.ACTION_UP,
                FAKE_COORD_X * 10, FAKE_COORD_Y * 10, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals(3, mGestureHandler.getNumberOfPendingMotionEventsForTesting());

        event = motionEvent(MotionEvent.ACTION_DOWN, eventTime + 20, eventTime + 20);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals(4, mGestureHandler.getNumberOfPendingMotionEventsForTesting());

        event = MotionEvent.obtain(
                downTime, eventTime + 20, MotionEvent.ACTION_UP,
                FAKE_COORD_X, FAKE_COORD_Y, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals(5, mGestureHandler.getNumberOfPendingMotionEventsForTesting());

        mGestureHandler.confirmTouchEvent(
                ContentViewGestureHandler.INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS);
        assertEquals("The queue should have been drained until first down since no consumer exists",
                2, mGestureHandler.getNumberOfPendingMotionEventsForTesting());
        assertEquals(MotionEvent.ACTION_DOWN,
                mGestureHandler.peekFirstInPendingMotionEventsForTesting().getActionMasked());

        mGestureHandler.confirmTouchEvent(
                ContentViewGestureHandler.INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS);
        assertEquals("The queue should have been drained",
                0, mGestureHandler.getNumberOfPendingMotionEventsForTesting());
    }

    /**
     * Verify that when a touch event handler is registered the touch events stop getting queued
     * after we received INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS.
     * @throws Exception
     */
    @SmallTest
    @Feature({"Gestures"})
    public void testFlingOutOfTouchHandler() throws Exception {
        final long downTime = SystemClock.uptimeMillis();
        final long eventTime = SystemClock.uptimeMillis();

        MotionEvent event = motionEvent(MotionEvent.ACTION_DOWN, downTime, downTime);

        mGestureHandler.hasTouchEventHandlers(true);

        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals(1, mGestureHandler.getNumberOfPendingMotionEventsForTesting());
        assertTrue("Should not have a pending gesture", mMockGestureDetector.mLastEvent == null);
        assertFalse("Should not have a pending LONG_PRESS", mLongPressDetector.hasPendingMessage());

        mGestureHandler.confirmTouchEvent(
                ContentViewGestureHandler.INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS);
        assertEquals(0, mGestureHandler.getNumberOfPendingMotionEventsForTesting());
        assertEquals("The down touch event should have been sent to the Gesture Detector",
                event.getEventTime(), mMockGestureDetector.mLastEvent.getEventTime());
        assertEquals("The down touch event should have been sent to the Gesture Detector",
                MotionEvent.ACTION_DOWN, mMockGestureDetector.mLastEvent.getActionMasked());

        event = MotionEvent.obtain(
                downTime, eventTime + 5, MotionEvent.ACTION_MOVE,
                FAKE_COORD_X * 5, FAKE_COORD_Y * 5, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals(0, mGestureHandler.getNumberOfPendingMotionEventsForTesting());
        assertEquals("Motion events should be going to the Gesture Detector directly",
                event.getEventTime(), mMockGestureDetector.mLastEvent.getEventTime());
        assertEquals("Motion events should be going to the Gesture Detector directly",
                MotionEvent.ACTION_MOVE, mMockGestureDetector.mLastEvent.getActionMasked());

        event = MotionEvent.obtain(
                downTime, eventTime + 10, MotionEvent.ACTION_UP,
                FAKE_COORD_X * 5, FAKE_COORD_Y * 5, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals(0, mGestureHandler.getNumberOfPendingMotionEventsForTesting());
        assertEquals("Motion events should be going to the Gesture Detector directly",
                event.getEventTime(), mMockGestureDetector.mLastEvent.getEventTime());
        assertEquals("Motion events should be going to the Gesture Detector directly",
                MotionEvent.ACTION_UP, mMockGestureDetector.mLastEvent.getActionMasked());

        // Synchronous, no need to wait.
        assertTrue("Should have a fling", mMockListener.mLastFling1 != null);
        assertTrue("Should not have a long press", mMockListener.mLastLongPress == null);
    }

    /**
     * Verifies that a single tap doesn't cause a long press event to be sent.
     * @throws Exception
     */
    @SmallTest
    @Feature({"Gestures"})
    public void testNoLongPressIsSentForSingleTapOutOfTouchHandler() throws Exception {
        final long downTime = SystemClock.uptimeMillis();
        final long eventTime = SystemClock.uptimeMillis();

        MotionEvent event = motionEvent(MotionEvent.ACTION_DOWN, downTime, downTime);

        mGestureHandler.hasTouchEventHandlers(true);

        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals(1, mGestureHandler.getNumberOfPendingMotionEventsForTesting());
        assertTrue("Should not have a pending gesture", mMockGestureDetector.mLastEvent == null);
        assertFalse("Should not have a pending LONG_PRESS", mLongPressDetector.hasPendingMessage());

        event = motionEvent(MotionEvent.ACTION_UP, downTime, eventTime + 5);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals(2, mGestureHandler.getNumberOfPendingMotionEventsForTesting());

        mGestureHandler.confirmTouchEvent(
                ContentViewGestureHandler.INPUT_EVENT_ACK_STATE_NOT_CONSUMED);

        assertEquals(1, mGestureHandler.getNumberOfPendingMotionEventsForTesting());
        assertEquals("The down touch event should have been sent to the Gesture Detector",
                event.getDownTime(), mMockGestureDetector.mLastEvent.getEventTime());
        assertEquals("The next event should be ACTION_UP",
                MotionEvent.ACTION_UP,
                mGestureHandler.peekFirstInPendingMotionEventsForTesting().getActionMasked());

        mGestureHandler.confirmTouchEvent(
                ContentViewGestureHandler.INPUT_EVENT_ACK_STATE_NOT_CONSUMED);

        assertEquals(0, mGestureHandler.getNumberOfPendingMotionEventsForTesting());
        assertEquals("The up touch event should have been sent to the Gesture Detector",
                event.getEventTime(), mMockGestureDetector.mLastEvent.getEventTime());

        assertFalse("Should not have a pending LONG_PRESS", mLongPressDetector.hasPendingMessage());
    }

    /**
     * Verify that a DOWN followed by a MOVE will trigger fling (but not LONG).
     * @throws Exception
     */
    @SmallTest
    @Feature({"Gestures"})
    public void testGestureFlingAndCancelLongClick() throws Exception {
        final long downTime = SystemClock.uptimeMillis();
        final long eventTime = SystemClock.uptimeMillis();

        MotionEvent event = motionEvent(MotionEvent.ACTION_DOWN, downTime, downTime);

        assertFalse(mGestureHandler.onTouchEvent(event));
        assertTrue("Should have a pending gesture", mMockGestureDetector.mLastEvent != null);
        assertTrue("Should have a pending LONG_PRESS", mLongPressDetector.hasPendingMessage());

        event = MotionEvent.obtain(
                downTime, eventTime + 5, MotionEvent.ACTION_MOVE,
                FAKE_COORD_X * 10, FAKE_COORD_Y * 10, 0);
        mLongPressDetector.cancelLongPressIfNeeded(event);
        assertTrue("Should not have a pending LONG_PRESS", !mLongPressDetector.hasPendingMessage());
        assertTrue(mGestureHandler.onTouchEvent(event));

        event = MotionEvent.obtain(
                downTime, eventTime + 10, MotionEvent.ACTION_UP,
                FAKE_COORD_X * 10, FAKE_COORD_Y * 10, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));

        // Synchronous, no need to wait.
        assertTrue("Should have a fling", mMockListener.mLastFling1 != null);
        assertTrue("Should not have a long press", mMockListener.mLastLongPress == null);
    }

    /**
     * Verify that for a normal scroll the following events are sent:
     * - GESTURE_SCROLL_START
     * - GESTURE_SCROLL_BY
     * - GESTURE_SCROLL_END
     * @throws Exception
     */
    @SmallTest
    @Feature({"Gestures"})
    public void testScrollEventActionUpSequence() throws Exception {
        checkScrollEventSequenceForEndActionType(MotionEvent.ACTION_UP);
    }

    /**
     * Verify that for a cancelled scroll the following events are sent:
     * - GESTURE_SCROLL_START
     * - GESTURE_SCROLL_BY
     * - GESTURE_SCROLL_END
     * @throws Exception
     */
    @SmallTest
    @Feature({"Gestures"})
    public void testScrollEventActionCancelSequence() throws Exception {
        checkScrollEventSequenceForEndActionType(MotionEvent.ACTION_CANCEL);
    }

    private void checkScrollEventSequenceForEndActionType(int endActionType) throws Exception {
        final long downTime = SystemClock.uptimeMillis();
        final long eventTime = SystemClock.uptimeMillis();
        final int scrollToX = FAKE_COORD_X + 100;
        final int scrollToY = FAKE_COORD_Y + 100;

        GestureRecordingMotionEventDelegate mockDelegate =
                new GestureRecordingMotionEventDelegate();
        mGestureHandler = new ContentViewGestureHandler(
                getInstrumentation().getTargetContext(), mockDelegate, mMockZoomManager);

        MotionEvent event = motionEvent(MotionEvent.ACTION_DOWN, downTime, downTime);

        assertTrue(mGestureHandler.onTouchEvent(event));

        event = MotionEvent.obtain(
                downTime, eventTime + 1000, MotionEvent.ACTION_MOVE, scrollToX, scrollToY, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertTrue(mGestureHandler.isNativeScrolling());
        assertTrue("A scrollStart event should have been sent",
                mockDelegate.mGestureTypeList.contains(
                        ContentViewGestureHandler.GESTURE_SCROLL_START));
        assertEquals("We should have started scrolling",
                ContentViewGestureHandler.GESTURE_SCROLL_BY,
                mockDelegate.mMostRecentGestureEvent.mType);
        assertEquals("Only tapDown, tapCancel, scrollBegin and scrollBy should have been sent",
                4, mockDelegate.mGestureTypeList.size());
        assertEquals("scrollBegin should be sent before scrollBy",
                ContentViewGestureHandler.GESTURE_SCROLL_START,
                (int) mockDelegate.mGestureTypeList.get(2));
        assertEquals("scrollBegin should have the time of the ACTION_MOVE",
                eventTime + 1000, (long) mockDelegate.mGestureTimeList.get(2));

        event = MotionEvent.obtain(
                downTime, eventTime + 1000, endActionType, scrollToX, scrollToY, 0);
        mGestureHandler.onTouchEvent(event);
        assertFalse(mGestureHandler.isNativeScrolling());
        assertTrue("A scrollEnd event should have been sent",
                mockDelegate.mGestureTypeList.contains(
                        ContentViewGestureHandler.GESTURE_SCROLL_END));
        assertEquals("We should have stopped scrolling",
                ContentViewGestureHandler.GESTURE_SCROLL_END,
                mockDelegate.mMostRecentGestureEvent.mType);
        assertEquals("Only tapDown, scrollBegin and scrollBy and scrollEnd should have been sent",
                5, mockDelegate.mGestureTypeList.size());
    }

    /**
     * Verify that for a normal fling (fling after scroll) the following events are sent:
     * - GESTURE_SCROLL_BEGIN
     * - GESTURE_FLING_START
     * and GESTURE_FLING_CANCEL is sent on the next touch.
     * @throws Exception
     */
    @SmallTest
    @Feature({"Gestures"})
    public void testFlingEventSequence() throws Exception {
        final long downTime = SystemClock.uptimeMillis();
        final long eventTime = SystemClock.uptimeMillis();

        GestureRecordingMotionEventDelegate mockDelegate =
                new GestureRecordingMotionEventDelegate();
        mGestureHandler = new ContentViewGestureHandler(
                getInstrumentation().getTargetContext(), mockDelegate, mMockZoomManager);

        MotionEvent event = motionEvent(MotionEvent.ACTION_DOWN, downTime, downTime);

        assertTrue(mGestureHandler.onTouchEvent(event));

        event = MotionEvent.obtain(
                downTime, eventTime + 10, MotionEvent.ACTION_MOVE,
                FAKE_COORD_X * 5, FAKE_COORD_Y * 5, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertTrue(mGestureHandler.isNativeScrolling());
        assertTrue("A scrollStart event should have been sent",
                mockDelegate.mGestureTypeList.contains(
                        ContentViewGestureHandler.GESTURE_SCROLL_START));
        assertEquals("We should have started scrolling",
                ContentViewGestureHandler.GESTURE_SCROLL_BY,
                mockDelegate.mMostRecentGestureEvent.mType);
        assertEquals("Only tapDown, tapCancel, scrollBegin and scrollBy should have been sent",
                4, mockDelegate.mGestureTypeList.size());
        assertEquals("scrollBegin should be sent before scrollBy",
                ContentViewGestureHandler.GESTURE_SCROLL_START,
                (int) mockDelegate.mGestureTypeList.get(2));
        assertEquals("scrollBegin should have the time of the ACTION_MOVE",
                eventTime + 10, (long) mockDelegate.mGestureTimeList.get(2));

        event = MotionEvent.obtain(
                downTime, eventTime + 15, MotionEvent.ACTION_UP,
                FAKE_COORD_X * 10, FAKE_COORD_Y * 10, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertFalse(mGestureHandler.isNativeScrolling());
        assertEquals("We should have started flinging",
                ContentViewGestureHandler.GESTURE_FLING_START,
                        mockDelegate.mMostRecentGestureEvent.mType);
        assertTrue("A scroll end event should not have been sent",
                !mockDelegate.mGestureTypeList.contains(
                        ContentViewGestureHandler.GESTURE_SCROLL_END));
        assertEquals("The last up should have caused flingStart to be sent",
                5, mockDelegate.mGestureTypeList.size());
        assertEquals("flingStart should have the time of the ACTION_UP",
                eventTime + 15, (long) mockDelegate.mGestureTimeList.get(4));

        event = motionEvent(MotionEvent.ACTION_DOWN, downTime + 50, downTime + 50);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertTrue("A flingCancel should have been sent",
                mockDelegate.mGestureTypeList.contains(
                        ContentViewGestureHandler.GESTURE_FLING_CANCEL));
        assertEquals("Only tapDown and flingCancel should have been sent",
                7, mockDelegate.mGestureTypeList.size());
    }

    /**
     * Verify that a zero-velocity fling is never forwarded, and cancels any
     * previous fling or scroll sequence.
     * @throws Exception
     */
    @SmallTest
    @Feature({"Gestures"})
    public void testZeroVelocityFling() throws Exception {
        final long downTime = SystemClock.uptimeMillis();
        final long eventTime = SystemClock.uptimeMillis();

        GestureRecordingMotionEventDelegate mockDelegate =
                new GestureRecordingMotionEventDelegate();
        mGestureHandler = new ContentViewGestureHandler(
                getInstrumentation().getTargetContext(), mockDelegate, mMockZoomManager);

        mGestureHandler.fling(eventTime, 5, 5, 0, 0);
        assertEquals("A zero-velocity fling should not be forwrded",
                null, mockDelegate.mMostRecentGestureEvent);

        mGestureHandler.fling(eventTime, 5, 5, 5, 0);
        assertEquals("Subsequent flings should work properly",
                ContentViewGestureHandler.GESTURE_FLING_START,
                        mockDelegate.mMostRecentGestureEvent.mType);

        mGestureHandler.fling(eventTime, 5, 5, 0, 0);
        assertEquals("A zero-velocity fling should cancel any outstanding fling",
                ContentViewGestureHandler.GESTURE_FLING_CANCEL,
                        mockDelegate.mMostRecentGestureEvent.mType);

        MotionEvent event = motionEvent(MotionEvent.ACTION_DOWN, downTime, downTime);
        assertTrue(mGestureHandler.onTouchEvent(event));
        event = MotionEvent.obtain(
                downTime, eventTime + 10, MotionEvent.ACTION_MOVE,
                FAKE_COORD_X * 5, FAKE_COORD_Y * 5, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertTrue(mGestureHandler.isNativeScrolling());
        assertTrue("A scrollStart event should have been sent",
                mockDelegate.mGestureTypeList.contains(
                        ContentViewGestureHandler.GESTURE_SCROLL_START));
        assertEquals("We should have started scrolling",
                ContentViewGestureHandler.GESTURE_SCROLL_BY,
                mockDelegate.mMostRecentGestureEvent.mType);

        mGestureHandler.fling(eventTime, 5, 5, 0, 0);
        assertEquals("A zero-velicty fling should end the current scroll sequence",
                ContentViewGestureHandler.GESTURE_SCROLL_END,
                        mockDelegate.mMostRecentGestureEvent.mType);
    }

    /**
     * Verify that a show pressed state gesture followed by a long press followed by the focus
     * loss in the window due to context menu cancels show pressed.
     * @throws Exception
     */
    @SmallTest
    @Feature({"Gestures"})
    public void testShowPressCancelOnWindowFocusLost() throws Exception {
        final long time = SystemClock.uptimeMillis();
        GestureRecordingMotionEventDelegate mockDelegate =
                new GestureRecordingMotionEventDelegate();
        mGestureHandler = new ContentViewGestureHandler(
                getInstrumentation().getTargetContext(), mockDelegate, mMockZoomManager);
        mLongPressDetector = new LongPressDetector(
                getInstrumentation().getTargetContext(), mGestureHandler);
        mGestureHandler.setTestDependencies(mLongPressDetector, null, null);

        MotionEvent event = motionEvent(MotionEvent.ACTION_DOWN, time, time);
        mGestureHandler.onTouchEvent(event);

        mGestureHandler.sendShowPressedStateGestureForTesting();
        assertEquals("A show pressed state event should have been sent",
                ContentViewGestureHandler.GESTURE_SHOW_PRESSED_STATE,
                        mockDelegate.mMostRecentGestureEvent.mType);
        assertEquals("Only showPressedState and tapDown should have been sent",
                2, mockDelegate.mGestureTypeList.size());

        mLongPressDetector.startLongPressTimerIfNeeded(event);
        mLongPressDetector.sendLongPressGestureForTest();

        assertEquals("Only should have sent only LONG_PRESS event",
                3, mockDelegate.mGestureTypeList.size());
        assertEquals("Should have a long press event next",
                ContentViewGestureHandler.GESTURE_LONG_PRESS,
                mockDelegate.mGestureTypeList.get(2).intValue());

        // The long press triggers window focus loss by opening a context menu
        mGestureHandler.onWindowFocusLost();

        assertEquals("Only should have sent only GESTURE_TAP_CANCEL event",
                4, mockDelegate.mGestureTypeList.size());
        assertEquals("Should have a gesture show press cancel event next",
                ContentViewGestureHandler.GESTURE_TAP_CANCEL,
                mockDelegate.mGestureTypeList.get(3).intValue());
    }

    /**
     * Verify that a recent show pressed state gesture is canceled when scrolling begins.
     * @throws Exception
     */
    @SmallTest
    @Feature({"Gestures"})
    public void testShowPressCancelWhenScrollBegins() throws Exception {
        final long downTime = SystemClock.uptimeMillis();
        final long eventTime = SystemClock.uptimeMillis();

        GestureRecordingMotionEventDelegate mockDelegate =
                new GestureRecordingMotionEventDelegate();
        mGestureHandler = new ContentViewGestureHandler(
                getInstrumentation().getTargetContext(), mockDelegate, mMockZoomManager);
        mLongPressDetector = new LongPressDetector(
                getInstrumentation().getTargetContext(), mGestureHandler);

        MotionEvent event = motionEvent(MotionEvent.ACTION_DOWN, downTime, downTime);

        assertTrue(mGestureHandler.onTouchEvent(event));
        assertFalse("Should not have a pending LONG_PRESS", mLongPressDetector.hasPendingMessage());

        mGestureHandler.sendShowPressedStateGestureForTesting();

        assertEquals("A show pressed state event should have been sent",
                ContentViewGestureHandler.GESTURE_SHOW_PRESSED_STATE,
                        mockDelegate.mMostRecentGestureEvent.mType);
        assertEquals("Only tapDown and showPressedState should have been sent",
                2, mockDelegate.mGestureTypeList.size());

        event = MotionEvent.obtain(
                downTime, eventTime + 10, MotionEvent.ACTION_MOVE,
                FAKE_COORD_X * 5, FAKE_COORD_Y * 5, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));

        assertEquals("We should have started scrolling",
                ContentViewGestureHandler.GESTURE_SCROLL_BY,
                        mockDelegate.mMostRecentGestureEvent.mType);
        assertTrue("A show press cancel event should have been sent",
                mockDelegate.mGestureTypeList.contains(
                        ContentViewGestureHandler.GESTURE_TAP_CANCEL));
        assertEquals("Only tapDown, showPressedState, showPressCancel, scrollBegin and scrollBy" +
                " should have been sent",
                5, mockDelegate.mGestureTypeList.size());

        event = MotionEvent.obtain(
                downTime, eventTime + 15, MotionEvent.ACTION_UP,
                FAKE_COORD_X * 10, FAKE_COORD_Y * 10, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals("We should have started flinging",
                ContentViewGestureHandler.GESTURE_FLING_START,
                        mockDelegate.mMostRecentGestureEvent.mType);
        assertTrue("A scroll end event should not have been sent",
                !mockDelegate.mGestureTypeList.contains(
                        ContentViewGestureHandler.GESTURE_SCROLL_END));
        assertEquals("The last up should have caused flingStart to be sent",
                6, mockDelegate.mGestureTypeList.size());
    }

    /**
     * Verify that double tap is correctly handled including the recent show pressed state gesture
     * cancellation.
     * @throws Exception
     */
    @SmallTest
    @Feature({"Gestures"})
    public void testDoubleTap() throws Exception {
        final long downTime = SystemClock.uptimeMillis();
        final long eventTime = SystemClock.uptimeMillis();

        GestureRecordingMotionEventDelegate mockDelegate =
                new GestureRecordingMotionEventDelegate();
        mGestureHandler = new ContentViewGestureHandler(
                getInstrumentation().getTargetContext(), mockDelegate, mMockZoomManager);
        mLongPressDetector = new LongPressDetector(
                getInstrumentation().getTargetContext(), mGestureHandler);

        MotionEvent event = motionEvent(MotionEvent.ACTION_DOWN, downTime, downTime);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertFalse("Should not have a pending LONG_PRESS", mLongPressDetector.hasPendingMessage());

        mGestureHandler.sendShowPressedStateGestureForTesting();
        assertEquals("GESTURE_SHOW_PRESSED_STATE should have been sent",
                ContentViewGestureHandler.GESTURE_SHOW_PRESSED_STATE,
                mockDelegate.mMostRecentGestureEvent.mType);
        assertEquals("Only GESTURE_TAP_DOWN and GESTURE_SHOW_PRESSED_STATE should have been sent",
                2, mockDelegate.mGestureTypeList.size());

        event = MotionEvent.obtain(
                downTime, eventTime + 5, MotionEvent.ACTION_UP,
                FAKE_COORD_X, FAKE_COORD_Y, 0);
        mGestureHandler.onTouchEvent(event);
        assertEquals("A GESTURE_SINGLE_TAP_UNCONFIRMED event should have been sent",
                ContentViewGestureHandler.GESTURE_SINGLE_TAP_UNCONFIRMED,
                mockDelegate.mMostRecentGestureEvent.mType);
        assertEquals("Only GESTURE_TAP_DOWN, " +
                "GESTURE_SHOW_PRESSED_STATE and " +
                "GESTURE_SINGLE_TAP_UNCONFIRMED should have been sent",
                3, mockDelegate.mGestureTypeList.size());

        event = MotionEvent.obtain(
                eventTime + 10, eventTime + 10, MotionEvent.ACTION_DOWN,
                FAKE_COORD_X, FAKE_COORD_Y, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals("A GESTURE_TAP_DOWN event should have been sent ",
                ContentViewGestureHandler.GESTURE_TAP_DOWN,
                mockDelegate.mMostRecentGestureEvent.mType);
        assertEquals("Only GESTURE_TAP_DOWN, " +
                "GESTURE_SHOW_PRESSED_STATE, " +
                "GESTURE_SINGLE_TAP_UNCONFIRMED," +
                "GESTURE_TAP_CANCEL and" +
                "GESTURE_TAP_DOWN should have been sent",
                5, mockDelegate.mGestureTypeList.size());

        // Moving a very small amount of distance should not trigger the double tap drag zoom mode.
        event = MotionEvent.obtain(
                eventTime + 10, eventTime + 10, MotionEvent.ACTION_MOVE,
                FAKE_COORD_X, FAKE_COORD_Y + 1, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals("Only GESTURE_TAP_DOWN, " +
                "GESTURE_SHOW_PRESSED_STATE, " +
                "GESTURE_SINGLE_TAP_UNCONFIRMED and" +
                "GESTURE_TAP_CANCEL and" +
                "GESTURE_TAP_DOWN should have been sent",
                5, mockDelegate.mGestureTypeList.size());

        event = MotionEvent.obtain(
                eventTime + 10, eventTime + 15, MotionEvent.ACTION_UP,
                FAKE_COORD_X, FAKE_COORD_Y + 1, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals("A double tap should have occurred",
                ContentViewGestureHandler.GESTURE_DOUBLE_TAP,
                mockDelegate.mMostRecentGestureEvent.mType);
        assertEquals("Only GESTURE_TAP_DOWN, " +
                "GESTURE_SHOW_PRESSED_STATE, " +
                "GESTURE_SINGLE_TAP_UNCONFIRMED, " +
                "GESTURE_TAP_CANCEL, " +
                "GESTURE_TAP_DOWN, " +
                "GESTURE_DOUBLE_TAP should have been sent",
                6, mockDelegate.mGestureTypeList.size());
    }

    /**
     * Verify that double tap drag zoom feature is correctly executed.
     * @throws Exception
     */
    @SmallTest
    @Feature({"Gestures"})
    public void testDoubleTapDragZoom() throws Exception {
        final long downTime1 = SystemClock.uptimeMillis();
        final long downTime2 = downTime1 + 100;

        GestureRecordingMotionEventDelegate mockDelegate =
                new GestureRecordingMotionEventDelegate();
        mGestureHandler = new ContentViewGestureHandler(
                getInstrumentation().getTargetContext(), mockDelegate, mMockZoomManager);
        mLongPressDetector = new LongPressDetector(
                getInstrumentation().getTargetContext(), mGestureHandler);

        MotionEvent event = motionEvent(MotionEvent.ACTION_DOWN, downTime1, downTime1);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertFalse("Should not have a pending LONG_PRESS", mLongPressDetector.hasPendingMessage());

        mGestureHandler.sendShowPressedStateGestureForTesting();
        assertEquals("GESTURE_SHOW_PRESSED_STATE should have been sent",
                ContentViewGestureHandler.GESTURE_SHOW_PRESSED_STATE,
                        mockDelegate.mMostRecentGestureEvent.mType);
        assertEquals("Only GESTURE_TAP_DOWN and GESTURE_SHOW_PRESSED_STATE should have been sent",
                2, mockDelegate.mGestureTypeList.size());


        event = MotionEvent.obtain(
                downTime1, downTime1 + 5, MotionEvent.ACTION_UP,
                FAKE_COORD_X, FAKE_COORD_Y, 0);
        mGestureHandler.onTouchEvent(event);
        assertEquals("A GESTURE_SINGLE_TAP_UNCONFIRMED event should have been sent",
                ContentViewGestureHandler.GESTURE_SINGLE_TAP_UNCONFIRMED,
                mockDelegate.mMostRecentGestureEvent.mType);
        assertEquals("Only GESTURE_TAP_DOWN, " +
                "GESTURE_SHOW_PRESSED_STATE and " +
                "GESTURE_TAP_UNCONFIRMED " +
                "should have been sent",
                3, mockDelegate.mGestureTypeList.size());

        event = MotionEvent.obtain(
                downTime2, downTime2, MotionEvent.ACTION_DOWN,
                FAKE_COORD_X, FAKE_COORD_Y, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals("GESTURE_TAP_DOWN should have been sent",
                ContentViewGestureHandler.GESTURE_TAP_DOWN,
                mockDelegate.mMostRecentGestureEvent.mType);
        assertEquals("Only GESTURE_TAP_DOWN, " +
                "GESTURE_SHOW_PRESSED_STATE, " +
                "GESTURE_TAP_UNCONFIRMED," +
                "GESTURE_TAP_CANCEL and" +
                "GESTURE_TAP_DOWN should have been sent",
                5, mockDelegate.mGestureTypeList.size());

        event = MotionEvent.obtain(
                downTime2, downTime2 + 5, MotionEvent.ACTION_MOVE,
                FAKE_COORD_X, FAKE_COORD_Y + 100, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertTrue("GESTURE_SCROLL_START should have been sent",
                mockDelegate.mGestureTypeList.contains(
                        ContentViewGestureHandler.GESTURE_SCROLL_START));
        assertEquals("GESTURE_PINCH_BEGIN should have been sent",
                ContentViewGestureHandler.GESTURE_PINCH_BEGIN,
                mockDelegate.mMostRecentGestureEvent.mType);
        assertEquals("Only GESTURE_TAP_DOWN, " +
                "GESTURE_SHOW_PRESSED_STATE, " +
                "GESTURE_TAP_UNCONFIRMED," +
                "GESTURE_TAP_CANCEL, " +
                "GESTURE_TAP_DOWN, " +
                "GESTURE_TAP_CANCEL, " +
                "GESTURE_SCROLL_START, and " +
                "GESTURE_PINCH_BEGIN should have been sent",
                8, mockDelegate.mGestureTypeList.size());

        event = MotionEvent.obtain(
                downTime2, downTime2 + 10, MotionEvent.ACTION_MOVE,
                FAKE_COORD_X, FAKE_COORD_Y + 200, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertTrue("GESTURE_SCROLL_BY should have been sent",
                mockDelegate.mGestureTypeList.contains(
                        ContentViewGestureHandler.GESTURE_SCROLL_BY));
        assertEquals("GESTURE_PINCH_BY should have been sent",
                ContentViewGestureHandler.GESTURE_PINCH_BY,
                mockDelegate.mMostRecentGestureEvent.mType);
        assertEquals("Only GESTURE_TAP_DOWN, " +
                "GESTURE_SHOW_PRESSED_STATE, " +
                "GESTURE_TAP_UNCONFIRMED," +
                "GESTURE_TAP_CANCEL, " +
                "GESTURE_TAP_DOWN, " +
                "GESTURE_TAP_CANCEL, " +
                "GESTURE_SCROLL_START," +
                "GESTURE_PINCH_BEGIN, " +
                "GESTURE_SCROLL_BY, and " +
                "GESTURE_PINCH_BY should have been sent",
                10, mockDelegate.mGestureTypeList.size());

        event = MotionEvent.obtain(
                downTime2, downTime2 + 15, MotionEvent.ACTION_UP,
                FAKE_COORD_X, FAKE_COORD_Y + 200, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertTrue("GESTURE_PINCH_END should have been sent",
                mockDelegate.mGestureTypeList.contains(
                        ContentViewGestureHandler.GESTURE_PINCH_END));
        assertEquals("GESTURE_SCROLL_END should have been sent",
                ContentViewGestureHandler.GESTURE_SCROLL_END,
                mockDelegate.mMostRecentGestureEvent.mType);
        assertEquals("Only GESTURE_TAP_DOWN, " +
                "GESTURE_SHOW_PRESSED_STATE, " +
                "GESTURE_TAP_UNCONFIRMED," +
                "GESTURE_TAP_CANCEL, " +
                "GESTURE_TAP_DOWN, " +
                "GESTURE_TAP_CANCEL, " +
                "GESTURE_SCROLL_START," +
                "GESTURE_PINCH_BEGIN, " +
                "GESTURE_SCROLL_BY," +
                "GESTURE_PINCH_BY, " +
                "GESTURE_PINCH_END, and " +
                "GESTURE_SCROLL_END should have been sent",
                12, mockDelegate.mGestureTypeList.size());
    }

    /**
     * Verify that double tap drag zoom feature is not invoked
     * when it is disabled..
     * @throws Exception
     */
    @SmallTest
    @Feature({"Gestures"})
    public void testDoubleTapDragZoomNothingWhenDisabled() throws Exception {
        final long downTime1 = SystemClock.uptimeMillis();
        final long downTime2 = downTime1 + 100;

        GestureRecordingMotionEventDelegate mockDelegate =
                new GestureRecordingMotionEventDelegate();
        mGestureHandler = new ContentViewGestureHandler(
                getInstrumentation().getTargetContext(), mockDelegate, mMockZoomManager);
        mGestureHandler.updateDoubleTapSupport(false);

        MotionEvent event = motionEvent(MotionEvent.ACTION_DOWN, downTime1, downTime1);
        assertTrue(mGestureHandler.onTouchEvent(event));

        event = MotionEvent.obtain(
                downTime1, downTime1 + 5, MotionEvent.ACTION_UP,
                FAKE_COORD_X, FAKE_COORD_Y, 0);
        mGestureHandler.onTouchEvent(event);

        event = MotionEvent.obtain(
                downTime2, downTime2, MotionEvent.ACTION_DOWN,
                FAKE_COORD_X, FAKE_COORD_Y, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));

        event = MotionEvent.obtain(
                downTime2, downTime2 + 5, MotionEvent.ACTION_MOVE,
                FAKE_COORD_X, FAKE_COORD_Y + 100, 0);

        // The move should become a scroll, as double tap and drag to zoom is
        // disabled.
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertTrue("GESTURE_SCROLL_START should have been sent",
                mockDelegate.mGestureTypeList.contains(
                        ContentViewGestureHandler.GESTURE_SCROLL_START));
        assertFalse("No GESTURE_PINCH_BEGIN should have been sent",
                mockDelegate.mGestureTypeList.contains(
                        ContentViewGestureHandler.GESTURE_PINCH_BEGIN));

        event = MotionEvent.obtain(
                downTime2, downTime2 + 10, MotionEvent.ACTION_MOVE,
                FAKE_COORD_X, FAKE_COORD_Y + 200, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals("GESTURE_SCROLL_BY should have been sent",
                ContentViewGestureHandler.GESTURE_SCROLL_BY,
                mockDelegate.mMostRecentGestureEvent.mType);
        assertEquals("GESTURE_SCROLL_BY should have been sent",
                event.getEventTime(),
                mockDelegate.mMostRecentGestureEvent.getTimeMs());
        assertTrue("No GESTURE_PINCH_BY should have been sent",
                ContentViewGestureHandler.GESTURE_PINCH_BY !=
                mockDelegate.mMostRecentGestureEvent.mType);

        event = MotionEvent.obtain(
                downTime2, downTime2 + 15, MotionEvent.ACTION_UP,
                FAKE_COORD_X, FAKE_COORD_Y + 200, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertFalse("No GESTURE_PINCH_END should have been sent",
                mockDelegate.mGestureTypeList.contains(
                        ContentViewGestureHandler.GESTURE_PINCH_END));
    }

    /**
     * Mock MotionEventDelegate that remembers the most recent gesture event.
     */
    static class GestureRecordingMotionEventDelegate implements MotionEventDelegate {
        static class GestureEvent {
            private final int mType;
            private final long mTimeMs;
            private final int mX;
            private final int mY;
            private final Bundle mExtraParams;

            public GestureEvent(int type, long timeMs, int x, int y, Bundle extraParams) {
                mType = type;
                mTimeMs = timeMs;
                mX = x;
                mY = y;
                mExtraParams = extraParams;
            }

            public int getType() {
                return mType;
            }

            public long getTimeMs() {
                return mTimeMs;
            }

            public int getX() {
                return mX;
            }

            public int getY() {
                return mY;
            }

            public Bundle getExtraParams() {
                return mExtraParams;
            }
        }
        private GestureEvent mMostRecentGestureEvent;
        private final ArrayList<Integer> mGestureTypeList = new ArrayList<Integer>();
        private final ArrayList<Long> mGestureTimeList = new ArrayList<Long>();

        @Override
        public boolean sendTouchEvent(long timeMs, int action, TouchPoint[] pts) {
            return true;
        }

        @Override
        public boolean sendGesture(int type, long timeMs, int x, int y, Bundle extraParams) {
            Log.i(TAG, "Gesture event received with type id " + type);
            mMostRecentGestureEvent = new GestureEvent(type, timeMs, x, y, extraParams);
            mGestureTypeList.add(mMostRecentGestureEvent.mType);
            mGestureTimeList.add(timeMs);
            return true;
        }

        @Override
        public void sendSingleTapUMA(int type) {
            // Not implemented.
        }

        @Override
        public void sendActionAfterDoubleTapUMA(int type,
                boolean clickDelayEnabled) {
            // Not implemented.
        }

        @Override
        public void invokeZoomPicker() {
            // Not implemented.
        }

        public GestureEvent getMostRecentGestureEvent() {
            return mMostRecentGestureEvent;
        }
    }

    /**
     * Generate a scroll gesture and verify that the resulting scroll motion event has both absolute
     * and relative position information.
     */
    @SmallTest
    @Feature({"Gestures"})
    public void testScrollUpdateCoordinates() {
        final int deltaX = 16;
        final int deltaY = 84;
        final long downTime = SystemClock.uptimeMillis();

        GestureRecordingMotionEventDelegate delegate = new GestureRecordingMotionEventDelegate();
        ContentViewGestureHandler gestureHandler = new ContentViewGestureHandler(
                getInstrumentation().getTargetContext(), delegate, mMockZoomManager);
        MotionEvent event = motionEvent(MotionEvent.ACTION_DOWN, downTime, downTime);
        assertTrue(gestureHandler.onTouchEvent(event));

        // Move twice, because the first move gesture is discarded.
        event = MotionEvent.obtain(
                downTime, downTime + 5, MotionEvent.ACTION_MOVE,
                FAKE_COORD_X - deltaX / 2, FAKE_COORD_Y - deltaY / 2, 0);
        assertTrue(gestureHandler.onTouchEvent(event));

        event = MotionEvent.obtain(
                downTime, downTime + 10, MotionEvent.ACTION_MOVE,
                FAKE_COORD_X - deltaX, FAKE_COORD_Y - deltaY, 0);
        assertTrue(gestureHandler.onTouchEvent(event));

        // Make sure the reported gesture event has all the expected data.
        GestureRecordingMotionEventDelegate.GestureEvent gestureEvent =
                delegate.getMostRecentGestureEvent();
        assertNotNull(gestureEvent);
        assertEquals(ContentViewGestureHandler.GESTURE_SCROLL_BY, gestureEvent.getType());
        assertEquals(downTime + 10, gestureEvent.getTimeMs());
        assertEquals(FAKE_COORD_X - deltaX, gestureEvent.getX());
        assertEquals(FAKE_COORD_Y - deltaY, gestureEvent.getY());

        Bundle extraParams = gestureEvent.getExtraParams();
        assertNotNull(extraParams);
        // No horizontal delta because of snapping.
        assertEquals(0, extraParams.getInt(ContentViewGestureHandler.DISTANCE_X));
        assertEquals(deltaY / 2, extraParams.getInt(ContentViewGestureHandler.DISTANCE_Y));
    }

    /**
     * Verify that the timer of LONG_PRESS will be cancelled when scrolling begins so
     * LONG_PRESS and LONG_TAP won't be triggered.
     *
     * @throws Exception
     */
    @SmallTest
    @Feature({"Gestures"})
    public void testLongPressAndTapCancelWhenScrollBegins() throws Exception {
        final long downTime = SystemClock.uptimeMillis();
        final long eventTime = SystemClock.uptimeMillis();

        GestureRecordingMotionEventDelegate mockDelegate =
                new GestureRecordingMotionEventDelegate();
        mGestureHandler = new ContentViewGestureHandler(
                getInstrumentation().getTargetContext(), mockDelegate, mMockZoomManager);
        mLongPressDetector = mGestureHandler.getLongPressDetector();

        MotionEvent event = motionEvent(MotionEvent.ACTION_DOWN, downTime, downTime);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertTrue("Should have a pending LONG_PRESS", mLongPressDetector.hasPendingMessage());
        event = MotionEvent.obtain(
                downTime, eventTime + 5, MotionEvent.ACTION_MOVE,
                FAKE_COORD_X * 5, FAKE_COORD_Y * 5, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        event = MotionEvent.obtain(
                downTime, eventTime + 10, MotionEvent.ACTION_MOVE,
                FAKE_COORD_X * 10, FAKE_COORD_Y * 10, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertTrue("Should not have a pending LONG_PRESS", !mLongPressDetector.hasPendingMessage());

        // No LONG_TAP because LONG_PRESS timer is cancelled.
        assertFalse("No LONG_PRESS should be sent",
                mockDelegate.mGestureTypeList.contains(
                        ContentViewGestureHandler.GESTURE_LONG_PRESS));
        assertFalse("No LONG_TAP should be sent",
                mockDelegate.mGestureTypeList.contains(
                        ContentViewGestureHandler.GESTURE_LONG_TAP));
    }

    /**
     * Verifies that when hasTouchEventHandlers changes while in a gesture, that the pending
     * queue does not grow continually.
     */
    @SmallTest
    @Feature({"Gestures"})
    public void testHasTouchEventHandlersChangesInGesture() {
        final long downTime = SystemClock.uptimeMillis();
        final long eventTime = SystemClock.uptimeMillis();

        mGestureHandler = new ContentViewGestureHandler(
                getInstrumentation().getTargetContext(), new MockMotionEventDelegate(),
                mMockZoomManager);
        mLongPressDetector = new LongPressDetector(
                getInstrumentation().getTargetContext(), mGestureHandler);

        MotionEvent event = motionEvent(MotionEvent.ACTION_DOWN, downTime, downTime);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals(0, mGestureHandler.getNumberOfPendingMotionEventsForTesting());
        assertFalse("Should not have a pending LONG_PRESS", mLongPressDetector.hasPendingMessage());

        event = MotionEvent.obtain(
                downTime, eventTime + 5, MotionEvent.ACTION_MOVE,
                FAKE_COORD_X * 5, FAKE_COORD_Y * 5, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals(0, mGestureHandler.getNumberOfPendingMotionEventsForTesting());

        mGestureHandler.hasTouchEventHandlers(true);

        event = MotionEvent.obtain(
                downTime, eventTime + 10, MotionEvent.ACTION_MOVE,
                FAKE_COORD_X * 10, FAKE_COORD_Y * 10, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals(0, mGestureHandler.getNumberOfPendingMotionEventsForTesting());

        event = MotionEvent.obtain(
                downTime, eventTime + 15, MotionEvent.ACTION_MOVE,
                FAKE_COORD_X * 15, FAKE_COORD_Y * 15, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals(0, mGestureHandler.getNumberOfPendingMotionEventsForTesting());
    }

    /**
     * Verify that LONG_TAP is triggered after LongPress followed by an UP.
     *
     * @throws Exception
     */
    @SmallTest
    @Feature({"Gestures"})
    public void testGestureLongTap() throws Exception {
        final long downTime = SystemClock.uptimeMillis();
        final long eventTime = SystemClock.uptimeMillis();

        GestureRecordingMotionEventDelegate mockDelegate =
                new GestureRecordingMotionEventDelegate();
        mGestureHandler = new ContentViewGestureHandler(
                getInstrumentation().getTargetContext(), mockDelegate, mMockZoomManager);
        mLongPressDetector = mGestureHandler.getLongPressDetector();

        MotionEvent event = motionEvent(MotionEvent.ACTION_DOWN, downTime, downTime);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertTrue("Should have a pending LONG_PRESS", mLongPressDetector.hasPendingMessage());

        mLongPressDetector.sendLongPressGestureForTest();

        assertEquals("A LONG_PRESS gesture should have been sent",
                ContentViewGestureHandler.GESTURE_LONG_PRESS,
                        mockDelegate.mMostRecentGestureEvent.mType);

        event = motionEvent(MotionEvent.ACTION_UP, downTime, eventTime + 1000);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals("A LONG_TAP gesture should have been sent",
                ContentViewGestureHandler.GESTURE_LONG_TAP,
                        mockDelegate.mMostRecentGestureEvent.mType);
    }

    /**
     * Verify that the touch slop region is removed from the first scroll delta to avoid a jump when
     * starting to scroll.
     * @throws Exception
     */
    @SmallTest
    @Feature({"Gestures"})
    public void testTouchSlopRemovedFromScroll() throws Exception {
        Context context = getInstrumentation().getTargetContext();
        final long downTime = SystemClock.uptimeMillis();
        final long eventTime = SystemClock.uptimeMillis();
        final int scaledTouchSlop = ViewConfiguration.get(context).getScaledTouchSlop();
        final int scrollDelta = 5;

        GestureRecordingMotionEventDelegate mockDelegate =
                new GestureRecordingMotionEventDelegate();
        mGestureHandler = new ContentViewGestureHandler(
                context, mockDelegate, mMockZoomManager);

        MotionEvent event = motionEvent(MotionEvent.ACTION_DOWN, downTime, downTime);
        assertTrue(mGestureHandler.onTouchEvent(event));

        event = MotionEvent.obtain(
                downTime, eventTime + 10, MotionEvent.ACTION_MOVE,
                FAKE_COORD_X, FAKE_COORD_Y + scaledTouchSlop + scrollDelta, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));

        assertEquals("We should have started scrolling",
                ContentViewGestureHandler.GESTURE_SCROLL_BY,
                mockDelegate.mMostRecentGestureEvent.mType);

        GestureRecordingMotionEventDelegate.GestureEvent gestureEvent =
                mockDelegate.getMostRecentGestureEvent();
        assertNotNull(gestureEvent);
        Bundle extraParams = gestureEvent.getExtraParams();
        assertEquals(0, extraParams.getInt(ContentViewGestureHandler.DISTANCE_X));
        assertEquals(-scrollDelta, extraParams.getInt(ContentViewGestureHandler.DISTANCE_Y));
    }

    /**
     * Verify that touch moves are deferred if they are within the touch slop region
     * and the touch sequence is not being consumed.
     * @throws Exception
     */
    @SmallTest
    @Feature({"Gestures"})
    public void testTouchMoveWithinTouchSlopDeferred() throws Exception {
        Context context = getInstrumentation().getTargetContext();
        final long downTime = SystemClock.uptimeMillis();
        final long eventTime = SystemClock.uptimeMillis();
        final int scaledTouchSlop = ViewConfiguration.get(context).getScaledTouchSlop();
        final int lessThanSlopScrollDelta = scaledTouchSlop / 2;
        final int greaterThanSlopScrollDelta = scaledTouchSlop * 2;

        mGestureHandler.hasTouchEventHandlers(true);

        MotionEvent event = motionEvent(MotionEvent.ACTION_DOWN, downTime, downTime);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals("The touch down should have been forwarded",
                TouchPoint.TOUCH_EVENT_TYPE_START, mMockMotionEventDelegate.mLastTouchAction);
        assertEquals(1, mGestureHandler.getNumberOfPendingMotionEventsForTesting());

        event = MotionEvent.obtain(
                downTime, eventTime + 10, MotionEvent.ACTION_MOVE,
                FAKE_COORD_X, FAKE_COORD_Y + lessThanSlopScrollDelta, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals(2, mGestureHandler.getNumberOfPendingMotionEventsForTesting());

        mGestureHandler.confirmTouchEvent(
                ContentViewGestureHandler.INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
        assertEquals("The less-than-slop touch move should not have been forwarded",
                TouchPoint.TOUCH_EVENT_TYPE_START, mMockMotionEventDelegate.mLastTouchAction);
        assertEquals(0, mGestureHandler.getNumberOfPendingMotionEventsForTesting());

        event = MotionEvent.obtain(
                downTime, eventTime + 10, MotionEvent.ACTION_MOVE,
                FAKE_COORD_X, FAKE_COORD_Y + greaterThanSlopScrollDelta, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals("The touch move should have been forwarded",
                TouchPoint.TOUCH_EVENT_TYPE_MOVE, mMockMotionEventDelegate.mLastTouchAction);
        mGestureHandler.confirmTouchEvent(
                ContentViewGestureHandler.INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
        assertEquals(0, mGestureHandler.getNumberOfPendingMotionEventsForTesting());
    }

    /**
     * Verify that touch moves are not deferred even if they are within the touch slop region
     * when the touch sequence is being consumed.
     * @throws Exception
     */
    @SmallTest
    @Feature({"Gestures"})
    public void testTouchMoveWithinTouchSlopNotDeferredIfJavascriptConsumingGesture()
            throws Exception {
        Context context = getInstrumentation().getTargetContext();
        final long downTime = SystemClock.uptimeMillis();
        final long eventTime = SystemClock.uptimeMillis();
        final int scaledTouchSlop = ViewConfiguration.get(context).getScaledTouchSlop();
        final int lessThanSlopScrollDelta = scaledTouchSlop / 2;

        mGestureHandler.hasTouchEventHandlers(true);

        MotionEvent event = motionEvent(MotionEvent.ACTION_DOWN, downTime, downTime);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals("The touch down should have been forwarded",
                TouchPoint.TOUCH_EVENT_TYPE_START, mMockMotionEventDelegate.mLastTouchAction);
        assertEquals(1, mGestureHandler.getNumberOfPendingMotionEventsForTesting());

        event = MotionEvent.obtain(
                downTime, eventTime + 10, MotionEvent.ACTION_MOVE,
                FAKE_COORD_X, FAKE_COORD_Y + lessThanSlopScrollDelta, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals(2, mGestureHandler.getNumberOfPendingMotionEventsForTesting());

        mGestureHandler.confirmTouchEvent(
                ContentViewGestureHandler.INPUT_EVENT_ACK_STATE_CONSUMED);
        assertEquals("The less-than-slop touch move should have been forwarded",
                TouchPoint.TOUCH_EVENT_TYPE_MOVE, mMockMotionEventDelegate.mLastTouchAction);
    }

    private static void sendLastScrollByEvent(ContentViewGestureHandler handler) {
        final long downTime = SystemClock.uptimeMillis();
        final long eventTime = SystemClock.uptimeMillis();
        MotionEvent event = motionEvent(MotionEvent.ACTION_DOWN, downTime, downTime);
        assertTrue(handler.onTouchEvent(event));
        event = MotionEvent.obtain(
                downTime, eventTime + 10, MotionEvent.ACTION_MOVE,
                FAKE_COORD_X, FAKE_COORD_Y + 30, 0);
        assertTrue(handler.onTouchEvent(event));
    }

    private static void sendLastZoomEvent(
            ContentViewGestureHandler handler, MockZoomManager zoomManager) {
        zoomManager.pinchOnMoveEvents(handler);
        final long downTime = SystemClock.uptimeMillis();
        final long eventTime = SystemClock.uptimeMillis();
        MotionEvent event = motionEvent(MotionEvent.ACTION_DOWN, downTime, downTime);
        assertTrue(handler.onTouchEvent(event));
        event = MotionEvent.obtain(
                downTime, eventTime + 10, MotionEvent.ACTION_MOVE,
                FAKE_COORD_X, FAKE_COORD_Y + 30, 0);
        assertTrue(handler.onTouchEvent(event));
    }

    private static void sendLastPinchEvent(ContentViewGestureHandler handler) {
        final long downTime = SystemClock.uptimeMillis();
        final long eventTime = SystemClock.uptimeMillis();
        handler.pinchBegin(downTime, FAKE_COORD_X, FAKE_COORD_Y);
        handler.pinchBy(eventTime + 10, FAKE_COORD_X, FAKE_COORD_Y, 2);
    }

    /**
     * Verify that a DOWN followed shortly by an UP will trigger
     * a GESTURE_SINGLE_TAP_UNCONFIRMED event immediately.
     *
     * @throws Exception
     */
    @SmallTest
    @Feature({"Gestures"})
    public void testGestureEventsSingleTapUnconfirmed() throws Exception {
        final long downTime = SystemClock.uptimeMillis();
        final long eventTime = SystemClock.uptimeMillis();

        GestureRecordingMotionEventDelegate mockDelegate =
                new GestureRecordingMotionEventDelegate();
        mGestureHandler = new ContentViewGestureHandler(
                getInstrumentation().getTargetContext(), mockDelegate, mMockZoomManager);

        MotionEvent event = motionEvent(MotionEvent.ACTION_DOWN, downTime, downTime);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals("A TAP_DOWN gesture should have been sent",
                ContentViewGestureHandler.GESTURE_TAP_DOWN,
                        mockDelegate.mMostRecentGestureEvent.mType);

        event = motionEvent(MotionEvent.ACTION_UP, downTime, eventTime + 10);
        assertFalse(mGestureHandler.onTouchEvent(event));
        assertEquals("A GESTURE_SINGLE_TAP_UNCONFIRMED gesture should have been sent",
                ContentViewGestureHandler.GESTURE_SINGLE_TAP_UNCONFIRMED,
                        mockDelegate.mMostRecentGestureEvent.mType);

        assertTrue("Should not have confirmed a single tap yet",
                mMockListener.mLastSingleTap == null);
    }

    /**
     * Verify that a tap-ending event will follow a TAP_DOWN event.
     *
     * @throws Exception
     */
    @SmallTest
    @Feature({"Gestures"})
    public void testTapDownFollowedByTapEndingEvent() throws Exception {
        long downTime = SystemClock.uptimeMillis();
        long eventTime = SystemClock.uptimeMillis();

        GestureRecordingMotionEventDelegate mockDelegate =
                new GestureRecordingMotionEventDelegate();
        mGestureHandler = new ContentViewGestureHandler(
                getInstrumentation().getTargetContext(), mockDelegate, mMockZoomManager);

        MotionEvent event = motionEvent(MotionEvent.ACTION_DOWN, downTime, downTime);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals(ContentViewGestureHandler.GESTURE_TAP_DOWN,
                mockDelegate.mMostRecentGestureEvent.mType);
        assertTrue(mGestureHandler.needsTapEndingEventForTesting());
        event = motionEvent(MotionEvent.ACTION_UP, downTime, eventTime + 5);
        assertFalse(mGestureHandler.onTouchEvent(event));
        assertEquals(ContentViewGestureHandler.GESTURE_SINGLE_TAP_UNCONFIRMED,
                mockDelegate.mMostRecentGestureEvent.mType);
        assertTrue("An unconfirmed tap does not terminate the tap down.",
                mGestureHandler.needsTapEndingEventForTesting());

        // A confirmed tap is a tap-ending event.
        downTime += 20;
        eventTime += 20;
        mockDelegate.mGestureTypeList.clear();
        mGestureHandler.updateShouldDisableDoubleTap(true);
        event = MotionEvent.obtain(
                downTime, downTime, MotionEvent.ACTION_DOWN,
                FAKE_COORD_X, FAKE_COORD_Y, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertTrue(mGestureHandler.needsTapEndingEventForTesting());
        event = MotionEvent.obtain(
                downTime, eventTime + 5, MotionEvent.ACTION_UP,
                FAKE_COORD_X, FAKE_COORD_Y, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals(ContentViewGestureHandler.GESTURE_SINGLE_TAP_CONFIRMED,
                mockDelegate.mMostRecentGestureEvent.mType);
        assertFalse("A confirmed single tap should terminate the tap down.",
                mGestureHandler.needsTapEndingEventForTesting());

        // A double tap gesture is a tap-ending event.
        downTime += 20;
        eventTime += 20;
        mockDelegate.mGestureTypeList.clear();
        mGestureHandler.updateShouldDisableDoubleTap(false);
        event = MotionEvent.obtain(
                downTime, downTime, MotionEvent.ACTION_DOWN,
                FAKE_COORD_X, FAKE_COORD_Y, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertTrue(mGestureHandler.needsTapEndingEventForTesting());
        event = MotionEvent.obtain(
                downTime, eventTime + 5, MotionEvent.ACTION_UP,
                FAKE_COORD_X, FAKE_COORD_Y, 0);
        event = MotionEvent.obtain(
                eventTime + 10, eventTime + 10, MotionEvent.ACTION_DOWN,
                FAKE_COORD_X, FAKE_COORD_Y, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertTrue(mGestureHandler.needsTapEndingEventForTesting());
        event = MotionEvent.obtain(
                eventTime + 10, eventTime + 15, MotionEvent.ACTION_UP,
                FAKE_COORD_X, FAKE_COORD_Y, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals(ContentViewGestureHandler.GESTURE_DOUBLE_TAP,
                mockDelegate.mMostRecentGestureEvent.mType);
        assertFalse("A double tap should terminate the tap down.",
                mGestureHandler.needsTapEndingEventForTesting());

        // A double tap drag gesture will trigger a tap-ending event.
        downTime += 20;
        eventTime += 20;
        mockDelegate.mGestureTypeList.clear();
        mGestureHandler.updateShouldDisableDoubleTap(false);
        event = MotionEvent.obtain(
                downTime, downTime, MotionEvent.ACTION_DOWN,
                FAKE_COORD_X, FAKE_COORD_Y, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertTrue(mGestureHandler.needsTapEndingEventForTesting());
        event = MotionEvent.obtain(
                downTime, eventTime + 5, MotionEvent.ACTION_UP,
                FAKE_COORD_X, FAKE_COORD_Y, 0);
        event = MotionEvent.obtain(
                eventTime + 10, eventTime + 10, MotionEvent.ACTION_DOWN,
                FAKE_COORD_X, FAKE_COORD_Y, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertTrue(mGestureHandler.needsTapEndingEventForTesting());
        event = MotionEvent.obtain(
                eventTime + 10, eventTime + 15, MotionEvent.ACTION_MOVE,
                FAKE_COORD_X, FAKE_COORD_Y + 100, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertFalse("A double tap drag should terminate the tap down.",
                mGestureHandler.needsTapEndingEventForTesting());
        assertTrue(mockDelegate.mGestureTypeList.contains(
                ContentViewGestureHandler.GESTURE_SCROLL_START));
        assertTrue(mockDelegate.mGestureTypeList.contains(
                ContentViewGestureHandler.GESTURE_TAP_CANCEL));
        event = MotionEvent.obtain(
                eventTime + 10, eventTime + 20, MotionEvent.ACTION_UP,
                FAKE_COORD_X, FAKE_COORD_Y, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertFalse(mGestureHandler.needsTapEndingEventForTesting());

        // A scroll event will trigger a tap-ending (cancel) event.
        downTime += 25;
        eventTime += 25;
        mockDelegate.mGestureTypeList.clear();
        event = MotionEvent.obtain(
                downTime, downTime, MotionEvent.ACTION_DOWN,
                FAKE_COORD_X, FAKE_COORD_Y, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertTrue(mGestureHandler.needsTapEndingEventForTesting());
        event = MotionEvent.obtain(
                downTime, eventTime + 5, MotionEvent.ACTION_MOVE,
                FAKE_COORD_X, FAKE_COORD_Y + 100, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertTrue(mockDelegate.mGestureTypeList.contains(
                ContentViewGestureHandler.GESTURE_SCROLL_START));
        assertTrue(mockDelegate.mGestureTypeList.contains(
                ContentViewGestureHandler.GESTURE_TAP_CANCEL));
        assertFalse("A scroll should terminate the tap down.",
                   mGestureHandler.needsTapEndingEventForTesting());
        assertFalse(mGestureHandler.needsTapEndingEventForTesting());
    }

    /**
     * Verify that touch move events are properly coalesced.
     * @throws Exception
     */
    @SmallTest
    @Feature({"Gestures"})
    public void testTouchMoveCoalescing() throws Exception {
        final long downTime = SystemClock.uptimeMillis();
        final long eventTime = SystemClock.uptimeMillis();

        mGestureHandler.hasTouchEventHandlers(true);

        MotionEvent event = motionEvent(MotionEvent.ACTION_DOWN, downTime, downTime);
        assertTrue(mGestureHandler.onTouchEvent(event));
        mGestureHandler.confirmTouchEvent(ContentViewGestureHandler.INPUT_EVENT_ACK_STATE_CONSUMED);
        assertEquals(TouchPoint.TOUCH_EVENT_TYPE_START, mMockMotionEventDelegate.mLastTouchAction);

        event = MotionEvent.obtain(
                downTime, eventTime + 5, MotionEvent.ACTION_MOVE,
                FAKE_COORD_X * 5, FAKE_COORD_Y * 5, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertFalse("Should not have a pending LONG_PRESS", mLongPressDetector.hasPendingMessage());
        assertEquals("Initial move events should offered to javascript and added to the queue",
                1, mGestureHandler.getNumberOfPendingMotionEventsForTesting());
        assertEquals(TouchPoint.TOUCH_EVENT_TYPE_MOVE, mMockMotionEventDelegate.mLastTouchAction);

        event = MotionEvent.obtain(
                downTime, eventTime + 10, MotionEvent.ACTION_MOVE,
                FAKE_COORD_X * 10, FAKE_COORD_Y * 10, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals("Move events already sent to javascript should not be coalesced",
                2, mGestureHandler.getNumberOfPendingMotionEventsForTesting());

        event = MotionEvent.obtain(
                downTime, eventTime + 15, MotionEvent.ACTION_MOVE,
                FAKE_COORD_X * 15, FAKE_COORD_Y * 15, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals("Similar pending move events should be coalesced",
                2, mGestureHandler.getNumberOfPendingMotionEventsForTesting());

        PointerProperties pp1 = new PointerProperties();
        pp1.id = 0;
        pp1.toolType = MotionEvent.TOOL_TYPE_FINGER;
        PointerProperties pp2 = new PointerProperties();
        pp2.id = 1;
        pp2.toolType = MotionEvent.TOOL_TYPE_FINGER;
        PointerProperties[] properties = new PointerProperties[] { pp1, pp2 };

        PointerCoords pc1 = new PointerCoords();
        pc1.x = FAKE_COORD_X * 10;
        pc1.y = FAKE_COORD_Y * 10;
        pc1.pressure = 1;
        pc1.size = 1;
        PointerCoords pc2 = new PointerCoords();
        pc2.x = FAKE_COORD_X * 15;
        pc2.y = FAKE_COORD_Y * 15;
        pc2.pressure = 1;
        pc2.size = 1;
        PointerCoords[] coords = new PointerCoords[] { pc1, pc2 };

        event = MotionEvent.obtain(
                downTime, eventTime + 20, MotionEvent.ACTION_MOVE,
                2, properties, coords, 0, 0, 1.0f, 1.0f, 0, 0, 0, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals("Move events with different pointer counts should not be coalesced",
                3, mGestureHandler.getNumberOfPendingMotionEventsForTesting());

        event = MotionEvent.obtain(
                downTime, eventTime + 25, MotionEvent.ACTION_MOVE,
                2, properties, coords, 0, 0, 1.0f, 1.0f, 0, 0, 0, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals("Move events with similar pointer counts should be coalesced",
                3, mGestureHandler.getNumberOfPendingMotionEventsForTesting());

        event = motionEvent(MotionEvent.ACTION_DOWN, downTime, downTime);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals("Move events should not be coalesced with other events",
                4, mGestureHandler.getNumberOfPendingMotionEventsForTesting());
    }

    /**
     * Verify that synchronous confirmTouchEvent() calls made from the MotionEventDelegate behave
     * properly.
     * @throws Exception
     */
    @SmallTest
    @Feature({"Gestures"})
    public void testSynchronousConfirmTouchEvent() throws Exception {
        final long downTime = SystemClock.uptimeMillis();
        final long eventTime = SystemClock.uptimeMillis();

        mGestureHandler.hasTouchEventHandlers(true);

        mMockMotionEventDelegate.disableSynchronousConfirmTouchEvent();

        // Queue an asynchronously handled event.
        MotionEvent event = motionEvent(MotionEvent.ACTION_DOWN, downTime, downTime);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals(1, mGestureHandler.getNumberOfPendingMotionEventsForTesting());

        // Queue another event; this will remain in the queue until the first event is confirmed.
        event = MotionEvent.obtain(
                downTime, eventTime + 5, MotionEvent.ACTION_MOVE,
                FAKE_COORD_X * 10, FAKE_COORD_Y * 10, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals(2, mGestureHandler.getNumberOfPendingMotionEventsForTesting());

        // Enable synchronous event confirmation upon dispatch.
        mMockMotionEventDelegate.enableSynchronousConfirmTouchEvent(
                mGestureHandler, ContentViewGestureHandler.INPUT_EVENT_ACK_STATE_CONSUMED);

        // Confirm the original event; this should dispatch the second event and confirm it
        // synchronously.
        mGestureHandler.confirmTouchEvent(
                ContentViewGestureHandler.INPUT_EVENT_ACK_STATE_CONSUMED);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals(0, mGestureHandler.getNumberOfPendingMotionEventsForTesting());
        assertEquals(TouchPoint.TOUCH_EVENT_TYPE_MOVE, mMockMotionEventDelegate.mLastTouchAction);

        // Adding events to any empty queue will trigger synchronous dispatch and confirmation.
        event = MotionEvent.obtain(
                downTime, eventTime + 10, MotionEvent.ACTION_MOVE,
                FAKE_COORD_X * 5, FAKE_COORD_Y * 5, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals(0, mGestureHandler.getNumberOfPendingMotionEventsForTesting());
    }

    /**
     * Verify that no double tap gestures are created if the gesture handler is
     * told to disable double tap gesture detection (according to the logic in
     * ContentViewCore.onRenderCoordinatesUpdated).
     * @throws Exception
     */
    @SmallTest
    @Feature({"Gestures"})
    public void testNoDoubleTapWhenDoubleTapDisabled() throws Exception {
        final long downTime = SystemClock.uptimeMillis();
        final long eventTime = SystemClock.uptimeMillis();

        GestureRecordingMotionEventDelegate mockDelegate =
                new GestureRecordingMotionEventDelegate();
        mGestureHandler = new ContentViewGestureHandler(
                getInstrumentation().getTargetContext(), mockDelegate, mMockZoomManager);
        mGestureHandler.updateShouldDisableDoubleTap(true);

        MotionEvent event = MotionEvent.obtain(
                downTime, downTime, MotionEvent.ACTION_DOWN,
                FAKE_COORD_X, FAKE_COORD_Y, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals("Only GESTURE_TAP_DOWN should have been sent",
                1, mockDelegate.mGestureTypeList.size());

        event = MotionEvent.obtain(
                downTime, eventTime + 5, MotionEvent.ACTION_UP,
                FAKE_COORD_X, FAKE_COORD_Y, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals("A GESTURE_SINGLE_TAP_CONFIRMED event should have been sent",
                ContentViewGestureHandler.GESTURE_SINGLE_TAP_CONFIRMED,
                mockDelegate.mMostRecentGestureEvent.mType);
        assertEquals("Only GESTURE_TAP_DOWN and GESTURE_SINGLE_TAP_CONFIRMED " +
                "should have been sent",
                2, mockDelegate.mGestureTypeList.size());

        event = MotionEvent.obtain(
                eventTime + 10, eventTime + 10, MotionEvent.ACTION_DOWN,
                FAKE_COORD_X, FAKE_COORD_Y, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals("Only GESTURE_TAP_DOWN, " +
                "GESTURE_SINGLE_TAP_CONFIRMED and " +
                "GESTURE_TAP_DOWN should have been sent",
                3, mockDelegate.mGestureTypeList.size());

        event = MotionEvent.obtain(
                eventTime + 10, eventTime + 15, MotionEvent.ACTION_UP,
                FAKE_COORD_X, FAKE_COORD_Y, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals("A double tap should not have occurred",
                ContentViewGestureHandler.GESTURE_SINGLE_TAP_CONFIRMED,
                mockDelegate.mMostRecentGestureEvent.mType);
        assertEquals("Only GESTURE_TAP_DOWN, " +
                "GESTURE_SINGLE_TAP_CONFIRMED, " +
                "GESTURE_TAP_DOWN and " +
                "GESTURE_SINGLE_TAP_CONFIRMED should have been sent",
                4, mockDelegate.mGestureTypeList.size());
    }

    /**
     * Verify that double tap drag zoom feature is not invoked when the gesture
     * handler is told to disable double tap gesture detection (according to the
     * logic in ContentViewCore.onRenderCoordinatesUpdated).
     * The second tap sequence should be treated just as the first would be.
     * @throws Exception
     */
    @SmallTest
    @Feature({"Gestures"})
    public void testNoDoubleTapDragZoomWhenDoubleTapDisabled() throws Exception {
        final long downTime1 = SystemClock.uptimeMillis();
        final long downTime2 = downTime1 + 100;

        GestureRecordingMotionEventDelegate mockDelegate =
                new GestureRecordingMotionEventDelegate();
        mGestureHandler = new ContentViewGestureHandler(
                getInstrumentation().getTargetContext(), mockDelegate, mMockZoomManager);
        mGestureHandler.updateShouldDisableDoubleTap(true);

        MotionEvent event = motionEvent(MotionEvent.ACTION_DOWN, downTime1, downTime1);
        assertTrue(mGestureHandler.onTouchEvent(event));

        event = MotionEvent.obtain(
                downTime1, downTime1 + 5, MotionEvent.ACTION_UP,
                FAKE_COORD_X, FAKE_COORD_Y, 0);
        mGestureHandler.onTouchEvent(event);

        event = MotionEvent.obtain(
                downTime2, downTime2, MotionEvent.ACTION_DOWN,
                FAKE_COORD_X, FAKE_COORD_Y, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));

        event = MotionEvent.obtain(
                downTime2, downTime2 + 5, MotionEvent.ACTION_MOVE,
                FAKE_COORD_X, FAKE_COORD_Y + 100, 0);

        // The move should become a scroll, as double tap and drag to zoom is
        // disabled.
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertTrue("GESTURE_SCROLL_START should have been sent",
                mockDelegate.mGestureTypeList.contains(
                        ContentViewGestureHandler.GESTURE_SCROLL_START));
        assertFalse("No GESTURE_PINCH_BEGIN should have been sent",
                mockDelegate.mGestureTypeList.contains(
                        ContentViewGestureHandler.GESTURE_PINCH_BEGIN));

        event = MotionEvent.obtain(
                downTime2, downTime2 + 10, MotionEvent.ACTION_MOVE,
                FAKE_COORD_X, FAKE_COORD_Y + 200, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals("GESTURE_SCROLL_BY should have been sent",
                ContentViewGestureHandler.GESTURE_SCROLL_BY,
                mockDelegate.mMostRecentGestureEvent.mType);
        assertEquals("GESTURE_SCROLL_BY should have been sent",
                event.getEventTime(),
                mockDelegate.mMostRecentGestureEvent.getTimeMs());
        assertTrue("No GESTURE_PINCH_BY should have been sent",
                ContentViewGestureHandler.GESTURE_PINCH_BY !=
                mockDelegate.mMostRecentGestureEvent.mType);

        event = MotionEvent.obtain(
                downTime2, downTime2 + 15, MotionEvent.ACTION_UP,
                FAKE_COORD_X, FAKE_COORD_Y + 200, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertFalse("No GESTURE_PINCH_END should have been sent",
                mockDelegate.mGestureTypeList.contains(
                        ContentViewGestureHandler.GESTURE_PINCH_END));
    }

    /**
     * Verify that setting a fixed page scale (or a mobile viewport) during a double
     * tap drag zoom disables double tap detection after the gesture has ended.
     * @throws Exception
     */
    @SmallTest
    @Feature({"Gestures"})
    public void testFixedPageScaleDuringDoubleTapDragZoom() throws Exception {
        long downTime1 = SystemClock.uptimeMillis();
        long downTime2 = downTime1 + 100;

        GestureRecordingMotionEventDelegate mockDelegate =
                new GestureRecordingMotionEventDelegate();
        mGestureHandler = new ContentViewGestureHandler(
                getInstrumentation().getTargetContext(), mockDelegate, mMockZoomManager);
        mLongPressDetector = new LongPressDetector(
                getInstrumentation().getTargetContext(), mGestureHandler);

        // Start a double-tap drag gesture.
        MotionEvent event = motionEvent(MotionEvent.ACTION_DOWN, downTime1, downTime1);
        assertTrue(mGestureHandler.onTouchEvent(event));
        mGestureHandler.sendShowPressedStateGestureForTesting();
        event = MotionEvent.obtain(
                downTime1, downTime1 + 5, MotionEvent.ACTION_UP,
                FAKE_COORD_X, FAKE_COORD_Y, 0);
        mGestureHandler.onTouchEvent(event);
        event = MotionEvent.obtain(
                downTime2, downTime2, MotionEvent.ACTION_DOWN,
                FAKE_COORD_X, FAKE_COORD_Y, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        event = MotionEvent.obtain(
                downTime2, downTime2 + 5, MotionEvent.ACTION_MOVE,
                FAKE_COORD_X, FAKE_COORD_Y + 100, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertTrue("GESTURE_SCROLL_START should have been sent",
                mockDelegate.mGestureTypeList.contains(
                        ContentViewGestureHandler.GESTURE_SCROLL_START));
        assertEquals("GESTURE_PINCH_BEGIN should have been sent",
                ContentViewGestureHandler.GESTURE_PINCH_BEGIN,
                mockDelegate.mMostRecentGestureEvent.mType);

        // Simulate setting a fixed page scale (or a mobile viewport);
        // this should not disrupt the current double-tap gesture.
        mGestureHandler.updateShouldDisableDoubleTap(true);

        // Double tap zoom updates should continue.
        event = MotionEvent.obtain(
                downTime2, downTime2 + 10, MotionEvent.ACTION_MOVE,
                FAKE_COORD_X, FAKE_COORD_Y + 200, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertTrue("GESTURE_SCROLL_BY should have been sent",
                mockDelegate.mGestureTypeList.contains(
                        ContentViewGestureHandler.GESTURE_SCROLL_BY));
        assertEquals("GESTURE_PINCH_BY should have been sent",
                ContentViewGestureHandler.GESTURE_PINCH_BY,
                mockDelegate.mMostRecentGestureEvent.mType);
        event = MotionEvent.obtain(
                downTime2, downTime2 + 15, MotionEvent.ACTION_UP,
                FAKE_COORD_X, FAKE_COORD_Y + 200, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertTrue("GESTURE_PINCH_END should have been sent",
                mockDelegate.mGestureTypeList.contains(
                        ContentViewGestureHandler.GESTURE_PINCH_END));
        assertEquals("GESTURE_SCROLL_END should have been sent",
                ContentViewGestureHandler.GESTURE_SCROLL_END,
                mockDelegate.mMostRecentGestureEvent.mType);

        // The double-tap gesture has finished, but the page scale is fixed.
        // The same event sequence should not generate any double tap getsures.
        mockDelegate.mGestureTypeList.clear();
        downTime1 += 200;
        downTime2 += 200;

        // Start a double-tap drag gesture.
        event = motionEvent(MotionEvent.ACTION_DOWN, downTime1, downTime1);
        assertTrue(mGestureHandler.onTouchEvent(event));
        event = MotionEvent.obtain(
                downTime1, downTime1 + 5, MotionEvent.ACTION_UP,
                FAKE_COORD_X, FAKE_COORD_Y, 0);
        mGestureHandler.onTouchEvent(event);
        event = MotionEvent.obtain(
                downTime2, downTime2, MotionEvent.ACTION_DOWN,
                FAKE_COORD_X, FAKE_COORD_Y, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        event = MotionEvent.obtain(
                downTime2, downTime2 + 5, MotionEvent.ACTION_MOVE,
                FAKE_COORD_X, FAKE_COORD_Y + 100, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertTrue("GESTURE_SCROLL_START should have been sent",
                mockDelegate.mGestureTypeList.contains(
                        ContentViewGestureHandler.GESTURE_SCROLL_START));
        assertFalse("GESTURE_PINCH_BEGIN should not have been sent",
                mockDelegate.mGestureTypeList.contains(
                        ContentViewGestureHandler.GESTURE_PINCH_BEGIN));

        // Double tap zoom updates should not be sent.
        // Instead, the second tap drag becomes a scroll gesture sequence.
        event = MotionEvent.obtain(
                downTime2, downTime2 + 10, MotionEvent.ACTION_MOVE,
                FAKE_COORD_X, FAKE_COORD_Y + 200, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertTrue("GESTURE_SCROLL_BY should have been sent",
                mockDelegate.mGestureTypeList.contains(
                        ContentViewGestureHandler.GESTURE_SCROLL_BY));
        assertFalse("GESTURE_PINCH_BY should not have been sent",
                mockDelegate.mGestureTypeList.contains(
                        ContentViewGestureHandler.GESTURE_PINCH_BY));
        event = MotionEvent.obtain(
                downTime2, downTime2 + 15, MotionEvent.ACTION_UP,
                FAKE_COORD_X, FAKE_COORD_Y + 200, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertFalse("GESTURE_PINCH_END should not have been sent",
                mockDelegate.mGestureTypeList.contains(
                        ContentViewGestureHandler.GESTURE_PINCH_END));
    }

    /**
     * Verify that a secondary pointer press with no consumer does not interfere
     * with Javascript touch handling.
     *
     * @throws Exception
     */
    @SmallTest
    @Feature({"Gestures"})
    public void testSecondaryPointerWithNoConsumer() throws Exception {
        final long downTime = SystemClock.uptimeMillis();
        final long eventTime = SystemClock.uptimeMillis();

        mGestureHandler.hasTouchEventHandlers(true);

        // Queue a primary pointer press, a secondary pointer press and release,
        // and a primary pointer move.
        MotionEvent event = motionEvent(MotionEvent.ACTION_DOWN, downTime, downTime);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals(1, mGestureHandler.getNumberOfPendingMotionEventsForTesting());

        event = motionEvent(MotionEvent.ACTION_POINTER_DOWN, downTime, eventTime);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals(2, mGestureHandler.getNumberOfPendingMotionEventsForTesting());

        event = motionEvent(MotionEvent.ACTION_POINTER_UP, downTime, eventTime + 10);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals(3, mGestureHandler.getNumberOfPendingMotionEventsForTesting());

        event = MotionEvent.obtain(
                downTime, eventTime + 15, MotionEvent.ACTION_MOVE,
                FAKE_COORD_X * 5, FAKE_COORD_Y * 5, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals(4, mGestureHandler.getNumberOfPendingMotionEventsForTesting());

        // Simulate preventDefault from Javascript, forcing all touch events to Javascript
        // for the current sequence.
        mGestureHandler.confirmTouchEvent(
                ContentViewGestureHandler.INPUT_EVENT_ACK_STATE_CONSUMED);
        assertEquals(3, mGestureHandler.getNumberOfPendingMotionEventsForTesting());

        mGestureHandler.confirmTouchEvent(
                ContentViewGestureHandler.INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS);
        assertEquals("Even if the secondary pointer has no consumer, continue sending events",
                2, mGestureHandler.getNumberOfPendingMotionEventsForTesting());

        mGestureHandler.confirmTouchEvent(
                ContentViewGestureHandler.INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS);
        assertEquals("Even if the secondary pointer has no consumer, continue sending events",
                1, mGestureHandler.getNumberOfPendingMotionEventsForTesting());

        mGestureHandler.confirmTouchEvent(
                ContentViewGestureHandler.INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
        assertEquals(0, mGestureHandler.getNumberOfPendingMotionEventsForTesting());

        assertEquals("No gestures should result from the Javascript-consumed sequence",
                0, mMockMotionEventDelegate.mTotalSentGestureCount);
    }

    /**
     * Verify that multiple touch sequences in the queue are handled properly when
     * the Javascript response is different for each.
     *
     * @throws Exception
     */
    @SmallTest
    @Feature({"Gestures"})
    public void testMultiplyEnqueuedTouches() throws Exception {
        final long downTime = SystemClock.uptimeMillis();
        final long eventTime = SystemClock.uptimeMillis();

        mGestureHandler.hasTouchEventHandlers(true);
        mGestureHandler.updateDoubleTapSupport(false);

        // Queue a tap sequence.
        MotionEvent event = motionEvent(MotionEvent.ACTION_DOWN, downTime, downTime);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals(1, mGestureHandler.getNumberOfPendingMotionEventsForTesting());

        event = motionEvent(MotionEvent.ACTION_UP, downTime, eventTime + 5);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals(2, mGestureHandler.getNumberOfPendingMotionEventsForTesting());

        // Queue a scroll sequence.
        event = motionEvent(MotionEvent.ACTION_DOWN, downTime + 10, downTime + 10);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals(3, mGestureHandler.getNumberOfPendingMotionEventsForTesting());

        event = MotionEvent.obtain(
                downTime + 10, eventTime + 15, MotionEvent.ACTION_MOVE,
                FAKE_COORD_X * 5, FAKE_COORD_Y * 5, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertFalse(mGestureHandler.isNativeScrolling());
        assertEquals(4, mGestureHandler.getNumberOfPendingMotionEventsForTesting());

        event = motionEvent(MotionEvent.ACTION_UP, downTime + 10, downTime + 20);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals(5, mGestureHandler.getNumberOfPendingMotionEventsForTesting());

        // Consume the first gesture.
        mGestureHandler.confirmTouchEvent(
                ContentViewGestureHandler.INPUT_EVENT_ACK_STATE_CONSUMED);
        assertEquals(4, mGestureHandler.getNumberOfPendingMotionEventsForTesting());

        mGestureHandler.confirmTouchEvent(
                ContentViewGestureHandler.INPUT_EVENT_ACK_STATE_CONSUMED);
        assertEquals(3, mGestureHandler.getNumberOfPendingMotionEventsForTesting());

        // Don't consume the second gesture; it should be fed to the gesture detector.
        mGestureHandler.confirmTouchEvent(
                ContentViewGestureHandler.INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
        assertEquals(2, mGestureHandler.getNumberOfPendingMotionEventsForTesting());
        assertEquals("The down touch event should have been sent to the gesture detector",
                MotionEvent.ACTION_DOWN, mMockGestureDetector.mLastEvent.getActionMasked());
        assertFalse(mGestureHandler.isNativeScrolling());

        mGestureHandler.confirmTouchEvent(
                ContentViewGestureHandler.INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
        assertEquals("The move touch event should have been sent to the gesture detector",
                MotionEvent.ACTION_MOVE, mMockGestureDetector.mLastEvent.getActionMasked());
    }

    /**
     * Verify that only complete gestures are forwarded to Javascript if we receive
     * a touch handler notification.
     * @throws Exception
     */
    @SmallTest
    @Feature({"Gestures"})
    public void testOnlyCompleteGesturesForwardedToTouchHandler() throws Exception {
        final long downTime = SystemClock.uptimeMillis();
        final long eventTime = SystemClock.uptimeMillis();

        mGestureHandler.hasTouchEventHandlers(false);

        MotionEvent event = motionEvent(MotionEvent.ACTION_DOWN, downTime, downTime);
        mGestureHandler.onTouchEvent(event);
        assertEquals("Initial down events should not be sent to Javascript without a touch handler",
                0, mGestureHandler.getNumberOfPendingMotionEventsForTesting());
        assertTrue("Should have a pending gesture", mMockGestureDetector.mLastEvent != null);
        mMockGestureDetector.mLastEvent = null;

        mGestureHandler.hasTouchEventHandlers(true);

        event = MotionEvent.obtain(
                downTime, eventTime + 5, MotionEvent.ACTION_MOVE,
                FAKE_COORD_X * 5, FAKE_COORD_Y * 5, 0);
        mGestureHandler.onTouchEvent(event);
        assertEquals("A move event should only be offered to javascript if the down was offered",
                0, mGestureHandler.getNumberOfPendingMotionEventsForTesting());
        assertTrue("Should have a pending gesture", mMockGestureDetector.mLastEvent != null);

        event = motionEvent(MotionEvent.ACTION_POINTER_DOWN, downTime, eventTime + 10);
        mGestureHandler.onTouchEvent(event);
        assertEquals("A pointer event should only be offered to Javascript if the down was offered",
                0, mGestureHandler.getNumberOfPendingMotionEventsForTesting());

        // Ensure that redundant notifications have no effect.
        mGestureHandler.hasTouchEventHandlers(true);

        event = motionEvent(MotionEvent.ACTION_POINTER_UP, downTime, eventTime + 15);
        mGestureHandler.onTouchEvent(event);
        assertEquals("A pointer event should only be offered to Javascript if the down was offered",
                0, mGestureHandler.getNumberOfPendingMotionEventsForTesting());

        event = motionEvent(MotionEvent.ACTION_UP, downTime, eventTime + 20);
        mGestureHandler.onTouchEvent(event);
        assertEquals("A pointer event should only be offered to Javascript if the down was offered",
                0, mGestureHandler.getNumberOfPendingMotionEventsForTesting());

        event = motionEvent(MotionEvent.ACTION_DOWN, downTime + 25, downTime + 25);
        mGestureHandler.onTouchEvent(event);
        assertEquals("A down event should be offered to Javascript with a registered touch handler",
                1, mGestureHandler.getNumberOfPendingMotionEventsForTesting());
    }

    /**
     * Verify that no timeout-based gestures are triggered after a touch event
     * is consumed.  In particular, LONG_PRESS and SHOW_PRESS should not fire
     * if TouchStart went unconsumed, but subsequent TouchMoves are consumed.
     *
     * @throws Exception
     */
    @SmallTest
    @Feature({"Gestures"})
    public void testNoTimeoutGestureAfterTouchConsumed() throws Exception {
        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                setUp();

                final long downTime = SystemClock.uptimeMillis();
                final long eventTime = SystemClock.uptimeMillis();

                mGestureHandler.hasTouchEventHandlers(true);

                MotionEvent event = motionEvent(MotionEvent.ACTION_DOWN, downTime, eventTime);
                assertTrue(mGestureHandler.onTouchEvent(event));
                mGestureHandler.confirmTouchEvent(
                        ContentViewGestureHandler.INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
                assertTrue("Should have a pending LONG_PRESS",
                        mLongPressDetector.hasPendingMessage());

                event = MotionEvent.obtain(
                        downTime, eventTime + 10, MotionEvent.ACTION_MOVE,
                        FAKE_COORD_X, FAKE_COORD_Y + 200, 0);
                assertTrue(mGestureHandler.onTouchEvent(event));
                mGestureHandler.confirmTouchEvent(
                        ContentViewGestureHandler.INPUT_EVENT_ACK_STATE_CONSUMED);
                assertFalse("Should not have a pending LONG_PRESS",
                        mLongPressDetector.hasPendingMessage());
            }
        });
        assertFalse(mMockListener.mShowPressCalled.await(
                ScalableTimeout.ScaleTimeout(ViewConfiguration.getTapTimeout() + 10),
                TimeUnit.MILLISECONDS));
        assertFalse(mMockListener.mLongPressCalled.await(
                ScalableTimeout.ScaleTimeout(ViewConfiguration.getLongPressTimeout() + 10),
                TimeUnit.MILLISECONDS));
    }

    /**
     * Verify that a TAP_DOWN will be followed by a TAP_CANCEL if the first
     * touch is unconsumed, but the subsequent touch is consumed.
     *
     * @throws Exception
     */
    @SmallTest
    @Feature({"Gestures"})
    public void testTapCancelledAfterTouchConsumed() throws Exception {
        final long downTime = SystemClock.uptimeMillis();
        final long eventTime = SystemClock.uptimeMillis();

        GestureRecordingMotionEventDelegate mockDelegate =
                new GestureRecordingMotionEventDelegate();
        mGestureHandler = new ContentViewGestureHandler(
                getInstrumentation().getTargetContext(), mockDelegate, mMockZoomManager);
        mGestureHandler.hasTouchEventHandlers(true);

        MotionEvent event = motionEvent(MotionEvent.ACTION_DOWN, downTime, downTime);
        assertTrue(mGestureHandler.onTouchEvent(event));
        mGestureHandler.confirmTouchEvent(
                ContentViewGestureHandler.INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
        assertEquals("A TAP_DOWN gesture should have been sent",
                ContentViewGestureHandler.GESTURE_TAP_DOWN,
                        mockDelegate.mMostRecentGestureEvent.mType);

        event = MotionEvent.obtain(
                downTime, eventTime + 10, MotionEvent.ACTION_MOVE,
                FAKE_COORD_X, FAKE_COORD_Y + 200, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        mGestureHandler.confirmTouchEvent(
                ContentViewGestureHandler.INPUT_EVENT_ACK_STATE_CONSUMED);
        assertEquals("A TAP_CANCEL gesture should have been sent",
                ContentViewGestureHandler.GESTURE_TAP_CANCEL,
                        mockDelegate.mMostRecentGestureEvent.mType);

        event = MotionEvent.obtain(
                downTime, eventTime + 15, MotionEvent.ACTION_MOVE,
                FAKE_COORD_X, FAKE_COORD_Y + 400, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        mGestureHandler.confirmTouchEvent(
                ContentViewGestureHandler.INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
        assertEquals("No further gestures should be sent",
                ContentViewGestureHandler.GESTURE_TAP_CANCEL,
                        mockDelegate.mMostRecentGestureEvent.mType);
    }
}
