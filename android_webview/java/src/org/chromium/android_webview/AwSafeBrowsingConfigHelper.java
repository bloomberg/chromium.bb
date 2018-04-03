// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.support.annotation.IntDef;
import android.support.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.CommandLine;
import org.chromium.base.Log;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.metrics.RecordHistogram;

/**
 * Helper class for getting the configuration settings related to safebrowsing in WebView.
 */
@JNINamespace("android_webview")
public class AwSafeBrowsingConfigHelper {
    private static final String TAG = "AwSafeBrowsingConfi-";

    private static final String OPT_IN_META_DATA_STR = "android.webkit.WebView.EnableSafeBrowsing";
    private static final boolean DEFAULT_USER_OPT_IN = false;

    private static Boolean sSafeBrowsingUserOptIn;

    // Used to record the UMA histogram SafeBrowsing.WebView.AppOptIn. Since these values are
    // persisted to logs, they should never be renumbered nor reused.
    @IntDef({AppOptIn.NO_PREFERENCE, AppOptIn.OPT_IN, AppOptIn.OPT_OUT})
    @interface AppOptIn {
        int NO_PREFERENCE = 0;
        int OPT_IN = 1;
        int OPT_OUT = 2;

        int COUNT = 3;
    }

    private static void recordAppOptIn(@AppOptIn int value) {
        RecordHistogram.recordEnumeratedHistogram(
                "SafeBrowsing.WebView.AppOptIn", value, AppOptIn.COUNT);
    }

    public static void maybeEnableSafeBrowsingFromManifest(final Context appContext) {
        Boolean appOptIn = getAppOptInPreference(appContext);
        if (appOptIn == null) {
            recordAppOptIn(AppOptIn.NO_PREFERENCE);
        } else if (appOptIn) {
            recordAppOptIn(AppOptIn.OPT_IN);
        } else {
            recordAppOptIn(AppOptIn.OPT_OUT);
        }

        // If the app specifies something, fallback to the app's preference, otherwise check for the
        // existence of the CLI switch.
        AwContentsStatics.setSafeBrowsingEnabledByManifest(
                appOptIn == null ? !isDisabledByCommandLine() : appOptIn);

        Callback<Boolean> cb =
                optin -> setSafeBrowsingUserOptIn(optin == null ? DEFAULT_USER_OPT_IN : optin);
        PlatformServiceBridge.getInstance().querySafeBrowsingUserConsent(appContext, cb);
    }

    private static boolean isDisabledByCommandLine() {
        CommandLine cli = CommandLine.getInstance();
        // Disable flag has higher precedence than the default
        return cli.hasSwitch(AwSwitches.WEBVIEW_DISABLE_SAFEBROWSING_SUPPORT);
    }

    /**
     * Checks the application manifest for Safe Browsing opt-in preference.
     *
     * @param appContext application context.
     * @return true if app has opted in, false if opted out, and null if no preference specified.
     */
    @Nullable
    private static Boolean getAppOptInPreference(Context appContext) {
        try {
            ApplicationInfo info = appContext.getPackageManager().getApplicationInfo(
                    appContext.getPackageName(), PackageManager.GET_META_DATA);
            if (info.metaData == null) {
                // No <meta-data> tag was found.
                return null;
            }
            return info.metaData.containsKey(OPT_IN_META_DATA_STR)
                    ? info.metaData.getBoolean(OPT_IN_META_DATA_STR)
                    : null;
        } catch (PackageManager.NameNotFoundException e) {
            // This should never happen.
            Log.e(TAG, "App could not find itself by package name!");
            return false;
        }
    }

    // Can be called from any thread. This returns true or false, depending on user opt-in
    // preference. This returns null if we don't know yet what the user's preference is.
    public static Boolean getSafeBrowsingUserOptIn() {
        return sSafeBrowsingUserOptIn;
    }

    // Should only be called once during startup after we receive the result of the underlying GMS
    // API.
    public static void setSafeBrowsingUserOptIn(boolean optin) {
        sSafeBrowsingUserOptIn = optin;

        RecordHistogram.recordBooleanHistogram("SafeBrowsing.WebView.UserOptIn", optin);
    }

    // Not meant to be instantiated.
    private AwSafeBrowsingConfigHelper() {}
}
