// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

import android.os.Handler;
import android.os.Looper;

import org.chromium.content.browser.BrowserThreadUtilsImpl;

import java.util.concurrent.Callable;
import java.util.concurrent.FutureTask;

/**
 * Helper methods to deal with UI thread related tasks.
 */
public class BrowserThreadUtils {
    private BrowserThreadUtils() {}

    /**
     * Sets a flag specifying that a new UI thread handler is being created to
     * avoid creating it twice.
     */
    public static void setWillOverrideUiThread(boolean willOverrideUiThread) {
        BrowserThreadUtilsImpl.setWillOverrideUiThread(willOverrideUiThread);
    }

    /**
     * Sets a specific Looper as the UI thread.
     *
     * @param The looper to be used as the UI thread.
     */
    public static void setUiThread(Looper looper) {
        BrowserThreadUtilsImpl.setUiThread(looper);
    }

    /**
     * @return The Handler for the UI thread.
     */
    public static Handler getUiThreadHandler() {
        return BrowserThreadUtilsImpl.getUiThreadHandler();
    }

    /**
     * @return The Looper for the UI thread.
     */
    public static Looper getUiThreadLooper() {
        return BrowserThreadUtilsImpl.getUiThreadLooper();
    }

    /**
     * Run the supplied FutureTask on the main thread. The task will run immediately if the current
     * thread is the main thread.
     *
     * @param task The FutureTask to run
     * @return The queried task (to aid inline construction)
     */
    public static <T> FutureTask<T> runOnUiThread(FutureTask<T> task) {
        return BrowserThreadUtilsImpl.runOnUiThread(task);
    }

    /**
     * Run the supplied Callable on the main thread. The task will run immediately if the current
     * thread is the main thread.
     *
     * @param c The Callable to run
     * @return A FutureTask wrapping the callable to retrieve results
     */
    public static <T> FutureTask<T> runOnUiThread(Callable<T> c) {
        return BrowserThreadUtilsImpl.runOnUiThread(c);
    }

    /**
     * Throw an exception (when DCHECKs are enabled) if currently not running on the UI thread.
     *
     * Can be disabled by setThreadAssertsDisabledForTesting(true).
     */
    public static void assertOnUiThread() {
        BrowserThreadUtilsImpl.assertOnUiThread();
    }

    /**
     * Throw an exception (regardless of build) if currently not running on the UI thread.
     *
     * Can be disabled by setThreadAssertsEnabledForTesting(false).
     *
     * @see #assertOnUiThread()
     */
    public static void checkUiThread() {
        BrowserThreadUtilsImpl.checkUiThread();
    }

    /**
     * Throw an exception (when DCHECKs are enabled) if currently running on the UI thread.
     *
     * Can be disabled by setThreadAssertsDisabledForTesting(true).
     */
    public static void assertOnBackgroundThread() {
        BrowserThreadUtilsImpl.assertOnBackgroundThread();
    }

    /**
     * @return true iff the current thread is the main (UI) thread.
     */
    public static boolean runningOnUiThread() {
        return BrowserThreadUtilsImpl.runningOnUiThread();
    }
}
