// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.test.util;

import static org.chromium.base.test.util.ScalableTimeout.scaleTimeout;

import android.os.SystemClock;

import junit.framework.Assert;

import org.chromium.base.ThreadUtils;

import java.util.concurrent.Callable;

/**
 * Helper methods for creating and managing criteria.
 *
 * <p>
 * If possible, use callbacks or testing delegates instead of criteria as they
 * do not introduce any polling delays.  Should only use Criteria if no suitable
 * other approach exists.
 *
 * <p>
 * <pre>
 * Sample Usage:
 * <code>
 * public void waitForTabFullyLoaded(final Tab tab) {
 *     CriteriaHelper.pollUiThread(new Criteria() {
 *         {@literal @}Override
 *         public boolean isSatisfied() {
 *             if (tab.getWebContents() == null) {
 *                 updateFailureReason("Tab has no web contents");
 *                 return false;
 *             }
 *             updateFailureReason("Tab not fully loaded");
 *             return tab.isLoading();
 *         }
 *     });
 * }
 * </code>
 * </pre>
 */
public class CriteriaHelper {

    /** The default maximum time to wait for a criteria to become valid. */
    public static final long DEFAULT_MAX_TIME_TO_POLL = scaleTimeout(3000);
    /** The default polling interval to wait between checking for a satisfied criteria. */
    public static final long DEFAULT_POLLING_INTERVAL = 50;

    /**
     * Checks whether the given Criteria is satisfied at a given interval, until either
     * the criteria is satisfied, or the specified maxTimeoutMs number of ms has elapsed.
     *
     * <p>
     * This evaluates the Criteria on the Instrumentation thread, which more often than not is not
     * correct in an InstrumentationTest. Use
     * {@link #pollUiThread(Criteria, long, long)} instead.
     *
     * @param criteria The Criteria that will be checked.
     * @param maxTimeoutMs The maximum number of ms that this check will be performed for
     *                     before timeout.
     * @param checkIntervalMs The number of ms between checks.
     */
    public static void pollInstrumentationThread(Criteria criteria, long maxTimeoutMs,
            long checkIntervalMs) {
        boolean isSatisfied = criteria.isSatisfied();
        long startTime = SystemClock.uptimeMillis();
        while (!isSatisfied && SystemClock.uptimeMillis() - startTime < maxTimeoutMs) {
            try {
                Thread.sleep(checkIntervalMs);
            } catch (InterruptedException e) {
                // Catch the InterruptedException. If the exception occurs before maxTimeoutMs
                // and the criteria is not satisfied, the while loop will run again.
            }
            isSatisfied = criteria.isSatisfied();
        }
        Assert.assertTrue(criteria.getFailureReason(), isSatisfied);
    }

    /**
     * Checks whether the given Criteria is satisfied polling at a default interval.
     *
     * <p>
     * This evaluates the Criteria on the test thread, which more often than not is not correct
     * in an InstrumentationTest.  Use {@link #pollUiThread(Criteria)} instead.
     *
     * @param criteria The Criteria that will be checked.
     *
     * @see #pollInstrumentationThread(Criteria, long, long)
     */
    public static void pollInstrumentationThread(Criteria criteria) {
        pollInstrumentationThread(criteria, DEFAULT_MAX_TIME_TO_POLL, DEFAULT_POLLING_INTERVAL);
    }

    /**
     * Checks whether the given Criteria is satisfied polling at a given interval on the UI
     * thread, until either the criteria is satisfied, or the maxTimeoutMs number of ms has elapsed.
     *
     * @param criteria The Criteria that will be checked.
     * @param maxTimeoutMs The maximum number of ms that this check will be performed for
     *                     before timeout.
     * @param checkIntervalMs The number of ms between checks.
     *
     * @see #pollInstrumentationThread(Criteria)
     */
    public static void pollUiThread(final Criteria criteria, long maxTimeoutMs,
            long checkIntervalMs) {
        final Callable<Boolean> callable = new Callable<Boolean>() {
            @Override
            public Boolean call() throws Exception {
                return criteria.isSatisfied();
            }
        };

        pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return ThreadUtils.runOnUiThreadBlockingNoException(callable);
            }

            @Override
            public String getFailureReason() {
                return criteria.getFailureReason();
            }
        }, maxTimeoutMs, checkIntervalMs);
    }

    /**
     * Checks whether the given Criteria is satisfied polling at a default interval on the UI
     * thread.
     * @param criteria The Criteria that will be checked.
     *
     * @see #pollInstrumentationThread(Criteria)
     */
    public static void pollUiThread(final Criteria criteria) {
        pollUiThread(criteria, DEFAULT_MAX_TIME_TO_POLL, DEFAULT_POLLING_INTERVAL);
    }
}
