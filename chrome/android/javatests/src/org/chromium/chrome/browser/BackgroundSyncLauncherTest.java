// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.content.Context;
import android.test.InstrumentationTestCase;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.test.util.AdvancedMockContext;
import org.chromium.base.test.util.Feature;

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
        mLauncher = BackgroundSyncLauncher.create(mContext);
    }

    @Override
    protected void tearDown() throws Exception {
        BackgroundSyncLauncher.setGCMEnabled(true);
        super.tearDown();
    }

    private void deleteLauncherInstance() {
        mLauncher.destroy();
        mLauncher = null;
    }

    private Boolean shouldLaunchWhenNextOnlineSync() {
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

        BackgroundSyncLauncher.shouldLaunchWhenNextOnline(mContext, callback);
        try {
            // Wait on the callback to be called.
            semaphore.acquire();
        } catch (InterruptedException e) {
            fail("Failed to acquire semaphore");
        }
        return mShouldLaunchResult;
    }

    @SmallTest
    @Feature({"BackgroundSync"})
    public void testHasInstance() {
        assertTrue(BackgroundSyncLauncher.hasInstance());
        mLauncher.destroy();
        assertFalse(BackgroundSyncLauncher.hasInstance());
    }

    @SmallTest
    @Feature({"BackgroundSync"})
    public void testDefaultNoLaunch() {
        assertFalse(shouldLaunchWhenNextOnlineSync());
    }

    @SmallTest
    @Feature({"BackgroundSync"})
    public void testSetLaunchWhenNextOnline() {
        assertFalse(shouldLaunchWhenNextOnlineSync());
        mLauncher.launchBrowserWhenNextOnlineIfStopped(mContext, true);
        assertTrue(shouldLaunchWhenNextOnlineSync());
        mLauncher.launchBrowserWhenNextOnlineIfStopped(mContext, false);
        assertFalse(shouldLaunchWhenNextOnlineSync());
    }

    @SmallTest
    @Feature({"BackgroundSync"})
    public void testNewLauncherDisablesNextOnline() {
        mLauncher.launchBrowserWhenNextOnlineIfStopped(mContext, true);
        assertTrue(shouldLaunchWhenNextOnlineSync());

        // Simulate restarting the browser by deleting the launcher and creating a new one.
        deleteLauncherInstance();
        mLauncher = BackgroundSyncLauncher.create(mContext);
        assertFalse(shouldLaunchWhenNextOnlineSync());
    }
}
