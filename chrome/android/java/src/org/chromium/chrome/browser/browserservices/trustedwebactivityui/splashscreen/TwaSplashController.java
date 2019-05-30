// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.trustedwebactivityui.splashscreen;

import static android.view.ViewGroup.LayoutParams.MATCH_PARENT;

import android.app.Activity;
import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.Matrix;
import android.os.Build;
import android.os.Bundle;
import android.support.customtabs.TrustedWebUtils;
import android.support.customtabs.TrustedWebUtils.SplashScreenParamKey;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;

import org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider;
import org.chromium.chrome.browser.customtabs.TranslucentCustomTabActivity;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.InflationObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.util.ColorUtils;
import org.chromium.chrome.browser.util.IntentUtils;
import org.chromium.chrome.browser.webapps.SplashController;
import org.chromium.chrome.browser.webapps.SplashDelegate;
import org.chromium.chrome.browser.webapps.SplashscreenObserver;
import org.chromium.content_public.browser.ScreenOrientationProvider;
import org.chromium.ui.base.ActivityWindowAndroid;

import javax.inject.Inject;

/**
 * Orchestrates the flow of showing and removing splash screens for apps based on Trusted Web
 * Activities.
 *
 * The flow is as follows:
 * - TWA client app verifies conditions for showing splash screen. If the checks pass, it shows the
 * splash screen immediately.
 * - The client passes the URI to a file with the splash image to
 * {@link android.support.customtabs.CustomTabsService}. The image is decoded and put into
 * {@link SplashImageHolder}.
 * - The client then launches a TWA, at which point the Bitmap is already available.
 * - ChromeLauncherActivity calls {@link #handleIntent}, which starts
 * {@link TranslucentCustomTabActivity} - a CustomTabActivity with translucent style. The
 * translucency is necessary in order to avoid a flash that might be seen when starting the activity
 * before the splash screen is attached.
 * - {@link TranslucentCustomTabActivity} creates an instance of {@link TwaSplashController} which
 * immediately displays the splash screen in an ImageView on top of the rest of view hierarchy.
 * - It also immediately removes the translucency. See comment in {@link SplashController} for more
 * details.
 * - It waits for the page to load, and removes the splash image once first paint (or a failure)
 * occurs.
 *
 * Lifecycle: this class is resolved only once when CustomTabActivity is launched, and is
 * gc-ed when it finishes its job (to that end, it removes all observers it has set).
 * If these lifecycle assumptions change, consider whether @ActivityScope needs to be added.
 */
