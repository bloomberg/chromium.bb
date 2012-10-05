// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.test.util;

import android.app.Activity;
import android.app.Instrumentation;
import android.view.View;

import java.lang.reflect.Field;
import java.util.concurrent.Semaphore;
import java.util.concurrent.TimeUnit;

import junit.framework.Assert;

/**
 * Collection of UI utilities.
 */
public class UiUtils {
    private static final int WAIT_FOR_RESPONSE_MS = 10000;  // timeout to wait for runOnUiThread()

    /**
     * Runs the runnable on the UI thread.
     *
     * @param activity The activity on which the runnable must run.
     * @param runnable The runnable to run.
     */
    public static void runOnUiThread(Activity activity, final Runnable runnable) {
        final Semaphore finishedSemaphore = new Semaphore(0);
        activity.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                runnable.run();
                finishedSemaphore.release();
            }});
        try {
            Assert.assertTrue(finishedSemaphore.tryAcquire(1, WAIT_FOR_RESPONSE_MS,
                    TimeUnit.MILLISECONDS));
        } catch (InterruptedException ignored) {
            Assert.assertTrue("Interrupted while waiting for main thread Runnable", false);
        }
    }

    /**
     * Waits for the UI thread to settle down.
     * <p>
     * Waits for an extra period of time after the UI loop is idle.
     *
     * @param instrumentation Instrumentation object used by the test.
     */
    public static void settleDownUI(Instrumentation instrumentation) throws InterruptedException {
        instrumentation.waitForIdleSync();
        Thread.sleep(1000);
    }

    /**
     * Find the parent View for the Id across all activities, as dialogs
     * will be displayed in a separate activity. Note that in the case
     * of several views sharing the same ID across activities, this will
     * return the first view found.
     *
     * @param viewId The Id of the view.
     * @return the view which contains the given id or null if no view with the given id exists.
     */
    public static View findParentViewForIdAcrossActivities(int viewId) {
        View[] availableViews = null;
        try {
            Class<?> windowManager = Class.forName("android.view.WindowManagerImpl");

            Field viewsField = windowManager.getDeclaredField("mViews");
            Field instanceField = windowManager.getDeclaredField("sWindowManager");
            viewsField.setAccessible(true);
            instanceField.setAccessible(true);
            Object instance = instanceField.get(null);
            availableViews = (View[]) viewsField.get(instance);
        } catch(Exception e) {
            Assert.fail("Could not fetch the available views.");
        }

        if (availableViews == null || availableViews.length == 0) {
            return null;
        }

        for (View view : availableViews) {
            if (view.findViewById(viewId) != null) {
                return view;
            }
        }

        return null;
    }
}
