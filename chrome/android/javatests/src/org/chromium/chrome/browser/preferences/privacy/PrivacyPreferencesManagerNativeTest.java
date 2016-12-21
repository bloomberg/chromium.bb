// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.privacy;

import android.content.Context;
import android.content.SharedPreferences;
import android.support.test.filters.SmallTest;
import android.test.UiThreadTest;

import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.base.test.util.AdvancedMockContext;
import org.chromium.base.test.util.Feature;
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
    public void testSyncUsageAndCrashReporting {
        CommandLine.init(null);
        PermissionContext context = new PermissionContext(getInstrumentation().getTargetContext());
        PrefServiceBridge prefBridge = PrefServiceBridge.getInstance();
        SharedPreferences pref = ContextUtils.getAppSharedPreferences();
        PrivacyPreferencesManager preferenceManager = new PrivacyPreferencesManager(context);

        // Setup prefs to be out of sync.
        prefBridge.setMetricsReportingEnabled(false);
        pref.edit().putBoolean(PrivacyPreferencesManager.PREF_METRICS_REPORTING, true).apply();

        preferenceManager.syncUsageAndCrashReportingPrefs();
        assertTrue("Native preference should be True ", prefBridge.isMetricsReportingEnabled());
    }

    @SmallTest
    @Feature({"Android-AppBase"})
    @UiThreadTest
    public void testSetUsageAndCrashReporting() {
        CommandLine.init(null);
        PermissionContext context = new PermissionContext(getInstrumentation().getTargetContext());
        PrefServiceBridge prefBridge = PrefServiceBridge.getInstance();
        SharedPreferences pref = ContextUtils.getAppSharedPreferences();
        PrivacyPreferencesManager preferenceManager = new PrivacyPreferencesManager(context);

        preferenceManager.setUsageAndCrashReporting(true);
        assertTrue(pref.getBoolean(PrivacyPreferencesManager.PREF_METRICS_REPORTING, false));
        assertTrue("Native preference should be True ", prefBridge.isMetricsReportingEnabled());

        preferenceManager.setUsageAndCrashReporting(false);
        assertFalse(pref.getBoolean(PrivacyPreferencesManager.PREF_METRICS_REPORTING, false));
        assertFalse("Native preference should be False ", prefBridge.isMetricsReportingEnabled());
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
}
