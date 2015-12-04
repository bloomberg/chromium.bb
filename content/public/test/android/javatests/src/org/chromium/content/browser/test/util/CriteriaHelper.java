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
 * public void waitForTabTitle(final Tab tab, final String title) {
 *     CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
 *         {@literal @}Override
 *         public boolean isSatisified() {
 *             updateFailureReason("Tab title did not match -- expected: " + title
 *                     + ", actual: " + tab.getTitle());
 *             return TextUtils.equals(tab.getTitle(), title);
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
     * @param criteria The Criteria that will be checked.
     * @param maxTimeoutMs The maximum number of ms that this check will be performed for
     * before timeout.
     * @param checkIntervalMs The number of ms between checks.
     * @throws InterruptedException
     */
    public static void pollForCriteria(Criteria criteria, long maxTimeoutMs,
            long checkIntervalMs) throws InterruptedException {
        boolean isSatisfied = criteria.isSatisfied();
        long startTime = SystemClock.uptimeMillis();
        while (!isSatisfied && SystemClock.uptimeMillis() - startTime < maxTimeoutMs) {
            Thread.sleep(checkIntervalMs);
            isSatisfied = criteria.isSatisfied();
        }
        Assert.assertTrue(criteria.getFailureReason(), isSatisfied);
    }

    /**
     * Checks whether the given Criteria is satisfied polling at a default interval.
     *
     * @param criteria The Criteria that will be checked.
     * @throws InterruptedException
     * @see #pollForCriteria(Criteria, long, long)
     */
    public static void pollForCriteria(Criteria criteria) throws InterruptedException {
        pollForCriteria(criteria, DEFAULT_MAX_TIME_TO_POLL, DEFAULT_POLLING_INTERVAL);
    }

    /**
     * Checks whether the given Criteria is satisfied polling at a given interval on the UI
     * thread, until either the criteria is satisfied, or the maxTimeoutMs number of ms has elapsed.
     *
     * @param criteria The Criteria that will be checked.
     * @param maxTimeoutMs The maximum number of ms that this check will be performed for
     * before timeout.
     * @param checkIntervalMs The number of ms between checks.
     * @throws InterruptedException
     * @see #pollForCriteria(Criteria)
     */
    public static void pollForUIThreadCriteria(final Criteria criteria, long maxTimeoutMs,
            long checkIntervalMs) throws InterruptedException {
        final Callable<Boolean> callable = new Callable<Boolean>() {
            @Override
            public Boolean call() throws Exception {
                return criteria.isSatisfied();
            }
        };

        pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return ThreadUtils.runOnUiThreadBlockingNoException(callable);
            }
        }, maxTimeoutMs, checkIntervalMs);
    }

    /**
     * Checks whether the given Criteria is satisfied polling at a default interval on the UI
     * thread.
     * @param criteria The Criteria that will be checked.
     * @throws InterruptedException
     * @see #pollForCriteria(Criteria)
     */
    public static void pollForUIThreadCriteria(final Criteria criteria)
            throws InterruptedException {
        pollForUIThreadCriteria(criteria, DEFAULT_MAX_TIME_TO_POLL, DEFAULT_POLLING_INTERVAL);
    }
}
