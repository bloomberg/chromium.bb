// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.precache;

import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences.Editor;
import android.support.test.filters.SmallTest;
import android.test.InstrumentationTestCase;

import com.google.android.gms.gcm.GcmNetworkManager;
import com.google.android.gms.gcm.Task;

import org.chromium.base.ContextUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.AdvancedMockContext;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.components.precache.MockDeviceState;

/**
 * Tests of {@link PrecacheController}.
 */
public class PrecacheControllerTest extends InstrumentationTestCase {
    private Context mContext;
    private MockPrecacheLauncher mPrecacheLauncher;
    private MockPrecacheController mPrecacheController;
    private MockPrecacheTaskScheduler mPrecacheTaskScheduler;

    /**
     * Mock of the {@link PrecacheLauncher}.
     */
    static class MockPrecacheLauncher extends PrecacheLauncher {

        private MockPrecacheController mController;

        public int destroyCnt = 0;
        public int startCnt = 0;
        public int cancelCnt = 0;

        public void setController(MockPrecacheController controller) {
            mController = controller;
        }

        @Override
        public void destroy() {
            destroyCnt++;
        }

        @Override
        public void start() {
            startCnt++;
        }

        @Override
        public void cancel() {
            cancelCnt++;
        }

        @Override
        protected void onPrecacheCompleted(boolean precacheStarted) {
            mController.handlePrecacheCompleted(precacheStarted);
        }
    }

    static class MockPrecacheTaskScheduler extends PrecacheTaskScheduler {
        public int schedulePeriodicCnt = 0;
        public int scheduleContinuationCnt = 0;
        public int cancelPeriodicCnt = 0;
        public int cancelContinuationCnt = 0;

        @Override
        boolean canScheduleTasks(Context context) {
            return false;
        }

        @Override
        boolean scheduleTask(Context context, Task task) {
            if (!canScheduleTasks(context)) return true;
            if (PrecacheController.PERIODIC_TASK_TAG.equals(task.getTag())) {
                schedulePeriodicCnt++;
            } else if (PrecacheController.CONTINUATION_TASK_TAG.equals(task.getTag())) {
                scheduleContinuationCnt++;
            }
            return true;
        }

