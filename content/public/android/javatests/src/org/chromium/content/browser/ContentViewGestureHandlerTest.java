// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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

import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.Deque;
import java.util.concurrent.CountDownLatch;

import org.chromium.base.test.util.Feature;
import org.chromium.content.browser.ContentViewGestureHandler.MotionEventDelegate;
import org.chromium.content.browser.third_party.GestureDetector;
import org.chromium.content.browser.third_party.GestureDetector.SimpleOnGestureListener;

/**
 * Test suite for ContentViewGestureHandler.
 */
public class ContentViewGestureHandlerTest extends InstrumentationTestCase {
    private static final int FAKE_COORD_X = 42;
    private static final int FAKE_COORD_Y = 24;

    private static final String TAG = ContentViewGestureHandler.class.toString();
    private MockListener mMockListener;
    private MockGestureDetector mMockGestureDetector;
    private ContentViewGestureHandler mGestureHandler;
    private LongPressDetector mLongPressDetector;

    static class MockListener extends GestureDetector.SimpleOnGestureListener {
        MotionEvent mLastLongPress;
        MotionEvent mLastSingleTap;
        MotionEvent mLastFling1;
        CountDownLatch mLongPressCalled;

        public MockListener() {
            mLongPressCalled = new CountDownLatch(1);
        }

        @Override
        public void onLongPress(MotionEvent e) {
            mLastLongPress = MotionEvent.obtain(e);
            mLongPressCalled.countDown();
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
        @Override
        public boolean sendTouchEvent(long timeMs, int action, TouchPoint[] pts) {
            return true;
        }

        @Override
        public boolean sendGesture(int type, long timeMs, int x, int y, Bundle extraParams) {
            Log.i(TAG,"Gesture event received with type id " + type);
            return true;
        }

        @Override
        public boolean didUIStealScroll(float x, float y) {
            // Not implemented.
            return false;
        }

        @Override
        public void invokeZoomPicker() {
            // Not implemented.
        }

        @Override
        public boolean hasFixedPageScale() {
            return false;
        }
    }

    static class MockZoomManager extends ZoomManager {
        MockZoomManager(Context context, ContentViewCore contentViewCore) {
            super(context, contentViewCore);
        }

        @Override
        public boolean processTouchEvent(MotionEvent event) {
            return false;
        }
    }

    private MotionEvent motionEvent(int action, long downTime, long eventTime) {
        return MotionEvent.obtain(downTime, eventTime, action, FAKE_COORD_X, FAKE_COORD_Y, 0);
    }

