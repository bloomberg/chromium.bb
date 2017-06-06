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
import android.graphics.drawable.Drawable;
import android.net.Uri;
import android.text.TextUtils;

import org.chromium.webapk.lib.common.WebApkConstants;
import org.chromium.webapk.lib.common.WebApkMetaDataKeys;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.Comparator;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/**
 * Contains utility methods for interacting with WebAPKs.
 */
public class WebApkUtils {
    public static final String SHARED_PREF_RUNTIME_HOST = "runtime_host";

    /**
     * The package names of the channels of Chrome that support WebAPKs. The most preferred one
     * comes first.
     */
    private static List<String> sBrowsersSupportingWebApk = new ArrayList<String>(
            Arrays.asList("com.google.android.apps.chrome", "com.android.chrome", "com.chrome.beta",
                    "com.chrome.dev", "com.chrome.canary"));

    /** Stores information about a potential host browser for the WebAPK. */
    public static class BrowserItem {
        private String mPackageName;
        private CharSequence mApplicationLabel;
        private Drawable mIcon;
        private boolean mSupportsWebApks;

        public BrowserItem(String packageName, CharSequence applicationLabel, Drawable icon,
                boolean supportsWebApks) {
            mPackageName = packageName;
            mApplicationLabel = applicationLabel;
            mIcon = icon;
            mSupportsWebApks = supportsWebApks;
        }

        /** Returns the package name of a browser. */
        public String getPackageName() {
            return mPackageName;
        }

        /** Returns the application name of a browser. */
        public CharSequence getApplicationName() {
            return mApplicationLabel;
        }

        /** Returns a drawable of the browser icon. */
        public Drawable getApplicationIcon() {
            return mIcon;
        }

        /** Returns whether the browser supports WebAPKs. */
        public boolean supportsWebApks() {
            return mSupportsWebApks;
        }
    }

    /**
     * Caches the package name of the host browser. {@link sHostPackage} might refer to a browser
     * which has been uninstalled. A notification can keep the WebAPK process alive after the host
     * browser has been uninstalled.
     */
    private static String sHostPackage;

    /** For testing only. */
    public static void resetCachedHostPackageForTesting() {
        sHostPackage = null;
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
        if (sHostPackage == null) {
            sHostPackage = getHostBrowserPackageNameInternal(context);
            if (sHostPackage != null) {
                writeHostBrowserToSharedPref(context, sHostPackage);
            }
        }

        return sHostPackage;
    }

    /** Returns the <meta-data> value in the Android Manifest for {@link key}. */
    public static String readMetaDataFromManifest(Context context, String key) {
        ApplicationInfo ai = null;
        try {
            ai = context.getPackageManager().getApplicationInfo(
                    context.getPackageName(), PackageManager.GET_META_DATA);
        } catch (NameNotFoundException e) {
            return null;
        }
        return ai.metaData.getString(key);
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

        return null;
    }

    /**
     * Returns a list of browsers to choose host browser from. The list includes all the installed
     * browsers, and if none of the installed browser supports WebAPKs, Chrome will be added to the
     * list as well.
     */
    public static List<BrowserItem> getBrowserInfosForHostBrowserSelection(
            PackageManager packageManager) {
        Intent browserIntent = new Intent(Intent.ACTION_VIEW, Uri.parse("http://"));
        List<ResolveInfo> resolvedActivityList = packageManager.queryIntentActivities(
                browserIntent, PackageManager.MATCH_DEFAULT_ONLY);

        boolean hasBrowserSupportingWebApks = false;
        List<BrowserItem> browsers = new ArrayList<>();
        for (ResolveInfo info : resolvedActivityList) {
            boolean supportsWebApk = false;
            if (sBrowsersSupportingWebApk.contains(info.activityInfo.packageName)) {
                supportsWebApk = true;
                hasBrowserSupportingWebApks = true;
            }
            browsers.add(new BrowserItem(info.activityInfo.packageName,
                    info.loadLabel(packageManager), info.loadIcon(packageManager), supportsWebApk));
        }

        Collections.sort(browsers, new Comparator<BrowserItem>() {
            @Override
            public int compare(BrowserItem a, BrowserItem b) {
                if (a.mSupportsWebApks == b.mSupportsWebApks) {
                    return a.getPackageName().compareTo(b.getPackageName());
                }
                return a.mSupportsWebApks ? -1 : 1;
            }
        });

        if (hasBrowserSupportingWebApks) return browsers;

        // TODO(hanxi): add Chrome's icon to WebAPKs.
        browsers.add(new BrowserItem("com.android.chrome", "Chrome", null, true));
        return browsers;
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
    public static Set<String> getInstalledBrowsers(PackageManager packageManager) {
        Intent browserIntent = new Intent(Intent.ACTION_VIEW, Uri.parse("http://"));
        List<ResolveInfo> resolvedActivityList = packageManager.queryIntentActivities(
                browserIntent, PackageManager.MATCH_DEFAULT_ONLY);

        Set<String> packagesSupportingWebApks = new HashSet<String>();
        for (ResolveInfo info : resolvedActivityList) {
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

    /** Deletes the SharedPreferences. */
    public static void deleteSharedPref(Context context) {
        SharedPreferences sharedPref =
                context.getSharedPreferences(WebApkConstants.PREF_PACKAGE, Context.MODE_PRIVATE);
        SharedPreferences.Editor editor = sharedPref.edit();
        editor.clear();
        editor.apply();
    }
}
