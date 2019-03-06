// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.os.Handler;
import android.os.Looper;

import org.chromium.base.task.PostTask;
import org.chromium.content_public.browser.UiThreadTaskTraits;

import java.util.concurrent.Callable;
import java.util.concurrent.FutureTask;

import javax.annotation.concurrent.GuardedBy;

/**
 * Helper methods to deal with UI thread related tasks.
 */
public class BrowserThreadUtilsImpl {
    private BrowserThreadUtilsImpl() {}

    private static final Object sLock = new Object();

    @GuardedBy("sLock")
    private static boolean sWillOverride;

    @GuardedBy("sLock")
    private static Handler sUiThreadHandler;

    private static boolean sThreadAssertsDisabled;

    public static void setWillOverrideUiThread(boolean willOverrideUiThread) {
        synchronized (sLock) {
            sWillOverride = willOverrideUiThread;
        }
    }

    public static void setUiThread(Looper looper) {
        synchronized (sLock) {
            if (looper == null) {
                // Used to reset the looper after tests.
                sUiThreadHandler = null;
                return;
            }
            if (sUiThreadHandler != null && sUiThreadHandler.getLooper() != looper) {
                throw new RuntimeException("UI thread looper is already set to "
                        + sUiThreadHandler.getLooper() + " (Main thread looper is "
                        + Looper.getMainLooper() + "), cannot set to new looper " + looper);
            } else {
                sUiThreadHandler = new Handler(looper);
            }
        }
    }

    public static Handler getUiThreadHandler() {
        synchronized (sLock) {
            if (sUiThreadHandler == null) {
                if (sWillOverride) {
                    throw new RuntimeException("Did not yet override the UI thread");
                }
                sUiThreadHandler = new Handler(Looper.getMainLooper());
            }
            return sUiThreadHandler;
        }
    }

    public static Looper getUiThreadLooper() {
        return getUiThreadHandler().getLooper();
    }

    /**
     * Run the supplied FutureTask on the main thread. The task will run immediately if the current
     * thread is the main thread.
     *
     * @param task The FutureTask to run
     * @return The queried task (to aid inline construction)
     */
    public static <T> FutureTask<T> runOnUiThread(FutureTask<T> task) {
        PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT, task);
        return task;
    }

    /**
     * Run the supplied Callable on the main thread. The task will run immediately if the current
     * thread is the main thread.
     *
     * @param c The Callable to run
     * @return A FutureTask wrapping the callable to retrieve results
     */
    public static <T> FutureTask<T> runOnUiThread(Callable<T> c) {
        return runOnUiThread(new FutureTask<T>(c));
    }

    /**
     * Throw an exception (when DCHECKs are enabled) if currently not running on the UI thread.
     *
     * Can be disabled by setThreadAssertsDisabledForTesting(true).
     */
    public static void assertOnUiThread() {
        if (sThreadAssertsDisabled) return;

        assert runningOnUiThread() : "Must be called on the UI thread.";
    }

    /**
     * Throw an exception (regardless of build) if currently not running on the UI thread.
     *
     * Can be disabled by setThreadAssertsEnabledForTesting(false).
     *
     * @see #assertOnUiThread()
     */
    public static void checkUiThread() {
        if (!sThreadAssertsDisabled && !runningOnUiThread()) {
            throw new IllegalStateException("Must be called on the UI thread.");
        }
    }

    /**
     * Throw an exception (when DCHECKs are enabled) if currently running on the UI thread.
     *
     * Can be disabled by setThreadAssertsDisabledForTesting(true).
     */
    public static void assertOnBackgroundThread() {
        if (sThreadAssertsDisabled) return;

        assert !runningOnUiThread() : "Must be called on a thread other than UI.";
    }

    /**
     * Disables thread asserts.
     *
     * Can be used by tests where code that normally runs multi-threaded is going to run
     * single-threaded for the test (otherwise asserts that are valid in production would fail in
     * those tests).
     */
    public static void setThreadAssertsDisabledForTesting(boolean disabled) {
        sThreadAssertsDisabled = disabled;
    }

    /**
     * @return true iff the current thread is the main (UI) thread.
     */
    public static boolean runningOnUiThread() {
        return getUiThreadHandler().getLooper() == Looper.myLooper();
    }
}
