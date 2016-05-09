// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.privacy;

import android.content.Context;
import android.content.SharedPreferences;
import android.test.InstrumentationTestCase;
import android.test.UiThreadTest;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.base.test.util.AdvancedMockContext;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;

public class PrivacyPreferencesManagerTest extends InstrumentationTestCase {

    private static final boolean CELLULAR_DEVICE = true;
    private static final boolean WIFI_DEVICE = false;

    private static final boolean CONNECTED = true;
    private static final boolean DISCONNECTED = false;

    private static final boolean WIFI_ON = true;
    private static final boolean WIFI_OFF = false;

    private static final boolean UPLOAD_OK = true;
    private static final boolean UPLOAD_NOT_PERMITTED = false;

    private static final UserUploadPreference UPLOAD_ALWAYS = UserUploadPreference.ALWAYS;
    private static final UserUploadPreference UPLOAD_WIFI_ONLY = UserUploadPreference.WIFI_ONLY;
    private static final UserUploadPreference UPLOAD_NEVER = UserUploadPreference.NEVER;

    private static final boolean EXPERIMENT_ENABLED = true;
    private static final boolean EXPERIMENT_DISABLED = false;
    private static final boolean METRIC_REPORTING_SET = true;
    private static final boolean METRIC_REPORTING_NOT_SET = false;
    private static final boolean METRIC_REPORTING_ENABLED = true;
    private static final boolean METRIC_REPORTING_DISABLED = false;

    // Perform the same test a few times to make sure any sort of
    // caching still works.
    private static final int REPS = 3;

    /**
     * Enum used to specify user upload preference that is easy to read.
     */
    private static enum UserUploadPreference {
        ALWAYS(R.string.crash_dump_always_upload_value),
        WIFI_ONLY(R.string.crash_dump_only_with_wifi_value),
        NEVER(R.string.crash_dump_never_upload_value);

        private final int mValueId;

        UserUploadPreference(int valueId) {
            mValueId = valueId;
        }

        public String toStringValue(Context context) {
            return context.getString(mValueId);
        }

        public boolean toBooleanValue() {
            return !equals(NEVER);  // return true for WIFI_ONLY for now
        }
    }

