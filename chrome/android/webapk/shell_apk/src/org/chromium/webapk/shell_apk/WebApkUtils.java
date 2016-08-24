// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webapk.shell_apk;

import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;
import android.os.Bundle;

import org.chromium.webapk.lib.common.WebApkMetaDataKeys;

/**
 * Contains utility methods for interacting with WebAPKs.
 */
public class WebApkUtils {

    /**
     * Caches the value read from Application Metadata which specifies the host browser's package
     * name.
     */
    private static String sHostPackage;

    /**
     * Returns a Context for the host browser that was specified when building the WebAPK.
     * @param context A context.
     * @return The remote context. Returns null on an error.
     */
    public static Context getHostBrowserContext(Context context) {
        try {
            String hostPackage = getHostBrowserPackageName(context);
            return context.getApplicationContext().createPackageContext(
                            hostPackage,
                            Context.CONTEXT_IGNORE_SECURITY | Context.CONTEXT_INCLUDE_CODE);
        } catch (NameNotFoundException e) {
            e.printStackTrace();
        }
        return null;
    }

    /**
     * Returns the package name for the host browser that was specified when building the WebAPK.
     * @param context A context.
     * @return The package name. Returns null on an error.
     */
    public static String getHostBrowserPackageName(Context context) {
        if (sHostPackage != null) return sHostPackage;
        String hostPackage = null;
        try {
            ApplicationInfo ai = context.getPackageManager().getApplicationInfo(
                    context.getPackageName(), PackageManager.GET_META_DATA);
            Bundle bundle = ai.metaData;
            hostPackage = bundle.getString(WebApkMetaDataKeys.RUNTIME_HOST);
        } catch (NameNotFoundException e) {
            e.printStackTrace();
        }
        // Set {@link sHostPackage} to a non-null value so that the value is computed only once.
        sHostPackage = hostPackage != null ? hostPackage : "";
        return sHostPackage;
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
}
