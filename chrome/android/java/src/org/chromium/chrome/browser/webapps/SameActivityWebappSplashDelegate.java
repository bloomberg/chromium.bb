// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.app.Activity;
import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.util.DisplayMetrics;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ContextUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.NativeInitObserver;
import org.chromium.chrome.browser.metrics.SameActivityWebappUmaCache;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserverRegistrar;
import org.chromium.chrome.browser.util.ColorUtils;
import org.chromium.webapk.lib.common.splash.SplashLayout;

/**
 * Delegate for when splash screen is shown by Chrome (as opposed to by external non-Chrome
 * activity).
 */
public class SameActivityWebappSplashDelegate implements SplashDelegate, NativeInitObserver {
    public static final int HIDE_ANIMATION_DURATION_MS = 300;
    public static final String HISTOGRAM_SPLASHSCREEN_DURATION = "Webapp.Splashscreen.Duration";
    public static final String HISTOGRAM_SPLASHSCREEN_HIDES = "Webapp.Splashscreen.Hides";

    private Activity mActivity;
    private ActivityLifecycleDispatcher mLifecycleDispatcher;
    private TabObserverRegistrar mTabObserverRegistrar;

    /** Whether native was loaded. Native must be loaded in order to record metrics. */
    private boolean mNativeLoaded;

    private WebappInfo mWebappInfo;

    private SameActivityWebappUmaCache mUmaCache;

    private WebApkSplashNetworkErrorObserver mWebApkNetworkErrorObserver;

    public SameActivityWebappSplashDelegate(Activity activity,
            ActivityLifecycleDispatcher lifecycleDispatcher,
            TabObserverRegistrar tabObserverRegistrar) {
        mActivity = activity;
        mLifecycleDispatcher = lifecycleDispatcher;
        mTabObserverRegistrar = tabObserverRegistrar;

        mLifecycleDispatcher.register(this);
    }

    @Override
    public View buildSplashView(WebappInfo webappInfo) {
        mWebappInfo = webappInfo;

        if (mWebappInfo.isForWebApk()) {
            mWebApkNetworkErrorObserver =
                    new WebApkSplashNetworkErrorObserver(mActivity, mWebappInfo.name());
            mTabObserverRegistrar.registerTabObserver(mWebApkNetworkErrorObserver);
        }

        Context context = ContextUtils.getApplicationContext();
        final int backgroundColor = ColorUtils.getOpaqueColor(webappInfo.backgroundColor(
                ApiCompatibilityUtils.getColor(context.getResources(), R.color.webapp_default_bg)));

        ViewGroup splashScreen = new FrameLayout(context);
        splashScreen.setBackgroundColor(backgroundColor);

        if (webappInfo.isForWebApk()) {
            initializeLayout(webappInfo, splashScreen, backgroundColor,
                    ((WebApkInfo) webappInfo).splashIcon());
            return splashScreen;
        }

        WebappDataStorage storage =
                WebappRegistry.getInstance().getWebappDataStorage(webappInfo.id());
        if (storage == null) {
            initializeLayout(webappInfo, splashScreen, backgroundColor, null);
            return splashScreen;
        }

        storage.getSplashScreenImage(new WebappDataStorage.FetchCallback<Bitmap>() {
            @Override
            public void onDataRetrieved(Bitmap splashImage) {
                initializeLayout(webappInfo, splashScreen, backgroundColor, splashImage);
            }
        });
        return splashScreen;
    }

    @Override
    public void onFinishNativeInitialization() {
        mNativeLoaded = true;
        if (mUmaCache != null) mUmaCache.commitMetrics();
    }

    @Override
    public void onSplashHidden(Tab tab, @SplashController.SplashHidesReason int reason,
            long startTimestamp, long endTimestamp) {
        if (mWebApkNetworkErrorObserver != null) {
            mTabObserverRegistrar.unregisterTabObserver(mWebApkNetworkErrorObserver);
            tab.removeObserver(mWebApkNetworkErrorObserver);
            mWebApkNetworkErrorObserver = null;
        }
        mLifecycleDispatcher.unregister(SameActivityWebappSplashDelegate.this);

        recordSplashHiddenUma(reason, startTimestamp, endTimestamp);

        mActivity = null;
    }

