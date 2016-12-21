// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.content.Context;
import android.content.Intent;
import android.support.test.filters.MediumTest;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.FlakyTest;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.test.ChromeTabbedActivityTestBase;
import org.chromium.chrome.test.util.ApplicationTestUtils;

import java.util.concurrent.Callable;

/**
 * Tests for the PowerBroadcastReceiver.
 */
@RetryOnFailure
public class PowerBroadcastReceiverTest extends ChromeTabbedActivityTestBase {
    private static final long MS_INTERVAL = 1000;
    private static final long MS_RUNNABLE_DELAY = 2500;
    private static final long MS_TIMEOUT = 5000;

    private static final MockPowerManagerHelper sScreenOff = new MockPowerManagerHelper(false);
    private static final MockPowerManagerHelper sScreenOn = new MockPowerManagerHelper(true);

    /** Mocks out PowerBroadcastReceiver.ServiceRunnable. */
    private static class MockServiceRunnable extends PowerBroadcastReceiver.ServiceRunnable {
        public CallbackHelper postHelper = new CallbackHelper();
        public CallbackHelper cancelHelper = new CallbackHelper();
        public CallbackHelper runHelper = new CallbackHelper();
        public CallbackHelper runActionsHelper = new CallbackHelper();

        @Override
        public void setState(int state) {
            super.setState(state);
            if (state == STATE_POSTED) {
                postHelper.notifyCalled();
            } else if (state == STATE_CANCELED) {
                cancelHelper.notifyCalled();
            } else if (state == STATE_COMPLETED) {
                runHelper.notifyCalled();
            }
        }

        @Override
        public long getDelayToRun() {
            return MS_RUNNABLE_DELAY;
        }

        @Override
        public void runActions() {
            // Don't let the real services start.
            runActionsHelper.notifyCalled();
        }
    }

    /** Mocks out PowerBroadcastReceiver.PowerManagerHelper. */
    private static class MockPowerManagerHelper extends PowerBroadcastReceiver.PowerManagerHelper {
        private final boolean mScreenIsOn;

        public MockPowerManagerHelper(boolean screenIsOn) {
            mScreenIsOn = screenIsOn;
        }

        @Override
        public boolean isScreenOn(Context context) {
            return mScreenIsOn;
        }
    }

    private MockServiceRunnable mRunnable;
    private PowerBroadcastReceiver mReceiver;

    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityFromLauncher();
    }

    @Override
    public void setUp() throws Exception {
        super.setUp();

        mReceiver = ThreadUtils.runOnUiThreadBlocking(new Callable<PowerBroadcastReceiver>() {
            @Override
            public PowerBroadcastReceiver call() throws Exception {
                return ChromeActivitySessionTracker.getInstance()
                        .getPowerBroadcastReceiverForTesting();
            }
        });

        // Set up our mock runnable.
        mRunnable = new MockServiceRunnable();
        mReceiver.setServiceRunnableForTests(mRunnable);

        // Initially claim that the screen is on.
        mReceiver.setPowerManagerHelperForTests(sScreenOn);
    }

    /**
     * Check if the runnable is posted and run while the screen is on.
     */
    @MediumTest
    @Feature({"Omaha", "Sync"})
    public void testRunnableRunsWithScreenOn() throws Exception {
        // Pause & resume to post the runnable.
        ApplicationTestUtils.fireHomeScreenIntent(getInstrumentation().getTargetContext());
        int postCount = mRunnable.postHelper.getCallCount();
        int runCount = mRunnable.runHelper.getCallCount();
        ApplicationTestUtils.launchChrome(getInstrumentation().getTargetContext());

        // Relaunching Chrome should cause the runnable to trigger.
        mRunnable.postHelper.waitForCallback(postCount, 1);
        mRunnable.runHelper.waitForCallback(runCount, 1);
        assertEquals(0, mRunnable.cancelHelper.getCallCount());
        assertFalse("Still listening for power broadcasts.", mReceiver.isRegistered());
    }

    /**
     * Check that the runnable gets posted and canceled when Main is sent to the background.
     */
    @FlakyTest(message = "https://crbug.com/579363")
    @MediumTest
    @Feature({"Omaha", "Sync"})
    public void testRunnableGetsCanceled() throws Exception {
        // Pause & resume to post the runnable.
        ApplicationTestUtils.fireHomeScreenIntent(getInstrumentation().getTargetContext());
        int postCount = mRunnable.postHelper.getCallCount();
        ApplicationTestUtils.launchChrome(getInstrumentation().getTargetContext());
        mRunnable.postHelper.waitForCallback(postCount, 1);
        assertEquals(0, mRunnable.runHelper.getCallCount());
        assertEquals(0, mRunnable.cancelHelper.getCallCount());

        // Pause before the runnable has a chance to run.  Should cause the runnable to be canceled.
        int cancelCount = mRunnable.cancelHelper.getCallCount();
        ApplicationTestUtils.fireHomeScreenIntent(getInstrumentation().getTargetContext());
        mRunnable.cancelHelper.waitForCallback(cancelCount, 1);
        assertEquals(0, mRunnable.runHelper.getCallCount());
        assertFalse("Still listening for power broadcasts.", mReceiver.isRegistered());
    }

    /**
     * Check that the runnable gets run only while the screen is on.
     */
    @MediumTest
    @Feature({"Omaha", "Sync"})
    @FlakyTest(message = "https://crbug.com/587138")
    public void testRunnableGetsRunWhenScreenIsOn() throws Exception {
        // Claim the screen is off.
        mReceiver.setPowerManagerHelperForTests(sScreenOff);

        // Pause & resume.  Because the screen is off, nothing should happen.
        ApplicationTestUtils.fireHomeScreenIntent(getInstrumentation().getTargetContext());
        ApplicationTestUtils.launchChrome(getInstrumentation().getTargetContext());
        assertTrue("Isn't waiting for power broadcasts.", mReceiver.isRegistered());
        assertEquals(0, mRunnable.postHelper.getCallCount());
        assertEquals(0, mRunnable.runHelper.getCallCount());
        assertEquals(0, mRunnable.cancelHelper.getCallCount());

        // Pretend to turn the screen on.
        int postCount = mRunnable.postHelper.getCallCount();
        int runCount = mRunnable.runHelper.getCallCount();
        mReceiver.setPowerManagerHelperForTests(sScreenOn);
        Intent intent = new Intent(Intent.ACTION_SCREEN_ON);
        mReceiver.onReceive(getInstrumentation().getTargetContext(), intent);

        // The runnable should run now that the screen is on.
        mRunnable.postHelper.waitForCallback(postCount, 1);
        mRunnable.runHelper.waitForCallback(runCount, 1);
        assertEquals(0, mRunnable.cancelHelper.getCallCount());
        assertFalse("Still listening for power broadcasts.", mReceiver.isRegistered());
    }
}
