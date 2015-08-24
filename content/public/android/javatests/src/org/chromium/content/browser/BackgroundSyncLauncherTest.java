// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.content.Context;
import android.content.Intent;
import android.net.ConnectivityManager;
import android.test.InstrumentationTestCase;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.test.util.AdvancedMockContext;
import org.chromium.base.test.util.Feature;

import java.util.concurrent.Semaphore;

/**
 * Tests {@link BackgroundSyncLauncherService} and {@link BackgroundSyncLauncherService.Receiver}.
 */
public class BackgroundSyncLauncherTest extends InstrumentationTestCase {
    private Context mContext;
    private BackgroundSyncLauncher mLauncher;
    private MockReceiver mLauncherServiceReceiver;
    private Boolean mShouldLaunchResult;

    static class MockReceiver extends BackgroundSyncLauncherService.Receiver {
        private boolean mIsOnline = true;
        private boolean mDidStartService;

        public void setOnline(boolean online) {
            mIsOnline = online;
        }

        @Override
        protected boolean isOnline(Context context) {
            return mIsOnline;
        }

        @Override
        protected void startService(Context context) {
            startServiceImpl();
        }

        private void startServiceImpl() {
            mDidStartService = true;
        }

        protected void checkExpectations(boolean expectedStartService) {
            assertEquals("StartedService", expectedStartService, mDidStartService);
        }
    }

    @Override
    protected void setUp() throws Exception {
        mContext = new AdvancedMockContext(getInstrumentation().getTargetContext());
        mLauncher = BackgroundSyncLauncher.create(mContext);
        mLauncherServiceReceiver = new MockReceiver();
    }

    private void deleteLauncherInstance() {
        mLauncher.destroy();
        mLauncher = null;
    }

    private void startOnReceiveAndVerify(boolean shouldStart) {
        mLauncherServiceReceiver.onReceive(
                mContext, new Intent(ConnectivityManager.CONNECTIVITY_ACTION));
        mLauncherServiceReceiver.checkExpectations(shouldStart);
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
        mLauncher.setLaunchWhenNextOnline(mContext, true);
        assertTrue(shouldLaunchWhenNextOnlineSync());
        mLauncher.setLaunchWhenNextOnline(mContext, false);
        assertFalse(shouldLaunchWhenNextOnlineSync());
    }

    @SmallTest
    @Feature({"BackgroundSync"})
    public void testNewLauncherDisablesNextOnline() {
        mLauncher.setLaunchWhenNextOnline(mContext, true);
        assertTrue(shouldLaunchWhenNextOnlineSync());

        // Simulate restarting the browser by deleting the launcher and creating a new one.
        deleteLauncherInstance();
        mLauncher = BackgroundSyncLauncher.create(mContext);
        assertFalse(shouldLaunchWhenNextOnlineSync());
    }

    @SmallTest
    @Feature({"BackgroundSync"})
    public void testNoFireWhenInstanceExists() {
        mLauncher.setLaunchWhenNextOnline(mContext, true);
        mLauncherServiceReceiver.setOnline(true);
        startOnReceiveAndVerify(false);

        deleteLauncherInstance();
        startOnReceiveAndVerify(true);
    }

    @SmallTest
    @Feature({"BackgroundSync"})
    public void testReceiverOffline() {
        mLauncher.setLaunchWhenNextOnline(mContext, true);
        mLauncherServiceReceiver.setOnline(false);
        deleteLauncherInstance();
        startOnReceiveAndVerify(false);
    }

    @SmallTest
    @Feature({"BackgroundSync"})
    public void testReceiverOnline() {
        mLauncher.setLaunchWhenNextOnline(mContext, true);
        mLauncherServiceReceiver.setOnline(true);
        deleteLauncherInstance();
        startOnReceiveAndVerify(true);
    }

    @SmallTest
    @Feature({"BackgroundSync"})
    public void testStartingService() {
        Intent serviceIntent = new Intent(mContext, BackgroundSyncLauncherService.class);
        MockReceiver.startWakefulService(mContext, serviceIntent);
    }
}
