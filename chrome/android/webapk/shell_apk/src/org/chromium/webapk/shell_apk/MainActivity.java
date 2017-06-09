// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webapk.shell_apk;

import android.app.Activity;
import android.content.ActivityNotFoundException;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.ResolveInfo;
import android.net.Uri;
import android.os.AsyncTask;
import android.os.Bundle;
import android.text.TextUtils;
import android.util.Log;

import org.chromium.webapk.lib.common.WebApkConstants;
import org.chromium.webapk.lib.common.WebApkMetaDataKeys;

import java.io.File;
import java.net.MalformedURLException;
import java.net.URL;
import java.util.List;

/**
 * WebAPK's main Activity.
 */
public class MainActivity extends Activity {
    private static final String LAST_RESORT_HOST_BROWSER = "com.android.chrome";
    private static final String LAST_RESORT_HOST_BROWSER_APPLICATION_NAME = "Google Chrome";
    private static final String TAG = "cr_MainActivity";

    /**
     * Name of class which launches browser in WebAPK mode.
     */
    private static final String HOST_BROWSER_LAUNCHER_CLASS_NAME =
            "org.chromium.webapk.lib.runtime_library.HostBrowserLauncher";

    // Action for launching {@link WebappLauncherActivity}. Must stay in sync with
    // {@link WebappLauncherActivity#ACTION_START_WEBAPP}.
    public static final String ACTION_START_WEBAPK =
            "com.google.android.apps.chrome.webapps.WebappManager.ACTION_START_WEBAPP";

    // Must stay in sync with
    // {@link org.chromium.chrome.browser.ShortcutHelper#REUSE_URL_MATCHING_TAB_ELSE_NEW_TAB}.
    private static final String REUSE_URL_MATCHING_TAB_ELSE_NEW_TAB =
            "REUSE_URL_MATCHING_TAB_ELSE_NEW_TAB";

    /**
     * Key for passing app icon id.
     */
    private static final String KEY_APP_ICON_ID = "app_icon_id";

    /**
     * The URL to launch the WebAPK. Used in the case of deep-links (intents from other apps) that
     * fall into the WebAPK scope.
     */
    private String mOverrideUrl;

    /** The "start_url" baked in the AndroidManifest.xml. */
    private String mStartUrl;

    /**
     * Creates install Intent.
     * @param packageName Package to install.
     * @return The intent.
     */
    private static Intent createInstallIntent(String packageName) {
        String marketUrl = "market://details?id=" + packageName;
        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(marketUrl));
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        return intent;
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        Bundle metadata = WebApkUtils.readMetaData(this);
        if (metadata == null) {
            finish();
            return;
        }

        mOverrideUrl = getOverrideUrl();
        mStartUrl = (mOverrideUrl != null) ? mOverrideUrl
                                           : metadata.getString(WebApkMetaDataKeys.START_URL);
        if (mStartUrl == null) {
            finish();
            return;
        }

        Log.v(TAG, "Url of the WebAPK: " + mStartUrl);
        String packageName = getPackageName();
        Log.v(TAG, "Package name of the WebAPK:" + packageName);

        String runtimeHostInPreferences = WebApkUtils.getHostBrowserFromSharedPreference(this);
        String runtimeHost = WebApkUtils.getHostBrowserPackageName(this);
        if (!TextUtils.isEmpty(runtimeHostInPreferences)
                && !runtimeHostInPreferences.equals(runtimeHost)) {
            deleteSharedPref(this);
            deleteInternalStorageAsync();
        }

        if (!TextUtils.isEmpty(runtimeHost)) {
            launchInHostBrowser(runtimeHost);
            finish();
            return;
        }

        String hostUrl = "";
        try {
            hostUrl = new URL(mStartUrl).getHost();
        } catch (MalformedURLException e) {
            Log.e(TAG, "Invalid URL of the WebApk.");
            finish();
            return;
        }

