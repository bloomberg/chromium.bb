// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.physicalweb;

import android.annotation.TargetApi;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothManager;
import android.content.Context;
import android.os.AsyncTask;
import android.os.Build;

import org.chromium.base.ContextUtils;
import org.chromium.base.SysUtils;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlService;
import org.chromium.components.location.LocationUtils;

/**
 * This class provides the basic interface to the Physical Web feature.
 */
public class PhysicalWeb {
    public static final int OPTIN_NOTIFY_MAX_TRIES = 1;
    private static final String FEATURE_NAME = "PhysicalWeb";
    private static final int MIN_ANDROID_VERSION = 18;

    /**
     * Evaluates whether the environment is one in which the Physical Web should
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
     * @return boolean {@code true} if the preference is On.
     */
    public static boolean isPhysicalWebPreferenceEnabled() {
        return false;
    }

    /**
     * Checks whether the Physical Web onboard flow is active and the user has
     * not yet elected to either enable or decline the feature.
     *
     * @return boolean {@code true} if onboarding is complete.
     */
    public static boolean isOnboarding() {
        return false;
    }

    /**
     * Performs various Physical Web operations that should happen on startup.
     */
    public static void onChromeStart() {
        // In the case that the user has disabled our flag and restarted, this is a minimal code
        // path to disable our subscription to Nearby.
        if (!featureIsEnabled()) {
            return;
        }

        updateScans();
        // The PhysicalWebUma call in this method should be called only when the native library
        // is loaded.  This is always the case on chrome startup.
        PhysicalWebUma.uploadDeferredMetrics();

        // We can remove this block after M60.
        new AsyncTask<Void, Void, Void>() {
            @Override
            protected Void doInBackground(Void... params) {
                ContextUtils.getAppSharedPreferences().edit()
                        .remove("physical_web_notify_count")
                        .apply();
                return null;
            }
        }.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
    }

    /**
     * Checks if this device should have Physical Web automatically enabled.
     */
    private static boolean shouldAutoEnablePhysicalWeb() {
        LocationUtils locationUtils = LocationUtils.getInstance();
        // LowEndDevice check must come first in order to short circuit more intensive routines
        // potentially run by LocationUtils.
        return !SysUtils.isLowEndDevice() && locationUtils.isSystemLocationSettingEnabled()
                && locationUtils.hasAndroidLocationPermission()
                && TemplateUrlService.getInstance().isDefaultSearchEngineGoogle()
                && !Profile.getLastUsedProfile().isOffTheRecord();
    }

    /**
     * Check if bluetooth is on and enabled.
     */
    @TargetApi(Build.VERSION_CODES.LOLLIPOP)
    public static boolean bluetoothIsEnabled() {
        Context context = ContextUtils.getApplicationContext();
        BluetoothManager bluetoothManager =
                (BluetoothManager) context.getSystemService(Context.BLUETOOTH_SERVICE);
        BluetoothAdapter bluetoothAdapter = bluetoothManager.getAdapter();
        return bluetoothAdapter != null && bluetoothAdapter.isEnabled();
    }

    /**
     * Check if the device bluetooth hardware supports BLE advertisements.
     */
    @TargetApi(Build.VERSION_CODES.LOLLIPOP)
    public static boolean hasBleAdvertiseCapability() {
        Context context = ContextUtils.getApplicationContext();
        BluetoothManager bluetoothManager =
                (BluetoothManager) context.getSystemService(Context.BLUETOOTH_SERVICE);
        BluetoothAdapter bluetoothAdapter = bluetoothManager.getAdapter();
        return bluetoothAdapter != null && bluetoothAdapter.getBluetoothLeAdvertiser() != null;
    }

    /**
     * Examines the environment in order to decide whether we should begin or end a scan.
     */
    public static void updateScans() {}
}
