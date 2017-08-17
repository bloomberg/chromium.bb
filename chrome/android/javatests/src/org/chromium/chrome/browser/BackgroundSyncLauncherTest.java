// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.support.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;

import java.util.concurrent.ExecutionException;
import java.util.concurrent.Semaphore;

/**
 * Tests {@link BackgroundSyncLauncher}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
public class BackgroundSyncLauncherTest {
    private BackgroundSyncLauncher mLauncher;
    private Boolean mShouldLaunchResult;

    @Before
    public void setUp() throws Exception {
        BackgroundSyncLauncher.setGCMEnabled(false);
        RecordHistogram.setDisabledForTests(true);
        mLauncher = BackgroundSyncLauncher.create();
        // Ensure that the initial task is given enough time to complete.
        waitForLaunchBrowserTask();
    }

    @After
    public void tearDown() throws Exception {
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
                shouldLaunch -> {
                    mShouldLaunchResult = shouldLaunch;
                    semaphore.release();
                };

        BackgroundSyncLauncher.shouldLaunchBrowserIfStopped(callback);
        try {
            // Wait on the callback to be called.
            semaphore.acquire();
        } catch (InterruptedException e) {
            Assert.fail("Failed to acquire semaphore");
        }
        return mShouldLaunchResult;
    }

    private void waitForLaunchBrowserTask() {
        try {
            mLauncher.mLaunchBrowserIfStoppedTask.get();
        } catch (InterruptedException e) {
            Assert.fail("Launch task was interrupted");
        } catch (ExecutionException e) {
            Assert.fail("Launch task had execution exception");
        }
    }

    @Test
    @SmallTest
    @Feature({"BackgroundSync"})
    @RetryOnFailure
    public void testHasInstance() {
        Assert.assertTrue(BackgroundSyncLauncher.hasInstance());
        mLauncher.destroy();
        Assert.assertFalse(BackgroundSyncLauncher.hasInstance());
    }

    @Test
    @SmallTest
    @Feature({"BackgroundSync"})
    public void testDefaultNoLaunch() {
        Assert.assertFalse(shouldLaunchBrowserIfStoppedSync());
    }

    @Test
    @SmallTest
    @Feature({"BackgroundSync"})
    @RetryOnFailure
    public void testSetLaunchWhenNextOnline() {
        Assert.assertFalse(shouldLaunchBrowserIfStoppedSync());
        mLauncher.launchBrowserIfStopped(true, 0);
        waitForLaunchBrowserTask();
        Assert.assertTrue(shouldLaunchBrowserIfStoppedSync());
        mLauncher.launchBrowserIfStopped(false, 0);
        waitForLaunchBrowserTask();
        Assert.assertFalse(shouldLaunchBrowserIfStoppedSync());
    }

    @Test
    @SmallTest
    @Feature({"BackgroundSync"})
    @RetryOnFailure
    public void testNewLauncherDisablesNextOnline() {
        mLauncher.launchBrowserIfStopped(true, 0);
        waitForLaunchBrowserTask();
        Assert.assertTrue(shouldLaunchBrowserIfStoppedSync());

        // Simulate restarting the browser by deleting the launcher and creating a new one.
        deleteLauncherInstance();
        mLauncher = BackgroundSyncLauncher.create();
        waitForLaunchBrowserTask();
        Assert.assertFalse(shouldLaunchBrowserIfStoppedSync());
    }
}