        List<ResolveInfo> infos = WebApkUtils.getInstalledBrowserResolveInfos(getPackageManager());
        if (hasBrowserSupportingWebApks(infos)) {
            showChooseHostBrowserDialog(infos, hostUrl);
        } else {
            showInstallHostBrowserDialog(hostUrl, metadata);
        }
    }

    /** Deletes the SharedPreferences. */
    private void deleteSharedPref(Context context) {
        SharedPreferences sharedPref =
                context.getSharedPreferences(WebApkConstants.PREF_PACKAGE, Context.MODE_PRIVATE);
        SharedPreferences.Editor editor = sharedPref.edit();
        editor.clear();
        editor.apply();
    }

    /** Deletes the internal storage asynchronously. */
    private void deleteInternalStorageAsync() {
        new AsyncTask<Void, Void, Void>() {
            @Override
            protected Void doInBackground(Void... params) {
                deletePath(getCacheDir());
                deletePath(getFilesDir());
                return null;
            }
        }.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
    }

    private void deletePath(File file) {
        if (file == null) return;

        if (file.isDirectory()) {
            File[] children = file.listFiles();
            if (children != null) {
                for (File child : children) {
                    deletePath(child);
                }
            }
        }

        if (!file.delete()) {
            Log.e(TAG, "Failed to delete : " + file.getAbsolutePath());
        }
    }

    private void launchInHostBrowser(String runtimeHost) {
        boolean forceNavigation = false;
        int source = getIntent().getIntExtra(WebApkConstants.EXTRA_SOURCE, 0);
        if (mOverrideUrl != null) {
            if (source == WebApkConstants.SHORTCUT_SOURCE_UNKNOWN) {
                source = WebApkConstants.SHORTCUT_SOURCE_EXTERNAL_INTENT;
            }
            forceNavigation = getIntent().getBooleanExtra(
                    WebApkConstants.EXTRA_WEBAPK_FORCE_NAVIGATION, true);
        }

        // The override URL is non null when the WebAPK is launched from a deep link. The WebAPK
        // should navigate to the URL in the deep link even if the WebAPK is already open.
        Intent intent = new Intent();
        intent.setAction(ACTION_START_WEBAPK);
        intent.setPackage(runtimeHost);
        intent.putExtra(WebApkConstants.EXTRA_URL, mStartUrl)
                .putExtra(WebApkConstants.EXTRA_SOURCE, source)
                .putExtra(WebApkConstants.EXTRA_WEBAPK_PACKAGE_NAME, getPackageName())
                .putExtra(WebApkConstants.EXTRA_WEBAPK_FORCE_NAVIGATION, forceNavigation);

        try {
            startActivity(intent);
        } catch (ActivityNotFoundException e) {
            Log.w(TAG, "Unable to launch browser in WebAPK mode.");
            e.printStackTrace();
        }
    }

    /**
     * Launches the Play Store with the host browser's page.
     */
    private void installBrowser(String hostBrowserPackageName) {
        try {
            startActivity(createInstallIntent(hostBrowserPackageName));
        } catch (ActivityNotFoundException e) {
        }
    }

    /** Retrieves URL from the intent's data. Returns null if a URL could not be retrieved. */
    private String getOverrideUrl() {
        String overrideUrl = getIntent().getDataString();
        if (overrideUrl != null
                && (overrideUrl.startsWith("https:") || overrideUrl.startsWith("http:"))) {
            return overrideUrl;
        }
        return null;
    }

    /** Returns whether there is any installed browser supporting WebAPKs. */
    private static boolean hasBrowserSupportingWebApks(List<ResolveInfo> resolveInfos) {
        List<String> browsersSupportingWebApk = WebApkUtils.getBrowsersSupportingWebApk();
        for (ResolveInfo info : resolveInfos) {
            if (browsersSupportingWebApk.contains(info.activityInfo.packageName)) {
                return true;
            }
        }
        return false;
    }

    /** Shows a dialog to choose the host browser. */
    private void showChooseHostBrowserDialog(List<ResolveInfo> infos, String hostUrl) {
        ChooseHostBrowserDialog.DialogListener listener =
                new ChooseHostBrowserDialog.DialogListener() {
                    @Override
                    public void onHostBrowserSelected(String selectedHostBrowser) {
                        launchInHostBrowser(selectedHostBrowser);
                        WebApkUtils.writeHostBrowserToSharedPref(
                                MainActivity.this, selectedHostBrowser);
                        finish();
                    }
                    @Override
                    public void onQuit() {
                        finish();
                    }
                };
        ChooseHostBrowserDialog.show(this, listener, infos, hostUrl);
    }

    /** Shows a dialog to install the host browser. */
    private void showInstallHostBrowserDialog(String hostUrl, Bundle metadata) {
        String lastResortHostBrowserPackageName =
                metadata.getString(WebApkMetaDataKeys.RUNTIME_HOST);
        String lastResortHostBrowserApplicationName =
                metadata.getString(WebApkMetaDataKeys.RUNTIME_HOST_APPLICATION_NAME);

        if (TextUtils.isEmpty(lastResortHostBrowserPackageName)) {
            // WebAPKs without runtime host specified in the AndroidManifest.xml always install
            // Google Chrome as the default host browser.
            lastResortHostBrowserPackageName = LAST_RESORT_HOST_BROWSER;
            lastResortHostBrowserApplicationName = LAST_RESORT_HOST_BROWSER_APPLICATION_NAME;
        }

        InstallHostBrowserDialog.DialogListener listener =
                new InstallHostBrowserDialog.DialogListener() {
                    @Override
                    public void onConfirmInstall(String packageName) {
                        installBrowser(packageName);
                        WebApkUtils.writeHostBrowserToSharedPref(MainActivity.this, packageName);
                        finish();
                    }
                    @Override
                    public void onConfirmQuit() {
                        finish();
                    }
                };

        InstallHostBrowserDialog.show(this, listener, hostUrl, lastResortHostBrowserPackageName,
                lastResortHostBrowserApplicationName, R.drawable.last_resort_runtime_host_logo);
    }
}
