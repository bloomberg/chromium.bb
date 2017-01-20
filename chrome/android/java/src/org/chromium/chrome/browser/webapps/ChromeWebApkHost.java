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
import android.text.TextUtils;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ChromeVersionInfo;
import org.chromium.chrome.browser.preferences.ChromePreferenceManager;
import org.chromium.components.variations.VariationsAssociatedData;
import org.chromium.webapk.lib.client.WebApkValidator;

/**
 * Contains functionality needed for Chrome to host WebAPKs.
 */
public class ChromeWebApkHost {
    /** Flag to enable installing WebAPKs using Google Play. */
    private static final String PLAY_INSTALL = "play_install";

    private static final String TAG = "ChromeWebApkHost";

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

    /** Return whether installing WebAPKs using Google Play is enabled. */
    public static boolean canUseGooglePlayToInstallWebApk() {
        if (!isEnabled()) return false;
        return TextUtils.equals(VariationsAssociatedData.getVariationParamValue(
                ChromeFeatureList.IMPROVED_A2HS, PLAY_INSTALL), "true");
    }

    @CalledByNative
    private static boolean areWebApkEnabled() {
        return ChromeWebApkHost.isEnabled();
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

        if (ChromeFeatureList.isEnabled(ChromeFeatureList.IMPROVED_A2HS)
                && !canUseGooglePlayToInstallWebApk()
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
}
