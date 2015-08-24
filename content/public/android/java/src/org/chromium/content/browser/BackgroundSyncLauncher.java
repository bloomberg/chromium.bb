// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.content.Context;
import android.content.SharedPreferences;
import android.os.AsyncTask;
import android.preference.PreferenceManager;

import org.chromium.base.VisibleForTesting;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

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
     * Callback for {@link #shouldLaunchWhenNextOnline}. The run method is invoked on the UI thread.
     */
    public static interface ShouldLaunchCallback { public void run(Boolean shouldLaunch); }

    /**
     * Returns whether the browser should be launched when the device next goes online.
     * This is set by C++ and reset to false each time {@link BackgroundSyncLauncher}'s singleton is
     * created (the native browser is started). This call is asynchronous and will run the callback
     * on the UI thread when complete.
     * @param context The application context.
     * @param sharedPreferences The shared preferences.
     */
    protected static void shouldLaunchWhenNextOnline(
            final Context context, final ShouldLaunchCallback callback) {
        new AsyncTask<Void, Void, Boolean>() {
            @Override
            protected Boolean doInBackground(Void... params) {
                SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(context);
                return prefs.getBoolean(PREF_BACKGROUND_SYNC_LAUNCH_NEXT_ONLINE, false);
            }
            @Override
            protected void onPostExecute(Boolean shouldLaunch) {
                callback.run(shouldLaunch);
            }
        }.execute();
    }

    /**
     * Set interest (or disinterest) in launching the browser the next time the device goes online
     * after the browser closes. On creation of the {@link BackgroundSyncLauncher} class (on browser
     * start) this value is reset to false. This is set by C++ and reset to false each time
     * {@link BackgroundSyncLauncher}'s singleton is created (the native browser is started). This
     * call is asynchronous.
     */
    @VisibleForTesting
    @CalledByNative
    protected void setLaunchWhenNextOnline(final Context context, final boolean shouldLaunch) {
        new AsyncTask<Void, Void, Void>() {
            @Override
            protected Void doInBackground(Void... params) {
                SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(context);
                prefs.edit()
                        .putBoolean(PREF_BACKGROUND_SYNC_LAUNCH_NEXT_ONLINE, shouldLaunch)
                        .apply();
                return null;
            }
        }.execute();
    }

    /**
     * True if the native browser has started and created an instance of {@link
     * BackgroundSyncLauncher}.
     */
    protected static boolean hasInstance() {
        return sInstance != null;
    }

    private BackgroundSyncLauncher(Context context) {
        setLaunchWhenNextOnline(context, false);
    }
}