// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webapk.shell_apk;

import android.content.Context;
import android.content.SharedPreferences;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.util.Log;

import org.chromium.webapk.lib.common.WebApkUtils;

import java.io.File;
import java.util.Scanner;

/**
 * Creates ClassLoader for WebAPK-specific dex file in Chrome APK's assets.
 */
public class HostBrowserClassLoader {
    private static final String TAG = "cr_HostBrowserClassLoader";

    /**
     * Name of the shared preferences file.
     */
    private static final String PREF_PACKAGE = "org.chromium.webapk.shell_apk";

    /**
     * Name of the shared preference for Chrome's version code.
     */
    private static final String REMOTE_VERSION_CODE_PREF =
            "org.chromium.webapk.shell_apk.version_code";

    /**
     * Name of the shared preference for the version number of the dynamically loaded dex.
     */
    private static final String RUNTIME_DEX_VERSION_PREF =
            "org.chromium.webapk.shell_apk.dex_version";

    /*
     * ClassLoader for WebAPK dex. Static so that the same ClassLoader is used for app's lifetime.
     */
    private static ClassLoader sClassLoader;

    /**
     * Guards all access to {@link sClassLoader}.
     */
    private static final Object sLock = new Object();

    /**
     * Gets / creates ClassLoader for WebAPK dex.
     * @param context WebAPK's context.
     * @param canaryClassname Class to load to check that ClassLoader is valid.
     * @return The ClassLoader.
     */
    public static ClassLoader getClassLoaderInstance(Context context, String canaryClassName) {
        synchronized (sLock) {
            if (sClassLoader == null) {
                sClassLoader = createClassLoader(context, canaryClassName);
            }
        }
        return sClassLoader;
    }

    /**
     * Creates ClassLoader for WebAPK dex.
     * @param context WebAPK's context.
     * @param canaryClassName Class to load to check that ClassLoader is valid.
     * @return The ClassLoader.
     */
    private static ClassLoader createClassLoader(Context context, String canaryClassName) {
        Context remoteContext = WebApkUtils.getHostBrowserContext(context);
        if (remoteContext == null) {
            Log.w(TAG, "Failed to get remote context.");
            return null;
        }

        SharedPreferences preferences =
                context.getSharedPreferences(PREF_PACKAGE, Context.MODE_PRIVATE);

        int runtimeDexVersion = preferences.getInt(RUNTIME_DEX_VERSION_PREF, -1);
        int newRuntimeDexVersion = checkForNewRuntimeDexVersion(preferences, remoteContext);
        if (newRuntimeDexVersion == -1) {
            newRuntimeDexVersion = runtimeDexVersion;
        }
        File localDexDir = context.getDir("dex", Context.MODE_PRIVATE);
        if (newRuntimeDexVersion != runtimeDexVersion) {
            Log.w(TAG, "Delete cached dex files.");
            DexLoader.deleteCachedDexes(localDexDir);
        }

        String dexAssetName = WebApkUtils.getRuntimeDexName(newRuntimeDexVersion);
        File remoteDexFile =
                new File(remoteContext.getDir("dex", Context.MODE_PRIVATE), dexAssetName);
        return DexLoader.load(remoteContext, dexAssetName, canaryClassName,
                remoteDexFile, localDexDir);
    }

    /**
     * Checks if there is a new "runtime dex" version number. If there is a new version number,
     * updates SharedPreferences.
     * @param preferences WebAPK's SharedPreferences.
     * @param remoteContext
     * @return The new "runtime dex" version number. -1 if there is no new version number.
     */
    private static int checkForNewRuntimeDexVersion(
            SharedPreferences preferences, Context remoteContext) {
        // The "runtime dex" version only changes when {@link remoteContext}'s APK version code
        // changes. Checking the APK's version code is less expensive than reading from the APK's
        // assets.
        PackageInfo remotePackageInfo = null;
        try {
            remotePackageInfo = remoteContext.getPackageManager().getPackageInfo(
                    remoteContext.getPackageName(), 0);
        } catch (PackageManager.NameNotFoundException e) {
            Log.e(TAG, "Failed to get remote package info.");
            return -1;
        }

        int cachedRemoteVersionCode = preferences.getInt(REMOTE_VERSION_CODE_PREF, -1);
        if (cachedRemoteVersionCode == remotePackageInfo.versionCode) {
            return -1;
        }

        int runtimeDexVersion = readAssetContentsToInt(remoteContext, "webapk_dex_version.txt");
        SharedPreferences.Editor editor = preferences.edit();
        editor.putInt(REMOTE_VERSION_CODE_PREF, remotePackageInfo.versionCode);
        editor.putInt(RUNTIME_DEX_VERSION_PREF, runtimeDexVersion);
        editor.apply();
        return runtimeDexVersion;
    }

    /**
     * Returns the first integer in an asset file's contents.
     * @param context
     * @param assetName The name of the asset.
     * @return The first integer.
     */
    private static int readAssetContentsToInt(Context context, String assetName) {
        Scanner scanner = null;
        int value = -1;
        try {
            scanner = new Scanner(context.getAssets().open(assetName));
            value = scanner.nextInt();
            scanner.close();
        } catch (Exception e) {
        } finally {
            if (scanner != null) {
                try {
                    scanner.close();
                } catch (Exception e) {
                }
            }
        }
        return value;
    }
}
