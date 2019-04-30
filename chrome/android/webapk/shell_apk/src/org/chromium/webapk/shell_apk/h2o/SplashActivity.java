// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webapk.shell_apk.h2o;

import android.app.Activity;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.os.Bundle;
import android.os.SystemClock;
import android.widget.FrameLayout;

import org.chromium.webapk.lib.common.WebApkMetaDataKeys;
import org.chromium.webapk.lib.common.WebApkMetaDataUtils;
import org.chromium.webapk.lib.common.splash.SplashLayout;
import org.chromium.webapk.shell_apk.HostBrowserLauncher;
import org.chromium.webapk.shell_apk.HostBrowserLauncherParams;
import org.chromium.webapk.shell_apk.LaunchHostBrowserSelector;
import org.chromium.webapk.shell_apk.R;
import org.chromium.webapk.shell_apk.WebApkUtils;

/** Displays splash screen. */
public class SplashActivity extends Activity {
    private static final String SAVED_INSTANCE_STATE_WAS_BROWSER_LAUNCHED = "wasBrowserLaunched";

    private boolean mWasBrowserLaunched;
    private boolean mFinishOnResume;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        final long activityStartTimeMs = SystemClock.elapsedRealtime();
        super.onCreate(savedInstanceState);

        if (savedInstanceState != null) {
            // The activity was killed by the Android OOM killer. If the activity was recreated as a
            // result of getting a deep link intent, onNewIntent() will be called prior to
            // onResume(). Otherwise, assume that the activity was recreated as a result of the
            // browser activity finishing.
            mFinishOnResume = true;
            mWasBrowserLaunched =
                    savedInstanceState.getBoolean(SAVED_INSTANCE_STATE_WAS_BROWSER_LAUNCHED);
        }

        showSplashScreen();
        selectHostBrowser(activityStartTimeMs);
    }

    @Override
    public void onNewIntent(Intent intent) {
        super.onNewIntent(intent);
        setIntent(intent);

        mFinishOnResume = false;

        selectHostBrowser(-1);
    }

    @Override
    public void onResume() {
        super.onResume();

        if (mFinishOnResume && mWasBrowserLaunched) {
            WebApkUtils.finishAndRemoveTask(this);
            return;
        }
        mFinishOnResume = true;
    }

    @Override
    public void onSaveInstanceState(Bundle outState) {
        outState.putBoolean(SAVED_INSTANCE_STATE_WAS_BROWSER_LAUNCHED, mWasBrowserLaunched);
    }

    private void selectHostBrowser(final long activityStartTimeMs) {
        new LaunchHostBrowserSelector(this).selectHostBrowser(
                new LaunchHostBrowserSelector.Callback() {
                    @Override
                    public void onBrowserSelected(
                            String hostBrowserPackageName, boolean dialogShown) {
                        if (hostBrowserPackageName == null) {
                            finish();
                            return;
                        }
                        HostBrowserLauncherParams params =
                                HostBrowserLauncherParams.createForIntent(SplashActivity.this,
                                        getIntent(), hostBrowserPackageName, dialogShown,
                                        activityStartTimeMs);
                        onHostBrowserSelected(params);
                    }
                });
    }

    private void showSplashScreen() {
        Bundle metadata = WebApkUtils.readMetaData(this);
        Resources resources = getResources();

        Bitmap icon = WebApkUtils.decodeBitmapFromDrawable(resources, R.drawable.splash_icon);
        @SplashLayout.IconClassification
        int iconClassification = SplashLayout.classifyIcon(resources, icon, false);

        FrameLayout layout = new FrameLayout(this);
        setContentView(layout);

        int backgroundColor = WebApkUtils.getColor(resources, R.color.background_color);
        SplashLayout.createLayout(this, layout, icon, false /* isIconAdaptive */,
                iconClassification, resources.getString(R.string.name),
                WebApkUtils.shouldUseLightForegroundOnBackground(backgroundColor));

        int themeColor = (int) WebApkMetaDataUtils.getLongFromMetaData(
                metadata, WebApkMetaDataKeys.THEME_COLOR, Color.BLACK);
        WebApkUtils.setStatusBarColor(
                getWindow(), WebApkUtils.getDarkenedColorForStatusBar(themeColor));

        int orientation = WebApkUtils.computeScreenLockOrientationFromMetaData(this, metadata);
        if (orientation != ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED) {
            setRequestedOrientation(orientation);
        }
    }

    /** Called once the host browser has been selected. */
    private void onHostBrowserSelected(HostBrowserLauncherParams params) {
        if (params == null) {
            finish();
            return;
        }

        Context appContext = getApplicationContext();

        if (!H2OLauncher.shouldIntentLaunchSplashActivity(params)) {
            HostBrowserLauncher.launch(appContext, params);
            H2OLauncher.changeEnabledComponentsAndKillShellApk(appContext,
                    new ComponentName(appContext, H2OMainActivity.class),
                    new ComponentName(appContext, H2OOpaqueMainActivity.class));
            finish();
            return;
        }

        mWasBrowserLaunched = true;
        H2OLauncher.launch(this, params);
    }
}
