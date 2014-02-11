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
import android.view.InputDevice;
import android.view.MotionEvent;
import android.view.MotionEvent.PointerCoords;
import android.view.MotionEvent.PointerProperties;
import android.view.ViewConfiguration;

import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.ScalableTimeout;
import org.chromium.content.browser.ContentViewGestureHandler.MotionEventDelegate;

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
    private MockMotionEventDelegate mMockMotionEventDelegate;
    private ContentViewGestureHandler mGestureHandler;

    static class MockMotionEventDelegate implements MotionEventDelegate {
        static class GestureEvent {
            public final int mType;
            public final long mTimeMs;
            public final int mX;
            public final int mY;
            public final Bundle mExtraParams;

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
        GestureEvent mMostRecentGestureEvent;
        GestureEvent mActiveScrollStartEvent;
        int mMostRecentMotionEventAction = -1;
        final ArrayList<Integer> mGestureTypeList = new ArrayList<Integer>();
        final ArrayList<Long> mGestureTimeList = new ArrayList<Long>();

        final CountDownLatch mLongPressCalled = new CountDownLatch(1);
        final CountDownLatch mShowPressCalled = new CountDownLatch(1);

        @Override
        public void onTouchEventHandlingBegin(MotionEvent event) {
            mMostRecentMotionEventAction = event.getActionMasked();
        }

        @Override
        public void onTouchEventHandlingEnd() {
        }

        @Override
        public boolean onGestureEventCreated(
                int type, long timeMs, int x, int y, Bundle extraParams) {
            Log.i(TAG, "Gesture event received with type id " + type);
            mMostRecentGestureEvent = new GestureEvent(type, timeMs, x, y, extraParams);
            mGestureTypeList.add(mMostRecentGestureEvent.mType);
            mGestureTimeList.add(timeMs);
            if (type == GestureEventType.SCROLL_START) {
                mActiveScrollStartEvent = mMostRecentGestureEvent;
            } else if (type == GestureEventType.SCROLL_END
                           || type == GestureEventType.FLING_CANCEL) {
                mActiveScrollStartEvent = null;
            } else if (type == GestureEventType.LONG_PRESS) {
                mLongPressCalled.countDown();
            } else if (type == GestureEventType.SHOW_PRESS) {
                mShowPressCalled.countDown();
            }

            return true;
        }
    }

    private static MotionEvent motionEvent(int action, long downTime, long eventTime) {
        return MotionEvent.obtain(downTime, eventTime, action, FAKE_COORD_X, FAKE_COORD_Y, 0);
    }

    @Override
    public void setUp() {
        mMockMotionEventDelegate = new MockMotionEventDelegate();
        mGestureHandler = new ContentViewGestureHandler(
                getInstrumentation().getTargetContext(), mMockMotionEventDelegate);
        mGestureHandler.updateMultiTouchSupport(false);
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

        mGestureHandler.updateDoubleTapSupport(false);

        MotionEvent event = motionEvent(MotionEvent.ACTION_DOWN, downTime, downTime);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals(GestureEventType.TAP_DOWN,
                mMockMotionEventDelegate.mMostRecentGestureEvent.mType);

        event = motionEvent(MotionEvent.ACTION_UP, downTime, eventTime + 10);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals(GestureEventType.SINGLE_TAP_CONFIRMED,
                mMockMotionEventDelegate.mMostRecentGestureEvent.mType);
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

        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals(GestureEventType.TAP_DOWN,
                mMockMotionEventDelegate.mMostRecentGestureEvent.mType);

        event = MotionEvent.obtain(
                downTime, eventTime + 5, MotionEvent.ACTION_MOVE,
                FAKE_COORD_X * 10, FAKE_COORD_Y * 10, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));

        event = MotionEvent.obtain(
                downTime, eventTime + 10, MotionEvent.ACTION_UP,
                FAKE_COORD_X * 10, FAKE_COORD_Y * 10, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals(GestureEventType.FLING_START,
                mMockMotionEventDelegate.mMostRecentGestureEvent.mType);
        assertFalse(mMockMotionEventDelegate.mGestureTypeList.contains(
                GestureEventType.LONG_PRESS));
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

        MotionEvent event = motionEvent(MotionEvent.ACTION_DOWN, downTime, downTime);

        assertTrue(mGestureHandler.onTouchEvent(event));

        event = MotionEvent.obtain(
                downTime, eventTime + 1000, MotionEvent.ACTION_MOVE, scrollToX, scrollToY, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertTrue(mGestureHandler.isNativeScrolling());
        assertTrue("A scrollStart event should have been sent",
                mMockMotionEventDelegate.mGestureTypeList.contains(
                        GestureEventType.SCROLL_START));
        assertEquals("We should have started scrolling",
                GestureEventType.SCROLL_BY,
                mMockMotionEventDelegate.mMostRecentGestureEvent.mType);
        assertEquals("Only tapDown, tapCancel, scrollBegin and scrollBy should have been sent",
                4, mMockMotionEventDelegate.mGestureTypeList.size());
        assertEquals("scrollBegin should be sent before scrollBy",
                GestureEventType.SCROLL_START,
                (int) mMockMotionEventDelegate.mGestureTypeList.get(2));
        assertEquals("scrollBegin should have the time of the ACTION_MOVE",
                eventTime + 1000, (long) mMockMotionEventDelegate.mGestureTimeList.get(2));

        event = MotionEvent.obtain(
                downTime, eventTime + 1000, endActionType, scrollToX, scrollToY, 0);
        mGestureHandler.onTouchEvent(event);
        assertFalse(mGestureHandler.isNativeScrolling());
        assertTrue("A scrollEnd event should have been sent",
                mMockMotionEventDelegate.mGestureTypeList.contains(
                        GestureEventType.SCROLL_END));
        assertEquals("We should have stopped scrolling",
                GestureEventType.SCROLL_END,
                mMockMotionEventDelegate.mMostRecentGestureEvent.mType);
        assertEquals("Only tapDown, scrollBegin and scrollBy and scrollEnd should have been sent",
                5, mMockMotionEventDelegate.mGestureTypeList.size());
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

        MotionEvent event = motionEvent(MotionEvent.ACTION_DOWN, downTime, downTime);

        assertTrue(mGestureHandler.onTouchEvent(event));

        event = MotionEvent.obtain(
                downTime, eventTime + 10, MotionEvent.ACTION_MOVE,
                FAKE_COORD_X * 5, FAKE_COORD_Y * 5, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertTrue(mGestureHandler.isNativeScrolling());
        assertTrue("A scrollStart event should have been sent",
                mMockMotionEventDelegate.mGestureTypeList.contains(
                        GestureEventType.SCROLL_START));
        assertEquals("We should have started scrolling",
                GestureEventType.SCROLL_BY,
                mMockMotionEventDelegate.mMostRecentGestureEvent.mType);
        assertEquals("Only tapDown, tapCancel, scrollBegin and scrollBy should have been sent",
                4, mMockMotionEventDelegate.mGestureTypeList.size());
        assertEquals("scrollBegin should be sent before scrollBy",
                GestureEventType.SCROLL_START,
                (int) mMockMotionEventDelegate.mGestureTypeList.get(2));
        MockMotionEventDelegate.GestureEvent startEvent =
                mMockMotionEventDelegate.mActiveScrollStartEvent;
        assertNotNull(startEvent);
        assertEquals("scrollBegin should have the time of the ACTION_MOVE",
                eventTime + 10, (long) startEvent.getTimeMs());
        int hintX = startEvent.getExtraParams().getInt(ContentViewGestureHandler.DELTA_HINT_X);
        int hintY = startEvent.getExtraParams().getInt(ContentViewGestureHandler.DELTA_HINT_Y);
        // We don't want to take a dependency here on exactly how hints are calculated for a
        // fling (eg. may depend on velocity), so just validate the direction.
        assertTrue("scrollBegin hint should be in positive X axis",
                hintX > 0 && hintY > 0 && hintX > hintY);

        event = MotionEvent.obtain(
                downTime, eventTime + 15, MotionEvent.ACTION_UP,
                FAKE_COORD_X * 10, FAKE_COORD_Y * 10, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertFalse(mGestureHandler.isNativeScrolling());
        assertEquals("We should have started flinging",
                GestureEventType.FLING_START,
                        mMockMotionEventDelegate.mMostRecentGestureEvent.mType);
        assertTrue("A scroll end event should not have been sent",
                !mMockMotionEventDelegate.mGestureTypeList.contains(
                        GestureEventType.SCROLL_END));
        assertEquals("The last up should have caused flingStart to be sent",
                5, mMockMotionEventDelegate.mGestureTypeList.size());
        assertEquals("flingStart should have the time of the ACTION_UP",
                eventTime + 15, (long) mMockMotionEventDelegate.mGestureTimeList.get(4));
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

        MotionEvent event = motionEvent(MotionEvent.ACTION_DOWN, downTime, downTime);
        assertTrue(mGestureHandler.onTouchEvent(event));

        mGestureHandler.fling(eventTime, 5, 5, 0, 0);
        assertEquals("A zero-velocity fling should not be forwarded",
                GestureEventType.TAP_DOWN,
                mMockMotionEventDelegate.mMostRecentGestureEvent.mType);

        mGestureHandler.fling(eventTime, 5, 5, 5, 0);
        assertEquals("Subsequent flings should work properly",
                GestureEventType.FLING_START,
                        mMockMotionEventDelegate.mMostRecentGestureEvent.mType);

        event = MotionEvent.obtain(
                downTime, eventTime + 10, MotionEvent.ACTION_MOVE,
                FAKE_COORD_X * 5, FAKE_COORD_Y * 5, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertTrue(mGestureHandler.isNativeScrolling());
        assertTrue("A scrollStart event should have been sent",
                mMockMotionEventDelegate.mGestureTypeList.contains(
                        GestureEventType.SCROLL_START));
        assertEquals("We should have started scrolling",
                GestureEventType.SCROLL_BY,
                mMockMotionEventDelegate.mMostRecentGestureEvent.mType);

        mGestureHandler.fling(eventTime, 5, 5, 0, 0);
        assertEquals("A zero-velicty fling should end the current scroll sequence",
                GestureEventType.SCROLL_END,
                        mMockMotionEventDelegate.mMostRecentGestureEvent.mType);
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

        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                setUp();
                MotionEvent event = motionEvent(MotionEvent.ACTION_DOWN, time, time);
                mGestureHandler.onTouchEvent(event);
            }
        });

        assertTrue(mMockMotionEventDelegate.mShowPressCalled.await(
                ScalableTimeout.scaleTimeout(ViewConfiguration.getTapTimeout() + 10),
                TimeUnit.MILLISECONDS));

        assertTrue(mMockMotionEventDelegate.mLongPressCalled.await(
                ScalableTimeout.scaleTimeout(ViewConfiguration.getLongPressTimeout()
                        + ViewConfiguration.getTapTimeout() + 10),
                TimeUnit.MILLISECONDS));

        assertEquals("Should have sent SHOW_PRESS and LONG_PRESS events",
                3, mMockMotionEventDelegate.mGestureTypeList.size());
        assertEquals("Should have a show press event",
                GestureEventType.SHOW_PRESS,
                mMockMotionEventDelegate.mGestureTypeList.get(1).intValue());
        assertEquals("Should have a long press event",
                GestureEventType.LONG_PRESS,
                mMockMotionEventDelegate.mGestureTypeList.get(2).intValue());

        // The long press triggers window focus loss by opening a context menu
        mGestureHandler.onWindowFocusLost();

        assertEquals("Only should have sent only GESTURE_TAP_CANCEL event",
                4, mMockMotionEventDelegate.mGestureTypeList.size());
        assertEquals("Should have a gesture show press cancel event next",
                GestureEventType.TAP_CANCEL,
                mMockMotionEventDelegate.mGestureTypeList.get(3).intValue());
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

        MotionEvent event = motionEvent(MotionEvent.ACTION_DOWN, downTime, downTime);

        assertTrue(mGestureHandler.onTouchEvent(event));

        mGestureHandler.sendShowPressedStateGestureForTesting();

        assertEquals("A show pressed state event should have been sent",
                GestureEventType.SHOW_PRESS,
                        mMockMotionEventDelegate.mMostRecentGestureEvent.mType);
        assertEquals("Only tapDown and showPressedState should have been sent",
                2, mMockMotionEventDelegate.mGestureTypeList.size());

        event = MotionEvent.obtain(
                downTime, eventTime + 10, MotionEvent.ACTION_MOVE,
                FAKE_COORD_X * 5, FAKE_COORD_Y * 5, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));

        assertEquals("We should have started scrolling",
                GestureEventType.SCROLL_BY,
                        mMockMotionEventDelegate.mMostRecentGestureEvent.mType);
        assertTrue("A show press cancel event should have been sent",
                mMockMotionEventDelegate.mGestureTypeList.contains(
                        GestureEventType.TAP_CANCEL));
        assertEquals("Only tapDown, showPressedState, showPressCancel, scrollBegin and scrollBy" +
                " should have been sent",
                5, mMockMotionEventDelegate.mGestureTypeList.size());

        event = MotionEvent.obtain(
                downTime, eventTime + 15, MotionEvent.ACTION_UP,
                FAKE_COORD_X * 10, FAKE_COORD_Y * 10, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals("We should have started flinging",
                GestureEventType.FLING_START,
                        mMockMotionEventDelegate.mMostRecentGestureEvent.mType);
        assertTrue("A scroll end event should not have been sent",
                !mMockMotionEventDelegate.mGestureTypeList.contains(
                        GestureEventType.SCROLL_END));
        assertEquals("The last up should have caused flingStart to be sent",
                6, mMockMotionEventDelegate.mGestureTypeList.size());
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

        MotionEvent event = motionEvent(MotionEvent.ACTION_DOWN, downTime, downTime);
        assertTrue(mGestureHandler.onTouchEvent(event));

        mGestureHandler.sendShowPressedStateGestureForTesting();
        assertEquals("GESTURE_SHOW_PRESS should have been sent",
                GestureEventType.SHOW_PRESS,
                mMockMotionEventDelegate.mMostRecentGestureEvent.mType);
        assertEquals("Only GESTURE_TAP_DOWN and GESTURE_SHOW_PRESS should have been sent",
                2, mMockMotionEventDelegate.mGestureTypeList.size());

        event = MotionEvent.obtain(
                downTime, eventTime + 5, MotionEvent.ACTION_UP,
                FAKE_COORD_X, FAKE_COORD_Y, 0);
        mGestureHandler.onTouchEvent(event);
        assertEquals("A GESTURE_SINGLE_TAP_UNCONFIRMED event should have been sent",
                GestureEventType.SINGLE_TAP_UNCONFIRMED,
                mMockMotionEventDelegate.mMostRecentGestureEvent.mType);
        assertEquals("Only GESTURE_TAP_DOWN, " +
                "GESTURE_SHOW_PRESS and " +
                "GESTURE_SINGLE_TAP_UNCONFIRMED should have been sent",
                3, mMockMotionEventDelegate.mGestureTypeList.size());

        event = MotionEvent.obtain(
                eventTime + 10, eventTime + 10, MotionEvent.ACTION_DOWN,
                FAKE_COORD_X, FAKE_COORD_Y, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals("A GESTURE_TAP_DOWN event should have been sent ",
                GestureEventType.TAP_DOWN,
                mMockMotionEventDelegate.mMostRecentGestureEvent.mType);
        assertEquals("Only GESTURE_TAP_DOWN, " +
                "GESTURE_SHOW_PRESS, " +
                "GESTURE_SINGLE_TAP_UNCONFIRMED," +
                "GESTURE_TAP_CANCEL and" +
                "GESTURE_TAP_DOWN should have been sent",
                5, mMockMotionEventDelegate.mGestureTypeList.size());

        // Moving a very small amount of distance should not trigger the double tap drag zoom mode.
        event = MotionEvent.obtain(
                eventTime + 10, eventTime + 10, MotionEvent.ACTION_MOVE,
                FAKE_COORD_X, FAKE_COORD_Y + 1, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals("Only GESTURE_TAP_DOWN, " +
                "GESTURE_SHOW_PRESS, " +
                "GESTURE_SINGLE_TAP_UNCONFIRMED and" +
                "GESTURE_TAP_CANCEL and " +
                "GESTURE_TAP_DOWN should have been sent",
                5, mMockMotionEventDelegate.mGestureTypeList.size());

        event = MotionEvent.obtain(
                eventTime + 10, eventTime + 15, MotionEvent.ACTION_UP,
                FAKE_COORD_X, FAKE_COORD_Y + 1, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals("A double tap should have occurred",
                GestureEventType.DOUBLE_TAP,
                mMockMotionEventDelegate.mMostRecentGestureEvent.mType);
        assertEquals("Only GESTURE_TAP_DOWN, " +
                "GESTURE_SHOW_PRESS, " +
                "GESTURE_SINGLE_TAP_UNCONFIRMED, " +
                "GESTURE_TAP_CANCEL, " +
                "GESTURE_TAP_DOWN, " +
                "GESTURE_DOUBLE_TAP should have been sent",
                6, mMockMotionEventDelegate.mGestureTypeList.size());
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

        MotionEvent event = motionEvent(MotionEvent.ACTION_DOWN, downTime1, downTime1);
        assertTrue(mGestureHandler.onTouchEvent(event));

        mGestureHandler.sendShowPressedStateGestureForTesting();
        assertEquals("GESTURE_SHOW_PRESS should have been sent",
                GestureEventType.SHOW_PRESS,
                        mMockMotionEventDelegate.mMostRecentGestureEvent.mType);
        assertEquals("Only GESTURE_TAP_DOWN and GESTURE_SHOW_PRESS should have been sent",
                2, mMockMotionEventDelegate.mGestureTypeList.size());


        event = MotionEvent.obtain(
                downTime1, downTime1 + 5, MotionEvent.ACTION_UP,
                FAKE_COORD_X, FAKE_COORD_Y, 0);
        mGestureHandler.onTouchEvent(event);
        assertEquals("A GESTURE_SINGLE_TAP_UNCONFIRMED event should have been sent",
                GestureEventType.SINGLE_TAP_UNCONFIRMED,
                mMockMotionEventDelegate.mMostRecentGestureEvent.mType);
        assertEquals("Only GESTURE_TAP_DOWN, " +
                "GESTURE_SHOW_PRESS and " +
                "GESTURE_TAP_UNCONFIRMED " +
                "should have been sent",
                3, mMockMotionEventDelegate.mGestureTypeList.size());

        event = MotionEvent.obtain(
                downTime2, downTime2, MotionEvent.ACTION_DOWN,
                FAKE_COORD_X, FAKE_COORD_Y, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals("GESTURE_TAP_DOWN should have been sent",
                GestureEventType.TAP_DOWN,
                mMockMotionEventDelegate.mMostRecentGestureEvent.mType);
        assertEquals("Only GESTURE_TAP_DOWN, " +
                "GESTURE_SHOW_PRESS, " +
                "GESTURE_TAP_UNCONFIRMED," +
                "GESTURE_TAP_CANCEL and" +
                "GESTURE_TAP_DOWN should have been sent",
                5, mMockMotionEventDelegate.mGestureTypeList.size());

        event = MotionEvent.obtain(
                downTime2, downTime2 + 5, MotionEvent.ACTION_MOVE,
                FAKE_COORD_X, FAKE_COORD_Y + 100, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertTrue("GESTURE_SCROLL_START should have been sent",
                mMockMotionEventDelegate.mGestureTypeList.contains(
                        GestureEventType.SCROLL_START));
        MockMotionEventDelegate.GestureEvent startEvent =
                mMockMotionEventDelegate.mActiveScrollStartEvent;
        assertEquals(FAKE_COORD_X, startEvent.getX());
        assertEquals(FAKE_COORD_Y + 100, startEvent.getY());
        Bundle extraParams = startEvent.getExtraParams();
        assertNotNull(extraParams);
        assertEquals("GESTURE_SCROLL_START should have an X hint equal to the distance traveled",
                0, extraParams.getInt(ContentViewGestureHandler.DELTA_HINT_X));
        assertEquals("GESTURE_SCROLL_START should have an X hint equal to the distance traveled",
                100, extraParams.getInt(ContentViewGestureHandler.DELTA_HINT_Y));

        assertEquals("GESTURE_PINCH_BEGIN should have been sent",
                GestureEventType.PINCH_BEGIN,
                mMockMotionEventDelegate.mMostRecentGestureEvent.mType);
        assertEquals("Only GESTURE_TAP_DOWN, " +
                "GESTURE_SHOW_PRESS, " +
                "GESTURE_TAP_UNCONFIRMED," +
                "GESTURE_TAP_CANCEL, " +
                "GESTURE_TAP_DOWN, " +
                "GESTURE_TAP_CANCEL, " +
                "GESTURE_SCROLL_START, and " +
                "GESTURE_PINCH_BEGIN should have been sent",
                8, mMockMotionEventDelegate.mGestureTypeList.size());

        event = MotionEvent.obtain(
                downTime2, downTime2 + 10, MotionEvent.ACTION_MOVE,
                FAKE_COORD_X, FAKE_COORD_Y + 200, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertTrue("GESTURE_SCROLL_BY should have been sent",
                mMockMotionEventDelegate.mGestureTypeList.contains(
                        GestureEventType.SCROLL_BY));
        assertEquals("GESTURE_PINCH_BY should have been sent",
                GestureEventType.PINCH_BY,
                mMockMotionEventDelegate.mMostRecentGestureEvent.mType);
        assertEquals("Only GESTURE_TAP_DOWN, " +
                "GESTURE_SHOW_PRESS, " +
                "GESTURE_TAP_UNCONFIRMED," +
                "GESTURE_TAP_CANCEL, " +
                "GESTURE_TAP_DOWN, " +
                "GESTURE_TAP_CANCEL, " +
                "GESTURE_SCROLL_START," +
                "GESTURE_PINCH_BEGIN, " +
                "GESTURE_SCROLL_BY, and " +
                "GESTURE_PINCH_BY should have been sent",
                10, mMockMotionEventDelegate.mGestureTypeList.size());

        event = MotionEvent.obtain(
                downTime2, downTime2 + 15, MotionEvent.ACTION_UP,
                FAKE_COORD_X, FAKE_COORD_Y + 200, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertTrue("GESTURE_PINCH_END should have been sent",
                mMockMotionEventDelegate.mGestureTypeList.contains(
                        GestureEventType.PINCH_END));
        assertEquals("GESTURE_SCROLL_END should have been sent",
                GestureEventType.SCROLL_END,
                mMockMotionEventDelegate.mMostRecentGestureEvent.mType);
        assertEquals("Only GESTURE_TAP_DOWN, " +
                "GESTURE_SHOW_PRESS, " +
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
                12, mMockMotionEventDelegate.mGestureTypeList.size());
    }


    /**
     * Verify that double tap drag zoom is cancelled if the user presses a
     * secondary pointer.
     * @throws Exception
     */
    @SmallTest
    @Feature({"Gestures"})
    public void testDoubleTapDragZoomCancelledOnSecondaryPointerDown() throws Exception {
        final long downTime1 = SystemClock.uptimeMillis();
        final long downTime2 = downTime1 + 100;

        PointerProperties pp0 = new PointerProperties();
        pp0.id = 0;
        pp0.toolType = MotionEvent.TOOL_TYPE_FINGER;
        PointerProperties pp1 = new PointerProperties();
        pp1.id = 1;
        pp1.toolType = MotionEvent.TOOL_TYPE_FINGER;

        PointerCoords pc0 = new PointerCoords();
        pc0.x = FAKE_COORD_X;
        pc0.y = FAKE_COORD_Y;
        pc0.pressure = 1;
        pc0.size = 1;
        PointerCoords pc1 = new PointerCoords();
        pc1.x = FAKE_COORD_X + 50;
        pc1.y = FAKE_COORD_Y + 50;
        pc1.pressure = 1;
        pc1.size = 1;

        MotionEvent event = MotionEvent.obtain(
                downTime1, downTime1, MotionEvent.ACTION_DOWN,
                1, new PointerProperties[] { pp0 }, new PointerCoords[] { pc0 },
                0, 0, 1.0f, 1.0f, 0, 0, InputDevice.SOURCE_CLASS_POINTER, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));

        mGestureHandler.sendShowPressedStateGestureForTesting();
        assertEquals("GESTURE_SHOW_PRESS should have been sent",
                GestureEventType.SHOW_PRESS,
                        mMockMotionEventDelegate.mMostRecentGestureEvent.mType);
        assertEquals("Only GESTURE_TAP_DOWN and GESTURE_SHOW_PRESS should have been sent",
                2, mMockMotionEventDelegate.mGestureTypeList.size());

        event = MotionEvent.obtain(
                downTime1, downTime1 + 5, MotionEvent.ACTION_UP,
                1, new PointerProperties[] { pp0 }, new PointerCoords[] { pc0 },
                0, 0, 1.0f, 1.0f, 0, 0, InputDevice.SOURCE_CLASS_POINTER, 0);
        mGestureHandler.onTouchEvent(event);
        assertEquals("A GESTURE_SINGLE_TAP_UNCONFIRMED event should have been sent",
                GestureEventType.SINGLE_TAP_UNCONFIRMED,
                mMockMotionEventDelegate.mMostRecentGestureEvent.mType);
        assertEquals("Only GESTURE_TAP_DOWN, " +
                "GESTURE_SHOW_PRESS and " +
                "GESTURE_TAP_UNCONFIRMED " +
                "should have been sent",
                3, mMockMotionEventDelegate.mGestureTypeList.size());

        event = MotionEvent.obtain(
                downTime2, downTime2, MotionEvent.ACTION_DOWN,
                1, new PointerProperties[] { pp0 }, new PointerCoords[] { pc0 },
                0, 0, 1.0f, 1.0f, 0, 0, InputDevice.SOURCE_CLASS_POINTER, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals("GESTURE_TAP_DOWN should have been sent",
                GestureEventType.TAP_DOWN,
                mMockMotionEventDelegate.mMostRecentGestureEvent.mType);
        assertEquals("Only GESTURE_TAP_DOWN, " +
                "GESTURE_SHOW_PRESS, " +
                "GESTURE_TAP_UNCONFIRMED," +
                "GESTURE_TAP_CANCEL and" +
                "GESTURE_TAP_DOWN should have been sent",
                5, mMockMotionEventDelegate.mGestureTypeList.size());

        pc0.y = pc0.y - 30;

        event = MotionEvent.obtain(
                downTime2, downTime2 + 50, MotionEvent.ACTION_MOVE,
                1, new PointerProperties[] { pp0 }, new PointerCoords[] { pc0 },
                0, 0, 1.0f, 1.0f, 0, 0, InputDevice.SOURCE_CLASS_POINTER, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertTrue("GESTURE_SCROLL_START should have been sent",
                mMockMotionEventDelegate.mGestureTypeList.contains(
                        GestureEventType.SCROLL_START));

        assertEquals("GESTURE_PINCH_BEGIN should have been sent",
                GestureEventType.PINCH_BEGIN,
                mMockMotionEventDelegate.mMostRecentGestureEvent.mType);
        assertEquals("Only GESTURE_TAP_DOWN, " +
                "GESTURE_SHOW_PRESS, " +
                "GESTURE_TAP_UNCONFIRMED," +
                "GESTURE_TAP_CANCEL, " +
                "GESTURE_TAP_DOWN, " +
                "GESTURE_TAP_CANCEL, " +
                "GESTURE_SCROLL_START, and " +
                "GESTURE_PINCH_BEGIN should have been sent",
                8, mMockMotionEventDelegate.mGestureTypeList.size());

        event = MotionEvent.obtain(
                downTime2, downTime2 + 100, MotionEvent.ACTION_POINTER_DOWN,
                2, new PointerProperties[] { pp1, pp0 }, new PointerCoords[] { pc1, pc0 },
                0, 0, 1.0f, 1.0f, 0, 0, InputDevice.SOURCE_CLASS_POINTER, 0);
        mGestureHandler.onTouchEvent(event);
        assertTrue("GESTURE_PINCH_END should have been sent",
                mMockMotionEventDelegate.mGestureTypeList.contains(
                        GestureEventType.PINCH_END));
        assertEquals("GESTURE_SCROLL_END should have been sent",
                GestureEventType.SCROLL_END,
                mMockMotionEventDelegate.mMostRecentGestureEvent.mType);
        assertEquals("Only GESTURE_TAP_DOWN, " +
                "GESTURE_SHOW_PRESS, " +
                "GESTURE_TAP_UNCONFIRMED," +
                "GESTURE_TAP_CANCEL, " +
                "GESTURE_TAP_DOWN, " +
                "GESTURE_TAP_CANCEL, " +
                "GESTURE_SCROLL_START," +
                "GESTURE_PINCH_BEGIN, " +
                "GESTURE_PINCH_END, and " +
                "GESTURE_SCROLL_END should have been sent",
                10, mMockMotionEventDelegate.mGestureTypeList.size());

        event = MotionEvent.obtain(
                downTime2, downTime2 + 150, MotionEvent.ACTION_POINTER_UP,
                2, new PointerProperties[] { pp1, pp0 }, new PointerCoords[] { pc1, pc0 },
                0, 0, 1.0f, 1.0f, 0, 0, InputDevice.SOURCE_CLASS_POINTER, 0);
        mGestureHandler.onTouchEvent(event);
        assertEquals("No new gestures should have been sent",
                10, mMockMotionEventDelegate.mGestureTypeList.size());
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
                mMockMotionEventDelegate.mGestureTypeList.contains(
                        GestureEventType.SCROLL_START));
        assertFalse("No GESTURE_PINCH_BEGIN should have been sent",
                mMockMotionEventDelegate.mGestureTypeList.contains(
                        GestureEventType.PINCH_BEGIN));

        event = MotionEvent.obtain(
                downTime2, downTime2 + 10, MotionEvent.ACTION_MOVE,
                FAKE_COORD_X, FAKE_COORD_Y + 200, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals("GESTURE_SCROLL_BY should have been sent",
                GestureEventType.SCROLL_BY,
                mMockMotionEventDelegate.mMostRecentGestureEvent.mType);
        assertEquals("GESTURE_SCROLL_BY should have been sent",
                event.getEventTime(),
                mMockMotionEventDelegate.mMostRecentGestureEvent.getTimeMs());
        assertTrue("No GESTURE_PINCH_BY should have been sent",
                GestureEventType.PINCH_BY !=
                mMockMotionEventDelegate.mMostRecentGestureEvent.mType);

        event = MotionEvent.obtain(
                downTime2, downTime2 + 15, MotionEvent.ACTION_UP,
                FAKE_COORD_X, FAKE_COORD_Y + 200, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertFalse("No GESTURE_PINCH_END should have been sent",
                mMockMotionEventDelegate.mGestureTypeList.contains(
                        GestureEventType.PINCH_END));
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

        MotionEvent event = motionEvent(MotionEvent.ACTION_DOWN, downTime, downTime);
        assertTrue(mGestureHandler.onTouchEvent(event));

        // Move twice so that we get two GESTURE_SCROLL_BY events and can compare
        // the relative and absolute coordinates.
        event = MotionEvent.obtain(
                downTime, downTime + 5, MotionEvent.ACTION_MOVE,
                FAKE_COORD_X - deltaX / 2, FAKE_COORD_Y - deltaY / 2, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));

        event = MotionEvent.obtain(
                downTime, downTime + 10, MotionEvent.ACTION_MOVE,
                FAKE_COORD_X - deltaX, FAKE_COORD_Y - deltaY, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));

        // Make sure the reported gesture event has all the expected data.
        MockMotionEventDelegate.GestureEvent gestureEvent =
                mMockMotionEventDelegate.mMostRecentGestureEvent;
        assertNotNull(gestureEvent);
        assertEquals(GestureEventType.SCROLL_BY, gestureEvent.getType());
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
     * Generate a scroll gesture and verify that the resulting scroll start event
     * has the expected hint values.
     */
    @SmallTest
    @Feature({"Gestures"})
    public void testScrollStartValues() {
        final int deltaX = 13;
        final int deltaY = 89;
        final long downTime = SystemClock.uptimeMillis();

        MotionEvent event = motionEvent(MotionEvent.ACTION_DOWN, downTime, downTime);
        assertTrue(mGestureHandler.onTouchEvent(event));

        // Move twice such that the first event isn't sufficient to start
        // scrolling on it's own.
        event = MotionEvent.obtain(
                downTime, downTime + 5, MotionEvent.ACTION_MOVE,
                FAKE_COORD_X + 2, FAKE_COORD_Y + 1, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertNull("Expect scrolling hasn't yet started",
                mMockMotionEventDelegate.mActiveScrollStartEvent);

        event = MotionEvent.obtain(
                downTime, downTime + 10, MotionEvent.ACTION_MOVE,
                FAKE_COORD_X + deltaX, FAKE_COORD_Y + deltaY, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));

        MockMotionEventDelegate.GestureEvent startEvent =
                mMockMotionEventDelegate.mActiveScrollStartEvent;
        assertNotNull(startEvent);
        assertEquals(GestureEventType.SCROLL_START, startEvent.getType());
        assertEquals(downTime + 10, startEvent.getTimeMs());
        assertEquals(FAKE_COORD_X, startEvent.getX());
        assertEquals(FAKE_COORD_Y, startEvent.getY());

        Bundle extraParams = startEvent.getExtraParams();
        assertNotNull(extraParams);
        assertEquals(deltaX, extraParams.getInt(ContentViewGestureHandler.DELTA_HINT_X));
        assertEquals(deltaY, extraParams.getInt(ContentViewGestureHandler.DELTA_HINT_Y));
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

        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                setUp();

                MotionEvent event = motionEvent(MotionEvent.ACTION_DOWN, downTime, downTime);
                assertTrue(mGestureHandler.onTouchEvent(event));
                event = MotionEvent.obtain(
                        downTime, eventTime + 5, MotionEvent.ACTION_MOVE,
                        FAKE_COORD_X * 5, FAKE_COORD_Y * 5, 0);
                assertTrue(mGestureHandler.onTouchEvent(event));
                event = MotionEvent.obtain(
                        downTime, eventTime + 10, MotionEvent.ACTION_MOVE,
                        FAKE_COORD_X * 10, FAKE_COORD_Y * 10, 0);
                assertTrue(mGestureHandler.onTouchEvent(event));
            }
        });

        assertFalse(mMockMotionEventDelegate.mLongPressCalled.await(
                ScalableTimeout.scaleTimeout(ViewConfiguration.getLongPressTimeout()
                        + ViewConfiguration.getTapTimeout() + 10),
                TimeUnit.MILLISECONDS));

        // No LONG_TAP because LONG_PRESS timer is cancelled.
        assertFalse("No LONG_PRESS should be sent",
                mMockMotionEventDelegate.mGestureTypeList.contains(
                        GestureEventType.LONG_PRESS));
        assertFalse("No LONG_TAP should be sent",
                mMockMotionEventDelegate.mGestureTypeList.contains(
                        GestureEventType.LONG_TAP));
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

        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                setUp();
                mGestureHandler = new ContentViewGestureHandler(
                        getInstrumentation().getTargetContext(), mMockMotionEventDelegate);
                MotionEvent event = motionEvent(MotionEvent.ACTION_DOWN, downTime, downTime);
                assertTrue(mGestureHandler.onTouchEvent(event));
            }
        });

        assertTrue(mMockMotionEventDelegate.mLongPressCalled.await(
                ScalableTimeout.scaleTimeout(ViewConfiguration.getLongPressTimeout()
                        + ViewConfiguration.getTapTimeout() + 10),
                TimeUnit.MILLISECONDS));

        assertEquals("A LONG_PRESS gesture should have been sent",
                GestureEventType.LONG_PRESS,
                        mMockMotionEventDelegate.mMostRecentGestureEvent.mType);

        MotionEvent event = motionEvent(MotionEvent.ACTION_UP, downTime, eventTime + 1000);
        mGestureHandler.onTouchEvent(event);
        assertEquals("A LONG_TAP gesture should have been sent",
                GestureEventType.LONG_TAP,
                        mMockMotionEventDelegate.mMostRecentGestureEvent.mType);
    }

    /**
     * Verify that a LONG_PRESS gesture does not prevent further scrolling.
     *
     * @throws Exception
     */
    @SmallTest
    @Feature({"Gestures"})
    public void testGestureLongPressDoesNotPreventScrolling() throws Exception {
        final long downTime = SystemClock.uptimeMillis();
        final long eventTime = SystemClock.uptimeMillis();

        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                setUp();
                mGestureHandler = new ContentViewGestureHandler(
                        getInstrumentation().getTargetContext(), mMockMotionEventDelegate);
                MotionEvent event = motionEvent(MotionEvent.ACTION_DOWN, downTime, downTime);
                assertTrue(mGestureHandler.onTouchEvent(event));
            }
        });

        final long longPressTimeoutMs = ViewConfiguration.getLongPressTimeout()
                + ViewConfiguration.getTapTimeout() + 10;
        assertTrue(mMockMotionEventDelegate.mLongPressCalled.await(
                ScalableTimeout.scaleTimeout(longPressTimeoutMs), TimeUnit.MILLISECONDS));

        assertEquals("A LONG_PRESS gesture should have been sent",
                GestureEventType.LONG_PRESS,
                        mMockMotionEventDelegate.mMostRecentGestureEvent.mType);

        MotionEvent event = MotionEvent.obtain(
                downTime, eventTime + longPressTimeoutMs, MotionEvent.ACTION_MOVE,
                FAKE_COORD_X + 100, FAKE_COORD_Y + 100, 0);
        mGestureHandler.onTouchEvent(event);

        assertEquals("Scrolling should have started",
                GestureEventType.SCROLL_BY,
                        mMockMotionEventDelegate.mMostRecentGestureEvent.mType);
        assertTrue(mMockMotionEventDelegate.mGestureTypeList.contains(
                GestureEventType.SCROLL_START));
        assertTrue(mMockMotionEventDelegate.mGestureTypeList.contains(
                GestureEventType.TAP_CANCEL));

        event = motionEvent(MotionEvent.ACTION_UP, downTime, eventTime + longPressTimeoutMs);
        mGestureHandler.onTouchEvent(event);
        assertFalse("Scrolling should have prevented the LONG_TAP",
                mMockMotionEventDelegate.mGestureTypeList.contains(
                        GestureEventType.LONG_TAP));
    }

    /**
     * Verify that LONG_PRESS is not fired during a double-tap sequence.
     *
     * @throws Exception
     */
    @SmallTest
    @Feature({"Gestures"})
    public void testNoGestureLongPressDuringDoubleTap() throws Exception {
        final long downTime = SystemClock.uptimeMillis();
        final long eventTime = SystemClock.uptimeMillis();

        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                setUp();
                mGestureHandler.updateDoubleTapSupport(true);

                MotionEvent event = MotionEvent.obtain(
                        downTime, eventTime, MotionEvent.ACTION_DOWN,
                        FAKE_COORD_X, FAKE_COORD_Y, 0);
                assertTrue(mGestureHandler.onTouchEvent(event));

                event = MotionEvent.obtain(
                        downTime, eventTime + 1, MotionEvent.ACTION_UP,
                        FAKE_COORD_X, FAKE_COORD_Y, 0);
                mGestureHandler.onTouchEvent(event);
                assertEquals("A GESTURE_SINGLE_TAP_UNCONFIRMED event should have been sent",
                        GestureEventType.SINGLE_TAP_UNCONFIRMED,
                        mMockMotionEventDelegate.mMostRecentGestureEvent.mType);

                event = MotionEvent.obtain(
                        downTime + 2, eventTime + 2, MotionEvent.ACTION_DOWN,
                        FAKE_COORD_X, FAKE_COORD_Y, 0);
                assertTrue(mGestureHandler.onTouchEvent(event));
                assertEquals("A GESTURE_TAP_DOWN event should have been sent ",
                        GestureEventType.TAP_DOWN,
                        mMockMotionEventDelegate.mMostRecentGestureEvent.mType);
                assertTrue(mGestureHandler.isDoubleTapActive());
            }
        });

        final int longPressTimeoutMs = ViewConfiguration.getLongPressTimeout()
                + ViewConfiguration.getTapTimeout() + 10;

        assertFalse(mMockMotionEventDelegate.mLongPressCalled.await(
                longPressTimeoutMs, TimeUnit.MILLISECONDS));

        assertFalse("A LONG_PRESS gesture should not have been sent",
                GestureEventType.LONG_PRESS
                        == mMockMotionEventDelegate.mMostRecentGestureEvent.mType);

        MotionEvent event = MotionEvent.obtain(
                downTime + 2, eventTime + longPressTimeoutMs, MotionEvent.ACTION_MOVE,
                FAKE_COORD_X + 20, FAKE_COORD_Y + 20, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals("A double tap drag should have started",
                GestureEventType.PINCH_BEGIN,
                mMockMotionEventDelegate.mMostRecentGestureEvent.mType);
        assertTrue(mGestureHandler.isDoubleTapActive());

        event = MotionEvent.obtain(
                downTime + 2, eventTime + longPressTimeoutMs, MotionEvent.ACTION_UP,
                FAKE_COORD_X, FAKE_COORD_Y + 1, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals("A double tap drag should have ended",
                GestureEventType.SCROLL_END,
                mMockMotionEventDelegate.mMostRecentGestureEvent.mType);
        assertFalse(mGestureHandler.isDoubleTapActive());
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

        MotionEvent event = motionEvent(MotionEvent.ACTION_DOWN, downTime, downTime);
        assertTrue(mGestureHandler.onTouchEvent(event));

        event = MotionEvent.obtain(
                downTime, eventTime + 10, MotionEvent.ACTION_MOVE,
                FAKE_COORD_X, FAKE_COORD_Y + scaledTouchSlop + scrollDelta, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));

        assertEquals("We should have started scrolling",
                GestureEventType.SCROLL_BY,
                mMockMotionEventDelegate.mMostRecentGestureEvent.mType);

        MockMotionEventDelegate.GestureEvent gestureEvent =
                mMockMotionEventDelegate.mMostRecentGestureEvent;
        assertNotNull(gestureEvent);
        Bundle extraParams = gestureEvent.getExtraParams();
        assertEquals(0, extraParams.getInt(ContentViewGestureHandler.DISTANCE_X));
        assertEquals(-scrollDelta, extraParams.getInt(ContentViewGestureHandler.DISTANCE_Y));
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

        MotionEvent event = motionEvent(MotionEvent.ACTION_DOWN, downTime, downTime);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals("A TAP_DOWN gesture should have been sent",
                GestureEventType.TAP_DOWN,
                        mMockMotionEventDelegate.mMostRecentGestureEvent.mType);

        event = motionEvent(MotionEvent.ACTION_UP, downTime, eventTime + 10);
        assertFalse(mGestureHandler.onTouchEvent(event));
        assertEquals("A GESTURE_SINGLE_TAP_UNCONFIRMED gesture should have been sent",
                GestureEventType.SINGLE_TAP_UNCONFIRMED,
                        mMockMotionEventDelegate.mMostRecentGestureEvent.mType);
        assertFalse(mMockMotionEventDelegate.mGestureTypeList.contains(
                GestureEventType.SINGLE_TAP_CONFIRMED));
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

        MotionEvent event = motionEvent(MotionEvent.ACTION_DOWN, downTime, downTime);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals(GestureEventType.TAP_DOWN,
                mMockMotionEventDelegate.mMostRecentGestureEvent.mType);
        assertTrue(mGestureHandler.needsTapEndingEventForTesting());
        event = motionEvent(MotionEvent.ACTION_UP, downTime, eventTime + 5);
        mGestureHandler.onTouchEvent(event);
        assertEquals(GestureEventType.SINGLE_TAP_UNCONFIRMED,
                mMockMotionEventDelegate.mMostRecentGestureEvent.mType);
        assertTrue("An unconfirmed tap does not terminate the tap down.",
                mGestureHandler.needsTapEndingEventForTesting());

        // A confirmed tap is a tap-ending event.
        downTime += 20;
        eventTime += 20;
        mMockMotionEventDelegate.mGestureTypeList.clear();
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
        assertEquals(GestureEventType.SINGLE_TAP_CONFIRMED,
                mMockMotionEventDelegate.mMostRecentGestureEvent.mType);
        assertFalse("A confirmed single tap should terminate the tap down.",
                mGestureHandler.needsTapEndingEventForTesting());

        // A double tap gesture is a tap-ending event.
        downTime += 20;
        eventTime += 20;
        mMockMotionEventDelegate.mGestureTypeList.clear();
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
        assertEquals(GestureEventType.DOUBLE_TAP,
                mMockMotionEventDelegate.mMostRecentGestureEvent.mType);
        assertFalse("A double tap should terminate the tap down.",
                mGestureHandler.needsTapEndingEventForTesting());

        // A double tap drag gesture will trigger a tap-ending event.
        downTime += 20;
        eventTime += 20;
        mMockMotionEventDelegate.mGestureTypeList.clear();
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
        assertTrue(mMockMotionEventDelegate.mGestureTypeList.contains(
                GestureEventType.SCROLL_START));
        assertTrue(mMockMotionEventDelegate.mGestureTypeList.contains(
                GestureEventType.TAP_CANCEL));
        event = MotionEvent.obtain(
                eventTime + 10, eventTime + 20, MotionEvent.ACTION_UP,
                FAKE_COORD_X, FAKE_COORD_Y, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertFalse(mGestureHandler.needsTapEndingEventForTesting());

        // A scroll event will trigger a tap-ending (cancel) event.
        downTime += 25;
        eventTime += 25;
        mMockMotionEventDelegate.mGestureTypeList.clear();
        event = MotionEvent.obtain(
                downTime, downTime, MotionEvent.ACTION_DOWN,
                FAKE_COORD_X, FAKE_COORD_Y, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertTrue(mGestureHandler.needsTapEndingEventForTesting());
        event = MotionEvent.obtain(
                downTime, eventTime + 5, MotionEvent.ACTION_MOVE,
                FAKE_COORD_X, FAKE_COORD_Y + 100, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertTrue(mMockMotionEventDelegate.mGestureTypeList.contains(
                GestureEventType.SCROLL_START));
        assertTrue(mMockMotionEventDelegate.mGestureTypeList.contains(
                GestureEventType.TAP_CANCEL));
        assertFalse("A scroll should terminate the tap down.",
                   mGestureHandler.needsTapEndingEventForTesting());
        assertFalse(mGestureHandler.needsTapEndingEventForTesting());
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

        mGestureHandler.updateShouldDisableDoubleTap(true);

        MotionEvent event = MotionEvent.obtain(
                downTime, downTime, MotionEvent.ACTION_DOWN,
                FAKE_COORD_X, FAKE_COORD_Y, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals("Only GESTURE_TAP_DOWN should have been sent",
                1, mMockMotionEventDelegate.mGestureTypeList.size());

        event = MotionEvent.obtain(
                downTime, eventTime + 5, MotionEvent.ACTION_UP,
                FAKE_COORD_X, FAKE_COORD_Y, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals("A GESTURE_SINGLE_TAP_CONFIRMED event should have been sent",
                GestureEventType.SINGLE_TAP_CONFIRMED,
                mMockMotionEventDelegate.mMostRecentGestureEvent.mType);
        assertEquals("Only GESTURE_TAP_DOWN and GESTURE_SINGLE_TAP_CONFIRMED " +
                "should have been sent",
                2, mMockMotionEventDelegate.mGestureTypeList.size());

        event = MotionEvent.obtain(
                eventTime + 10, eventTime + 10, MotionEvent.ACTION_DOWN,
                FAKE_COORD_X, FAKE_COORD_Y, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals("Only GESTURE_TAP_DOWN, " +
                "GESTURE_SINGLE_TAP_CONFIRMED and " +
                "GESTURE_TAP_DOWN should have been sent",
                3, mMockMotionEventDelegate.mGestureTypeList.size());

        event = MotionEvent.obtain(
                eventTime + 10, eventTime + 15, MotionEvent.ACTION_UP,
                FAKE_COORD_X, FAKE_COORD_Y, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals("A double tap should not have occurred",
                GestureEventType.SINGLE_TAP_CONFIRMED,
                mMockMotionEventDelegate.mMostRecentGestureEvent.mType);
        assertEquals("Only GESTURE_TAP_DOWN, " +
                "GESTURE_SINGLE_TAP_CONFIRMED, " +
                "GESTURE_TAP_DOWN and " +
                "GESTURE_SINGLE_TAP_CONFIRMED should have been sent",
                4, mMockMotionEventDelegate.mGestureTypeList.size());
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
                mMockMotionEventDelegate.mGestureTypeList.contains(
                        GestureEventType.SCROLL_START));
        assertFalse("No GESTURE_PINCH_BEGIN should have been sent",
                mMockMotionEventDelegate.mGestureTypeList.contains(
                        GestureEventType.PINCH_BEGIN));

        event = MotionEvent.obtain(
                downTime2, downTime2 + 10, MotionEvent.ACTION_MOVE,
                FAKE_COORD_X, FAKE_COORD_Y + 200, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals("GESTURE_SCROLL_BY should have been sent",
                GestureEventType.SCROLL_BY,
                mMockMotionEventDelegate.mMostRecentGestureEvent.mType);
        assertEquals("GESTURE_SCROLL_BY should have been sent",
                event.getEventTime(),
                mMockMotionEventDelegate.mMostRecentGestureEvent.getTimeMs());
        assertTrue("No GESTURE_PINCH_BY should have been sent",
                GestureEventType.PINCH_BY !=
                mMockMotionEventDelegate.mMostRecentGestureEvent.mType);

        event = MotionEvent.obtain(
                downTime2, downTime2 + 15, MotionEvent.ACTION_UP,
                FAKE_COORD_X, FAKE_COORD_Y + 200, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertFalse("No GESTURE_PINCH_END should have been sent",
                mMockMotionEventDelegate.mGestureTypeList.contains(
                        GestureEventType.PINCH_END));
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
                mMockMotionEventDelegate.mGestureTypeList.contains(
                        GestureEventType.SCROLL_START));
        assertEquals("GESTURE_PINCH_BEGIN should have been sent",
                GestureEventType.PINCH_BEGIN,
                mMockMotionEventDelegate.mMostRecentGestureEvent.mType);

        // Simulate setting a fixed page scale (or a mobile viewport);
        // this should not disrupt the current double-tap gesture.
        mGestureHandler.updateShouldDisableDoubleTap(true);

        // Double tap zoom updates should continue.
        event = MotionEvent.obtain(
                downTime2, downTime2 + 10, MotionEvent.ACTION_MOVE,
                FAKE_COORD_X, FAKE_COORD_Y + 200, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertTrue("GESTURE_SCROLL_BY should have been sent",
                mMockMotionEventDelegate.mGestureTypeList.contains(
                        GestureEventType.SCROLL_BY));
        assertEquals("GESTURE_PINCH_BY should have been sent",
                GestureEventType.PINCH_BY,
                mMockMotionEventDelegate.mMostRecentGestureEvent.mType);
        event = MotionEvent.obtain(
                downTime2, downTime2 + 15, MotionEvent.ACTION_UP,
                FAKE_COORD_X, FAKE_COORD_Y + 200, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertTrue("GESTURE_PINCH_END should have been sent",
                mMockMotionEventDelegate.mGestureTypeList.contains(
                        GestureEventType.PINCH_END));
        assertEquals("GESTURE_SCROLL_END should have been sent",
                GestureEventType.SCROLL_END,
                mMockMotionEventDelegate.mMostRecentGestureEvent.mType);

        // The double-tap gesture has finished, but the page scale is fixed.
        // The same event sequence should not generate any double tap getsures.
        mMockMotionEventDelegate.mGestureTypeList.clear();
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
                mMockMotionEventDelegate.mGestureTypeList.contains(
                        GestureEventType.SCROLL_START));
        assertFalse("GESTURE_PINCH_BEGIN should not have been sent",
                mMockMotionEventDelegate.mGestureTypeList.contains(
                        GestureEventType.PINCH_BEGIN));

        // Double tap zoom updates should not be sent.
        // Instead, the second tap drag becomes a scroll gesture sequence.
        event = MotionEvent.obtain(
                downTime2, downTime2 + 10, MotionEvent.ACTION_MOVE,
                FAKE_COORD_X, FAKE_COORD_Y + 200, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertTrue("GESTURE_SCROLL_BY should have been sent",
                mMockMotionEventDelegate.mGestureTypeList.contains(
                        GestureEventType.SCROLL_BY));
        assertFalse("GESTURE_PINCH_BY should not have been sent",
                mMockMotionEventDelegate.mGestureTypeList.contains(
                        GestureEventType.PINCH_BY));
        event = MotionEvent.obtain(
                downTime2, downTime2 + 15, MotionEvent.ACTION_UP,
                FAKE_COORD_X, FAKE_COORD_Y + 200, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertFalse("GESTURE_PINCH_END should not have been sent",
                mMockMotionEventDelegate.mGestureTypeList.contains(
                        GestureEventType.PINCH_END));
    }

    /**
     * Verify that pinch zoom sends the proper event sequence.
     * @throws Exception
     */
    @SmallTest
    @Feature({"Gestures"})
    public void testPinchZoom() throws Exception {
        final long downTime = SystemClock.uptimeMillis();
        final long eventTime = SystemClock.uptimeMillis();
        final Context context = getInstrumentation().getTargetContext();
        final int scaledTouchSlop = ViewConfiguration.get(context).getScaledTouchSlop();

        mGestureHandler.updateMultiTouchSupport(true);

        final int secondaryCoordX = FAKE_COORD_X + 20 * scaledTouchSlop;
        final int secondaryCoordY = FAKE_COORD_Y + 20 * scaledTouchSlop;

        PointerProperties pp0 = new PointerProperties();
        pp0.id = 0;
        pp0.toolType = MotionEvent.TOOL_TYPE_FINGER;
        PointerProperties pp1 = new PointerProperties();
        pp1.id = 1;
        pp1.toolType = MotionEvent.TOOL_TYPE_FINGER;

        PointerCoords pc0 = new PointerCoords();
        pc0.x = FAKE_COORD_X;
        pc0.y = FAKE_COORD_Y;
        pc0.pressure = 1;
        pc0.size = 1;
        PointerCoords pc1 = new PointerCoords();
        pc1.x = secondaryCoordX;
        pc1.y = secondaryCoordY;
        pc1.pressure = 1;
        pc1.size = 1;

        MotionEvent event = MotionEvent.obtain(
                eventTime, eventTime, MotionEvent.ACTION_DOWN,
                1, new PointerProperties[] { pp0 }, new PointerCoords[] { pc0 },
                0, 0, 1.0f, 1.0f, 0, 0, InputDevice.SOURCE_CLASS_POINTER, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals(GestureEventType.TAP_DOWN,
                mMockMotionEventDelegate.mMostRecentGestureEvent.mType);
        assertEquals("Only GESTURE_TAP_DOWN should have been sent",
                1, mMockMotionEventDelegate.mGestureTypeList.size());

        event = MotionEvent.obtain(
                eventTime, eventTime, MotionEvent.ACTION_POINTER_DOWN,
                2, new PointerProperties[] { pp1, pp0 }, new PointerCoords[] { pc1, pc0 },
                0, 0, 1.0f, 1.0f, 0, 0, InputDevice.SOURCE_CLASS_POINTER, 0);
        mGestureHandler.onTouchEvent(event);
        assertEquals("No additional gestures should be triggered on a secondary pointer press",
                1, mMockMotionEventDelegate.mGestureTypeList.size());

        pc1.x = secondaryCoordX + 5 * scaledTouchSlop;
        pc1.y = secondaryCoordY + 5 * scaledTouchSlop;

        event = MotionEvent.obtain(
                eventTime, eventTime + 10, MotionEvent.ACTION_MOVE,
                2, new PointerProperties[] { pp1, pp0 }, new PointerCoords[] { pc1, pc0 },
                0, 0, 1.0f, 1.0f, 0, 0, InputDevice.SOURCE_CLASS_POINTER, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals("Only GESTURE_TAP_DOWN, " +
                "GESTURE_TAP_CANCEL, " +
                "GESTURE_SCROLL_BEGIN, " +
                "GESTURE_PINCH_BEGIN, " +
                "GESTURE_SCROLL_BY, and " +
                "GESTURE_PINCH_BY should have been sent",
                6, mMockMotionEventDelegate.mGestureTypeList.size());
        assertTrue(mMockMotionEventDelegate.mGestureTypeList.contains(
                GestureEventType.PINCH_BEGIN));
        assertTrue(mMockMotionEventDelegate.mGestureTypeList.contains(
                GestureEventType.SCROLL_START));
        assertTrue(mMockMotionEventDelegate.mGestureTypeList.contains(
                GestureEventType.PINCH_BY));
        assertTrue(mMockMotionEventDelegate.mGestureTypeList.contains(
                GestureEventType.SCROLL_BY));

        event = MotionEvent.obtain(
                eventTime, eventTime + 10, MotionEvent.ACTION_POINTER_UP,
                2, new PointerProperties[] { pp1, pp0 }, new PointerCoords[] { pc1, pc0 },
                0, 0, 1.0f, 1.0f, 0, 0, InputDevice.SOURCE_CLASS_POINTER, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals(GestureEventType.PINCH_END,
                mMockMotionEventDelegate.mMostRecentGestureEvent.mType);
        assertEquals("Only GESTURE_PINCH_END should have been sent",
                7, mMockMotionEventDelegate.mGestureTypeList.size());

        event = MotionEvent.obtain(
                eventTime, eventTime, MotionEvent.ACTION_UP,
                1, new PointerProperties[] { pp0 }, new PointerCoords[] { pc0 },
                0, 0, 1.0f, 1.0f, 0, 0, InputDevice.SOURCE_CLASS_POINTER, 0);
        mGestureHandler.onTouchEvent(event);
        assertEquals(GestureEventType.SCROLL_END,
                mMockMotionEventDelegate.mMostRecentGestureEvent.mType);
        assertEquals("Only GESTURE_SCROLL_END should have been sent",
                8, mMockMotionEventDelegate.mGestureTypeList.size());
    }

    /**
     * Verify that the timer of LONG_PRESS will be cancelled when scrolling begins so
     * LONG_PRESS and LONG_TAP won't be triggered.
     *
     * @throws Exception
     */
    @SmallTest
    @Feature({"Gestures"})
    public void testGesturesCancelledAfterLongPressCausesLostFocus() throws Exception {
        final long downTime = SystemClock.uptimeMillis();
        final long eventTime = SystemClock.uptimeMillis();

        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                setUp();
                mGestureHandler = new ContentViewGestureHandler(
                        getInstrumentation().getTargetContext(), mMockMotionEventDelegate);
                MotionEvent event = motionEvent(MotionEvent.ACTION_DOWN, downTime, downTime);
                assertTrue(mGestureHandler.onTouchEvent(event));
            }
        });

        final long longPressTimeoutMs = ViewConfiguration.getLongPressTimeout()
                + ViewConfiguration.getTapTimeout() + 10;
        assertTrue(mMockMotionEventDelegate.mLongPressCalled.await(
                ScalableTimeout.scaleTimeout(longPressTimeoutMs), TimeUnit.MILLISECONDS));

        assertEquals("A LONG_PRESS gesture should have been sent",
                GestureEventType.LONG_PRESS,
                        mMockMotionEventDelegate.mMostRecentGestureEvent.mType);

        mGestureHandler.onWindowFocusLost();

        assertEquals("The LONG_PRESS should have been cancelled by loss of focus",
                GestureEventType.TAP_CANCEL,
                        mMockMotionEventDelegate.mMostRecentGestureEvent.mType);

        MotionEvent event = motionEvent(
                MotionEvent.ACTION_UP, downTime, eventTime + longPressTimeoutMs);
        mGestureHandler.onTouchEvent(event);
        assertFalse("Tap cancellation should have prevented the LONG_TAP",
                mMockMotionEventDelegate.mGestureTypeList.contains(
                        GestureEventType.LONG_TAP));
    }

    /**
     * Verify that ignoring the remaining touch sequence triggers proper touch and gesture
     * cancellation.
     *
     * @throws Exception
     */
    @SmallTest
    @Feature({"Gestures"})
    public void testSetIgnoreRemainingTouchEvents() throws Exception {
        final long downTime = SystemClock.uptimeMillis();
        final long eventTime = SystemClock.uptimeMillis();

        mGestureHandler.setIgnoreRemainingTouchEvents();
        assertTrue("If there was no active touch sequence, ignoring it should be a no-op",
                mMockMotionEventDelegate.mGestureTypeList.isEmpty());
        assertEquals("No MotionEvents should have been generated",
                -1, mMockMotionEventDelegate.mMostRecentMotionEventAction);

        MotionEvent event = motionEvent(MotionEvent.ACTION_DOWN, downTime, downTime);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals(GestureEventType.TAP_DOWN,
                mMockMotionEventDelegate.mMostRecentGestureEvent.mType);

        mGestureHandler.setIgnoreRemainingTouchEvents();
        assertEquals("The TAP_DOWN should have been cancelled",
                GestureEventType.TAP_CANCEL,
                mMockMotionEventDelegate.mMostRecentGestureEvent.mType);
        assertEquals("An ACTION_CANCEL MotionEvent should have been inserted",
                MotionEvent.ACTION_CANCEL, mMockMotionEventDelegate.mMostRecentMotionEventAction);

        // Subsequent MotionEvent's are dropped until ACTION_DOWN.
        event = motionEvent(MotionEvent.ACTION_MOVE, downTime, eventTime + 5);
        assertFalse(mGestureHandler.onTouchEvent(event));

        event = motionEvent(MotionEvent.ACTION_UP, downTime, eventTime + 10);
        assertFalse(mGestureHandler.onTouchEvent(event));

        event = motionEvent(MotionEvent.ACTION_DOWN, downTime + 15, downTime + 15);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals(GestureEventType.TAP_DOWN,
                mMockMotionEventDelegate.mMostRecentGestureEvent.mType);
    }
}
