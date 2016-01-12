// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.webkit.ValueCallback;

import org.chromium.base.Log;
import org.chromium.base.annotations.JNINamespace;

/**
 * Java twin of the homonymous C++ class. The Java side is only responsible for
 * switching metrics on and off. Since the setting is a platform feature, it
 * must be obtained through PlatformServiceBridge.
 */
@JNINamespace("android_webview")
public class AwMetricsServiceClient {
    private static final String TAG = "AwMetricsServiceCli-";

    // Individual apps can use this meta-data tag in their manifest to opt out of metrics
    // reporting. See https://developer.android.com/reference/android/webkit/WebView.html
    private static final String OPT_OUT_META_DATA_STR = "android.webkit.WebView.MetricsOptOut";

    private static boolean isAppOptedOut(Context applicationContext) {
        try {
            ApplicationInfo info = applicationContext.getPackageManager().getApplicationInfo(
                    applicationContext.getPackageName(), PackageManager.GET_META_DATA);
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

    public AwMetricsServiceClient(Context applicationContext) {
        if (isAppOptedOut(applicationContext)) {
            return;
        }

        // Check if the user has consented.
        PlatformServiceBridge.getInstance(applicationContext)
                .setMetricsSettingListener(new ValueCallback<Boolean>() {
                    public void onReceiveValue(Boolean enabled) {
                        nativeSetMetricsEnabled(enabled);
                    }
                });
    }

    public static native void nativeSetMetricsEnabled(boolean enabled);
}
