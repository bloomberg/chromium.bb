// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.privacy;

import android.content.Context;
import android.support.test.filters.SmallTest;
import android.test.InstrumentationTestCase;
import android.test.UiThreadTest;

import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.base.test.util.AdvancedMockContext;
import org.chromium.base.test.util.Feature;

/**
 *  Tests "Usage and Crash reporting" preferences.
 */
public class PrivacyPreferencesManagerTest extends InstrumentationTestCase {

    private static final boolean CONNECTED = true;
    private static final boolean DISCONNECTED = false;

    private static final boolean WIFI_ON = true;
    private static final boolean WIFI_OFF = false;

    private static final boolean METRICS_UPLOAD_OK = true;
    private static final boolean METRICS_UPLOAD_NOT_PERMITTED = false;

    private static final boolean CRASH_NETWORK_OK = true;
    private static final boolean CRASH_NETWORK_NOT_PERMITTED = false;

    private static final boolean METRIC_REPORTING_ENABLED = true;
    private static final boolean METRIC_REPORTING_DISABLED = false;

    // Perform the same test a few times to make sure any sort of
    // caching still works.
    private static final int REPS = 3;

    @SmallTest
    @Feature({"Android-AppBase"})
    @UiThreadTest
    public void testAllowCrashDumpUploadNowCellDev() {
        CommandLine.init(null);
        runTest(CONNECTED, WIFI_ON, METRIC_REPORTING_ENABLED, METRICS_UPLOAD_OK, CRASH_NETWORK_OK);
        runTest(CONNECTED, WIFI_OFF, METRIC_REPORTING_ENABLED, METRICS_UPLOAD_OK,
                CRASH_NETWORK_NOT_PERMITTED);
        runTest(DISCONNECTED, WIFI_OFF, METRIC_REPORTING_ENABLED, METRICS_UPLOAD_NOT_PERMITTED,
                CRASH_NETWORK_NOT_PERMITTED);

        runTest(CONNECTED, WIFI_ON, METRIC_REPORTING_DISABLED, METRICS_UPLOAD_NOT_PERMITTED,
                CRASH_NETWORK_OK);
        runTest(CONNECTED, WIFI_OFF, METRIC_REPORTING_DISABLED, METRICS_UPLOAD_NOT_PERMITTED,
                CRASH_NETWORK_NOT_PERMITTED);
        runTest(DISCONNECTED, WIFI_OFF, METRIC_REPORTING_DISABLED, METRICS_UPLOAD_NOT_PERMITTED,
                CRASH_NETWORK_NOT_PERMITTED);
    }

    private void runTest(boolean isConnected, boolean wifiOn, boolean isMetricsReportingEnabled,
            boolean expectedMetricsUploadPermitted,
            boolean expectedNetworkAvailableForCrashUploads) {
        PermissionContext context = new PermissionContext(getInstrumentation().getTargetContext());
        ContextUtils.initApplicationContextForTests(context.getApplicationContext());
        PrivacyPreferencesManager preferenceManager = new MockPrivacyPreferencesManager(
                context, isConnected, wifiOn, isMetricsReportingEnabled);
        preferenceManager.enablePotentialCrashUploading();

        for (int i = 0; i < REPS; i++) {
            String state = String.format("[connected = %b, wifi = %b, reporting = %b]", isConnected,
                    wifiOn, isMetricsReportingEnabled);
            String msg = String.format("Metrics reporting should be %1$b for %2$s",
                    expectedMetricsUploadPermitted, state);
            assertEquals(msg, expectedMetricsUploadPermitted,
                    preferenceManager.isMetricsUploadPermitted());

            msg = String.format("Crash reporting should be %1$b for wifi %2$s",
                    expectedNetworkAvailableForCrashUploads, wifiOn);
            assertEquals(msg, expectedNetworkAvailableForCrashUploads,
                    preferenceManager.isNetworkAvailableForCrashUploads());
        }
    }

    private static class MockPrivacyPreferencesManager extends PrivacyPreferencesManager {
        private final boolean mIsConnected;
        private final boolean mIsWifi;

        MockPrivacyPreferencesManager(Context context, boolean isConnected, boolean isWifi,
                boolean isMetricsReportingEnabled) {
            super(context);
            mIsConnected = isConnected;
            mIsWifi = isWifi;

            setUsageAndCrashReporting(isMetricsReportingEnabled);
        }

        @Override
        public boolean isNetworkAvailable() {
            return mIsConnected;
        }

        @Override
        public boolean isWiFiOrEthernetNetwork() {
            return mIsWifi;
        }
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