    @SmallTest
    @Feature({"Android-AppBase"})
    @UiThreadTest
    public void testAllowCrashDumpUploadNowCellDev() {
        CommandLine.init(null);
        runTest(CELLULAR_DEVICE, UPLOAD_ALWAYS, CONNECTED, WIFI_ON, EXPERIMENT_DISABLED,
                METRIC_REPORTING_NOT_SET, METRIC_REPORTING_ENABLED, UPLOAD_OK);
        runTest(CELLULAR_DEVICE, UPLOAD_ALWAYS, DISCONNECTED, WIFI_ON, EXPERIMENT_DISABLED,
                METRIC_REPORTING_NOT_SET, METRIC_REPORTING_ENABLED, UPLOAD_NOT_PERMITTED);
        runTest(CELLULAR_DEVICE, UPLOAD_ALWAYS, CONNECTED, WIFI_OFF, EXPERIMENT_DISABLED,
                METRIC_REPORTING_NOT_SET, METRIC_REPORTING_ENABLED, UPLOAD_OK);
        runTest(CELLULAR_DEVICE, UPLOAD_ALWAYS, DISCONNECTED, WIFI_OFF, EXPERIMENT_DISABLED,
                METRIC_REPORTING_NOT_SET, METRIC_REPORTING_ENABLED, UPLOAD_NOT_PERMITTED);

        runTest(CELLULAR_DEVICE, UPLOAD_WIFI_ONLY, CONNECTED, WIFI_ON, EXPERIMENT_DISABLED,
                METRIC_REPORTING_NOT_SET, METRIC_REPORTING_ENABLED, UPLOAD_OK);
        runTest(CELLULAR_DEVICE, UPLOAD_WIFI_ONLY, DISCONNECTED, WIFI_ON, EXPERIMENT_DISABLED,
                METRIC_REPORTING_NOT_SET, METRIC_REPORTING_ENABLED, UPLOAD_NOT_PERMITTED);
        runTest(CELLULAR_DEVICE, UPLOAD_WIFI_ONLY, CONNECTED, WIFI_OFF, EXPERIMENT_DISABLED,
                METRIC_REPORTING_NOT_SET, METRIC_REPORTING_ENABLED, UPLOAD_NOT_PERMITTED);
        runTest(CELLULAR_DEVICE, UPLOAD_WIFI_ONLY, DISCONNECTED, WIFI_OFF, EXPERIMENT_DISABLED,
                METRIC_REPORTING_NOT_SET, METRIC_REPORTING_ENABLED, UPLOAD_NOT_PERMITTED);

        runTest(CELLULAR_DEVICE, UPLOAD_NEVER, CONNECTED, WIFI_ON, EXPERIMENT_DISABLED,
                METRIC_REPORTING_NOT_SET, METRIC_REPORTING_ENABLED, UPLOAD_NOT_PERMITTED);
        runTest(CELLULAR_DEVICE, UPLOAD_NEVER, DISCONNECTED, WIFI_ON, EXPERIMENT_DISABLED,
                METRIC_REPORTING_NOT_SET, METRIC_REPORTING_ENABLED, UPLOAD_NOT_PERMITTED);
        runTest(CELLULAR_DEVICE, UPLOAD_NEVER, CONNECTED, WIFI_OFF, EXPERIMENT_DISABLED,
                METRIC_REPORTING_NOT_SET, METRIC_REPORTING_ENABLED, UPLOAD_NOT_PERMITTED);
        runTest(CELLULAR_DEVICE, UPLOAD_NEVER, DISCONNECTED, WIFI_OFF, EXPERIMENT_DISABLED,
                METRIC_REPORTING_NOT_SET, METRIC_REPORTING_ENABLED, UPLOAD_NOT_PERMITTED);

        runTest(CELLULAR_DEVICE, UPLOAD_ALWAYS, CONNECTED, WIFI_ON, EXPERIMENT_ENABLED,
                METRIC_REPORTING_NOT_SET, METRIC_REPORTING_DISABLED, UPLOAD_OK);
        runTest(CELLULAR_DEVICE, UPLOAD_ALWAYS, DISCONNECTED, WIFI_ON, EXPERIMENT_ENABLED,
                METRIC_REPORTING_NOT_SET, METRIC_REPORTING_DISABLED, UPLOAD_NOT_PERMITTED);
        runTest(CELLULAR_DEVICE, UPLOAD_WIFI_ONLY, CONNECTED, WIFI_ON, EXPERIMENT_ENABLED,
                METRIC_REPORTING_NOT_SET, METRIC_REPORTING_DISABLED, UPLOAD_OK);
        runTest(CELLULAR_DEVICE, UPLOAD_NEVER, CONNECTED, WIFI_ON, EXPERIMENT_ENABLED,
                METRIC_REPORTING_NOT_SET, METRIC_REPORTING_ENABLED, UPLOAD_NOT_PERMITTED);

        runTest(CELLULAR_DEVICE, UPLOAD_ALWAYS, CONNECTED, WIFI_ON, EXPERIMENT_ENABLED,
                METRIC_REPORTING_SET, METRIC_REPORTING_ENABLED, UPLOAD_OK);
        runTest(CELLULAR_DEVICE, UPLOAD_ALWAYS, CONNECTED, WIFI_OFF, EXPERIMENT_ENABLED,
                METRIC_REPORTING_SET, METRIC_REPORTING_ENABLED, UPLOAD_OK);
        runTest(CELLULAR_DEVICE, UPLOAD_ALWAYS, CONNECTED, WIFI_ON, EXPERIMENT_ENABLED,
                METRIC_REPORTING_SET, METRIC_REPORTING_DISABLED, UPLOAD_NOT_PERMITTED);
        runTest(CELLULAR_DEVICE, UPLOAD_ALWAYS, DISCONNECTED, WIFI_ON, EXPERIMENT_ENABLED,
                METRIC_REPORTING_SET, METRIC_REPORTING_ENABLED, UPLOAD_NOT_PERMITTED);
    }

