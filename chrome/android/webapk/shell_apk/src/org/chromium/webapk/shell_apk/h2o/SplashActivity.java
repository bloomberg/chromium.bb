// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webapk.shell_apk.h2o;

import android.app.Activity;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.os.Bundle;
import android.os.SystemClock;
import android.view.View;
import android.view.ViewTreeObserver;

import org.chromium.webapk.lib.common.WebApkMetaDataKeys;
import org.chromium.webapk.lib.common.WebApkMetaDataUtils;
import org.chromium.webapk.shell_apk.HostBrowserLauncher;
import org.chromium.webapk.shell_apk.HostBrowserLauncherParams;
import org.chromium.webapk.shell_apk.LaunchHostBrowserSelector;
import org.chromium.webapk.shell_apk.WebApkUtils;

import java.io.ByteArrayOutputStream;
import java.io.IOException;

/** Displays splash screen. */
public class SplashActivity extends Activity {
    private static final String SAVED_INSTANCE_STATE_WAS_BROWSER_LAUNCHED = "wasBrowserLaunched";

    /** Whether {@link mSplashView} was laid out. */
    private boolean mSplashViewLaidOut;

    /** Task to screenshot and encode splash. */
    @SuppressWarnings("NoAndroidAsyncTaskCheck")
    private android.os.AsyncTask mScreenshotSplashTask;

    private View mSplashView;
    private HostBrowserLauncherParams mParams;
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
    public void onDestroy() {
        SplashContentProvider.clearCache();
        if (mScreenshotSplashTask != null) {
            mScreenshotSplashTask.cancel(false);
            mScreenshotSplashTask = null;
        }
        super.onDestroy();
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
        int themeColor = (int) WebApkMetaDataUtils.getLongFromMetaData(
                metadata, WebApkMetaDataKeys.THEME_COLOR, Color.BLACK);
        WebApkUtils.setStatusBarColor(
                getWindow(), WebApkUtils.getDarkenedColorForStatusBar(themeColor));

        int orientation = WebApkUtils.computeScreenLockOrientationFromMetaData(this, metadata);
        if (orientation != ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED) {
            setRequestedOrientation(orientation);
        }

        mSplashView = SplashUtils.createSplashView(this);
        mSplashView.getViewTreeObserver().addOnGlobalLayoutListener(
                new ViewTreeObserver.OnGlobalLayoutListener() {
                    @Override
                    public void onGlobalLayout() {
                        if (mSplashView.getWidth() == 0 || mSplashView.getHeight() == 0) return;

                        mSplashView.getViewTreeObserver().removeOnGlobalLayoutListener(this);
                        mSplashViewLaidOut = true;
                        maybeScreenshotSplash();
                    }
                });
        setContentView(mSplashView);
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

        mParams = params;
        maybeScreenshotSplash();
    }

    /**
     * Screenshots {@link mSplashView} if:
     * - host browser was selected
     * AND
     * - splash view was laid out
     */
    private void maybeScreenshotSplash() {
        if (mParams == null || !mSplashViewLaidOut) return;

        screenshotAndEncodeSplashInBackground();
    }

    /**
     * Launches the host browser on top of {@link SplashActivity}.
     * @param splashPngEncoded PNG-encoded screenshot of {@link mSplashView}.
     */
    private void launch(byte[] splashPngEncoded) {
        SplashContentProvider.cache(
                this, splashPngEncoded, mSplashView.getWidth(), mSplashView.getHeight());
        mWasBrowserLaunched = true;
        H2OLauncher.launch(this, mParams);
        mParams = null;
    }

    /**
     * Screenshots and PNG-encodes {@link mSplashView} on a background thread.
     */
    @SuppressWarnings("NoAndroidAsyncTaskCheck")
    private void screenshotAndEncodeSplashInBackground() {
        final Bitmap bitmap = SplashUtils.screenshotView(
                mSplashView, SplashContentProvider.MAX_TRANSFER_SIZE_BYTES);
        if (bitmap == null) {
            launch(null);
            return;
        }

        mScreenshotSplashTask =
                new android.os
                        .AsyncTask<Void, Void, byte[]>() {
                            @Override
                            protected byte[] doInBackground(Void... args) {
                                try (ByteArrayOutputStream out = new ByteArrayOutputStream()) {
                                    bitmap.compress(Bitmap.CompressFormat.PNG, 100, out);
                                    return out.toByteArray();
                                } catch (IOException e) {
                                }
                                return null;
                            }

                            @Override
                            protected void onPostExecute(byte[] splashPngEncoded) {
                                mScreenshotSplashTask = null;
                                launch(splashPngEncoded);
                            }

                            // Do nothing if task was cancelled.
                        }
                        .executeOnExecutor(android.os.AsyncTask.THREAD_POOL_EXECUTOR);
    }
}
