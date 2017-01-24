// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.content.Context;
import android.support.test.filters.SmallTest;
import android.test.InstrumentationTestCase;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.AdvancedMockContext;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RetryOnFailure;

import java.util.concurrent.ExecutionException;
import java.util.concurrent.Semaphore;

/**
 * Tests {@link BackgroundSyncLauncher}.
 */
public class BackgroundSyncLauncherTest extends InstrumentationTestCase {
    private Context mContext;
    private BackgroundSyncLauncher mLauncher;
    private Boolean mShouldLaunchResult;

    @Override
    protected void setUp() throws Exception {
        mContext = new AdvancedMockContext(getInstrumentation().getTargetContext());
        BackgroundSyncLauncher.setGCMEnabled(false);
        RecordHistogram.setDisabledForTests(true);
        mLauncher = BackgroundSyncLauncher.create(mContext);
        // Ensure that the initial task is given enough time to complete.
        waitForLaunchBrowserTask();
    }

    @Override
    public void tearDown() throws Exception {
        super.tearDown();
        RecordHistogram.setDisabledForTests(false);
    }

    private void deleteLauncherInstance() {
        mLauncher.destroy();
        mLauncher = null;
    }

    private Boolean shouldLaunchBrowserIfStoppedSync() {
        mShouldLaunchResult = false;

        // Use a semaphore to wait for the callback to be called.
        final Semaphore semaphore = new Semaphore(0);

        BackgroundSyncLauncher.ShouldLaunchCallback callback =
                new BackgroundSyncLauncher.ShouldLaunchCallback() {
                    @Override
                    public void run(Boolean shouldLaunch) {
                        mShouldLaunchResult = shouldLaunch;
                        semaphore.release();
                    }
                };

        BackgroundSyncLauncher.shouldLaunchBrowserIfStopped(callback);
        try {
            // Wait on the callback to be called.
            semaphore.acquire();
        } catch (InterruptedException e) {
            fail("Failed to acquire semaphore");
        }
        return mShouldLaunchResult;
    }

    private void waitForLaunchBrowserTask() {
        try {
            mLauncher.mLaunchBrowserIfStoppedTask.get();
        } catch (InterruptedException e) {
            fail("Launch task was interrupted");
        } catch (ExecutionException e) {
            fail("Launch task had execution exception");
        }
    }

    @SmallTest
    @Feature({"BackgroundSync"})
    @RetryOnFailure
    public void testHasInstance() {
        assertTrue(BackgroundSyncLauncher.hasInstance());
        mLauncher.destroy();
        assertFalse(BackgroundSyncLauncher.hasInstance());
    }

    @SmallTest
    @Feature({"BackgroundSync"})
    public void testDefaultNoLaunch() {
        assertFalse(shouldLaunchBrowserIfStoppedSync());
    }

    @SmallTest
    @Feature({"BackgroundSync"})
    @RetryOnFailure
    public void testSetLaunchWhenNextOnline() {
        assertFalse(shouldLaunchBrowserIfStoppedSync());
        mLauncher.launchBrowserIfStopped(true, 0);
        waitForLaunchBrowserTask();
        assertTrue(shouldLaunchBrowserIfStoppedSync());
        mLauncher.launchBrowserIfStopped(false, 0);
        waitForLaunchBrowserTask();
        assertFalse(shouldLaunchBrowserIfStoppedSync());
    }

    @SmallTest
    @Feature({"BackgroundSync"})
    @RetryOnFailure
    public void testNewLauncherDisablesNextOnline() {
        mLauncher.launchBrowserIfStopped(true, 0);
        waitForLaunchBrowserTask();
        assertTrue(shouldLaunchBrowserIfStoppedSync());

        // Simulate restarting the browser by deleting the launcher and creating a new one.
        deleteLauncherInstance();
        mLauncher = BackgroundSyncLauncher.create(mContext);
        waitForLaunchBrowserTask();
        assertFalse(shouldLaunchBrowserIfStoppedSync());
    }
}
