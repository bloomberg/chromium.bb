// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webapk.shell_apk;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.util.Log;

import java.lang.reflect.Method;

/**
 * WebAPK's main Activity.
 */
public class MainActivity extends Activity {
    private static final String TAG = "cr_MainActivity";

    /**
     * Name of class which launches browser in WebAPK mode.
     */
    private static final String HOST_BROWSER_LAUNCHER_CLASS_NAME =
            "org.chromium.webapk.lib.runtime_library.HostBrowserLauncher";

    /**
     * Key for passing app icon id.
     */
    private static final String KEY_APP_ICON_ID = "app_icon_id";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        launchHostBrowser();
        finish();
    }

    /**
     * Launches host browser in WebAPK mode.
     */
    public void launchHostBrowser() {
        ClassLoader webApkClassLoader = HostBrowserClassLoader.getClassLoaderInstance(
                this, HOST_BROWSER_LAUNCHER_CLASS_NAME);
        if (webApkClassLoader == null) {
            Log.w(TAG, "Unable to create ClassLoader.");
            return;
        }

        try {
            Class<?> hostBrowserLauncherClass =
                    webApkClassLoader.loadClass(HOST_BROWSER_LAUNCHER_CLASS_NAME);
            Method launchMethod = hostBrowserLauncherClass.getMethod(
                    "launch", Context.class, Intent.class, Bundle.class);
            Object hostBrowserLauncherInstance = hostBrowserLauncherClass.newInstance();
            Bundle bundle = new Bundle();
            bundle.putInt(KEY_APP_ICON_ID, R.drawable.app_icon);
            launchMethod.invoke(hostBrowserLauncherInstance, this, getIntent(), bundle);
        } catch (Exception e) {
            Log.w(TAG, "Unable to launch browser in WebAPK mode.");
            e.printStackTrace();
        }
    }
}
