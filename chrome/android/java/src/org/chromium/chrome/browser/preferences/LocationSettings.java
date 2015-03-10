// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences;

import android.content.Context;
import android.content.Intent;
import android.location.LocationManager;
import android.provider.Settings;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.CalledByNative;
import org.chromium.base.ThreadUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.annotations.SuppressFBWarnings;
import org.chromium.chrome.browser.ChromiumApplication;

/**
 * Provides methods for querying Android system-wide location settings as well as Chrome's internal
 * location setting.
 *
 * This class should be used only on the UI thread.
 */
public class LocationSettings {

    private static LocationSettings sInstance;

    protected final Context mContext;

    /**
     * Don't use this; use getInstance() instead. This should be used only by the Application inside
     * of createLocationSettings().
     */
    protected LocationSettings(Context context) {
        mContext = context;
    }

    /**
     * Returns the singleton instance of LocationSettings, creating it if needed.
     */
    @SuppressFBWarnings("LI_LAZY_INIT_STATIC")
    public static LocationSettings getInstance() {
        ThreadUtils.assertOnUiThread();
        if (sInstance == null) {
            ChromiumApplication application =
                    (ChromiumApplication) ApplicationStatus.getApplicationContext();
            sInstance = application.createLocationSettings();
        }
        return sInstance;
    }

    /**
     * Returns true if location is enabled system-wide.
     */
    @CalledByNative
    private static boolean staticIsSystemLocationSettingEnabled() {
        return LocationSettings.getInstance().isSystemLocationSettingEnabled();
    }

    /**
     * Returns true if location is enabled system-wide and the Chrome location setting is enabled.
     */
    public boolean areAllLocationSettingsEnabled() {
        return isChromeLocationSettingEnabled() && isSystemLocationSettingEnabled();
    }

    /**
     * Returns whether Chrome's user-configurable location setting is enabled.
     */
    public boolean isChromeLocationSettingEnabled() {
        return PrefServiceBridge.getInstance().isAllowLocationEnabled();
    }

    /**
     * Returns whether location is enabled system-wide, i.e. whether Chrome itself is able to access
     * location.
     */
    public boolean isSystemLocationSettingEnabled() {
        LocationManager locationManager =
                (LocationManager) mContext.getSystemService(Context.LOCATION_SERVICE);
        return (locationManager.isProviderEnabled(LocationManager.NETWORK_PROVIDER)
                || locationManager.isProviderEnabled(LocationManager.GPS_PROVIDER));
    }

    /**
     * Returns an intent to launch Android Location Settings.
     */
    public Intent getSystemLocationSettingsIntent() {
        Intent i = new Intent(Settings.ACTION_LOCATION_SOURCE_SETTINGS);
        i.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        return i;
    }

    @VisibleForTesting
    public static void setInstanceForTesting(LocationSettings instance) {
        sInstance = instance;
    }
}
