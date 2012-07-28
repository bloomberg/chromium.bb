// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.content.Context;
import android.os.Bundle;
import android.os.SystemClock;
import android.test.InstrumentationTestCase;
import android.test.suitebuilder.annotation.LargeTest;
import android.test.suitebuilder.annotation.SmallTest;
import android.util.Log;
import android.view.GestureDetector;
import android.view.MotionEvent;
import android.view.ViewConfiguration;
import android.view.GestureDetector.OnGestureListener;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

import org.chromium.base.test.Feature;
import org.chromium.base.test.ScalableTimeout;
import org.chromium.content.browser.ContentViewGestureHandler.MotionEventDelegate;

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
        MotionEvent mLastFling2;
        CountDownLatch mLongPressCalled;
        MotionEvent mLastScroll1;
        MotionEvent mLastScroll2;
        float mLastScrollDistanceX;
        float mLastScrollDistanceY;

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
            mLastScroll1 = e1;
            mLastScroll2 = e2;
            mLastScrollDistanceX = distanceX;
            mLastScrollDistanceY = distanceY;
            return true;
        }

        @Override
        public boolean onFling(MotionEvent e1, MotionEvent e2, float velocityX, float velocityY) {
            mLastFling1 = e1;
            mLastFling2 = e2;
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
            // Not implemented.
            return false;
        }

        @Override
        public boolean sendGesture(int type, long timeMs, int x, int y, Bundle extraParams) {
            Log.i(TAG,"Gesture event received with type id " + type);
            return false;
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
    @Feature({"Android-WebView"})
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
     * Verify that a DOWN followed by a MOVE will trigger fling (but not LONG).
     * @throws Exception
     */
    @SmallTest
    @Feature({"Android-WebView"})
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
}
