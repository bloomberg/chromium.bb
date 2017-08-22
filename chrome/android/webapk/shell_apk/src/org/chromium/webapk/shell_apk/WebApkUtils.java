// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webapk.shell_apk;

import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;
import android.content.pm.ResolveInfo;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.text.TextUtils;
import android.util.TypedValue;
import android.view.View;

import org.chromium.webapk.lib.common.WebApkConstants;
import org.chromium.webapk.lib.common.WebApkMetaDataKeys;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/**
 * Contains utility methods for interacting with WebAPKs.
 */
public class WebApkUtils {
    public static final String SHARED_PREF_RUNTIME_HOST = "runtime_host";
    public static final int PADDING_DP = 20;

    /**
     * The package names of the channels of Chrome that support WebAPKs. The most preferred one
     * comes first.
     */
    private static List<String> sBrowsersSupportingWebApk = new ArrayList<String>(
            Arrays.asList("com.google.android.apps.chrome", "com.android.chrome", "com.chrome.beta",
                    "com.chrome.dev", "com.chrome.canary"));

    /** Caches the package name of the host browser. */
    private static String sHostPackage;

    /** For testing only. */
    public static void resetCachedHostPackageForTesting() {
        sHostPackage = null;
    }

    /**
     * Returns a list of browsers that support WebAPKs. TODO(hanxi): Replace this function once we
     * figure out a better way to know which browser supports WebAPKs.
     */
    public static List<String> getBrowsersSupportingWebApk() {
        return sBrowsersSupportingWebApk;
    }

    /**
     * Returns a Context for the host browser that was specified when building the WebAPK.
     * @param context A context.
     * @return The remote context. Returns null on an error.
     */
    public static Context getHostBrowserContext(Context context) {
        try {
            String hostPackage = getHostBrowserPackageName(context);
            return context.getApplicationContext().createPackageContext(
                    hostPackage, Context.CONTEXT_IGNORE_SECURITY | Context.CONTEXT_INCLUDE_CODE);
        } catch (NameNotFoundException e) {
            e.printStackTrace();
        }
        return null;
    }

    /**
     * Returns the package name of the host browser to launch the WebAPK. Also caches the package
     * name in the SharedPreference if it is not null.
     * @param context A context.
     * @return The package name. Returns null on an error.
     */
    public static String getHostBrowserPackageName(Context context) {
        if (sHostPackage == null || !isInstalled(context.getPackageManager(), sHostPackage)) {
            sHostPackage = getHostBrowserPackageNameInternal(context);
            if (sHostPackage != null) {
                writeHostBrowserToSharedPref(context, sHostPackage);
            }
        }

        return sHostPackage;
    }

    /** Returns whether the application is installed. */
    private static boolean isInstalled(PackageManager packageManager, String packageName) {
        if (TextUtils.isEmpty(packageName)) return false;

        try {
            packageManager.getApplicationInfo(packageName, 0);
        } catch (PackageManager.NameNotFoundException e) {
            return false;
        }
        return true;
    }

    /** Returns the <meta-data> value in the Android Manifest for {@link key}. */
    public static String readMetaDataFromManifest(Context context, String key) {
        Bundle metadata = readMetaData(context);
        if (metadata == null) return null;

        return metadata.getString(key);
    }

    /** Returns the <meta-data> in the Android Manifest. */
    public static Bundle readMetaData(Context context) {
        ApplicationInfo ai = null;
        try {
            ai = context.getPackageManager().getApplicationInfo(
                    context.getPackageName(), PackageManager.GET_META_DATA);
        } catch (NameNotFoundException e) {
            return null;
        }
        return ai.metaData;
    }

    /**
     * Returns the new intent url, rewrite if necessary.
     * The WebAPK may have been launched as a result of an intent filter for a different
     * scheme or top level domain. Rewrite the scheme and host name to the scope's
     * scheme and host name in this case, and append orginal intent url if |loggedIntentUrlParam| is
     * set.
     */
    public static String rewriteIntentUrlIfNecessary(String startUrl, Bundle metadata) {
        String returnUrl = startUrl;
        String scopeUrl = metadata.getString(WebApkMetaDataKeys.SCOPE);
        if (!TextUtils.isEmpty(scopeUrl)) {
            Uri parsedStartUrl = Uri.parse(startUrl);
            Uri parsedScope = Uri.parse(scopeUrl);

            if (!parsedStartUrl.getScheme().equals(parsedScope.getScheme())
                    || !parsedStartUrl.getEncodedAuthority().equals(
                               parsedScope.getEncodedAuthority())) {
                Uri.Builder returnUrlBuilder =
                        parsedStartUrl.buildUpon()
                                .scheme(parsedScope.getScheme())
                                .encodedAuthority(parsedScope.getEncodedAuthority());

                String loggedIntentUrlParam =
                        metadata.getString(WebApkMetaDataKeys.LOGGED_INTENT_URL_PARAM);
                if (loggedIntentUrlParam != null && !TextUtils.isEmpty(loggedIntentUrlParam)) {
                    String orginalUrl = Uri.encode(startUrl).toString();
                    returnUrlBuilder.appendQueryParameter(loggedIntentUrlParam, orginalUrl);
                }

                returnUrl = returnUrlBuilder.build().toString();
            }
        }
        return returnUrl;
    }

