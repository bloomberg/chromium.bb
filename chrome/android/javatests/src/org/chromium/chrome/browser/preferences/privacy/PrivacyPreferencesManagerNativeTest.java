// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.privacy;

import android.content.Context;
import android.content.SharedPreferences;
import android.test.UiThreadTest;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.base.test.util.AdvancedMockContext;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.content.browser.test.NativeLibraryTestBase;

/**
 *  Tests "Usage and Crash reporting" settings screen.
 */
public class PrivacyPreferencesManagerNativeTest extends NativeLibraryTestBase {
    @Override
    protected void setUp() throws Exception {
        super.setUp();
        loadNativeLibraryAndInitBrowserProcess();
    }

    @SmallTest
    @Feature({"Android-AppBase"})
    @UiThreadTest
    public void testSyncUsageAndCrashReportingPrefsMobile() {
        testSyncUsageAndCrashReportingPrefs(true);
    }

    @SmallTest
    @Feature({"Android-AppBase"})
    @UiThreadTest
    public void testSyncUsageAndCrashReportingPrefsTablet() {
        testSyncUsageAndCrashReportingPrefs(false);
    }

    void testSyncUsageAndCrashReportingPrefs(boolean isMobile) {
        CommandLine.init(null);
        PermissionContext context = new PermissionContext(getInstrumentation().getTargetContext());
        PrefServiceBridge prefBridge = PrefServiceBridge.getInstance();
        SharedPreferences pref = ContextUtils.getAppSharedPreferences();

        // Not a cellular experiment and prefs are out of sync for mobile devices.
        PrivacyPreferencesManager preferenceManager =
                new MockPrivacyPreferencesManager(context, isMobile);
        preferenceManager.setCellularExperiment(false);
        prefBridge.setCrashReportingEnabled(false);
        SharedPreferences.Editor ed;
        if (isMobile) {
            ed = pref.edit().putString(PrivacyPreferencesManager.PREF_CRASH_DUMP_UPLOAD,
                    context.getString(R.string.crash_dump_always_upload_value));
        } else {
            ed = pref.edit().putBoolean(
                    PrivacyPreferencesManager.PREF_CRASH_DUMP_UPLOAD_NO_CELLULAR, true);
        }

        ed.apply();

        preferenceManager.syncUsageAndCrashReportingPrefs();
        assertTrue("Native preference should be True ", prefBridge.isCrashReportingEnabled());
    }

    private static class PermissionContext extends AdvancedMockContext {
        public PermissionContext(Context targetContext) {
            super(targetContext);
        }

        @Override
        public Object getSystemService(String name) {
            if (Context.CONNECTIVITY_SERVICE.equals(name)) {
                return null;
            }
            fail("Should not ask for any other service than the ConnectionManager.");
            return super.getSystemService(name);
        }
    }

    private static class MockPrivacyPreferencesManager extends PrivacyPreferencesManager {
        private final boolean mIsMobileCapable;
        MockPrivacyPreferencesManager(Context context, boolean isMobileCapable) {
            super(context);
            mIsMobileCapable = isMobileCapable;
        }

        @Override
        public boolean isMobileNetworkCapable() {
            return mIsMobileCapable;
        }
    }
}