public class TwaSplashController
        implements InflationObserver, SplashDelegate, SplashscreenObserver {
    private final SplashController mSplashController;
    private final Activity mActivity;
    private final ActivityWindowAndroid mActivityWindowAndroid;
    private final ActivityLifecycleDispatcher mLifecycleDispatcher;
    private final ScreenOrientationProvider mScreenOrientationProvider;
    private final SplashImageHolder mSplashImageCache;
    private final CustomTabIntentDataProvider mIntentDataProvider;

    @Inject
    public TwaSplashController(SplashController splashController, Activity activity,
            ActivityWindowAndroid activityWindowAndroid,
            ActivityLifecycleDispatcher lifecycleDispatcher,
            ScreenOrientationProvider screenOrientationProvider, SplashImageHolder splashImageCache,
            CustomTabIntentDataProvider intentDataProvider) {
        mSplashController = splashController;
        mActivity = activity;
        mActivityWindowAndroid = activityWindowAndroid;
        mLifecycleDispatcher = lifecycleDispatcher;
        mScreenOrientationProvider = screenOrientationProvider;
        mSplashImageCache = splashImageCache;
        mIntentDataProvider = intentDataProvider;

        long splashHideAnimationDurationMs = IntentUtils.safeGetInt(
                getSplashScreenParamsFromIntent(), SplashScreenParamKey.FADE_OUT_DURATION_MS, 0);
        mSplashController.setConfig(
                this, true /* isWindowInitiallyTranslucent */, splashHideAnimationDurationMs);

        mSplashController.addObserver(this);
        lifecycleDispatcher.register(this);

        // Setting the screen orientation while the activity is translucent throws an exception on
        // O (but not on O MR1). Delay setting it.
        if (Build.VERSION.SDK_INT == Build.VERSION_CODES.O) {
            mScreenOrientationProvider.delayOrientationRequests(mActivityWindowAndroid);
        }
    }

    @Override
    public View buildSplashView() {
        Bitmap bitmap = mSplashImageCache.takeImage(mIntentDataProvider.getSession());
        if (bitmap == null) {
            mLifecycleDispatcher.unregister(this);
            return null;
        }
        ImageView splashView = new ImageView(mActivity);
        splashView.setLayoutParams(new ViewGroup.LayoutParams(MATCH_PARENT, MATCH_PARENT));
        splashView.setImageBitmap(bitmap);
        applyCustomizationsToSplashScreenView(splashView);
        return splashView;
    }

    @Override
    public void onSplashHidden(Tab tab, @SplashController.SplashHidesReason int reason,
            long startTimestamp, long endTimestamp) {
        mLifecycleDispatcher.unregister(this); // Unregister to get gc-ed
    }

    @Override
    public boolean shouldWaitForSubsequentPageLoadToHideSplash() {
        return false;
    }

    @Override
    public void onPreInflationStartup() {}

    @Override
    public void onPostInflationStartup() {
        mSplashController.bringSplashBackToFront();
    }

    @Override
    public void onTranslucencyRemoved() {
        mScreenOrientationProvider.runDelayedOrientationRequests(mActivityWindowAndroid);
    }

    @Override
    public void onSplashscreenHidden(long startTimestamp, long endTimestamp) {}

    private void applyCustomizationsToSplashScreenView(ImageView imageView) {
        Bundle params = getSplashScreenParamsFromIntent();

        int backgroundColor =
                IntentUtils.safeGetInt(params, SplashScreenParamKey.BACKGROUND_COLOR, Color.WHITE);
        imageView.setBackgroundColor(ColorUtils.getOpaqueColor(backgroundColor));

        int scaleTypeOrdinal = IntentUtils.safeGetInt(params, SplashScreenParamKey.SCALE_TYPE, -1);
        ImageView.ScaleType[] scaleTypes = ImageView.ScaleType.values();
        ImageView.ScaleType scaleType;
        if (scaleTypeOrdinal < 0 || scaleTypeOrdinal >= scaleTypes.length) {
            scaleType = ImageView.ScaleType.CENTER;
        } else {
            scaleType = scaleTypes[scaleTypeOrdinal];
        }
        imageView.setScaleType(scaleType);

        if (scaleType != ImageView.ScaleType.MATRIX) return;
        float[] matrixValues = IntentUtils.safeGetFloatArray(
                params, SplashScreenParamKey.IMAGE_TRANSFORMATION_MATRIX);
        if (matrixValues == null || matrixValues.length != 9) return;
        Matrix matrix = new Matrix();
        matrix.setValues(matrixValues);
        imageView.setImageMatrix(matrix);
    }

    private Bundle getSplashScreenParamsFromIntent() {
        return mIntentDataProvider.getIntent().getBundleExtra(
                TrustedWebUtils.EXTRA_SPLASH_SCREEN_PARAMS);
    }

    /**
     * Returns true if the intent corresponds to a TWA with a splash screen.
     */
    public static boolean intentIsForTwaWithSplashScreen(Intent intent) {
        boolean isTrustedWebActivity = IntentUtils.safeGetBooleanExtra(
                intent, TrustedWebUtils.EXTRA_LAUNCH_AS_TRUSTED_WEB_ACTIVITY, false);
        boolean requestsSplashScreen = IntentUtils.safeGetParcelableExtra(
                                               intent, TrustedWebUtils.EXTRA_SPLASH_SCREEN_PARAMS)
                != null;
        return isTrustedWebActivity && requestsSplashScreen;
    }

    /**
     * Handles the intent if it should launch a TWA with splash screen.
     * @param activity Activity, from which to start the next one.
     * @param intent Incoming intent.
     * @return Whether the intent was handled.
     */
    public static boolean handleIntent(Activity activity, Intent intent) {
        if (!intentIsForTwaWithSplashScreen(intent)) return false;

        intent.setClassName(activity, TranslucentCustomTabActivity.class.getName());
        intent.addFlags(Intent.FLAG_ACTIVITY_NO_ANIMATION);
        activity.startActivity(intent);
        activity.overridePendingTransition(0, 0);
        return true;
    }
}