    /**
     * Returns the package name of the host browser to launch the WebAPK, or null if we did not find
     * one.
     */
    private static String getHostBrowserPackageNameInternal(Context context) {
        Set<String> installedBrowsers = getInstalledBrowsers(context.getPackageManager());
        if (installedBrowsers.isEmpty()) {
            return null;
        }

        // Gets the package name of the host browser if it is stored in the SharedPreference.
        String cachedHostBrowser = getHostBrowserFromSharedPreference(context);
        if (!TextUtils.isEmpty(cachedHostBrowser)
                && installedBrowsers.contains(cachedHostBrowser)) {
            return cachedHostBrowser;
        }

        // Gets the package name of the host browser if it is specified in AndroidManifest.xml.
        String hostBrowserFromManifest =
                readMetaDataFromManifest(context, WebApkMetaDataKeys.RUNTIME_HOST);
        if (!TextUtils.isEmpty(hostBrowserFromManifest)) {
            if (installedBrowsers.contains(hostBrowserFromManifest)) {
                return hostBrowserFromManifest;
            }
            return null;
        }

        // Gets the package name of the default browser on the Android device.
        // TODO(hanxi): Investigate the best way to know which browser supports WebAPKs.
        String defaultBrowser = getDefaultBrowserPackageName(context.getPackageManager());
        if (!TextUtils.isEmpty(defaultBrowser) && installedBrowsers.contains(defaultBrowser)
                && sBrowsersSupportingWebApk.contains(defaultBrowser)) {
            return defaultBrowser;
        }

        // If there is only one browser supporting WebAPK, and we can't decide which browser to use
        // by looking up cache, metadata and default browser, open with that browser.
        int availableBrowserCounter = 0;
        String lastSupportedBrowser = null;
        for (String packageName : installedBrowsers) {
            if (availableBrowserCounter > 1) break;
            if (sBrowsersSupportingWebApk.contains(packageName)) {
                availableBrowserCounter++;
                lastSupportedBrowser = packageName;
            }
        }
        if (availableBrowserCounter == 1) {
            return lastSupportedBrowser;
        }
        return null;
    }

    /** Returns a list of ResolveInfo for all of the installed browsers. */
    public static List<ResolveInfo> getInstalledBrowserResolveInfos(PackageManager packageManager) {
        Intent browserIntent = new Intent(Intent.ACTION_VIEW, Uri.parse("http://"));
        return packageManager.queryIntentActivities(browserIntent, PackageManager.MATCH_ALL);
    }

    /**
     * Returns the uid for the host browser that was specified when building the WebAPK.
     * @param context A context.
     * @return The application uid. Returns -1 on an error.
     */
    public static int getHostBrowserUid(Context context) {
        String hostPackageName = getHostBrowserPackageName(context);
        if (hostPackageName == null) {
            return -1;
        }
        try {
            PackageManager packageManager = context.getPackageManager();
            ApplicationInfo appInfo = packageManager.getApplicationInfo(
                    hostPackageName, PackageManager.GET_META_DATA);
            return appInfo.uid;
        } catch (NameNotFoundException e) {
            e.printStackTrace();
        }
        return -1;
    }

    /** Returns the package name of the host browser cached in the SharedPreferences. */
    public static String getHostBrowserFromSharedPreference(Context context) {
        SharedPreferences sharedPref =
                context.getSharedPreferences(WebApkConstants.PREF_PACKAGE, Context.MODE_PRIVATE);
        return sharedPref.getString(SHARED_PREF_RUNTIME_HOST, null);
    }

    /** Returns a set of package names of all the installed browsers on the device. */
    private static Set<String> getInstalledBrowsers(PackageManager packageManager) {
        List<ResolveInfo> resolvedInfos = getInstalledBrowserResolveInfos(packageManager);

        Set<String> packagesSupportingWebApks = new HashSet<String>();
        for (ResolveInfo info : resolvedInfos) {
            packagesSupportingWebApks.add(info.activityInfo.packageName);
        }
        return packagesSupportingWebApks;
    }

    /** Returns the package name of the default browser on the Android device. */
    private static String getDefaultBrowserPackageName(PackageManager packageManager) {
        Intent browserIntent = new Intent(Intent.ACTION_VIEW, Uri.parse("http://"));
        ResolveInfo resolveInfo =
                packageManager.resolveActivity(browserIntent, PackageManager.MATCH_DEFAULT_ONLY);
        if (resolveInfo == null || resolveInfo.activityInfo == null) return null;

        return resolveInfo.activityInfo.packageName;
    }

    /**
     * Writes the package name of the host browser to the SharedPreferences. If the host browser is
     * different than the previous one stored, delete the SharedPreference before storing the new
     * host browser.
     */
    public static void writeHostBrowserToSharedPref(Context context, String hostPackage) {
        if (TextUtils.isEmpty(hostPackage)) return;

        SharedPreferences sharedPref =
                context.getSharedPreferences(WebApkConstants.PREF_PACKAGE, Context.MODE_PRIVATE);
        SharedPreferences.Editor editor = sharedPref.edit();
        editor.putString(SHARED_PREF_RUNTIME_HOST, hostPackage);
        editor.apply();
    }

    /** Converts a dp value to a px value. */
    public static int dpToPx(Context context, int value) {
        return Math.round(TypedValue.applyDimension(
                TypedValue.COMPLEX_UNIT_DIP, value, context.getResources().getDisplayMetrics()));
    }

    /**
     * Android uses padding_left under API level 17 and uses padding_start after that.
     * If we set the padding in resource file, android will create duplicated resource xml
     * with the padding to be different.
     */
    @SuppressWarnings("deprecation")
    public static void setPadding(
            View view, Context context, int start, int top, int end, int bottom) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
            view.setPaddingRelative(dpToPx(context, start), dpToPx(context, top),
                    dpToPx(context, end), dpToPx(context, bottom));
        } else {
            view.setPadding(dpToPx(context, start), dpToPx(context, top), dpToPx(context, end),
                    dpToPx(context, bottom));
        }
    }
}
