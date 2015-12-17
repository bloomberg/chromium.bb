// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.physicalweb;

import android.content.Context;
import android.os.AsyncTask;

import org.chromium.base.CommandLine;
import org.chromium.chrome.browser.ChromeApplication;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.ChromeVersionInfo;
import org.chromium.chrome.browser.preferences.privacy.PrivacyPreferencesManager;

/**
 * This class provides the basic interface to the Physical Web feature.
 */
public class PhysicalWeb {
    /**
     * Evaluate whether the environment is one in which the Physical Web should
     * be enabled.
     * @return true if the PhysicalWeb should be enabled
     */
    public static boolean featureIsEnabled() {
        boolean allowedChannel =
                ChromeVersionInfo.isLocalBuild() || ChromeVersionInfo.isDevBuild();
        boolean switchEnabled =
                CommandLine.getInstance().hasSwitch(ChromeSwitches.ENABLE_PHYSICAL_WEB);
        return allowedChannel && switchEnabled;
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
     * Evaluate whether the Physical Web should be enabled when the application starts.
     *
     * @param context An instance of android.content.Context
     * @return true if the Physical Web should be started at launch
     */
    public static boolean shouldStartOnLaunch(Context context) {
        // TODO(mattreynolds): start for onboarding
        return featureIsEnabled() && isPhysicalWebPreferenceEnabled(context);
    }

    /**
     * Start the Physical Web feature.
     * At the moment, this only enables URL discovery over BLE.
     * @param application An instance of {@link ChromeApplication}, used to get the
     * appropriate PhysicalWebBleClient implementation.
     */
    public static void startPhysicalWeb(ChromeApplication application) {
        PhysicalWebBleClient physicalWebBleClient = PhysicalWebBleClient.getInstance(application);
        physicalWebBleClient.subscribe();
        clearUrlsAsync(application);
    }

    /**
     * Stop the Physical Web feature.
     * @param application An instance of {@link ChromeApplication}, used to get the
     * appropriate PhysicalWebBleClient implementation.
     */
    public static void stopPhysicalWeb(ChromeApplication application) {
        PhysicalWebBleClient physicalWebBleClient = PhysicalWebBleClient.getInstance(application);
        physicalWebBleClient.unsubscribe();
        clearUrlsAsync(application);
    }

    /**
     * Upload the collected UMA stats.
     * This method should be called only when the native library is loaded.
     * @param context A valid instance of Context.
     */
    public static void uploadDeferredMetrics(final Context context) {
        PhysicalWebUma.uploadDeferredMetrics(context);
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
