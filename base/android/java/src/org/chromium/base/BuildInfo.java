// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import android.content.Context;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;
import android.os.Build;
import android.os.Build.VERSION;
import android.text.TextUtils;

import org.chromium.base.annotations.CalledByNative;

/**
 * BuildInfo is a utility class providing easy access to {@link PackageInfo} information. This is
 * primarily of use for accessing package information from native code.
 */
public class BuildInfo {
    private static final String TAG = "BuildInfo";
    private static final int MAX_FINGERPRINT_LENGTH = 128;

    private static PackageInfo sBrowserPackageInfo;
    private static boolean sInitialized;

    /** The application name (e.g. "Chrome"). For WebView, this is name of the embedding app. */
    public final String hostPackageLabel;
    /** By default: same as versionCode. For WebView: versionCode of the embedding app. */
    public final int hostVersionCode;
    /** The packageName of Chrome/WebView. Use application context for host app packageName. */
    public final String packageName;
    /** The versionCode of the apk. */
    public final int versionCode;
    /** The versionName of Chrome/WebView. Use application context for host app versionName. */
    public final String versionName;
    /** Result of PackageManager.getInstallerPackageName(). Never null, but may be "". */
    public final String installerPackageName;
    /** The versionCode of Play Services (for crash reporting). */
    public final String gmsVersionCode;
    /** Formatted ABI string (for crash reporting). */
    public final String abiString;
    /** Truncated version of Build.FINGERPRINT (for crash reporting). */
    public final String androidBuildFingerprint;
    /** A string that is different each time the apk changes. */
    public final String extractedFileSuffix;

    private static class Holder { private static BuildInfo sInstance = new BuildInfo(); }

    @CalledByNative
    private static String[] getAll() {
        BuildInfo buildInfo = getInstance();
        String hostPackageName = ContextUtils.getApplicationContext().getPackageName();
        return new String[] {
                Build.BRAND, Build.DEVICE, Build.ID, Build.MANUFACTURER, Build.MODEL,
                String.valueOf(Build.VERSION.SDK_INT), Build.TYPE, Build.BOARD, hostPackageName,
                String.valueOf(buildInfo.hostVersionCode), buildInfo.hostPackageLabel,
                buildInfo.packageName, String.valueOf(buildInfo.versionCode), buildInfo.versionName,
                buildInfo.androidBuildFingerprint, buildInfo.gmsVersionCode,
                buildInfo.installerPackageName, buildInfo.abiString, BuildConfig.FIREBASE_APP_ID,
                buildInfo.extractedFileSuffix,
        };
    }

    private static String nullToEmpty(CharSequence seq) {
        return seq == null ? "" : seq.toString();
    }

    /**
     * @param packageInfo Package for Chrome/WebView (as opposed to host app).
     */
    public static void setBrowserPackageInfo(PackageInfo packageInfo) {
        assert !sInitialized;
        sBrowserPackageInfo = packageInfo;
    }

    public static BuildInfo getInstance() {
        return Holder.sInstance;
    }

    private BuildInfo() {
        sInitialized = true;
        try {
            Context appContext = ContextUtils.getApplicationContext();
            String hostPackageName = appContext.getPackageName();
            PackageManager pm = appContext.getPackageManager();
            PackageInfo pi = pm.getPackageInfo(hostPackageName, 0);
            hostVersionCode = pi.versionCode;
            if (sBrowserPackageInfo != null) {
                packageName = sBrowserPackageInfo.packageName;
                versionCode = sBrowserPackageInfo.versionCode;
                versionName = nullToEmpty(sBrowserPackageInfo.versionName);
                sBrowserPackageInfo = null;
            } else {
                packageName = hostPackageName;
                versionCode = hostVersionCode;
                versionName = nullToEmpty(pi.versionName);
            }

            hostPackageLabel = nullToEmpty(pm.getApplicationLabel(pi.applicationInfo));
            installerPackageName = nullToEmpty(pm.getInstallerPackageName(packageName));

            PackageInfo gmsPackageInfo = null;
            try {
                gmsPackageInfo = pm.getPackageInfo("com.google.android.gms", 0);
            } catch (NameNotFoundException e) {
                Log.d(TAG, "GMS package is not found.", e);
            }
            gmsVersionCode = gmsPackageInfo != null ? String.valueOf(gmsPackageInfo.versionCode)
                                                    : "gms versionCode not available.";

            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
                abiString = TextUtils.join(", ", Build.SUPPORTED_ABIS);
            } else {
                abiString = String.format("ABI1: %s, ABI2: %s", Build.CPU_ABI, Build.CPU_ABI2);
            }

            // Use lastUpdateTime when developing locally, since versionCode does not normally
            // change in this case.
            long version = versionCode > 10 ? versionCode : pi.lastUpdateTime;
            extractedFileSuffix = String.format("@%x", version);

            // The value is truncated, as this is used for crash and UMA reporting.
            androidBuildFingerprint = Build.FINGERPRINT.substring(
                    0, Math.min(Build.FINGERPRINT.length(), MAX_FINGERPRINT_LENGTH));
        } catch (NameNotFoundException e) {
            throw new RuntimeException(e);
        }
    }

    /**
     * Check if this is a debuggable build of Android. Use this to enable developer-only features.
     */
    public static boolean isDebugAndroid() {
        return "eng".equals(Build.TYPE) || "userdebug".equals(Build.TYPE);
    }

    // The markers Begin:BuildCompat and End:BuildCompat delimit code
    // that is autogenerated from Android sources.
    // Begin:BuildCompat P

    /**
     * Checks if the device is running on a pre-release version of Android P or newer.
     * <p>
     * <strong>Note:</strong> This method will return {@code false} on devices running release
     * versions of Android. When Android P is finalized for release, this method will be deprecated
     * and all calls should be replaced with {@code Build.SDK_INT >= Build.VERSION_CODES#P}.
     *
     * @return {@code true} if P APIs are available for use, {@code false} otherwise
     */
    public static boolean isAtLeastP() {
        return VERSION.CODENAME.equals("P") || VERSION.CODENAME.equals("Q");
    }

    /**
     * Checks if the application targets pre-release SDK P
     */
    public static boolean targetsAtLeastP() {
        return isAtLeastP()
                && ContextUtils.getApplicationContext().getApplicationInfo().targetSdkVersion
                == Build.VERSION_CODES.CUR_DEVELOPMENT;
    }

    // End:BuildCompat
}
