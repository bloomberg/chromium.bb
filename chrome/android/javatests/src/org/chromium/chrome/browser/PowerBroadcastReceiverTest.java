// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.content.Context;
import android.content.Intent;
import android.test.suitebuilder.annotation.MediumTest;

import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.preferences.Preferences;
import org.chromium.chrome.test.ChromeTabbedActivityTestBase;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.UiUtils;

/**
 * Tests for the PowerBroadcastReceiver.
 */
public class PowerBroadcastReceiverTest extends ChromeTabbedActivityTestBase {
    private static final long MS_INTERVAL = 1000;
    private static final long MS_RUNNABLE_DELAY = 2500;
    private static final long MS_TIMEOUT = 5000;

    private static final MockPowerManagerHelper sScreenOff = new MockPowerManagerHelper(false);
    private static final MockPowerManagerHelper sScreenOn = new MockPowerManagerHelper(true);

    /** Mocks out PowerBroadcastReceiver.ServiceRunnable. */
    private static class MockServiceRunnable extends PowerBroadcastReceiver.ServiceRunnable {
        private boolean mRan;
        private final long mDelay;

        MockServiceRunnable(long delay) {
            mDelay = delay;
        }

        @Override
        public long delayToRun() {
            return mDelay;
        }

        @Override
        public void run() {
            mRan = true;
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

    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityFromLauncher();
    }

    private PowerBroadcastReceiver getPowerBroadcastReceiver() {
        ChromeApplication app = (ChromeApplication) getActivity().getApplication();
        return app.getPowerBroadcastReceiver();
    }

    /**
     * Replaces the ServiceRunnable and PowerManagerHelper used by the PowerBroadcastReceiver
     * with mocked versions.
     * @return the MockServiceRunnable created by this function.
     */
    private MockServiceRunnable setUpMockRunnable() {
        MockServiceRunnable runnable = new MockServiceRunnable(MS_RUNNABLE_DELAY);
        getPowerBroadcastReceiver().setServiceRunnableForTests(runnable);
        return runnable;
    }

    /**
     * Pauses (and optionally resumes) Main by starting a different Activity.
     * @param resumeAfterPause Resume Main after the pause.
     */
    private void pauseMain(boolean resumeAfterPause) throws Exception {
        // Bring up the Preferences activity to put the Main activity in the background.
        UiUtils.settleDownUI(getInstrumentation());
        Preferences prefActivity = startPreferences(null);
        assertNotNull("Could not launch preferences activity.", prefActivity);

        if (resumeAfterPause) {
            // Finish the Activity to bring Main back.
            UiUtils.settleDownUI(getInstrumentation());
            prefActivity.finish();
        }
    }

    /**
     * Waits to see if the runnable was run.
     */
    public boolean runnableRan(final MockServiceRunnable runnable) throws Exception {
        return CriteriaHelper.pollForCriteria(
                new Criteria() {
                    @Override
                    public boolean isSatisfied() {
                        return runnable.mRan;
                    }
                },
                MS_TIMEOUT, MS_INTERVAL);
    }

    /**
     * Check if the runnable is posted and run while the screen is on.
     */
    /*
        @MediumTest
        @Feature({"Omaha", "Sync"})
        Bug http://crbug.com/156745
     */
    @DisabledTest
    public void testRunnableRunsWithScreenOn() throws Exception {
        // Claim the screen is on.
        PowerBroadcastReceiver receiver = getPowerBroadcastReceiver();
        receiver.setPowerManagerHelperForTests(sScreenOn);

        // Switch out the PowerBroadcastReceiver's ServiceRunnable.
        MockServiceRunnable runnable = setUpMockRunnable();

        // Pause & resume to post our mocked runnable.
        pauseMain(true);

        // Check to see that the runnable gets run.
        assertTrue("MockServiceRunnable didn't run after resuming Chrome.", runnableRan(runnable));
        assertFalse("PowerBroadcastReceiver was still registered.", receiver.isRegistered());
    }

    /**
     * Check that the runnable gets posted and canceled when Main is sent to the background.
     */
    @MediumTest
    @Feature({"Omaha", "Sync"})
    public void testRunnableGetsCanceled() throws Exception {
        // Claim the screen is on.
        PowerBroadcastReceiver receiver = getPowerBroadcastReceiver();
        receiver.setPowerManagerHelperForTests(sScreenOn);

        // Switch out the PowerBroadcastReceiver's Runnable with a MockServiceRunnable.
        MockServiceRunnable runnable = setUpMockRunnable();

        // Pause & resume to post our mocked runnable, then pause immediately after resuming so that
        // Main's handler cancels the runnable.
        pauseMain(true);
        pauseMain(false);

        // Wait enough time for the runnable to run(), if it's still in the handler.
        assertFalse("MockServiceRunnable ran after resuming Chrome.", runnableRan(runnable));
        assertFalse("PowerBroadcastReceiver was still registered.", receiver.isRegistered());
    }

    /**
     * Check that the runnable gets run only while the screen is on.
     */
    /*
        @MediumTest
        @Feature({"Omaha", "Sync"})
        Bug http://crbug.com/156745
     */
    @DisabledTest
    public void testRunnableGetsRunWhenScreenIsOn() throws Exception {
        ChromeTabbedActivity main = getActivity();
        PowerBroadcastReceiver receiver = getPowerBroadcastReceiver();

        // Claim the screen is off.
        receiver.setPowerManagerHelperForTests(sScreenOff);

        // Switch out the PowerBroadcastReceiver's Runnable with a MockServiceRunnable.
        MockServiceRunnable runnable = setUpMockRunnable();

        // Pause & resume to post our mocked runnable.
        pauseMain(true);

        // Wait enough time for the runnable to run(), if it's still in the handler.
        boolean ranWhileScreenOff = runnableRan(runnable);
        assertFalse("MockServiceRunnable ran even though screen was off.", ranWhileScreenOff);
        assertTrue("PowerBroadcastReceiver was not registered.", receiver.isRegistered());

        // Pretend to turn the screen on.
        receiver.setPowerManagerHelperForTests(sScreenOn);
        Intent intent = new Intent(Intent.ACTION_SCREEN_ON);
        receiver.onReceive(main, intent);

        // Wait enough time for the runnable to run(), if it's still in the handler.
        boolean ranWhileScreenOn = runnableRan(runnable);
        assertTrue("MockServiceRunnable didn't run when screen turned on.", ranWhileScreenOn);
        assertFalse("PowerBroadcastReceiver wasn't unregistered.", receiver.isRegistered());
    }
}
