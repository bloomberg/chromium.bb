// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webapk.shell_apk.h2o;

import android.content.ComponentName;
import android.content.Context;
import android.content.pm.PackageManager;
import android.os.Build;

import org.chromium.webapk.shell_apk.HostBrowserLauncherParams;

/** Contains methods for launching host browser where ShellAPK shows the splash screen. */
public class H2OLauncher {
    // Lowest version of Chromium which supports ShellAPK showing the splash screen.
    private static final int MINIMUM_REQUIRED_CHROMIUM_VERSION_NEW_SPLASH = Integer.MAX_VALUE;

    /**
     * Returns whether the main intent should launch SplashActivity.class for the given host browser
     * params.
     */
    public static boolean shouldMainIntentLaunchSplashActivity(HostBrowserLauncherParams params) {
        return params.getHostBrowserMajorChromiumVersion()
                >= MINIMUM_REQUIRED_CHROMIUM_VERSION_NEW_SPLASH
                && Build.VERSION.SDK_INT >= Build.VERSION_CODES.M;
    }

    /**
     * Changes which components are enabled.
     *
     * @param context
     * @param enableComponent Component to enable.
     * @param disableComponent Component to disable.
     */
    public static void changeEnabledComponentsAndKillShellApk(
            Context context, ComponentName enableComponent, ComponentName disableComponent) {
        PackageManager pm = context.getPackageManager();
        // The state change takes seconds if we do not let PackageManager kill the ShellAPK.
        pm.setComponentEnabledSetting(enableComponent,
                PackageManager.COMPONENT_ENABLED_STATE_ENABLED, PackageManager.DONT_KILL_APP);
        pm.setComponentEnabledSetting(
                disableComponent, PackageManager.COMPONENT_ENABLED_STATE_DISABLED, 0);
    }
}
