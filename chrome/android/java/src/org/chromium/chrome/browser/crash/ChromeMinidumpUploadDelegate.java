// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.crash;

import android.content.Context;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.preferences.ChromePreferenceManager;
import org.chromium.chrome.browser.preferences.privacy.PrivacyPreferencesManager;
import org.chromium.components.minidump_uploader.util.CrashReportingPermissionManager;
import org.chromium.components.minidump_uploader.util.MinidumpUploadDelegate;

/**
 * Chrome-specific implementation of the MinidumpUploadDelegate - providing control over different
 * parts of the Minidump Uploading process.
 */
public class ChromeMinidumpUploadDelegate implements MinidumpUploadDelegate {
    private static final String TAG = "ChromeMinidumpUploadDelegate";
    /**
     * Histogram related constants.
     */
    private static final String HISTOGRAM_NAME_PREFIX = "Tab.AndroidCrashUpload_";
    private static final int HISTOGRAM_MAX = 2;
    private static final int FAILURE = 0;
    private static final int SUCCESS = 1;

    @Override
    public void onSuccessfulUpload(Context context, @ProcessType String crashType) {
        // Increment the count of success by 1 and distinguish between different types of
        // crashes.
        ChromePreferenceManager.getInstance(context).incrementCrashSuccessUploadCount(crashType);
    }

    @Override
    public void onMaxedOutUploadFailures(Context context, @ProcessType String crashType) {
        // Increment the count of failure by 1 and distinguish between different types of
        // crashes.
        ChromePreferenceManager.getInstance(context).incrementCrashFailureUploadCount(crashType);
    }

    @Override
    public CrashReportingPermissionManager getCrashReportingPermissionManager() {
        return PrivacyPreferencesManager.getInstance();
    }

    /**
     * Stores the successes and failures from uploading crash to UMA,
     */
    public static void storeBreakpadUploadStatsInUma(ChromePreferenceManager pref) {
        for (String type : CRASH_TYPES) {
            for (int success = pref.getCrashSuccessUploadCount(type); success > 0; success--) {
                RecordHistogram.recordEnumeratedHistogram(
                        HISTOGRAM_NAME_PREFIX + type,
                        SUCCESS,
                        HISTOGRAM_MAX);
            }
            for (int fail = pref.getCrashFailureUploadCount(type); fail > 0; fail--) {
                RecordHistogram.recordEnumeratedHistogram(
                        HISTOGRAM_NAME_PREFIX + type,
                        FAILURE,
                        HISTOGRAM_MAX);
            }

            pref.setCrashSuccessUploadCount(type, 0);
            pref.setCrashFailureUploadCount(type, 0);
        }
    }
}

