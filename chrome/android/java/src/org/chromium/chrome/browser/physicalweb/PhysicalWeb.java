// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.physicalweb;

import android.content.Context;
import android.content.SharedPreferences;
import android.os.AsyncTask;
import android.os.Build;

import org.chromium.base.ContextUtils;
import org.chromium.chrome.browser.ChromeApplication;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.privacy.PrivacyPreferencesManager;

/**
 * This class provides the basic interface to the Physical Web feature.
 */
public class PhysicalWeb {
    public static final int OPTIN_NOTIFY_MAX_TRIES = 1;
    private static final String PREF_PHYSICAL_WEB_NOTIFY_COUNT = "physical_web_notify_count";
    private static final String FEATURE_NAME = "PhysicalWeb";
    private static final int MIN_ANDROID_VERSION = 18;

    /**
     * Evaluate whether the environment is one in which the Physical Web should
     * be enabled.
     * @return true if the PhysicalWeb should be enabled
     */
    public static boolean featureIsEnabled() {
        return ChromeFeatureList.isEnabled(FEATURE_NAME)
                && Build.VERSION.SDK_INT >= MIN_ANDROID_VERSION;
    }

    /**
     * Checks whether the Physical Web preference is switched to On.
     *
     * @param context An instance of android.content.Context
     * @return boolean {@code true} if the preference is On.
     */
    public static boolean isPhysicalWebPreferenceEnabled(Context context) {
        return PrivacyPreferencesManager.getInstance(context).isPhysicalWebEnabled();
    }

    /**
     * Checks whether the Physical Web onboard flow is active and the user has
     * not yet elected to either enable or decline the feature.
     *
     * @param context An instance of android.content.Context
     * @return boolean {@code true} if onboarding is complete.
     */
    public static boolean isOnboarding(Context context) {
        return PrivacyPreferencesManager.getInstance(context).isPhysicalWebOnboarding();
    }

    /**
     * Start the Physical Web feature.
     * At the moment, this only enables URL discovery over BLE.
     * @param application An instance of {@link ChromeApplication}, used to get the
     * appropriate PhysicalWebBleClient implementation.
     */
    public static void startPhysicalWeb(ChromeApplication application) {
        PhysicalWebBleClient physicalWebBleClient = PhysicalWebBleClient.getInstance(application);
        physicalWebBleClient.backgroundSubscribe();
        clearUrlsAsync(application);
    }

    /**
     * Stop the Physical Web feature.
     * @param application An instance of {@link ChromeApplication}, used to get the
     * appropriate PhysicalWebBleClient implementation.
     */
    public static void stopPhysicalWeb(ChromeApplication application) {
        PhysicalWebBleClient physicalWebBleClient = PhysicalWebBleClient.getInstance(application);
        physicalWebBleClient.backgroundUnsubscribe();
        clearUrlsAsync(application);
    }

    /**
     * Increments a value tracking how many times we've shown the Physical Web
     * opt-in notification.
     *
     * @param context An instance of android.content.Context
     */
    public static void recordOptInNotification(Context context) {
        SharedPreferences sharedPreferences =
                ContextUtils.getAppSharedPreferences();
        int value = sharedPreferences.getInt(PREF_PHYSICAL_WEB_NOTIFY_COUNT, 0);
        sharedPreferences.edit().putInt(PREF_PHYSICAL_WEB_NOTIFY_COUNT, value + 1).apply();
    }

    /**
     * Gets the current count of how many times a high-priority opt-in notification
     * has been shown.
     *
     * @param context An instance of android.content.Context
     * @return an integer representing the high-priority notifification display count.
     */
    public static int getOptInNotifyCount(Context context) {
        SharedPreferences sharedPreferences =
                ContextUtils.getAppSharedPreferences();
        return sharedPreferences.getInt(PREF_PHYSICAL_WEB_NOTIFY_COUNT, 0);
    }

    /**
     * Perform various Physical Web operations that should happen on startup.
     * @param application An instance of {@link ChromeApplication}.
     */
    public static void onChromeStart(ChromeApplication application) {
        // The PhysicalWebUma calls in this method should be called only when the native library is
        // loaded.  This is always the case on chrome startup.
        if (featureIsEnabled()
                && (isPhysicalWebPreferenceEnabled(application) || isOnboarding(application))) {
            startPhysicalWeb(application);
            PhysicalWebUma.uploadDeferredMetrics(application);
        } else {
            stopPhysicalWeb(application);
        }
    }

    private static void clearUrlsAsync(final Context context) {
        Runnable task = new Runnable() {
            @Override
            public void run() {
                UrlManager.getInstance(context).clearUrls();
            }
        };
        AsyncTask.THREAD_POOL_EXECUTOR.execute(task);
    }
}
