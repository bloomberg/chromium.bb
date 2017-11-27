// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.support.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.CommandLine;
import org.chromium.base.Log;
import org.chromium.base.annotations.JNINamespace;

/**
 * Helper class for getting the configuration settings related to safebrowsing in WebView.
 */
@JNINamespace("android_webview")
public class AwSafeBrowsingConfigHelper {
    private static final String TAG = "AwSafeBrowsingConfi-";

    private static final String OPT_IN_META_DATA_STR = "android.webkit.WebView.EnableSafeBrowsing";

    private static Boolean sSafeBrowsingUserOptIn;

    @SuppressWarnings("unchecked")
    public static void maybeEnableSafeBrowsingFromManifest(final Context appContext) {
        Boolean appOptIn = getAppOptInPreference(appContext);

        // If the app specifies something, fallback to the app's preference, otherwise check for the
        // existence of the CLI switch.
        AwContentsStatics.setSafeBrowsingEnabledByManifest(
                appOptIn == null ? getCommandLineOptIn() : appOptIn);

        Callback<Boolean> cb = optin -> setSafeBrowsingUserOptIn(optin == null ? false : optin);
        PlatformServiceBridge.getInstance().querySafeBrowsingUserConsent(appContext, cb);
    }

    private static boolean getCommandLineOptIn() {
        CommandLine cli = CommandLine.getInstance();
        // Disable flag has higher precedence than the enable flag
        if (cli.hasSwitch(AwSwitches.WEBVIEW_DISABLE_SAFEBROWSING_SUPPORT)) {
            return false;
        }
        return cli.hasSwitch(AwSwitches.WEBVIEW_ENABLE_SAFEBROWSING_SUPPORT)
                || cli.hasSwitch(AwSwitches.WEBVIEW_SAFEBROWSING_BLOCK_ALL_RESOURCES);
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

    public static void setSafeBrowsingUserOptIn(boolean optin) {
        sSafeBrowsingUserOptIn = optin;
    }

    // Not meant to be instantiated.
    private AwSafeBrowsingConfigHelper() {}
}
