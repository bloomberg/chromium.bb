// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.location;

import android.Manifest;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.Process;
import android.provider.Settings;
import android.text.TextUtils;

import org.chromium.base.ThreadUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.annotations.SuppressFBWarnings;

/**
 * Provides methods for querying Chrome's ability to use Android's location services.
 *
 * This class should be used only on the UI thread.
 */
public class LocationUtils {
    // Used to construct sInstance if that's null.
    private static Factory sFactory;

    private static LocationUtils sInstance;

    protected LocationUtils() {}

    /**
     * Returns the singleton instance of LocationSettings, creating it if needed.
     */
    @SuppressFBWarnings("LI_LAZY_INIT_STATIC")
    public static LocationUtils getInstance() {
        ThreadUtils.assertOnUiThread();
        if (sInstance == null) {
            if (sFactory == null) {
                sInstance = new LocationUtils();
            } else {
                sInstance = sFactory.create();
            }
        }
        return sInstance;
    }

    private boolean hasPermission(Context context, String name) {
        return context.checkPermission(name, Process.myPid(), Process.myUid())
                == PackageManager.PERMISSION_GRANTED;
    }

    /**
     * Returns true if Chromium has permission to access location.
     *
     * Check both hasAndroidLocationPermission() and isSystemLocationSettingEnabled() to determine
     * if Chromium's location requests will return results.
     */
    public boolean hasAndroidLocationPermission(Context context) {
        return hasPermission(context, Manifest.permission.ACCESS_COARSE_LOCATION)
                || hasPermission(context, Manifest.permission.ACCESS_FINE_LOCATION);
    }

    /**
     * Returns whether location services are enabled system-wide, i.e. whether any application is
     * able to access location.
     */
    public boolean isSystemLocationSettingEnabled(Context context) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT) {
            return Settings.Secure.getInt(context.getContentResolver(),
                           Settings.Secure.LOCATION_MODE, Settings.Secure.LOCATION_MODE_OFF)
                    != Settings.Secure.LOCATION_MODE_OFF;
        } else {
            return !TextUtils.isEmpty(Settings.Secure.getString(
                    context.getContentResolver(), Settings.Secure.LOCATION_PROVIDERS_ALLOWED));
        }
    }

    /**
     * Returns an intent to launch Android Location Settings.
     */
    public Intent getSystemLocationSettingsIntent() {
        Intent i = new Intent(Settings.ACTION_LOCATION_SOURCE_SETTINGS);
        i.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        return i;
    }

    /**
     * Instantiate this to explain how to create a LocationUtils instance in
     * LocationUtils.getInstance().
     */
    public interface Factory { public LocationUtils create(); }

    /**
     * Call this to use a different subclass of LocationUtils throughout the program.
     * This can be used by embedders in addition to tests.
     */
    @VisibleForTesting
    public static void setFactory(Factory factory) {
        sFactory = factory;
        sInstance = null;
    }
}
