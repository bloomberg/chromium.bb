// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media;

import android.app.Dialog;
import android.app.Instrumentation;
import android.os.SystemClock;
import android.support.v4.app.DialogFragment;
import android.support.v4.app.FragmentManager;
import android.view.MotionEvent;
import android.view.View;

import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.TestTouchUtils;

import java.util.ArrayList;
import java.util.concurrent.Callable;

/**
 * Test utils shared by MediaRouter and MediaRemote.
 */
public class RouterTestUtils {
    private static final String TAG = "RouterTestUtils";

    public static View waitForRouteButton(
            final ChromeActivity activity, final String chromecastName,
            int maxTimeoutMs, int intervalMs) {
        return waitForView(new Callable<View>() {
                @Override
                public View call() {
                    Dialog mediaRouteListDialog = getDialog(activity);
                    if (mediaRouteListDialog == null) {
                        Log.w(TAG, "Cannot find device selection dialog");
                        return null;
                    }
                    View mediaRouteList =
                            mediaRouteListDialog.findViewById(R.id.mr_chooser_list);
                    if (mediaRouteList == null) {
                        Log.w(TAG, "Cannot find device list");
                        return null;
                    }
                    ArrayList<View> routesWanted = new ArrayList<View>();
                    mediaRouteList.findViewsWithText(routesWanted, chromecastName,
                                                     View.FIND_VIEWS_WITH_TEXT);
                    if (routesWanted.size() == 0) {
                        Log.w(TAG, "Cannot find wanted device");
                        return null;
                    }
                    Log.i(TAG, "Found wanted device");
                    return routesWanted.get(0);
                }
            }, maxTimeoutMs, intervalMs);
    }

    public static Dialog waitForDialog(
            final ChromeActivity activity,
            int maxTimeoutMs, int intervalMs) {
        try {
            CriteriaHelper.pollUiThread(new Criteria() {
                    @Override
                    public boolean isSatisfied() {
                        try {
                            if (getDialog(activity) != null) {
                                Log.i(TAG, "Found device selection dialog");
                                return true;
                            } else {
                                Log.w(TAG, "Cannot find device selection dialog");
                                return false;
                            }
                        } catch (Exception e) {
                            return false;
                        }
                    }
                }, maxTimeoutMs, intervalMs);
            return getDialog(activity);
        } catch (Exception e) {
            return null;
        }
    }

    public static Dialog getDialog(ChromeActivity activity) {
        FragmentManager fm = activity.getSupportFragmentManager();
        if (fm == null) return null;
        return ((DialogFragment) fm.findFragmentByTag(
            "android.support.v7.mediarouter:MediaRouteChooserDialogFragment")).getDialog();
    }

    public static View waitForView(
            final Callable<View> getViewCallable, int maxTimeoutMs, int intervalMs) {
        try {
            CriteriaHelper.pollUiThread(new Criteria() {
                    @Override
                    public boolean isSatisfied() {
                        try {
                            return getViewCallable.call() != null;
                        } catch (Exception e) {
                            return false;
                        }
                    }
                }, maxTimeoutMs, intervalMs);
            return getViewCallable.call();
        } catch (Exception e) {
            return null;
        }
    }

    /**
     * Click a button. Unlike {@link RouterTestUtils#mouseSingleClickView} this directly accesses
     * the view and does not send motion events though the message queue. As such it doesn't require
     * the view to have been created by the instrumented activity, but gives less flexibility than
     * mouseSingleClickView. For example, if the view is hierachical, then clickButton will always
     * act on specified view, whereas mouseSingleClickView will send the events to the appropriate
     * child view. It is hence only really appropriate for simple views such as buttons.
     *
     * @param button the button to be clicked.
     */
    public static void clickButton(final View button) {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
                @Override
            public void run() {
                // Post the actual click to the button's message queue, to ensure that it has been
                // inflated before the click is received.
                button.post(new Runnable() {
                        @Override
                    public void run() {
                        button.performClick();
                    }
                });
            }
        });
    }

    private static void sendMouseAction(Instrumentation instrumentation, int action, long downTime,
            float x, float y) {
        long eventTime = SystemClock.uptimeMillis();
        MotionEvent.PointerCoords coords[] = new MotionEvent.PointerCoords[1];
        coords[0] = new MotionEvent.PointerCoords();
        coords[0].x = x;
        coords[0].y = y;
        MotionEvent.PointerProperties properties[] = new MotionEvent.PointerProperties[1];
        properties[0] = new MotionEvent.PointerProperties();
        properties[0].id = 0;
        properties[0].toolType = MotionEvent.TOOL_TYPE_FINGER;
        MotionEvent event = MotionEvent.obtain(downTime, eventTime, action, 1, properties, coords,
                0, 0, 0.0f, 0.0f, 0, 0, 0, 0);
        instrumentation.sendPointerSync(event);
        instrumentation.waitForIdleSync();
    }

    /**
     * Sends (synchronously) a single mouse click to an absolute screen coordinates.
     *
     * @param instrumentation Instrumentation object used by the test.
     * @param x Screen absolute x location.
     * @param y Screen absolute y location.
     */
    private static void mouseSingleClick(Instrumentation instrumentation, float x, float y) {
        long downTime = SystemClock.uptimeMillis();
        sendMouseAction(instrumentation, MotionEvent.ACTION_DOWN, downTime, x, y);
        sendMouseAction(instrumentation, MotionEvent.ACTION_UP, downTime, x, y);
    }

    /**
     * Sends (synchronously) a single mouse click to the View at the specified coordinates.
     *
     * @param instrumentation Instrumentation object used by the test.
     * @param v The view the coordinates are relative to.
     * @param x Relative x location to the view.
     * @param y Relative y location to the view.
     */
    public static void mouseSingleClickView(Instrumentation instrumentation, View v, int x,
            int y) {
        int location[] = TestTouchUtils.getAbsoluteLocationFromRelative(v, x, y);
        int absoluteX = location[0];
        int absoluteY = location[1];
        mouseSingleClick(instrumentation, absoluteX, absoluteY);
    }

    /**
     * Sends (synchronously) a single mouse click to the center of the View.
     *
     * @param instrumentation Instrumentation object used by the test.
     * @param v The view the coordinates are relative to.
     */
    public static void mouseSingleClickView(Instrumentation instrumentation, View v) {
        int x = v.getWidth() / 2;
        int y = v.getHeight() / 2;
        mouseSingleClickView(instrumentation, v, x, y);
    }
}
