// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webapk.shell_apk;

import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.os.IBinder;
import android.util.Log;

import org.chromium.webapk.lib.common.WebApkUtils;

import java.io.File;
import java.lang.reflect.Constructor;
import java.util.Scanner;

/**
 * Shell class for services provided by WebAPK to Chrome. Extracts code with implementation of
 * services from .dex file in Chrome APK.
 */
public class WebApkServiceFactory extends Service {
    private static final String TAG = "cr_WebApkServiceFactory";

    /**
     * Name of the class with IBinder API implementation.
     */
    private static final String WEBAPK_SERVICE_IMPL_CLASS_NAME =
            "org.chromium.webapk.lib.runtime_library.WebApkServiceImpl";

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

    /**
     * Key for passing id of icon to represent WebAPK notifications in status bar.
     */
    private static final String KEY_SMALL_ICON_ID = "small_icon_id";

    /**
     * Key for passing package name of only process allowed to call the service's methods.
     */
    private static final String KEY_HOST_BROWSER_PACKAGE = "host_browser_package";

    /*
     * ClassLoader for loading {@link WEBAPK_SERVICE_IMPL_CLASS_NAME}. Static so that all
     * {@link WebApkServiceFactory} service instatiations use the same ClassLoader during the app's
     * lifetime.
     */
    private static ClassLoader sClassLoader;

    @Override
    public IBinder onBind(Intent intent) {
        ClassLoader webApkClassLoader = getClassLoaderInstance(this);
        if (webApkClassLoader == null) {
            Log.w(TAG, "Unable to create ClassLoader.");
            return null;
        }

        try {
            Class<?> webApkServiceImplClass =
                    webApkClassLoader.loadClass(WEBAPK_SERVICE_IMPL_CLASS_NAME);
            Constructor<?> webApkServiceImplConstructor =
                    webApkServiceImplClass.getConstructor(Context.class, Bundle.class);
            String hostPackageName = WebApkUtils.getHostBrowserPackageName(this);
            Bundle bundle = new Bundle();
            bundle.putInt(KEY_SMALL_ICON_ID, R.drawable.app_icon);
            bundle.putString(KEY_HOST_BROWSER_PACKAGE, hostPackageName);
            return (IBinder) webApkServiceImplConstructor.newInstance(new Object[] {this, bundle});
        } catch (Exception e) {
            Log.w(TAG, "Unable to create WebApkServiceImpl.");
            e.printStackTrace();
            return null;
        }
    }

    /**
     * Gets / creates ClassLoader for loading {@link WEBAPK_SERVICE_IMPL_CLASS_NAME}.
     * @param context WebAPK's context.
     * @return The ClassLoader.
     */
    private static ClassLoader getClassLoaderInstance(Context context) {
        if (sClassLoader == null) {
            sClassLoader = createClassLoader(context);
        }
        return sClassLoader;
    }

    /**
     * Creates ClassLoader for loading {@link WEBAPK_SERVICE_IMPL_CLASS_NAME}.
     * @param context WebAPK's context.
     * @return The ClassLoader.
     */
    private static ClassLoader createClassLoader(Context context) {
        Context remoteContext = WebApkUtils.getHostBrowserContext(context);
        if (remoteContext == null) {
            Log.w(TAG, "Failed to get remote context.");
            return null;
        }

        SharedPreferences preferences = context.getSharedPreferences(PREF_PACKAGE, MODE_PRIVATE);

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
        return DexLoader.load(remoteContext, dexAssetName, WEBAPK_SERVICE_IMPL_CLASS_NAME,
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
