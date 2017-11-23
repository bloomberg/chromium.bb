// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import android.app.Application;
import android.content.Context;
import android.content.ContextWrapper;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.res.AssetManager;
import android.os.Build;
import android.preference.PreferenceManager;

import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.MainDex;

/**
 * This class provides Android application context related utility methods.
 */
@JNINamespace("base::android")
@MainDex
public class ContextUtils {
    private static final String TAG = "ContextUtils";
    private static Context sApplicationContext;

    /**
     * Initialization-on-demand holder. This exists for thread-safe lazy initialization.
     */
    private static class Holder {
        // Not final for tests.
        private static SharedPreferences sSharedPreferences = fetchAppSharedPreferences();
    }

    /**
     * Get the Android application context.
     *
     * Under normal circumstances there is only one application context in a process, so it's safe
     * to treat this as a global. In WebView it's possible for more than one app using WebView to be
     * running in a single process, but this mechanism is rarely used and this is not the only
     * problem in that scenario, so we don't currently forbid using it as a global.
     *
     * Do not downcast the context returned by this method to Application (or any subclass). It may
     * not be an Application object; it may be wrapped in a ContextWrapper. The only assumption you
     * may make is that it is a Context whose lifetime is the same as the lifetime of the process.
     */
    public static Context getApplicationContext() {
        return sApplicationContext;
    }

    /**
     * Initializes the java application context.
     *
     * This should be called exactly once early on during startup, before native is loaded and
     * before any other clients make use of the application context through this class.
     *
     * @param appContext The application context.
     */
    public static void initApplicationContext(Context appContext) {
        // Conceding that occasionally in tests, native is loaded before the browser process is
        // started, in which case the browser process re-sets the application context.
        if (sApplicationContext != null && sApplicationContext != appContext) {
            throw new RuntimeException("Attempting to set multiple global application contexts.");
        }
        initJavaSideApplicationContext(appContext);
    }

    /**
     * Only called by the static holder class and tests.
     *
     * @return The application-wide shared preferences.
     */
    private static SharedPreferences fetchAppSharedPreferences() {
        return PreferenceManager.getDefaultSharedPreferences(sApplicationContext);
    }

    /**
     * This is used to ensure that we always use the application context to fetch the default shared
     * preferences. This avoids needless I/O for android N and above. It also makes it clear that
     * the app-wide shared preference is desired, rather than the potentially context-specific one.
     *
     * @return application-wide shared preferences.
     */
    public static SharedPreferences getAppSharedPreferences() {
        return Holder.sSharedPreferences;
    }

    /**
     * Occasionally tests cannot ensure the application context doesn't change between tests (junit)
     * and sometimes specific tests has its own special needs, initApplicationContext should be used
     * as much as possible, but this method can be used to override it.
     *
     * @param appContext The new application context.
     */
    @VisibleForTesting
    public static void initApplicationContextForTests(Context appContext) {
        // ApplicationStatus.initialize should be called to setup activity tracking for tests
        // that use Robolectric and set the application context manually. Instead of changing all
        // tests that do so, the call was put here instead.
        // TODO(mheikal): Require param to be of type Application
        if (appContext instanceof Application) {
            ApplicationStatus.initialize((Application) appContext);
        }
        initJavaSideApplicationContext(appContext);
        Holder.sSharedPreferences = fetchAppSharedPreferences();
    }

    /**
     * Starts a service with {@code Intent}.  This will be started with the foreground expectation
     * on Android O and higher.
     * TODO(crbug.com/769683): Temporary measure until we roll the support library to 26.1.0.
     *
     * @param context Context to start Service from.
     * @param intent The description of the Service to start.
     *
     * @see {@link android.support.v4.content.ContextCompat#startForegroundService(Context,Intent)}
     * @see Context#startForegroundService(Intent)
     * @see Context#startService(Intent)
     */
    public static void startForegroundService(Context context, Intent intent) {
        if (Build.VERSION.SDK_INT >= 26) {
            context.startForegroundService(intent);
        } else {
            context.startService(intent);
        }
    }

    private static void initJavaSideApplicationContext(Context appContext) {
        if (appContext == null) {
            throw new RuntimeException("Global application context cannot be set to null.");
        }
        sApplicationContext = appContext;
    }

    /**
     * In most cases, {@link Context#getAssets()} can be used directly. Modified resources are
     * used downstream and are set up on application startup, and this method provides access to
     * regular assets before that initialization is complete.
     *
     * This method should ONLY be used for accessing files within the assets folder.
     *
     * @return Application assets.
     */
    public static AssetManager getApplicationAssets() {
        Context context = getApplicationContext();
        while (context instanceof ContextWrapper) {
            context = ((ContextWrapper) context).getBaseContext();
        }
        return context.getAssets();
    }
}
