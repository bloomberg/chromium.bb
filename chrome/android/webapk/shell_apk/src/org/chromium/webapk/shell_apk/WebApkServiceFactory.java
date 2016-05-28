// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webapk.shell_apk;

import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.os.IBinder;
import android.util.Log;

import org.chromium.webapk.lib.common.WebApkUtils;

import java.io.File;
import java.lang.reflect.Constructor;

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
                    webApkServiceImplClass.getConstructor(Context.class, int.class);
            return (IBinder) webApkServiceImplConstructor.newInstance(
                    new Object[] {this, R.drawable.app_icon});
        } catch (Exception e) {
            Log.w(TAG, "Unable to create WebApkServiceImpl.");
            e.printStackTrace();
            return null;
        }
    }

    /**
     * Gets / creates ClassLaoder for loading {@link WEBAPK_SERVICE_IMPL_CLASS_NAME}.
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

        File localDexDir = context.getDir("dex", Context.MODE_PRIVATE);
        File remoteDexFile =
                new File(remoteContext.getDir("dex", Context.MODE_PRIVATE), "web_apk.dex");
        return DexLoader.load(remoteContext, "web_apk.dex", WEBAPK_SERVICE_IMPL_CLASS_NAME,
                remoteDexFile, localDexDir);
    }
}
