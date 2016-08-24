// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webapk.shell_apk;

import android.app.Activity;
import android.content.ActivityNotFoundException;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;
import android.net.Uri;
import android.os.Bundle;
import android.util.Log;

import org.chromium.webapk.lib.common.WebApkConstants;
import org.chromium.webapk.lib.common.WebApkMetaDataKeys;

import java.lang.reflect.Method;
import java.net.URISyntaxException;

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

    /**
     * Key for passing app icon id.
     */
    private static final String KEY_APP_ICON_ID = "app_icon_id";

    /**
     * Creates install Intent.
     * @param packageName Package to install.
     * @return The intent.
     */
    public static Intent createInstallIntent(String packageName) {
        String marketUrl = "market://details?id=" + packageName;
        return new Intent(Intent.ACTION_VIEW, Uri.parse(marketUrl));
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        launch();
        finish();
    }

    /**
     * Launches WebAPK.
     */
    private void launch() {
        if (launchHostBrowserInWebApkMode()) {
            return;
        }
        String startUrl = getStartUrl();
        if (startUrl == null) {
            return;
        }
        if (launchBrowser(startUrl)) {
            return;
        }
        installBrowser();
    }

    /**
     * Launches host browser in WebAPK mode.
     * @return True if successful.
     */
    private boolean launchHostBrowserInWebApkMode() {
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
     * Launches browser (not necessarily the host browser).
     * @param startUrl URL to navigate browser to.
     * @return True if successful.
     */
    private boolean launchBrowser(String startUrl) {
        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(startUrl));
        intent.addCategory(Intent.CATEGORY_BROWSABLE);

        // The WebAPK can handle {@link startUrl}. Set a selector to prevent the WebAPK from
        // launching itself.
        try {
            Intent selectorIntent = Intent.parseUri("https://", Intent.URI_INTENT_SCHEME);
            intent.setSelector(selectorIntent);
        } catch (URISyntaxException e) {
            return false;
        }

        // Add extras in case that the URL is launched in Chrome.
        int source =
                getIntent().getIntExtra(WebApkConstants.EXTRA_SOURCE, Intent.URI_INTENT_SCHEME);
        intent.putExtra(REUSE_URL_MATCHING_TAB_ELSE_NEW_TAB, true)
              .putExtra(WebApkConstants.EXTRA_SOURCE, source);

        try {
            startActivity(intent);
        } catch (ActivityNotFoundException e) {
            return false;
        }
        return true;
    }

    /**
     * Launches the Play Store with the host browser's page.
     */
    private void installBrowser() {
        String hostBrowserPackageName = WebApkUtils.getHostBrowserPackageName(this);
        if (hostBrowserPackageName == null) {
            return;
        }

        try {
            startActivity(createInstallIntent(hostBrowserPackageName));
        } catch (ActivityNotFoundException e) {
        }
    }

    /**
     * Returns the URL that the browser should navigate to.
     */
    private String getStartUrl() {
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
        return appInfo.metaData.getString(WebApkMetaDataKeys.START_URL);
    }
}
