// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.os.StrictMode;

import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.preferences.ChromePreferenceManager;
import org.chromium.webapk.lib.client.WebApkValidator;

/**
 * Contains functionality needed for Chrome to host WebAPKs.
 */
public class ChromeWebApkHost {
    private static final String TAG = "ChromeWebApkHost";

    private static Boolean sEnabledForTesting;

    public static void init() {
        WebApkValidator.initWithBrowserHostSignature(ChromeWebApkHostSignature.EXPECTED_SIGNATURE);
    }

    public static void initForTesting(boolean enabled) {
        sEnabledForTesting = enabled;
    }

    public static boolean isEnabled() {
        if (sEnabledForTesting != null) return sEnabledForTesting;

        return isEnabledInPrefs();
    }

    @CalledByNative
    private static boolean areWebApkEnabled() {
        return ChromeWebApkHost.isEnabled();
    }

    /**
     * Check the cached value to figure out if the feature is enabled. We have
     * to use the cached value because native library may not yet been loaded.
     *
     * @return Whether the feature is enabled.
     */
    private static boolean isEnabledInPrefs() {
        // Will go away once the feature is enabled for everyone by default.
        StrictMode.ThreadPolicy oldPolicy = StrictMode.allowThreadDiskReads();
        try {
            return ChromePreferenceManager.getInstance(
                    ContextUtils.getApplicationContext()).getCachedWebApkRuntimeEnabled();
        } finally {
            StrictMode.setThreadPolicy(oldPolicy);
        }
    }

    /**
     * Once native is loaded we can consult the command-line (set via about:flags) and also finch
     * state to see if we should enable WebAPKs.
     */
    public static void cacheEnabledStateForNextLaunch() {
        boolean wasEnabled = isEnabledInPrefs();
        CommandLine instance = CommandLine.getInstance();
        boolean isEnabled = ChromeFeatureList.isEnabled(ChromeFeatureList.WEBAPKS)
                && instance.hasSwitch(ChromeSwitches.ENABLE_WEBAPK);

        if (isEnabled != wasEnabled) {
            Log.d(TAG, "WebApk setting changed (%s => %s)", wasEnabled, isEnabled);
            ChromePreferenceManager.getInstance(ContextUtils.getApplicationContext())
                    .setCachedWebApkRuntimeEnabled(isEnabled);
        }
    }
}
