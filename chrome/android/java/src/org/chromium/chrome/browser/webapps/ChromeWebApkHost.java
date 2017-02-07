// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.os.StrictMode;
import android.provider.Settings;
import android.support.v7.app.AlertDialog;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeApplication;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ChromeVersionInfo;
import org.chromium.chrome.browser.externalauth.ExternalAuthUtils;
import org.chromium.chrome.browser.externalauth.UserRecoverableErrorHandler;
import org.chromium.chrome.browser.preferences.ChromePreferenceManager;
import org.chromium.webapk.lib.client.WebApkValidator;

/**
 * Contains functionality needed for Chrome to host WebAPKs.
 */
public class ChromeWebApkHost {
    private static final String TAG = "ChromeWebApkHost";

    /** Whether installing WebAPks from Google Play is possible. */
    private static Boolean sCanUseGooglePlayInstall;

    private static Boolean sEnabledForTesting;

    public static void init() {
        WebApkValidator.initWithBrowserHostSignature(ChromeWebApkHostSignature.EXPECTED_SIGNATURE);
    }

    public static void initForTesting(boolean enabled) {
        sEnabledForTesting = enabled;
    }

    public static boolean isEnabled() {
        if (sEnabledForTesting != null) return sEnabledForTesting;

        return isEnabledInPrefs();
    }

    // Returns whether updating the WebAPK is enabled.
    public static boolean areUpdatesEnabled() {
        if (!isEnabled()) return false;

        // Updating a WebAPK without going through Google Play requires "installation from unknown
        // sources" to be enabled. It is confusing for a user to see a dialog asking them to enable
        // "installation from unknown sources" when they are in the middle of using the WebAPK (as
        // opposed to after requesting to add a WebAPK to the homescreen).
        return installingFromUnknownSourcesAllowed() || canUseGooglePlayToInstallWebApk();
    }

    /**
     * Initializes {@link sCanUseGooglePlayInstall}. It checks whether:
     * 1) WebAPKs are enabled.
     * 2) Google Play Service is available on the device.
     * 3) Google Play install is enabled by Chrome.
     * 4) Google Play is up-to-date and with gServices flags turned on.
     * It calls the Google Play Install API to update {@link sCanUseGooglePlayInstall}
     * asynchronously.
     */
    public static void initCanUseGooglePlayToInstallWebApk() {
        if (!isGooglePlayInstallEnabledByChromeFeature()
                || !ExternalAuthUtils.getInstance().canUseGooglePlayServices(
                        ContextUtils.getApplicationContext(),
                        new UserRecoverableErrorHandler.Silent())) {
            sCanUseGooglePlayInstall = false;
            return;
        }

        ChromeApplication application = (ChromeApplication) ContextUtils.getApplicationContext();
        GooglePlayWebApkInstallDelegate delegate = application.getGooglePlayWebApkInstallDelegate();
        if (delegate == null) {
            sCanUseGooglePlayInstall = false;
            return;
        }

        Callback<Boolean> callback = new Callback<Boolean>() {
            @Override
            public void onResult(Boolean success) {
                sCanUseGooglePlayInstall = success;
            }
        };
        delegate.canInstallWebApk(callback);
    }

    /**
     * Returns whether installing WebAPKs from Google Play is possible.
     * If {@link sCanUseGooglePlayInstall} hasn't been set yet, it returns false immediately and
     * calls the Google Play Install API to update {@link sCanUseGooglePlayInstall} asynchronously.
     */
    public static boolean canUseGooglePlayToInstallWebApk() {
        if (sCanUseGooglePlayInstall == null) {
            sCanUseGooglePlayInstall = false;
            initCanUseGooglePlayToInstallWebApk();
        }
        return sCanUseGooglePlayInstall;
    }

    /**
     * Returns whether Google Play install is enabled by Chrome. Does not check whether installing
     * from Google Play is possible.
     */
    public static boolean isGooglePlayInstallEnabledByChromeFeature() {
        return isEnabled() && LibraryLoader.isInitialized()
                && nativeCanUseGooglePlayToInstallWebApk();
    }