    @Override
    public int getSplashHideAnimationDurationMs() {
        return HIDE_ANIMATION_DURATION_MS;
    }

    @Override
    public boolean shouldWaitForSubsequentPageLoadToHideSplash() {
        return mWebApkNetworkErrorObserver != null
                && mWebApkNetworkErrorObserver.isNetworkErrorDialogVisible();
    }

    /** Sets the splash screen layout and sets the splash screen's title and icon. */
    private void initializeLayout(WebappInfo webappInfo, ViewGroup splashScreen,
            int backgroundColor, Bitmap splashImage) {
        Context context = ContextUtils.getApplicationContext();
        Resources resources = context.getResources();

        Bitmap selectedIcon = splashImage;
        boolean selectedIconGenerated = false;
        boolean selectedIconAdaptive = false;
        if (selectedIcon == null) {
            selectedIcon = webappInfo.icon();
            selectedIconGenerated = webappInfo.isIconGenerated();
            selectedIconAdaptive = webappInfo.isIconAdaptive();
        }
        @SplashLayout.IconClassification
        int selectedIconClassification = SplashLayout.classifyIcon(
                context.getResources(), selectedIcon, selectedIconGenerated);

        SplashLayout.createLayout(context, splashScreen, selectedIcon, selectedIconAdaptive,
                selectedIconClassification, webappInfo.name(),
                ColorUtils.shouldUseLightForegroundOnBackground(backgroundColor));

        recordUma(resources, webappInfo, selectedIconClassification, selectedIcon,
                (splashImage != null));
    }

    /** Called once the splash screen is hidden to record UMA metrics. */
    private void recordSplashHiddenUma(@SplashController.SplashHidesReason int reason,
            long startTimestamp, long endTimestamp) {
        RecordHistogram.recordEnumeratedHistogram(HISTOGRAM_SPLASHSCREEN_HIDES, reason,
                SplashController.SplashHidesReason.NUM_ENTRIES);

        RecordHistogram.recordMediumTimesHistogram(
                HISTOGRAM_SPLASHSCREEN_DURATION, endTimestamp - startTimestamp);
    }

    /**
     * Records splash screen UMA metrics.
     * @param resources
     * @param webappInfo
     * @param selectedIconClassification.
     * @param selectedIcon The icon used on the splash screen.
     * @param usingDedicatedIcon Whether the PWA provides different icons for the splash screen and
     *                           for the app icon.
     */
    private void recordUma(Resources resources, WebappInfo webappInfo,
            @SplashLayout.IconClassification int selectedIconClassification, Bitmap selectedIcon,
            boolean usingDedicatedIcon) {
        mUmaCache = new SameActivityWebappUmaCache();
        mUmaCache.recordSplashscreenBackgroundColor(webappInfo.hasValidBackgroundColor()
                        ? SameActivityWebappUmaCache.SplashColorStatus.CUSTOM
                        : SameActivityWebappUmaCache.SplashColorStatus.DEFAULT);
        mUmaCache.recordSplashscreenThemeColor(webappInfo.hasValidThemeColor()
                        ? SameActivityWebappUmaCache.SplashColorStatus.CUSTOM
                        : SameActivityWebappUmaCache.SplashColorStatus.DEFAULT);

        mUmaCache.recordSplashscreenIconType(selectedIconClassification, usingDedicatedIcon);
        if (selectedIconClassification != SplashLayout.IconClassification.INVALID) {
            DisplayMetrics displayMetrics = resources.getDisplayMetrics();
            mUmaCache.recordSplashscreenIconSize(Math.round(
                    selectedIcon.getScaledWidth(displayMetrics) / displayMetrics.density));
        }

        if (mNativeLoaded) mUmaCache.commitMetrics();
    }
}
