// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webapk.shell_apk;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;
import android.net.Uri;
import android.os.Bundle;
import android.util.Log;

import org.chromium.webapk.lib.common.WebApkConstants;
import org.chromium.webapk.lib.common.WebApkUtils;

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

    // Must stay in sync with
    // {@link org.chromium.chrome.browser.ShortcutHelper#REUSE_URL_MATCHING_TAB_ELSE_NEW_TAB}.
    private static final String REUSE_URL_MATCHING_TAB_ELSE_NEW_TAB =
            "REUSE_URL_MATCHING_TAB_ELSE_NEW_TAB";

    // Key for start URL in Android Manifest.
    private static final String META_DATA_START_URL = "startUrl";

    /**
     * Key for passing app icon id.
     */
    private static final String KEY_APP_ICON_ID = "app_icon_id";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        if (!launchHostBrowserInWebApkMode()) {
            // Launch browser in non-WebAPK mode.
            launchHostBrowser();
        }
        finish();
    }

    /**
     * Launches host browser in WebAPK mode.
     * @return True if successful.
     */
    public boolean launchHostBrowserInWebApkMode() {
        ClassLoader webApkClassLoader = HostBrowserClassLoader.getClassLoaderInstance(
                this, HOST_BROWSER_LAUNCHER_CLASS_NAME);
        if (webApkClassLoader == null) {
            Log.w(TAG, "Unable to create ClassLoader.");
            return false;
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
            return true;
        } catch (Exception e) {
            Log.w(TAG, "Unable to launch browser in WebAPK mode.");
            e.printStackTrace();
            return false;
        }
    }

    /**
     * Launches host browser in non-WebAPK mode.
     */
    public void launchHostBrowser() {
        String startUrl = getStartUrl();
        if (startUrl == null) {
            return;
        }
        int source = getIntent().getIntExtra(WebApkConstants.EXTRA_SOURCE, 0);
        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(startUrl));
        intent.setPackage(WebApkUtils.getHostBrowserPackageName(this));
        intent.putExtra(REUSE_URL_MATCHING_TAB_ELSE_NEW_TAB, true)
              .putExtra(WebApkConstants.EXTRA_SOURCE, source);
        startActivity(intent);
    }

    /**
     * Returns the URL that the browser should navigate to.
     */
    public String getStartUrl() {
        String overrideUrl = getIntent().getDataString();
        if (overrideUrl != null && overrideUrl.startsWith("https:")) {
            return overrideUrl;
        }

        ApplicationInfo appInfo;
        try {
            appInfo = getPackageManager().getApplicationInfo(
                    getPackageName(), PackageManager.GET_META_DATA);
        } catch (NameNotFoundException e) {
            return null;
        }
        return appInfo.metaData.getString(META_DATA_START_URL);
    }
}