    /**
     * Returns whether installing WebAPKs is possible either from "unknown resources" or Google
     * Play.
     */
    @CalledByNative
    private static boolean canInstallWebApk() {
        return isEnabled()
                && (canUseGooglePlayToInstallWebApk() || nativeCanInstallFromUnknownSources());
    }

    /**
     * Check the cached value to figure out if the feature is enabled. We have to use the cached
     * value because native library may not yet been loaded.
     * @return Whether the feature is enabled.
     */
    private static boolean isEnabledInPrefs() {
        // Will go away once the feature is enabled for everyone by default.
        StrictMode.ThreadPolicy oldPolicy = StrictMode.allowThreadDiskReads();
        try {
            return ChromePreferenceManager.getInstance(
                    ContextUtils.getApplicationContext()).getCachedWebApkRuntimeEnabled();
        } finally {
            StrictMode.setThreadPolicy(oldPolicy);
        }
    }

    /**
     * Show dialog warning user that "installation from unknown sources" is required by the WebAPK
     * experiment if the user enabled "Improved Add to Home screen" via chrome://flags.
     */
    public static void launchWebApkRequirementsDialogIfNeeded(Context context) {
        // Show dialog on Canary & Dev. Installation via "unknown sources" is disabled via
        // variations on other channels.
        if (!ChromeVersionInfo.isCanaryBuild() && !ChromeVersionInfo.isDevBuild()) return;

        // Update cached state. {@link #isEnabled()} and {@link #canUseGooglePlayToInstallWebApk()}
        // need the state to be up to date.
        cacheEnabledStateForNextLaunch();

        if (isEnabled() && !canUseGooglePlayToInstallWebApk()
                && !installingFromUnknownSourcesAllowed()) {
            showUnknownSourcesNeededDialog(context);
        }
    }

    /**
     * Once native is loaded we can consult the command-line (set via about:flags) and also finch
     * state to see if we should enable WebAPKs.
     */
    public static void cacheEnabledStateForNextLaunch() {
        ChromePreferenceManager preferenceManager =
                ChromePreferenceManager.getInstance(ContextUtils.getApplicationContext());

        boolean wasEnabled = isEnabledInPrefs();
        boolean isEnabled = ChromeFeatureList.isEnabled(ChromeFeatureList.IMPROVED_A2HS);
        if (isEnabled != wasEnabled) {
            Log.d(TAG, "WebApk setting changed (%s => %s)", wasEnabled, isEnabled);
            preferenceManager.setCachedWebApkRuntimeEnabled(isEnabled);
        }
    }

    /**
     * Returns whether the user has enabled installing apps from sources other than the Google Play
     * Store.
     */
    private static boolean installingFromUnknownSourcesAllowed() {
        Context applicationContext = ContextUtils.getApplicationContext();
        try {
            return Settings.Secure.getInt(applicationContext.getContentResolver(),
                           Settings.Secure.INSTALL_NON_MARKET_APPS)
                    == 1;
        } catch (Settings.SettingNotFoundException e) {
            return false;
        }
    }

    /**
     * Show dialog warning user that "installation from unknown sources" is required by the WebAPK
     * experiment.
     */
    private static void showUnknownSourcesNeededDialog(final Context context) {
        AlertDialog.Builder builder = new AlertDialog.Builder(context);
        builder.setTitle(R.string.webapk_unknown_sources_dialog_title);
        builder.setMessage(R.string.webapk_unknown_sources_dialog_message);
        builder.setPositiveButton(R.string.webapk_unknown_sources_settings_button,
                new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialog, int id) {
                        // Open Android Security settings.
                        Intent intent = new Intent(Settings.ACTION_SECURITY_SETTINGS);
                        context.startActivity(intent);
                    }
                });
        builder.setNegativeButton(android.R.string.cancel, new DialogInterface.OnClickListener() {
            @Override
            public void onClick(DialogInterface dialog, int id) {}
        });
        AlertDialog dialog = builder.create();
        dialog.show();
    }

    private static native boolean nativeCanUseGooglePlayToInstallWebApk();
    private static native boolean nativeCanInstallFromUnknownSources();
}
