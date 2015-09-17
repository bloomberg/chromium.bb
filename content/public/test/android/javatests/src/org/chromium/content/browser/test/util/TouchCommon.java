// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.test.util;

import android.app.Activity;
import android.os.SystemClock;
import android.test.TouchUtils;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewConfiguration;

import org.chromium.base.ThreadUtils;

import java.util.concurrent.Callable;
import java.util.concurrent.ExecutionException;

/**
 * Touch-related functionality reused across test cases.
 */
public class TouchCommon {
    // Prevent instantiation.
    private TouchCommon() {
    }

    /**
     * Starts (synchronously) a drag motion. Normally followed by dragTo() and dragEnd().
     *
     * @activity activity The activity where the touch action is being performed.
     * @param x
     * @param y
     * @param downTime (in ms)
     * @see TouchUtils
     */
    public static void dragStart(Activity activity, float x, float y, long downTime) {
        MotionEvent event = MotionEvent.obtain(downTime, downTime,
                MotionEvent.ACTION_DOWN, x, y, 0);
        dispatchTouchEvent(getRootViewForActivity(activity), event);
    }

    /**
     * Drags / moves (synchronously) to the specified coordinates. Normally preceeded by
     * dragStart() and followed by dragEnd()
     *
     * @activity activity The activity where the touch action is being performed.
     * @param fromX
     * @param toX
     * @param fromY
     * @param toY
     * @param stepCount
     * @param downTime (in ms)
     * @see TouchUtils
     */
    public static void dragTo(Activity activity, float fromX, float toX, float fromY,
            float toY, int stepCount, long downTime) {
        View rootView = getRootViewForActivity(activity);
        float x = fromX;
        float y = fromY;
        float yStep = (toY - fromY) / stepCount;
        float xStep = (toX - fromX) / stepCount;
        for (int i = 0; i < stepCount; ++i) {
            y += yStep;
            x += xStep;
            long eventTime = SystemClock.uptimeMillis();
            MotionEvent event = MotionEvent.obtain(downTime, eventTime,
                    MotionEvent.ACTION_MOVE, x, y, 0);
            dispatchTouchEvent(rootView, event);
        }
    }

    /**
     * Finishes (synchronously) a drag / move at the specified coordinate.
     * Normally preceeded by dragStart() and dragTo().
     *
     * @activity activity The activity where the touch action is being performed.
     * @param x
     * @param y
     * @param downTime (in ms)
     * @see TouchUtils
     */
    public static void dragEnd(Activity activity, float x, float y, long downTime) {
        long eventTime = SystemClock.uptimeMillis();
        MotionEvent event = MotionEvent.obtain(downTime, eventTime,
                MotionEvent.ACTION_UP, x, y, 0);
        dispatchTouchEvent(getRootViewForActivity(activity), event);
    }

    /**
     * Sends (synchronously) a single click to an absolute screen coordinates.
     *
     * @activity activity The activity where the touch action is being performed.
     * @param x screen absolute
     * @param y screen absolute
     * @see TouchUtils
     */
    public static void singleClick(Activity activity, float x, float y) {
        singleClickInternal(getRootViewForActivity(activity), x, y);
    }

    /**
     * Sends (synchronously) a single click to the View at the specified coordinates.
     *
     * @param v The view to be clicked.
     * @param x Relative x location to v
     * @param y Relative y location to v
     */
    public static void singleClickView(View v, int x, int y) {
        int location[] = getAbsoluteLocationFromRelative(v, x, y);
        int absoluteX = location[0];
        int absoluteY = location[1];
        singleClickInternal(v.getRootView(), absoluteX, absoluteY);
    }

    /**
     * Sends (synchronously) a single click to the center of the View.
     */
    public static void singleClickView(View v) {
        singleClickView(v, v.getWidth() / 2, v.getHeight() / 2);
    }

    private static void singleClickInternal(View view, float x, float y) {
        long downTime = SystemClock.uptimeMillis();
        long eventTime = SystemClock.uptimeMillis();

        MotionEvent event = MotionEvent.obtain(
                downTime, eventTime, MotionEvent.ACTION_DOWN, x, y, 0);
        dispatchTouchEvent(view, event);

        eventTime = SystemClock.uptimeMillis();
        event = MotionEvent.obtain(
                downTime, eventTime, MotionEvent.ACTION_UP, x, y, 0);
        dispatchTouchEvent(view, event);
    }

