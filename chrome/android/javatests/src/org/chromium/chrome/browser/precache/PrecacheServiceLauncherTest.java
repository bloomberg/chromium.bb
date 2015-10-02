// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.precache;

import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ApplicationInfo;
import android.preference.PreferenceManager;
import android.test.InstrumentationTestCase;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.test.util.AdvancedMockContext;
import org.chromium.base.test.util.Feature;
import org.chromium.components.precache.MockDeviceState;

import java.util.EnumSet;

/**
 * Tests the |PrecacheServiceLauncher|.
 */
public class PrecacheServiceLauncherTest extends InstrumentationTestCase {
    private MockPrecacheServiceLauncher mLauncher;
    private MockPrecacheLauncher mPrecacheLauncher;
    private Context mContext;

    class PrecacheMockContext extends AdvancedMockContext {
        @Override
        public String getPackageName() {
            return getInstrumentation().getTargetContext().getPackageName();
        }

        @Override
        public ApplicationInfo getApplicationInfo() {
            return getInstrumentation().getTargetContext().getApplicationInfo();
        }
    }

    static class MockPrecacheLauncher extends PrecacheLauncher {
        @Override
        public EnumSet<FailureReason> failureReasons() {
            return mFailureReasons;
        }

        public void setFailureReasons(EnumSet<FailureReason> reasons) {
            mFailureReasons = reasons;
        }

        @Override
        public void onPrecacheCompleted(boolean tryAgainSoon) {}

        private EnumSet<FailureReason> mFailureReasons = EnumSet.noneOf(FailureReason.class);
    }

    /**
     * Class mocks out the system-related function of |PrecacheServiceLauncher|
     * and provides a method to set a mocked elapsed realtime.
     */
    static class MockPrecacheServiceLauncher extends PrecacheServiceLauncher {
        private long mElapsedRealtime;
        protected boolean mCancelAlarmCalled;
        protected boolean mSetAlarmCalled;
        protected boolean mStartPrecacheCalled;

        @Override
        protected void setAlarmOnSystem(Context context, int type, long triggerAtMillis,
                PendingIntent operation) {
            mSetAlarmCalled = true;
        }

        @Override
        protected void cancelAlarmOnSystem(Context context, PendingIntent operation) {
            mCancelAlarmCalled = true;
        }

        @Override
        protected long getElapsedRealtimeOnSystem() {
            return mElapsedRealtime;
        }

        @Override
        protected void startPrecacheService(Context context) {
            mStartPrecacheCalled = true;
        }

        @Override
        protected void acquireWakeLock(Context context) {}

        protected void setElapsedRealtime(long elapsedRealtime) {
            mElapsedRealtime = elapsedRealtime;
        }
    }

    @Override
    public void setUp() throws Exception {
        super.setUp();
        mLauncher = new MockPrecacheServiceLauncher();
        mPrecacheLauncher = new MockPrecacheLauncher();
        mLauncher.setPrecacheLauncher(mPrecacheLauncher);
        mContext = new PrecacheMockContext();
        setPrecachingEnabled(true);
    }

    @Override
    public void tearDown() throws Exception {
        PrecacheService.setIsPrecaching(false);
        super.tearDown();
    }

    @SmallTest
    @Feature({"Precache"})
    public void testDoNothingIfNotEnabled() {
        setPrecachingEnabled(false);
        mPrecacheLauncher.setFailureReasons(EnumSet.of(FailureReason.NATIVE_SHOULD_RUN_IS_FALSE));
        mLauncher.setDeviceState(new MockDeviceState(0 /* stickyBatteryStatus */,
                true /* powerIsConnected */, true /* wifiIsAvailable */));
        mLauncher.setElapsedRealtime(PrecacheServiceLauncher.WAIT_UNTIL_NEXT_PRECACHE_MS);

        mLauncher.onReceive(mContext, new Intent(PrecacheServiceLauncher.ACTION_ALARM));

        assertFalse(mLauncher.mSetAlarmCalled);
        assertFalse(mLauncher.mCancelAlarmCalled);
        assertFalse(mLauncher.mStartPrecacheCalled);
        assertEquals(EnumSet.of(FailureReason.NATIVE_SHOULD_RUN_IS_FALSE),
                mLauncher.failureReasons(mContext));
    }

    @SmallTest
    @Feature({"Precache"})
    public void testGoodConditions() {
        mLauncher.setDeviceState(new MockDeviceState(0, true, true));

        mLauncher.onReceive(mContext, new Intent(PrecacheServiceLauncher.ACTION_ALARM));

        assertFalse(mLauncher.mSetAlarmCalled);
        assertFalse(mLauncher.mCancelAlarmCalled);
        assertTrue(mLauncher.mStartPrecacheCalled);
        assertEquals(EnumSet.noneOf(FailureReason.class), mLauncher.failureReasons(mContext));
    }

