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

import org.chromium.base.BuildInfo;
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
                            isHarmfulAppDetectionEnabled(appContext));
                }
            });
        }
    }

    private static boolean shouldEnableSafeBrowsingSupport(Context appContext) {
        return CommandLine.getInstance().hasSwitch(AwSwitches.WEBVIEW_ENABLE_SAFEBROWSING_SUPPORT)
                || (BuildInfo.isAtLeastO()
                           && !appHasMetadataKeyValue(appContext, OPT_IN_META_DATA_STR, false));
    }

    private static boolean appHasMetadataKeyValue(Context appContext, String key, boolean value) {
        try {
            ApplicationInfo info = appContext.getPackageManager().getApplicationInfo(
                    appContext.getPackageName(), PackageManager.GET_META_DATA);
            if (info.metaData == null) {
                // null means no such tag was found.
                return false;
            }
            return info.metaData.containsKey(key) ? info.metaData.getBoolean(key) == value : false;
        } catch (PackageManager.NameNotFoundException e) {
            // This should never happen.
            Log.e(TAG, "App could not find itself by package name!");
            return false;
        }
    }

    @SuppressLint("NewApi") // android.provider.Settings.Global#getInt requires API level 17
    private static boolean isHarmfulAppDetectionEnabled(Context applicationContext) {
        // Determine if the "Improve harmful app detection" functionality is enabled in
        // Android->System->Google->Security settings.
        ContentResolver contentResolver = applicationContext.getContentResolver();
        boolean user_consent =
                Settings.Secure.getInt(contentResolver, "package_verifier_user_consent", 1) > 0;
        boolean apk_upload = Settings.Global.getInt(contentResolver, "upload_apk_enable", 1) > 0;
        return user_consent && apk_upload;
    }

    // Not meant to be instantiated.
    private AwSafeBrowsingConfigHelper() {}
}