        @Override
        boolean cancelTask(Context context, String tag) {
            if (PrecacheController.PERIODIC_TASK_TAG.equals(tag)) {
                cancelPeriodicCnt++;
            } else if (PrecacheController.CONTINUATION_TASK_TAG.equals(tag)) {
                cancelContinuationCnt++;
            }
            return true;
        }
    }

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mContext = new AdvancedMockContext(getInstrumentation().getTargetContext());
        mPrecacheLauncher = new MockPrecacheLauncher();
        mPrecacheController = new MockPrecacheController(mContext);
        mPrecacheTaskScheduler = new MockPrecacheTaskScheduler();
        mPrecacheLauncher.setController(mPrecacheController);
        mPrecacheController.setPrecacheLauncher(mPrecacheLauncher);
        PrecacheController.setTaskScheduler(mPrecacheTaskScheduler);
        RecordHistogram.setDisabledForTests(true);
        Editor editor = ContextUtils.getAppSharedPreferences().edit();
        editor.putBoolean(PrecacheController.PREF_IS_PRECACHING_ENABLED, false);
        editor.apply();
    }

    @Override
    public void tearDown() throws Exception {
        super.tearDown();
        RecordHistogram.setDisabledForTests(false);
    }

    protected void verifyScheduledAndCanceledCounts(
            int expectedPeriodicScheduled, int expectedContinuationScheduled,
            int expectedPeriodicCanceled, int expectedContinuationCanceled) {
        if (!mPrecacheTaskScheduler.canScheduleTasks(mContext)) {
            expectedPeriodicScheduled = 0;
            expectedContinuationScheduled = 0;
        }
        assertEquals(expectedPeriodicScheduled, mPrecacheTaskScheduler.schedulePeriodicCnt);
        assertEquals(expectedContinuationScheduled, mPrecacheTaskScheduler.scheduleContinuationCnt);
        assertEquals(expectedPeriodicCanceled, mPrecacheTaskScheduler.cancelPeriodicCnt);
        assertEquals(expectedContinuationCanceled, mPrecacheTaskScheduler.cancelContinuationCnt);
    }

    protected void verifyLockCounts(int expectedAcquired, int expectedReleased) {
        assertEquals(expectedAcquired, mPrecacheController.acquiredLockCnt);
        assertEquals(expectedReleased, mPrecacheController.releasedLockCnt);
    }

    protected void verifyBeginPrecaching() {
        PrecacheController.setIsPrecachingEnabled(mContext, true);
        assertEquals(GcmNetworkManager.RESULT_SUCCESS,
                mPrecacheController.precache(PrecacheController.PERIODIC_TASK_TAG));
        assertTrue(mPrecacheController.isPrecaching());
        verifyLockCounts(1, 0);
        // Any existing completion tasks are canceled.
        verifyScheduledAndCanceledCounts(1, 0, 0, 1);
        assertEquals(1, mPrecacheLauncher.startCnt);
    }

    protected void verifyContinuationGetsPreemptedByPeriodicTask() {
        PrecacheController.setIsPrecachingEnabled(mContext, true);
        assertEquals(GcmNetworkManager.RESULT_SUCCESS,
                mPrecacheController.precache(PrecacheController.CONTINUATION_TASK_TAG));
        assertTrue(mPrecacheController.isPrecaching());
        verifyLockCounts(1, 0);
        // Any existing completion tasks are canceled.
        verifyScheduledAndCanceledCounts(1, 0, 0, 1);
        assertEquals(1, mPrecacheLauncher.startCnt);
    }

    @SmallTest
    @Feature({"Precache"})
    public void testStartPrecachingNotEnabled() {
        PrecacheController.setIsPrecachingEnabled(mContext, false);
        verifyScheduledAndCanceledCounts(0, 0, 0, 0);
        assertEquals(0, mPrecacheLauncher.startCnt);
        assertTrue(mPrecacheController.precache(PrecacheController.PERIODIC_TASK_TAG)
                        == GcmNetworkManager.RESULT_SUCCESS);
        assertFalse(mPrecacheController.isPrecaching());
        verifyLockCounts(0, 0);
        // All tasks are canceled.
        verifyScheduledAndCanceledCounts(0, 0, 1, 1);
        assertEquals(0, mPrecacheLauncher.startCnt);
    }

    @SmallTest
    @Feature({"Precache"})
    public void testStartPrecachingEnabled() {
        verifyBeginPrecaching();

        mPrecacheLauncher.onPrecacheCompleted(true);
        assertFalse(mPrecacheController.isPrecaching());
        // A continuation task is scheduled.
        verifyScheduledAndCanceledCounts(1, 1, 0, 1);
        verifyLockCounts(1, 1);
    }

    @SmallTest
    @Feature({"Precache"})
    @RetryOnFailure
    public void testStartWhenAlreadyStarted() {
        verifyBeginPrecaching();

        assertEquals(GcmNetworkManager.RESULT_FAILURE,
                mPrecacheController.precache(PrecacheController.PERIODIC_TASK_TAG));
        assertTrue(mPrecacheController.isPrecaching());
        // No additional tasks are scheduled or canceled.
        verifyScheduledAndCanceledCounts(1, 0, 0, 1);
        verifyLockCounts(1, 0);
    }

    @SmallTest
    @Feature({"Precache"})
    @RetryOnFailure
    public void testDeviceStateChangeCancels() {
        verifyBeginPrecaching();

        mPrecacheController.setDeviceState(new MockDeviceState(0, true, false));
        mPrecacheController.getDeviceStateReceiver().onReceive(mContext, new Intent());
        assertFalse(mPrecacheController.isPrecaching());
        // A continuation task is scheduled.
        verifyScheduledAndCanceledCounts(1, 1, 0, 1);
        verifyLockCounts(1, 1);
    }

    @SmallTest
    @Feature({"Precache"})
    public void testDeviceStateChangeDoesNotCancel() {
        verifyBeginPrecaching();

        mPrecacheController.setDeviceState(new MockDeviceState(0, true, true));
        mPrecacheController.getDeviceStateReceiver().onReceive(mContext, new Intent());
        assertTrue(mPrecacheController.isPrecaching());
        // No additional tasks are scheduled or canceled.
        verifyScheduledAndCanceledCounts(1, 0, 0, 1);
        verifyLockCounts(1, 0);
    }

    @SmallTest
    @Feature({"Precache"})
    @RetryOnFailure
    public void testDeviceStateChangeWhenNotPrecaching() {
        assertFalse(mPrecacheController.isPrecaching());
        mPrecacheController.setDeviceState(new MockDeviceState(0, false, true));
        mPrecacheController.getDeviceStateReceiver().onReceive(mContext, new Intent());
        assertFalse(mPrecacheController.isPrecaching());
        // No tasks are scheduled or canceled.
        verifyScheduledAndCanceledCounts(0, 0, 0, 0);
        verifyLockCounts(0, 0);
        // device state change when not running has no effect (maybe unregisters)
    }

    @SmallTest
    @Feature({"Precache"})
    @RetryOnFailure
    public void testTimeoutCancelsPrecaching() {
        verifyBeginPrecaching();

        mPrecacheController.getTimeoutRunnable().run();
        assertFalse(mPrecacheController.isPrecaching());
        // A continuation task is scheduled.
        verifyScheduledAndCanceledCounts(1, 1, 0, 1);
        verifyLockCounts(1, 1);
    }

    @SmallTest
    @Feature({"Precache"})
    @RetryOnFailure
    public void testTimeoutDoesNotCancelIfNotPrecaching() {
        assertFalse(mPrecacheController.isPrecaching());

        mPrecacheController.getTimeoutRunnable().run();
        assertFalse(mPrecacheController.isPrecaching());
        // No tasks are scheduled or canceled.
        verifyScheduledAndCanceledCounts(0, 0, 0, 0);
        verifyLockCounts(0, 0);
    }

    @SmallTest
    @Feature({"Precache"})
    @RetryOnFailure
    public void testPrecachingEnabledPreferences() {
        // Initial enable will schedule a periodic task.
        PrecacheController.setIsPrecachingEnabled(mContext, true);
        verifyScheduledAndCanceledCounts(1, 0, 0, 0);
        // Subsequent enable will not schedule or cancel tasks.
        PrecacheController.setIsPrecachingEnabled(mContext, true);
        verifyScheduledAndCanceledCounts(1, 0, 0, 0);

        // Disabling will cancel periodic and one-off tasks.
        PrecacheController.setIsPrecachingEnabled(mContext, false);
        verifyScheduledAndCanceledCounts(1, 0, 1, 1);
        // Subsequent disable will not schedule or cancel tasks.
        PrecacheController.setIsPrecachingEnabled(mContext, false);
        verifyScheduledAndCanceledCounts(1, 0, 1, 1);
    }
}
