// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

import java.util.concurrent.ExecutionException;
import java.util.concurrent.Executors;
import java.util.concurrent.Future;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Unit tests for the ChromeBackgroundServiceWaiter.
 */
@RunWith(JUnit4.class)
public class ChromeBackgroundServiceWaiterTest {
    private static final int TIMEOUT_SECONDS = 30;

    private AtomicBoolean mFinishedWaiting = new AtomicBoolean();
    private AtomicBoolean mWaitFailed = new AtomicBoolean();

    private boolean getFinishedWaiting() {
        return mFinishedWaiting.get();
    }

    private void setFinishedWaiting(boolean newValue) {
        mFinishedWaiting.getAndSet(newValue);
    }

    private boolean getWaitFailed() {
        return mWaitFailed.get();
    }

    @Test
    public void testWaiter() {
        final ChromeBackgroundServiceWaiter waiter =
                new ChromeBackgroundServiceWaiter(TIMEOUT_SECONDS);

        Future<?> future = Executors.newSingleThreadExecutor().submit(new Runnable() {
            @Override
            public void run() {
                // The finished waiting flag should not be set, if it is, the wait operation failed.
                if (getFinishedWaiting()) {
                    mWaitFailed.getAndSet(true);
                }
                waiter.onWaitDone();
            }
        });

        // Wait for the thread, and set the flag after we are done waiting.
        waiter.startWaiting();
        setFinishedWaiting(true);

        // Wait for the "onWaitDone" thread to complete.
        try {
            future.get();
        } catch (InterruptedException | ExecutionException e) {
            // Fail the test if we get an interrupted exception.
            fail("InterruptedException or ExecutionException " + e);
        }

        // Verify the thread unblocked, and the flag got set.
        assertTrue(getFinishedWaiting());
        assertFalse(getWaitFailed());
    }

}
