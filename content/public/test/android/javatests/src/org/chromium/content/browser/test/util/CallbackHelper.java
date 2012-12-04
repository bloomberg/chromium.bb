// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.test.util;

import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

/**
 * A helper class for listening to callbacks.
 */
public class CallbackHelper {
    protected static int WAIT_TIMEOUT_SECONDS = 5;

    private final Object mLock = new Object();
    protected int mCallCount = 0;

    /**
     * Gets the number of times the callback has been called.
     *
     * The call count can be used with the waitForCallback() method, indicating a point
     * in time after which the caller wishes to record calls to the callback.
     *
     * In order to wait for a callback caused by X, the call count should be obtained
     * before X occurs.
     *
     * NOTE: any call to the callback that occurs after the call count is obtained
     * will result in the corresponding wait call to resume execution. The call count
     * is intended to 'catch' callbacks that occur after X but before waitForCallback()
     * is called.
     */
    public int getCallCount() {
        synchronized(mLock) {
            return mCallCount;
        }
    }

    /**
     * Blocks until the callback is called the specified number of
     * times or throws an exception if we exceeded the specified time frame.
     *
     * This will wait for a callback to be called a specified number of times after
     * the point in time at which the call count was obtained.  The method will return
     * immediately if a call occurred the specified number of times after the
     * call count was obtained but before the method was called, otherwise the method will
     * block until the specified call count is reached.
     *
     * @param currentCallCount the value obtained by calling getCallCount().
     * @param numberOfCallsToWaitFor number of calls (counting since
     *                               currentCallCount was obtained) that we will wait for.
     * @param timeout timeout value. We will wait the specified amount of time for a single
     *                callback to occur so the method call may block up to
     *                <code>numberOfCallsToWaitFor * timeout</code> units.
     * @param unit timeout unit.
     * @throws InterruptedException
     * @throws TimeoutException Thrown if the method times out before onPageFinished is called.
     */
    public void waitForCallback(int currentCallCount, int numberOfCallsToWaitFor, long timeout,
            TimeUnit unit) throws InterruptedException, TimeoutException {
        assert mCallCount >= currentCallCount;
        assert numberOfCallsToWaitFor > 0;
        synchronized(mLock) {
            int callCountWhenDoneWaiting = currentCallCount + numberOfCallsToWaitFor;
            while (callCountWhenDoneWaiting > mCallCount) {
                int callCountBeforeWait = mCallCount;
                mLock.wait(unit.toMillis(timeout));
                if (callCountBeforeWait == mCallCount) {
                    throw new TimeoutException("waitForCallback timed out!");
                }
            }
        }
    }

    public void waitForCallback(int currentCallCount, int numberOfCallsToWaitFor)
            throws InterruptedException, TimeoutException {
        waitForCallback(currentCallCount, numberOfCallsToWaitFor,
                WAIT_TIMEOUT_SECONDS, TimeUnit.SECONDS);
    }

    public void waitForCallback(int currentCallCount)
            throws InterruptedException, TimeoutException {
        waitForCallback(currentCallCount, 1);
    }

    /**
     * Blocks until the criteria is satisfied or throws an exception
     * if the specified time frame is exceeded.
     * @param timeout timeout value.
     * @param unit timeout unit.
     */
    public void waitUntilCriteria(Criteria criteria, long timeout, TimeUnit unit)
            throws InterruptedException, TimeoutException {
        synchronized(mLock) {
            final long startTime = System.currentTimeMillis();
            boolean isSatisfied = criteria.isSatisfied();
            while (!isSatisfied &&
                    System.currentTimeMillis() - startTime < unit.toMillis(timeout)) {
                mLock.wait(unit.toMillis(timeout));
                isSatisfied = criteria.isSatisfied();
            }
            if (!isSatisfied) throw new TimeoutException("waitUntilCriteria timed out!");
        }
    }

    public void waitUntilCriteria(Criteria criteria)
            throws InterruptedException, TimeoutException {
        waitUntilCriteria(criteria, WAIT_TIMEOUT_SECONDS, TimeUnit.SECONDS);
    }

    /**
     * Should be called when the callback associated with this helper object is called.
     */
    public void notifyCalled() {
        synchronized(mLock) {
            mCallCount++;
            mLock.notifyAll();
        }
    }
}
