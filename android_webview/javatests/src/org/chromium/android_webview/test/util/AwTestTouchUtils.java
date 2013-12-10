// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test.util;

import android.os.SystemClock;
import android.view.MotionEvent;
import android.view.View;

import java.util.concurrent.CountDownLatch;

/**
 * A touch utility class that injects the events directly into the view.
 * TODO(mkosiba): Merge with TestTouchUtils.
 *
 * This is similar to TestTouchUtils but injects the events directly into the view class rather
 * than going through Instrumentation. This is so that we can avoid the INJECT_PERMISSIONS
 * exception when a modal dialog pops over the test activity.
 */
public class AwTestTouchUtils {
    private static void sendAction(View view, int action, long downTime, float x, float y) {
        long eventTime = SystemClock.uptimeMillis();
        MotionEvent event = MotionEvent.obtain(downTime, eventTime, action, x, y, 0);
        view.onTouchEvent(event);
    }

    private static long dragStart(View view, float x, float y) {
        long downTime = SystemClock.uptimeMillis();
        sendAction(view, MotionEvent.ACTION_DOWN, downTime, x, y);
        return downTime;
    }

    private static void dragTo(View view, float fromX, float toX, float fromY,
            float toY, int stepCount, long downTime) {
        float x = fromX;
        float y = fromY;
        float yStep = (toY - fromY) / stepCount;
        float xStep = (toX - fromX) / stepCount;
        for (int i = 0; i < stepCount; ++i) {
            y += yStep;
            x += xStep;
            sendAction(view, MotionEvent.ACTION_MOVE, downTime, x, y);
        }
    }

    private static void dragEnd(View view, float x, float y, long downTime) {
        sendAction(view, MotionEvent.ACTION_UP, downTime, x, y);
    }

    /**
     * Performs a drag between the given coordinates, specified relative to the given view.
     * This is safe to call from the instrumentation thread and will invoke the drag
     * asynchronously.
     *
     * @param view The view the coordinates are relative to.
     * @param fromX The relative x-coordinate of the start point of the drag.
     * @param toX The relative x-coordinate of the end point of the drag.
     * @param fromY The relative y-coordinate of the start point of the drag.
     * @param toY The relative y-coordinate of the end point of the drag.
     * @param stepCount The total number of motion events that should be generated during the drag.
     * @param completionLatch The .countDown method is called on this latch once the drag finishes.
     */
    public static void dragCompleteView(final View view, final int fromX, final int toX,
            final int fromY, final int toY, final int stepCount,
            final CountDownLatch completionLatch) {
        view.post(new Runnable() {
            @Override
            public void run() {
                long downTime = dragStart(view, fromX, fromY);
                dragTo(view, fromX, toX, fromY, toY, stepCount, downTime);
                dragEnd(view, toX, toY, downTime);
                if (completionLatch != null) {
                    completionLatch.countDown();
                }
            }
        });
    }
}