    @Override
    public void setUp() {
        mMockListener = new MockListener();
        mMockGestureDetector = new MockGestureDetector(
                getInstrumentation().getTargetContext(), mMockListener);
        mGestureHandler = new ContentViewGestureHandler(
                getInstrumentation().getTargetContext(), new MockMotionEventDelegate(),
                new MockZoomManager(getInstrumentation().getTargetContext(), null));
        mLongPressDetector = new LongPressDetector(
                getInstrumentation().getTargetContext(), mGestureHandler);
        mGestureHandler.setTestDependencies(
                mLongPressDetector, mMockGestureDetector, mMockListener);
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
        assertEquals(1, mGestureHandler.getNumberOfPendingMotionEvents());
        assertTrue("Should not have a pending gesture", mMockGestureDetector.mLastEvent == null);
        assertFalse("Should not have a pending LONG_PRESS", mLongPressDetector.hasPendingMessage());

        event = MotionEvent.obtain(
                downTime, eventTime + 5, MotionEvent.ACTION_MOVE,
                FAKE_COORD_X * 5, FAKE_COORD_Y * 5, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals(2, mGestureHandler.getNumberOfPendingMotionEvents());

        event = MotionEvent.obtain(
                downTime, eventTime + 10, MotionEvent.ACTION_MOVE,
                FAKE_COORD_X * 10, FAKE_COORD_Y * 10, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals("We should have coalesced move events into one"
                , 2, mGestureHandler.getNumberOfPendingMotionEvents());

        event = MotionEvent.obtain(
                downTime, eventTime + 15, MotionEvent.ACTION_UP,
                FAKE_COORD_X * 10, FAKE_COORD_Y * 10, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals(3, mGestureHandler.getNumberOfPendingMotionEvents());

        mGestureHandler.confirmTouchEvent(
                ContentViewGestureHandler.INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
        assertEquals(2, mGestureHandler.getNumberOfPendingMotionEvents());
        assertEquals(MotionEvent.ACTION_MOVE,
                mGestureHandler.peekFirstInPendingMotionEvents().getActionMasked());
        assertFalse("Pending LONG_PRESS should have been canceled",
                mLongPressDetector.hasPendingMessage());

        mGestureHandler.confirmTouchEvent(ContentViewGestureHandler.INPUT_EVENT_ACK_STATE_CONSUMED);
        assertEquals(1, mGestureHandler.getNumberOfPendingMotionEvents());
        assertEquals(MotionEvent.ACTION_UP,
                mGestureHandler.peekFirstInPendingMotionEvents().getActionMasked());

        mGestureHandler.confirmTouchEvent(ContentViewGestureHandler.INPUT_EVENT_ACK_STATE_CONSUMED);
        assertEquals(0, mGestureHandler.getNumberOfPendingMotionEvents());

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
        assertEquals(1, mGestureHandler.getNumberOfPendingMotionEvents());
        assertTrue("Should not have a pending gesture", mMockGestureDetector.mLastEvent == null);
        assertFalse("Should not have a pending LONG_PRESS", mLongPressDetector.hasPendingMessage());

        event = MotionEvent.obtain(
                downTime, eventTime + 5, MotionEvent.ACTION_MOVE,
                FAKE_COORD_X * 5, FAKE_COORD_Y * 5, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals(2, mGestureHandler.getNumberOfPendingMotionEvents());

        event = MotionEvent.obtain(
                downTime, eventTime + 10, MotionEvent.ACTION_MOVE,
                FAKE_COORD_X * 10, FAKE_COORD_Y * 10, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals("We should have coalesced move events into one"
                , 2, mGestureHandler.getNumberOfPendingMotionEvents());

        event = MotionEvent.obtain(
                downTime, eventTime + 15, MotionEvent.ACTION_UP,
                FAKE_COORD_X * 10, FAKE_COORD_Y * 10, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals(3, mGestureHandler.getNumberOfPendingMotionEvents());

        mGestureHandler.confirmTouchEvent(
                ContentViewGestureHandler.INPUT_EVENT_ACK_STATE_CONSUMED);
        assertEquals(2, mGestureHandler.getNumberOfPendingMotionEvents());
        assertEquals(MotionEvent.ACTION_MOVE,
                mGestureHandler.peekFirstInPendingMotionEvents().getActionMasked());

        mGestureHandler.confirmTouchEvent(
                ContentViewGestureHandler.INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
        assertEquals(1, mGestureHandler.getNumberOfPendingMotionEvents());
        assertEquals(MotionEvent.ACTION_UP,
                mGestureHandler.peekFirstInPendingMotionEvents().getActionMasked());
        assertTrue("Even though the last event was not consumed by JavaScript," +
                "it shouldn't have been sent to the Gesture Detector",
                        mMockGestureDetector.mLastEvent == null);

        mGestureHandler.confirmTouchEvent(ContentViewGestureHandler.INPUT_EVENT_ACK_STATE_CONSUMED);
        assertEquals(0, mGestureHandler.getNumberOfPendingMotionEvents());

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
                new MockZoomManager(getInstrumentation().getTargetContext(), null));
        mLongPressDetector = new LongPressDetector(
                getInstrumentation().getTargetContext(), mGestureHandler);

        MotionEvent event = motionEvent(MotionEvent.ACTION_DOWN, downTime, downTime);

        mGestureHandler.hasTouchEventHandlers(true);

        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals(1, mGestureHandler.getNumberOfPendingMotionEvents());
        assertFalse("Should not have a pending LONG_PRESS", mLongPressDetector.hasPendingMessage());

        event = MotionEvent.obtain(
                downTime, eventTime + 5, MotionEvent.ACTION_MOVE,
                FAKE_COORD_X * 5, FAKE_COORD_Y * 5, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals(2, mGestureHandler.getNumberOfPendingMotionEvents());

        event = MotionEvent.obtain(
                downTime, eventTime + 10, MotionEvent.ACTION_MOVE,
                FAKE_COORD_X * 10, FAKE_COORD_Y * 10, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals(2, mGestureHandler.getNumberOfPendingMotionEvents());

        event = MotionEvent.obtain(
                downTime, eventTime + 15, MotionEvent.ACTION_UP,
                FAKE_COORD_X * 10, FAKE_COORD_Y * 10, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals(3, mGestureHandler.getNumberOfPendingMotionEvents());

        event = motionEvent(MotionEvent.ACTION_DOWN, eventTime + 20, eventTime + 20);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals(4, mGestureHandler.getNumberOfPendingMotionEvents());

        event = MotionEvent.obtain(
                downTime, eventTime + 20, MotionEvent.ACTION_UP,
                FAKE_COORD_X, FAKE_COORD_Y, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals(5, mGestureHandler.getNumberOfPendingMotionEvents());

        mGestureHandler.confirmTouchEvent(
                ContentViewGestureHandler.INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS);
        assertEquals("The queue should have been drained until first down since no consumer exists",
                2, mGestureHandler.getNumberOfPendingMotionEvents());
        assertEquals(MotionEvent.ACTION_DOWN,
                mGestureHandler.peekFirstInPendingMotionEvents().getActionMasked());

        mGestureHandler.confirmTouchEvent(
                ContentViewGestureHandler.INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS);
        assertEquals("The queue should have been drained",
                0, mGestureHandler.getNumberOfPendingMotionEvents());
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
        assertEquals(1, mGestureHandler.getNumberOfPendingMotionEvents());
        assertTrue("Should not have a pending gesture", mMockGestureDetector.mLastEvent == null);
        assertFalse("Should not have a pending LONG_PRESS", mLongPressDetector.hasPendingMessage());

        mGestureHandler.confirmTouchEvent(
                ContentViewGestureHandler.INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS);
        assertEquals(0, mGestureHandler.getNumberOfPendingMotionEvents());
        assertEquals("The down touch event should have been sent to the Gesture Detector",
                event.getEventTime(), mMockGestureDetector.mLastEvent.getEventTime());
        assertEquals("The down touch event should have been sent to the Gesture Detector",
                MotionEvent.ACTION_DOWN, mMockGestureDetector.mLastEvent.getActionMasked());

        event = MotionEvent.obtain(
                downTime, eventTime + 5, MotionEvent.ACTION_MOVE,
                FAKE_COORD_X * 5, FAKE_COORD_Y * 5, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals(0, mGestureHandler.getNumberOfPendingMotionEvents());
        assertEquals("Motion events should be going to the Gesture Detector directly",
                event.getEventTime(), mMockGestureDetector.mLastEvent.getEventTime());
        assertEquals("Motion events should be going to the Gesture Detector directly",
                MotionEvent.ACTION_MOVE, mMockGestureDetector.mLastEvent.getActionMasked());

        event = MotionEvent.obtain(
                downTime, eventTime + 10, MotionEvent.ACTION_UP,
                FAKE_COORD_X * 5, FAKE_COORD_Y * 5, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals(0, mGestureHandler.getNumberOfPendingMotionEvents());
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
        assertEquals(1, mGestureHandler.getNumberOfPendingMotionEvents());
        assertTrue("Should not have a pending gesture", mMockGestureDetector.mLastEvent == null);
        assertFalse("Should not have a pending LONG_PRESS", mLongPressDetector.hasPendingMessage());

        event = motionEvent(MotionEvent.ACTION_UP, downTime, eventTime + 5);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals(2, mGestureHandler.getNumberOfPendingMotionEvents());

        mGestureHandler.confirmTouchEvent(
                ContentViewGestureHandler.INPUT_EVENT_ACK_STATE_NOT_CONSUMED);

        assertEquals(1, mGestureHandler.getNumberOfPendingMotionEvents());
        assertEquals("The down touch event should have been sent to the Gesture Detector",
                event.getDownTime(), mMockGestureDetector.mLastEvent.getEventTime());
        assertEquals("The next event should be ACTION_UP",
                MotionEvent.ACTION_UP,
                mGestureHandler.peekFirstInPendingMotionEvents().getActionMasked());

        mGestureHandler.confirmTouchEvent(
                ContentViewGestureHandler.INPUT_EVENT_ACK_STATE_NOT_CONSUMED);

        assertEquals(0, mGestureHandler.getNumberOfPendingMotionEvents());
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
                getInstrumentation().getTargetContext(), mockDelegate,
                new MockZoomManager(getInstrumentation().getTargetContext(), null));
        mLongPressDetector = new LongPressDetector(
                getInstrumentation().getTargetContext(), mGestureHandler);

        MotionEvent event = motionEvent(MotionEvent.ACTION_DOWN, downTime, downTime);

        assertTrue(mGestureHandler.onTouchEvent(event));
        assertFalse("Should not have a pending LONG_PRESS", mLongPressDetector.hasPendingMessage());

        mGestureHandler.sendShowPressedStateGestureForTest();

        assertEquals("A show pressed state event should have been sent",
                ContentViewGestureHandler.GESTURE_SHOW_PRESSED_STATE,
                        mockDelegate.mMostRecentGestureEvent.mType);
        assertEquals("Only flingCancel and showPressedState should have been sent",
                2, mockDelegate.mGestureTypeList.size());

        event = MotionEvent.obtain(
                downTime, eventTime + 5, MotionEvent.ACTION_MOVE,
                FAKE_COORD_X * 5, FAKE_COORD_Y * 5, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals("The first move should not do anything",
                2, mockDelegate.mGestureTypeList.size());

        event = MotionEvent.obtain(
                downTime, eventTime + 10, MotionEvent.ACTION_MOVE,
                FAKE_COORD_X * 10, FAKE_COORD_Y * 10, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals("We should have started scrolling",
                ContentViewGestureHandler.GESTURE_SCROLL_BY,
                        mockDelegate.mMostRecentGestureEvent.mType);
        assertTrue("A show press cancel event should have been sent",
                mockDelegate.mGestureTypeList.contains(
                        ContentViewGestureHandler.GESTURE_SHOW_PRESS_CANCEL));
        assertEquals("Only flingCancel, showPressedState," +
                "showPressCancel, scrollBegin and scrollBy should have been sent",
                        5, mockDelegate.mGestureTypeList.size());

        event = MotionEvent.obtain(
                downTime, eventTime + 15, MotionEvent.ACTION_UP,
                FAKE_COORD_X * 10, FAKE_COORD_Y * 10, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals("We should have started flinging",
                ContentViewGestureHandler.GESTURE_FLING_START,
                        mockDelegate.mMostRecentGestureEvent.mType);
        assertTrue("A scroll end event should have been sent",
                mockDelegate.mGestureTypeList.contains(
                        ContentViewGestureHandler.GESTURE_SCROLL_END));
        assertEquals(
                "The last up should have caused scrollEnd, flingCancel and flingStart to be sent",
                        8, mockDelegate.mGestureTypeList.size());
    }

    /**
     * Verify that a recent show pressed state gesture is canceled when double tap occurs.
     * @throws Exception
     */
    @SmallTest
    @Feature({"Gestures"})
    public void testShowPressCancelOnDoubleTap() throws Exception {
        final long downTime = SystemClock.uptimeMillis();
        final long eventTime = SystemClock.uptimeMillis();

        GestureRecordingMotionEventDelegate mockDelegate =
                new GestureRecordingMotionEventDelegate();
        mGestureHandler = new ContentViewGestureHandler(
                getInstrumentation().getTargetContext(), mockDelegate,
                new MockZoomManager(getInstrumentation().getTargetContext(), null));
        mLongPressDetector = new LongPressDetector(
                getInstrumentation().getTargetContext(), mGestureHandler);

        MotionEvent event = motionEvent(MotionEvent.ACTION_DOWN, downTime, downTime);

        assertTrue(mGestureHandler.onTouchEvent(event));
        assertFalse("Should not have a pending LONG_PRESS", mLongPressDetector.hasPendingMessage());

        mGestureHandler.sendShowPressedStateGestureForTest();

        assertEquals("A show pressed state event should have been sent",
                ContentViewGestureHandler.GESTURE_SHOW_PRESSED_STATE,
                        mockDelegate.mMostRecentGestureEvent.mType);
        assertEquals("Only flingCancel and showPressedState should have been sent",
                2, mockDelegate.mGestureTypeList.size());

        event = MotionEvent.obtain(
                downTime, eventTime + 5, MotionEvent.ACTION_UP,
                FAKE_COORD_X, FAKE_COORD_Y, 0);
        mGestureHandler.onTouchEvent(event);
        assertEquals("The first tap should not do anything",
                2, mockDelegate.mGestureTypeList.size());

        event = MotionEvent.obtain(
                eventTime + 10, eventTime + 10, MotionEvent.ACTION_DOWN,
                FAKE_COORD_X, FAKE_COORD_Y, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals("A double tap should have occurred",
                ContentViewGestureHandler.GESTURE_DOUBLE_TAP,
                        mockDelegate.mMostRecentGestureEvent.mType);
        assertTrue("A show press cancel event should have been sent",
                mockDelegate.mGestureTypeList.contains(
                        ContentViewGestureHandler.GESTURE_SHOW_PRESS_CANCEL));
        assertEquals("Only flingCancel, showPressedState," +
                "flingCancel, showPressCancel and doubleTap should have been sent",
                        5, mockDelegate.mGestureTypeList.size());
    }

    /**
     * Mock MotionEventDelegate that remembers the most recent gesture event.
     */
    static class GestureRecordingMotionEventDelegate implements MotionEventDelegate {
        public class GestureEvent {
            private int mType;
            private long mTimeMs;
            private int mX;
            private int mY;
            private Bundle mExtraParams;

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
        };
        private GestureEvent mMostRecentGestureEvent;
        private ArrayList<Integer> mGestureTypeList = new ArrayList<Integer>();

        @Override
        public boolean sendTouchEvent(long timeMs, int action, TouchPoint[] pts) {
            // Not implemented.
            return false;
        }

        @Override
        public boolean sendGesture(int type, long timeMs, int x, int y, Bundle extraParams) {
            mMostRecentGestureEvent = new GestureEvent(type, timeMs, x, y, extraParams);
            mGestureTypeList.add(mMostRecentGestureEvent.mType);
            return true;
        }

        @Override
        public boolean didUIStealScroll(float x, float y) {
            // Not implemented.
            return false;
        }

        @Override
        public void invokeZoomPicker() {
            // Not implemented.
        }

        @Override
        public boolean hasFixedPageScale() {
            return false;
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
                getInstrumentation().getTargetContext(), delegate,
                new MockZoomManager(getInstrumentation().getTargetContext(), null));

        MotionEvent event = motionEvent(MotionEvent.ACTION_DOWN, downTime, downTime);
        assertTrue(gestureHandler.onTouchEvent(event));
        assertNotNull(delegate.getMostRecentGestureEvent());

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
                getInstrumentation().getTargetContext(), mockDelegate,
                new MockZoomManager(getInstrumentation().getTargetContext(), null));
        mLongPressDetector = mGestureHandler.getLongPressDetector();

        MotionEvent event = motionEvent(MotionEvent.ACTION_DOWN, downTime, downTime);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertNotNull(mockDelegate.getMostRecentGestureEvent());
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
                getInstrumentation().getTargetContext(), mockDelegate,
                new MockZoomManager(getInstrumentation().getTargetContext(), null));
        mLongPressDetector = mGestureHandler.getLongPressDetector();

        MotionEvent event = motionEvent(MotionEvent.ACTION_DOWN, downTime, downTime);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertNotNull(mockDelegate.getMostRecentGestureEvent());
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

}