    @SmallTest
    @Feature({"Android-AppBase"})
    @UiThreadTest
    public void testAllowCrashDumpUploadNowWifiDev() {
        CommandLine.init(null);
        runTest(WIFI_DEVICE, UPLOAD_ALWAYS, CONNECTED, WIFI_ON, EXPERIMENT_DISABLED,
                METRIC_REPORTING_NOT_SET, METRIC_REPORTING_ENABLED, UPLOAD_OK);
        runTest(WIFI_DEVICE, UPLOAD_ALWAYS, DISCONNECTED, WIFI_OFF, EXPERIMENT_DISABLED,
                METRIC_REPORTING_NOT_SET, METRIC_REPORTING_ENABLED, UPLOAD_NOT_PERMITTED);

        runTest(WIFI_DEVICE, UPLOAD_NEVER, CONNECTED, WIFI_ON, EXPERIMENT_DISABLED,
                METRIC_REPORTING_NOT_SET, METRIC_REPORTING_ENABLED, UPLOAD_NOT_PERMITTED);
        runTest(WIFI_DEVICE, UPLOAD_NEVER, DISCONNECTED, WIFI_OFF, EXPERIMENT_DISABLED,
                METRIC_REPORTING_NOT_SET, METRIC_REPORTING_ENABLED, UPLOAD_NOT_PERMITTED);

        runTest(WIFI_DEVICE, UPLOAD_ALWAYS, CONNECTED, WIFI_ON, EXPERIMENT_ENABLED,
                METRIC_REPORTING_NOT_SET, METRIC_REPORTING_ENABLED, UPLOAD_OK);
        runTest(WIFI_DEVICE, UPLOAD_NEVER, CONNECTED, WIFI_ON, EXPERIMENT_ENABLED,
                METRIC_REPORTING_NOT_SET, METRIC_REPORTING_ENABLED, UPLOAD_NOT_PERMITTED);

        runTest(WIFI_DEVICE, UPLOAD_ALWAYS, CONNECTED, WIFI_ON, EXPERIMENT_ENABLED,
                METRIC_REPORTING_SET, METRIC_REPORTING_ENABLED, UPLOAD_OK);
        runTest(WIFI_DEVICE, UPLOAD_ALWAYS, CONNECTED, WIFI_ON, EXPERIMENT_ENABLED,
                METRIC_REPORTING_SET, METRIC_REPORTING_DISABLED, UPLOAD_NOT_PERMITTED);
    }

    private void runTest(boolean mobileCapable, UserUploadPreference userPreference,
            boolean isConnected, boolean wifiOn, boolean experimentEnabled,
            boolean hasSetMetricsReporting, boolean isMetricsReportinEnabled,
            boolean uploadPermitted) {
        PermissionContext context = new PermissionContext(getInstrumentation().getTargetContext());
        ContextUtils.initApplicationContextForTests(context.getApplicationContext());
        PrivacyPreferencesManager preferenceManager =
                new MockPrivacyPreferencesManager(context, mobileCapable, isConnected, wifiOn,
                        hasSetMetricsReporting, isMetricsReportinEnabled);
        preferenceManager.enablePotentialCrashUploading();
        preferenceManager.setCellularExperiment(experimentEnabled);

        for (int i = 0; i < REPS; i++) {
            setUpUserPreferences(context, userPreference);
            String state =
                    String.format("[cellular = %b, preference = %b, connected = %b, wifi = %b,"
                                    + "experiment = %b, reporting_set = %b, reporting = %b]",
                            mobileCapable, userPreference.toBooleanValue(), isConnected, wifiOn,
                            experimentEnabled, hasSetMetricsReporting, isMetricsReportinEnabled);
            boolean res = preferenceManager.isUploadPermitted();
            if (uploadPermitted) {
                assertTrue("Upload should be permitted for " + state, res);
            } else {
                assertFalse("Upload should NOT be permitted for " + state, res);
            }
        }
    }

    private void setUpUserPreferences(Context context, UserUploadPreference userPreference) {
        SharedPreferences pref = ContextUtils.getAppSharedPreferences();
        SharedPreferences.Editor ed = pref.edit()
                .putString(PrivacyPreferencesManager.PREF_CRASH_DUMP_UPLOAD,
                        userPreference.toStringValue(context))
                .putBoolean(PrivacyPreferencesManager.PREF_CRASH_DUMP_UPLOAD_NO_CELLULAR,
                        userPreference.toBooleanValue());
        ed.apply();
    }

    private static class MockPrivacyPreferencesManager extends PrivacyPreferencesManager {
        private final boolean mIsMobileCapable;
        private final boolean mIsConnected;
        private final boolean mIsWifi;

        MockPrivacyPreferencesManager(Context context, boolean isMobileCapable, boolean isConnected,
                boolean isWifi, boolean hasSetMetricsReporting, boolean isMetricsReportinEnabled) {
            super(context);
            mIsMobileCapable = isMobileCapable;
            mIsConnected = isConnected;
            mIsWifi = isWifi;

            if (hasSetMetricsReporting) setUsageAndCrashReporting(isMetricsReportinEnabled);
        }

        @Override
        public boolean isMobileNetworkCapable() {
            return mIsMobileCapable;
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
