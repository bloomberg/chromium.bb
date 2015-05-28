// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.content.Context;
import android.content.SharedPreferences;
import android.preference.PreferenceManager;

import org.chromium.base.CalledByNative;
import org.chromium.base.JNINamespace;
import org.chromium.base.VisibleForTesting;

/**
 * The {@link BackgroundSyncLauncher} singleton is created and owned by the C++ browser. It
 * registers interest in waking up the browser the next time the device goes online after the
 *browser closes via the {@link #setLaunchWhenNextOnline} method.
 *
 * Thread model: This class is to be run on the UI thread only.
 */
@JNINamespace("content")
public class BackgroundSyncLauncher {
    static final String PREF_BACKGROUND_SYNC_LAUNCH_NEXT_ONLINE = "bgsync_launch_next_online";

    // The instance of BackgroundSyncLauncher currently owned by a C++
    // BackgroundSyncLauncherAndroid, if any. If it is non-null then the browser is running.
    private static BackgroundSyncLauncher sInstance;

    private final SharedPreferences mSharedPreferences;

    /**
     * Create a BackgroundSyncLauncher object, which is owned by C++.
     * @param context The app context.
     */
    @VisibleForTesting
    @CalledByNative
    protected static BackgroundSyncLauncher create(Context context) {
        if (sInstance != null) {
            throw new IllegalStateException("Already instantiated");
        }

        sInstance = new BackgroundSyncLauncher(context);
        return sInstance;
    }

    /**
     * Called when the C++ counterpart is deleted.
     */
    @VisibleForTesting
    @CalledByNative
    protected void destroy() {
        assert sInstance == this;
        sInstance = null;
    }

    /**
     * Set interest (or disinterest) in launching the browser the next time the device goes online
     * after the browser closes. On creation of the {@link BackgroundSyncLauncher} class (on browser
     * start) this value is reset to false.
     */
    @VisibleForTesting
    @CalledByNative
    protected void setLaunchWhenNextOnline(boolean shouldLaunch) {
        mSharedPreferences.edit()
                .putBoolean(PREF_BACKGROUND_SYNC_LAUNCH_NEXT_ONLINE, shouldLaunch)
                .apply();
    }

    /**
     * Returns whether the browser should be launched when the device next goes online.
     * This is set by C++ and reset to false each time {@link BackgroundSyncLauncher}'s singleton is
     * created (the native browser is started).
     * @param sharedPreferences The shared preferences.
     */
    protected static boolean shouldLaunchWhenNextOnline(SharedPreferences sharedPreferences) {
        return sharedPreferences.getBoolean(PREF_BACKGROUND_SYNC_LAUNCH_NEXT_ONLINE, false);
    }

    /**
     * True if the native browser has started and created an instance of {@link
     * BackgroundSyncLauncher}.
     */
    protected static boolean hasInstance() {
        return sInstance != null;
    }

    private BackgroundSyncLauncher(Context context) {
        mSharedPreferences = PreferenceManager.getDefaultSharedPreferences(context);
        setLaunchWhenNextOnline(false);
    }
}