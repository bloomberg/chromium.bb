// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.precache;

import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.test.InstrumentationTestCase;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.test.util.AdvancedMockContext;
import org.chromium.base.test.util.Feature;
import org.chromium.components.precache.MockDeviceState;

import java.util.HashMap;
import java.util.Map;

/**
 * Tests the |PrecacheServiceLauncher|.
 */
public class PrecacheServiceLauncherTest extends InstrumentationTestCase {
    private Context getContext(boolean enabled, long lastTimeMs) {
        AdvancedMockContext context = new AdvancedMockContext() {
            @Override
            public String getPackageName() {
                return getInstrumentation().getTargetContext().getPackageName();
            }
        };
        Map<String, Object> map = new HashMap<String, Object>();
        map.put(PrecacheServiceLauncher.PREF_IS_PRECACHING_ENABLED, enabled);
        map.put(PrecacheServiceLauncher.PREF_PRECACHE_LAST_TIME, lastTimeMs);
        context.addSharedPreferences(context.getPackageName() + "_preferences", map);
        return context;
    }

    /**
     * Class mocks out the system-related function of |PrecacheServiceLauncher|
     * and provides a method to set a mocked elapsed realtime.
     */
    static class MockPrecacheServiceLauncher extends PrecacheServiceLauncher {
        private long mElapsedRealtime;
        private boolean mCancelAlarmCalled;
        private boolean mSetAlarmCalled;
        private boolean mStartPrecacheCalled;

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

        protected void checkExpectations(boolean expectedSetAlarm, boolean expectedCancelAlarm,
                boolean expectedStartPrecache) {
            assertEquals("Set alarm", expectedSetAlarm, mSetAlarmCalled);
            assertEquals("Cancel alarm", expectedCancelAlarm, mCancelAlarmCalled);
            assertEquals("Start precache", expectedStartPrecache, mStartPrecacheCalled);
        }
    }

    @SmallTest
    @Feature({"Precache"})
    public void testDoNothingIfNotEnabled() {
        MockPrecacheServiceLauncher launcher = new MockPrecacheServiceLauncher();
        MockDeviceState deviceState = new MockDeviceState(0, true, false, true);
        launcher.setDeviceState(deviceState);
        Context context = getContext(false, 0L);
        launcher.setElapsedRealtime(PrecacheServiceLauncher.WAIT_UNTIL_NEXT_PRECACHE_MS);
        launcher.onReceive(context, new Intent(PrecacheServiceLauncher.ACTION_ALARM));
        launcher.checkExpectations(false, false, false);
    }

    @SmallTest
    @Feature({"Precache"})
    public void testGoodConditions() {
        MockPrecacheServiceLauncher launcher = new MockPrecacheServiceLauncher();
        MockDeviceState deviceState = new MockDeviceState(0, true, false, true);
        launcher.setDeviceState(deviceState);
        Context context = getContext(true, 0L);
        launcher.setElapsedRealtime(PrecacheServiceLauncher.WAIT_UNTIL_NEXT_PRECACHE_MS);
        launcher.onReceive(context, new Intent(PrecacheServiceLauncher.ACTION_ALARM));
        launcher.checkExpectations(false, false, true);
    }

    @SmallTest
    @Feature({"Precache"})
    public void testNotEnoughTimeButGoodConditionsOtherwise() {
        MockPrecacheServiceLauncher launcher = new MockPrecacheServiceLauncher();
        MockDeviceState deviceState = new MockDeviceState(0, true, false, true);
        launcher.setDeviceState(deviceState);
        Context context = getContext(true, 0L);
        launcher.setElapsedRealtime(PrecacheServiceLauncher.WAIT_UNTIL_NEXT_PRECACHE_MS - 1);
        launcher.onReceive(context, new Intent(PrecacheServiceLauncher.ACTION_ALARM));
        launcher.checkExpectations(true, false, false);
    }

    @SmallTest
    @Feature({"Precache"})
    public void testEnoughTimeButNoPower() {
        MockPrecacheServiceLauncher launcher = new MockPrecacheServiceLauncher();
        MockDeviceState deviceState = new MockDeviceState(0, false, false, true);
        launcher.setDeviceState(deviceState);
        Context context = getContext(true, 0L);
        launcher.setElapsedRealtime(PrecacheServiceLauncher.WAIT_UNTIL_NEXT_PRECACHE_MS - 1);
        launcher.onReceive(context, new Intent(PrecacheServiceLauncher.ACTION_ALARM));
        launcher.checkExpectations(false, true, false);
    }
}