    /**
     * Sends (synchronously) a single click on the specified relative coordinates inside
     * a given view.
     *
     * @param view The view to be clicked.
     * @param x screen absolute
     * @param y screen absolute
     * @see TouchUtils
     */
    public static void singleClickViewRelative(View view, int x, int y) {
        long downTime = SystemClock.uptimeMillis();
        long eventTime = SystemClock.uptimeMillis();

        MotionEvent event = MotionEvent.obtain(
                downTime, eventTime, MotionEvent.ACTION_DOWN, x, y, 0);
        dispatchTouchEvent(view, event);

        eventTime = SystemClock.uptimeMillis();
        event = MotionEvent.obtain(
                downTime, eventTime, MotionEvent.ACTION_UP, x, y, 0);
        dispatchTouchEvent(view, event);
    }

    /**
     * Sends (synchronously) a long press to an absolute screen coordinates.
     *
     * @activity activity The activity where the touch action is being performed.
     * @param x screen absolute
     * @param y screen absolute
     * @see TouchUtils
     */
    public static void longPress(Activity activity, float x, float y) {
        longPressInternal(getRootViewForActivity(activity), x, y);
    }

    /**
     * Sends (synchronously) a long press to the View at the specified coordinates.
     *
     * @param v The view to be clicked.
     * @param x Relative x location to v
     * @param y Relative y location to v
     */
    public static void longPressView(View v, int x, int y) {
        int location[] = getAbsoluteLocationFromRelative(v, x, y);
        int absoluteX = location[0];
        int absoluteY = location[1];
        longPressInternal(v.getRootView(), absoluteX, absoluteY);
    }

    private static void longPressInternal(View view, float x, float y) {
        long downTime = SystemClock.uptimeMillis();
        long eventTime = SystemClock.uptimeMillis();

        MotionEvent event = MotionEvent.obtain(
                downTime, eventTime, MotionEvent.ACTION_DOWN, x, y, 0);
        dispatchTouchEvent(view, event);

        int longPressTimeout = ViewConfiguration.getLongPressTimeout();

        // Long press is flaky with just longPressTimeout. Doubling the time to be safe.
        SystemClock.sleep(longPressTimeout * 2);

        eventTime = SystemClock.uptimeMillis();
        event = MotionEvent.obtain(
                downTime, eventTime, MotionEvent.ACTION_UP, x, y, 0);
        dispatchTouchEvent(view, event);
    }

    private static View getRootViewForActivity(final Activity activity) {
        try {
            View view = ThreadUtils.runOnUiThreadBlocking(new Callable<View>() {
                @Override
                public View call() throws Exception {
                    return activity.findViewById(android.R.id.content).getRootView();
                }
            });
            assert view != null : "Failed to find root view for activity";
            return view;
        } catch (ExecutionException e) {
            throw new RuntimeException("Dispatching touch event failed", e);
        }
    }

    /**
     * Send a MotionEvent to the specified view instead of the root view.
     * For example AutofillPopup window that is above the root view.
     * @param view The view that should receive the event.
     * @param event The view to be dispatched.
     */
    private static void dispatchTouchEvent(final View view, final MotionEvent event) {
        try {
            ThreadUtils.runOnUiThreadBlocking(new Runnable() {
                @Override
                public void run() {
                    view.dispatchTouchEvent(event);
                }
            });
        } catch (Throwable e) {
            throw new RuntimeException("Dispatching touch event failed", e);
        }
    }

    /**
     * Returns the absolute location in screen coordinates from location relative
     * to view.
     * @param v The view the coordinates are relative to.
     * @param x Relative x location.
     * @param y Relative y location.
     * @return absolute x and y location in an array.
     */
    private static int[] getAbsoluteLocationFromRelative(View v, int x, int y) {
        int location[] = new int[2];
        v.getLocationOnScreen(location);
        location[0] += x;
        location[1] += y;
        return location;
    }
}
