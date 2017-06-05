// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;

import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

/**
 * Determines whether metrics should be enabled.
 *
 * This requires the following steps:
 * 1) Check the platform's metrics consent setting.
 * 2) Check if the app has opted out.
 * 3) Wait for the native AwMetricsServiceClient to call nativeInitialized.
 * 4) If enabled, inform the native AwMetricsServiceClient via nativeSetMetricsEnabled.
 *
 * Step 1 is done asynchronously and the result is passed to setConsentSetting, which does step 2.
 * This happens in parallel with native AwMetricsServiceClient initialization; either
 * nativeInitialized or setConsentSetting might fire first. Whichever fires second should call
 * nativeSetMetricsEnabled.
 */
@JNINamespace("android_webview")
public class AwMetricsServiceClient {
    private static final String TAG = "AwMetricsServiceCli-";

    // Individual apps can use this meta-data tag in their manifest to opt out of metrics
    // reporting. See https://developer.android.com/reference/android/webkit/WebView.html
    private static final String OPT_OUT_META_DATA_STR = "android.webkit.WebView.MetricsOptOut";

    private static boolean sIsClientReady; // Is the native AwMetricsServiceClient initialized?
    private static boolean sShouldEnable; // Have steps 1 and 2 passed?

    private static boolean isAppOptedOut(Context appContext) {
        try {
            ApplicationInfo info = appContext.getPackageManager().getApplicationInfo(
                    appContext.getPackageName(), PackageManager.GET_META_DATA);
            if (info.metaData == null) {
                // null means no such tag was found.
                return false;
            }
            // getBoolean returns false if the key is not found, which is what we want.
            return info.metaData.getBoolean(OPT_OUT_META_DATA_STR);
        } catch (PackageManager.NameNotFoundException e) {
            // This should never happen.
            Log.e(TAG, "App could not find itself by package name!");
            // The conservative thing is to assume the app HAS opted out.
            return true;
        }
    }

    public static void setConsentSetting(Context appContext, boolean userConsent) {
        ThreadUtils.assertOnUiThread();

        if (!userConsent || isAppOptedOut(appContext)) {
            // Metrics defaults to off, so no need to call nativeSetMetricsEnabled(false).
            return;
        }

        sShouldEnable = true;
        if (sIsClientReady) {
            nativeSetMetricsEnabled(true);
        }
    }

    @CalledByNative
    public static void nativeInitialized() {
        ThreadUtils.assertOnUiThread();
        sIsClientReady = true;
        if (sShouldEnable) {
            nativeSetMetricsEnabled(true);
        }
    }

    @CalledByNative
    public static String getWebViewPackageName() {
        return AwBrowserProcess.getWebViewPackageName();
    }

    public static native void nativeSetMetricsEnabled(boolean enabled);
}