    @SmallTest
    @Feature({"Precache"})
    public void testNotEnoughTimeButGoodConditionsOtherwise() {
        mLauncher.setDeviceState(new MockDeviceState(0, true, true));
        setLastPrecacheMs(0L);

        mLauncher.onReceive(mContext, new Intent(PrecacheServiceLauncher.ACTION_ALARM));

        assertTrue(mLauncher.mSetAlarmCalled);
        assertFalse(mLauncher.mCancelAlarmCalled);
        assertFalse(mLauncher.mStartPrecacheCalled);
        assertEquals(EnumSet.of(FailureReason.NOT_ENOUGH_TIME_SINCE_LAST_PRECACHE),
                mLauncher.failureReasons(mContext));
    }

    @SmallTest
    @Feature({"Precache"})
    public void testEnoughTimeButNoPower() {
        mLauncher.setDeviceState(new MockDeviceState(0, false, true));
        mLauncher.setElapsedRealtime(PrecacheServiceLauncher.WAIT_UNTIL_NEXT_PRECACHE_MS);

        mLauncher.onReceive(mContext, new Intent(PrecacheServiceLauncher.ACTION_ALARM));

        assertFalse(mLauncher.mSetAlarmCalled);
        assertTrue(mLauncher.mCancelAlarmCalled);
        assertFalse(mLauncher.mStartPrecacheCalled);
        assertEquals(EnumSet.of(FailureReason.NO_POWER), mLauncher.failureReasons(mContext));
    }

    @SmallTest
    @Feature({"Precache"})
    public void testAllFailureReasons() {
        mPrecacheLauncher.setFailureReasons(
                EnumSet.of(FailureReason.UPDATE_PRECACHING_ENABLED_NEVER_CALLED,
                        FailureReason.SYNC_NOT_INITIALIZED,
                        FailureReason.PRERENDER_PRIVACY_PREFERENCE_NOT_ENABLED,
                        FailureReason.NATIVE_SHOULD_RUN_IS_FALSE));
        mLauncher.setDeviceState(new MockDeviceState(0, false, false));
        setLastPrecacheMs(0L);
        PrecacheService.setIsPrecaching(true);

        mLauncher.onReceive(mContext, new Intent(PrecacheServiceLauncher.ACTION_ALARM));

        assertFalse(mLauncher.mSetAlarmCalled);
        assertTrue(mLauncher.mCancelAlarmCalled);
        assertFalse(mLauncher.mStartPrecacheCalled);
        assertEquals(EnumSet.allOf(FailureReason.class), mLauncher.failureReasons(mContext));
    }

    @SmallTest
    @Feature({"Precache"})
    public void testStateIsResetAfterReboot() {
        // 1. Precache is successfully run at time X.
        mLauncher.setDeviceState(new MockDeviceState(0, true, true));
        mLauncher.setElapsedRealtime(PrecacheServiceLauncher.WAIT_UNTIL_NEXT_PRECACHE_MS);
        mLauncher.onReceive(mContext, new Intent(PrecacheServiceLauncher.ACTION_ALARM));

        // precachingFinishedInternal() is not called in the test. Fake the time being updated.
        setLastPrecacheMs(PrecacheServiceLauncher.WAIT_UNTIL_NEXT_PRECACHE_MS);

        assertTrue(mLauncher.mStartPrecacheCalled);
        mLauncher.mStartPrecacheCalled = false;

        // 2. The device loses power and reboots.
        mLauncher.setDeviceState(new MockDeviceState(0, false /* power */, true));
        mLauncher.setElapsedRealtime(0);

        // 3. Some intent is triggered, which allows the reciever to notice the reboot and reset
        // lastPrecacheTimeMs.
        mLauncher.onReceive(mContext, new Intent(PrecacheServiceLauncher.ACTION_ALARM));

        assertTrue(mLauncher.mCancelAlarmCalled);
        assertEquals(EnumSet.of(FailureReason.NO_POWER), mLauncher.failureReasons(mContext));
        mLauncher.mCancelAlarmCalled = false;

        // 4. Precache is successfully run at time X+1.
        mLauncher.setDeviceState(new MockDeviceState(0, true, true));
        mLauncher.setElapsedRealtime(PrecacheServiceLauncher.WAIT_UNTIL_NEXT_PRECACHE_MS + 1);

        mLauncher.onReceive(mContext, new Intent(PrecacheServiceLauncher.ACTION_ALARM));

        assertEquals(EnumSet.noneOf(FailureReason.class), mLauncher.failureReasons(mContext));
        assertTrue(mLauncher.mStartPrecacheCalled);
    }

    private void setPrecachingEnabled(boolean enabled) {
        PreferenceManager.getDefaultSharedPreferences(mContext)
                .edit()
                .putBoolean(PrecacheServiceLauncher.PREF_IS_PRECACHING_ENABLED, enabled)
                .commit();
    }

    private void setLastPrecacheMs(long ms) {
        PreferenceManager.getDefaultSharedPreferences(mContext)
                .edit()
                .putLong(PrecacheServiceLauncher.PREF_PRECACHE_LAST_TIME, ms)
                .apply();
    }
}
