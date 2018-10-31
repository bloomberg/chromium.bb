// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webapk.shell_apk.h2o;

import android.content.ComponentName;
import android.content.Context;
import android.content.pm.PackageManager;

import org.chromium.webapk.shell_apk.HostBrowserLauncher;
import org.chromium.webapk.shell_apk.HostBrowserLauncherParams;
import org.chromium.webapk.shell_apk.TransparentLauncherActivity;

/**
 * Handles android.intent.action.MAIN intents if the host browser does not support "showing a
 * transparent window in WebAPK mode till the URL has been loaded".
 */
public class H2OMainActivity extends TransparentLauncherActivity {
    /** Returns whether {@link H2OMainActivity} is enabled. */
    public static boolean checkComponentEnabled(Context context) {
        PackageManager pm = context.getPackageManager();
        ComponentName component = new ComponentName(context, H2OMainActivity.class);
        int enabledSetting = pm.getComponentEnabledSetting(component);
        // Component is enabled by default.
        return enabledSetting == PackageManager.COMPONENT_ENABLED_STATE_ENABLED
                || enabledSetting == PackageManager.COMPONENT_ENABLED_STATE_DEFAULT;
    }

    @Override
    protected void onHostBrowserSelected(HostBrowserLauncherParams params) {
        if (params == null) {
            finish();
            return;
        }

        Context appContext = getApplicationContext();
        HostBrowserLauncher.launch(appContext, params);

        if (H2OLauncher.shouldIntentLaunchSplashActivity(params)) {
            H2OLauncher.changeEnabledComponentsAndKillShellApk(appContext,
                    new ComponentName(appContext, SplashActivity.class), getComponentName());
        }

        finish();
    }
}
