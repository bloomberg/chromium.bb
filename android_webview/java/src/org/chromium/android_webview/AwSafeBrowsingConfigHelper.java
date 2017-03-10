// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.annotation.SuppressLint;
import android.content.ContentResolver;
import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.os.AsyncTask;
import android.provider.Settings;

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

    public static void maybeInitSafeBrowsingFromSettings(final Context appContext) {
        if (AwSafeBrowsingConfigHelper.shouldEnableSafeBrowsingSupport(appContext)) {
            // Assume safebrowsing on by default initially.
            AwContentsStatics.setSafeBrowsingEnabled(true);
            // Fetch Android settings related to safe-browsing in the background.
            AsyncTask.THREAD_POOL_EXECUTOR.execute(new Runnable() {
                @Override
                public void run() {
                    AwContentsStatics.setSafeBrowsingEnabled(
                            isScanDeviceForSecurityThreatsEnabled(appContext));
                }
            });
        }
    }

    private static boolean shouldEnableSafeBrowsingSupport(Context appContext) {
        return CommandLine.getInstance().hasSwitch(AwSwitches.WEBVIEW_ENABLE_SAFEBROWSING_SUPPORT)
                || appHasOptedIn(appContext);
    }

    private static boolean appHasOptedIn(Context appContext) {
        try {
            ApplicationInfo info = appContext.getPackageManager().getApplicationInfo(
                    appContext.getPackageName(), PackageManager.GET_META_DATA);
            if (info.metaData == null) {
                // null means no such tag was found.
                return false;
            }
            return info.metaData.containsKey(OPT_IN_META_DATA_STR)
                    ? info.metaData.getBoolean(OPT_IN_META_DATA_STR)
                    : false;
        } catch (PackageManager.NameNotFoundException e) {
            // This should never happen.
            Log.e(TAG, "App could not find itself by package name!");
            return false;
        }
    }

    @SuppressLint("NewApi") // android.provider.Settings.Global#getInt requires API level 17
    private static boolean isScanDeviceForSecurityThreatsEnabled(Context applicationContext) {
        // Determine if the "Scan device for security threats" functionality is enabled in
        // Android->System->Google->Security settings.
        ContentResolver contentResolver = applicationContext.getContentResolver();
        return Settings.Secure.getInt(contentResolver, "package_verifier_user_consent", 1) > 0;
    }

    // Not meant to be instantiated.
    private AwSafeBrowsingConfigHelper() {}
}
