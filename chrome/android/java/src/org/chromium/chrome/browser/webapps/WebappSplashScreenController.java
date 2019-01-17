// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.support.annotation.IntDef;
import android.util.DisplayMetrics;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewTreeObserver;
import android.widget.FrameLayout;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ContextUtils;
import org.chromium.base.ObserverList;
import org.chromium.base.TraceEvent;
import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.WarmupManager;
import org.chromium.chrome.browser.compositor.CompositorViewHolder;
import org.chromium.chrome.browser.metrics.WebApkUma;
import org.chromium.chrome.browser.metrics.WebappUma;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.util.ColorUtils;
import org.chromium.net.NetError;
import org.chromium.net.NetworkChangeNotifier;
import org.chromium.webapk.lib.common.splash.SplashLayout;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Shows and hides splash screen. */
public class WebappSplashScreenController extends EmptyTabObserver {
    // SplashHidesReason defined in tools/metrics/histograms/enums.xml.
    @IntDef({SplashHidesReason.PAINT, SplashHidesReason.LOAD_FINISHED,
            SplashHidesReason.LOAD_FAILED, SplashHidesReason.CRASH})
    @Retention(RetentionPolicy.SOURCE)
    public @interface SplashHidesReason {
        int PAINT = 0;
        int LOAD_FINISHED = 1;
        int LOAD_FAILED = 2;
        int CRASH = 3;
        int NUM_ENTRIES = 4;
    }

    // No error.
    public static final int ERROR_OK = 0;

    /** Used to schedule splash screen hiding. */
    private CompositorViewHolder mCompositorViewHolder;

    /** View to which the splash screen is added. */
    private ViewGroup mParentView;

    /** Whether native was loaded. Native must be loaded in order to record metrics. */
    private boolean mNativeLoaded;

    /** Whether the splash screen layout was initialized. */
    private boolean mInitializedLayout;

    private ViewGroup mSplashScreen;
    private WebappUma mWebappUma;

    /** The error code of the navigation. */
    private int mErrorCode;

    private WebApkOfflineDialog mOfflineDialog;

    /** Indicates whether reloading is allowed. */
    private boolean mAllowReloads;

    private String mAppName;

    private boolean mIsForWebApk;

    private ObserverList<SplashscreenObserver> mObservers;

    public WebappSplashScreenController() {
        mWebappUma = new WebappUma();
        mObservers = new ObserverList<>();
        addObserver(mWebappUma);
    }

    /** Shows the splash screen. */
    public void showSplashScreen(ViewGroup parentView, final WebappInfo webappInfo) {
        mParentView = parentView;
        mIsForWebApk = webappInfo.isForWebApk();
        mAppName = webappInfo.name();

        Context context = ContextUtils.getApplicationContext();
        final int backgroundColor = ColorUtils.getOpaqueColor(webappInfo.backgroundColor(
                ApiCompatibilityUtils.getColor(context.getResources(), R.color.webapp_default_bg)));

        mSplashScreen = new FrameLayout(context);
        mSplashScreen.setBackgroundColor(backgroundColor);
        mParentView.addView(mSplashScreen);
        startSplashscreenTraceEvents();

        notifySplashscreenVisible();

        if (mIsForWebApk) {
            initializeLayout(webappInfo, backgroundColor, ((WebApkInfo) webappInfo).splashIcon());
            return;
        }

        WebappDataStorage storage =
                WebappRegistry.getInstance().getWebappDataStorage(webappInfo.id());
        if (storage == null) {
            initializeLayout(webappInfo, backgroundColor, null);
            return;
        }

        storage.getSplashScreenImage(new WebappDataStorage.FetchCallback<Bitmap>() {
            @Override
            public void onDataRetrieved(Bitmap splashImage) {
                initializeLayout(webappInfo, backgroundColor, splashImage);
            }
        });
    }

    /**
     * Transfers a {@param viewHierarchy} to the splashscreen's parent view while keeping the
     * splashscreen on top.
     */
    public void setViewHierarchyBelowSplashscreen(ViewGroup viewHierarchy) {
        WarmupManager.transferViewHeirarchy(viewHierarchy, mParentView);
        mParentView.bringChildToFront(mSplashScreen);
    }

    /** Should be called once native has loaded. */
    public void onFinishedNativeInit(Tab tab, CompositorViewHolder compositorViewHolder) {
        mNativeLoaded = true;
        mCompositorViewHolder = compositorViewHolder;
        tab.addObserver(this);
        if (mInitializedLayout) mWebappUma.commitMetrics();
    }

    @VisibleForTesting
    ViewGroup getSplashScreenForTests() {
        return mSplashScreen;
    }

    @Override
    public void didFirstVisuallyNonEmptyPaint(Tab tab) {
        if (canHideSplashScreen()) {
            hideSplashScreenOnDrawingFinished(tab, SplashHidesReason.PAINT);
        }
    }

    @Override
    public void onPageLoadFinished(Tab tab, String url) {
        if (canHideSplashScreen()) {
            hideSplashScreenOnDrawingFinished(tab, SplashHidesReason.LOAD_FINISHED);
        }
    }

    @Override
    public void onPageLoadFailed(Tab tab, int errorCode) {
        if (canHideSplashScreen()) {
            animateHidingSplashScreen(tab, SplashHidesReason.LOAD_FAILED);
        }
    }

    @Override
    public void onCrash(Tab tab) {
        animateHidingSplashScreen(tab, SplashHidesReason.CRASH);
    }

    @Override
    public void onDidFinishNavigation(final Tab tab, final String url, boolean isInMainFrame,
            boolean isErrorPage, boolean hasCommitted, boolean isSameDocument,
            boolean isFragmentNavigation, Integer pageTransition, int errorCode,
            int httpStatusCode) {
        if (!mIsForWebApk || !isInMainFrame) return;

        mErrorCode = errorCode;
        switch (mErrorCode) {
            case ERROR_OK:
                if (mOfflineDialog != null) {
                    mOfflineDialog.cancel();
                    mOfflineDialog = null;
                }
                break;
            case NetError.ERR_NETWORK_CHANGED:
                onNetworkChanged(tab);
                break;
            default:
                onNetworkError(tab, errorCode);
                break;
        }
        WebApkUma.recordNetworkErrorWhenLaunch(-errorCode);
    }

    protected boolean canHideSplashScreen() {
        if (!mIsForWebApk) return true;
        return mErrorCode != NetError.ERR_INTERNET_DISCONNECTED
                && mErrorCode != NetError.ERR_NETWORK_CHANGED;
    }

    private void onNetworkChanged(Tab tab) {
        if (!mAllowReloads) return;

        // It is possible that we get {@link NetError.ERR_NETWORK_CHANGED} during the first
        // reload after the device is online. The navigation will fail until the next auto
        // reload fired by {@link NetErrorHelperCore}. We call reload explicitly to reduce the
        // waiting time.
        tab.reloadIgnoringCache();
        mAllowReloads = false;
    }

    private void onNetworkError(final Tab tab, int errorCode) {
        if (mOfflineDialog != null || tab.getActivity() == null) return;

        final NetworkChangeNotifier.ConnectionTypeObserver observer =
                new NetworkChangeNotifier.ConnectionTypeObserver() {
                    @Override
                    public void onConnectionTypeChanged(int connectionType) {
                        if (!NetworkChangeNotifier.isOnline()) return;

                        NetworkChangeNotifier.removeConnectionTypeObserver(this);
                        tab.reloadIgnoringCache();
                        // One more reload is allowed after the network connection is back.
                        mAllowReloads = true;
                    }
                };

        NetworkChangeNotifier.addConnectionTypeObserver(observer);
        mOfflineDialog = new WebApkOfflineDialog();
        mOfflineDialog.show(tab.getActivity(), mAppName, errorCode);
    }

    /** Sets the splash screen layout and sets the splash screen's title and icon. */
    private void initializeLayout(WebappInfo webappInfo, int backgroundColor, Bitmap splashImage) {
        mInitializedLayout = true;
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
        int selectedIconClassification =
                SplashLayout.classifyIcon(resources, selectedIcon, selectedIconGenerated);

        SplashLayout.createLayout(context, mSplashScreen, selectedIcon, selectedIconAdaptive,
                selectedIconClassification, webappInfo.name(),
                ColorUtils.shouldUseLightForegroundOnBackground(backgroundColor));

        recordUma(resources, webappInfo, selectedIconClassification, selectedIcon,
                (splashImage != null));
        if (mNativeLoaded) mWebappUma.commitMetrics();
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
        mWebappUma.recordSplashscreenBackgroundColor(webappInfo.hasValidBackgroundColor()
                        ? WebappUma.SplashColorStatus.CUSTOM
                        : WebappUma.SplashColorStatus.DEFAULT);
        mWebappUma.recordSplashscreenThemeColor(webappInfo.hasValidThemeColor()
                        ? WebappUma.SplashColorStatus.CUSTOM
                        : WebappUma.SplashColorStatus.DEFAULT);

        mWebappUma.recordSplashscreenIconType(selectedIconClassification, usingDedicatedIcon);
        if (selectedIconClassification != SplashLayout.IconClassification.INVALID) {
            DisplayMetrics displayMetrics = resources.getDisplayMetrics();
            mWebappUma.recordSplashscreenIconSize(Math.round(
                    selectedIcon.getScaledWidth(displayMetrics) / displayMetrics.density));
        }
    }

    /**
     * Schedules the splash screen hiding once the compositor has finished drawing a frame.
     *
     * Without this callback we were seeing a short flash of white between the splash screen and
     * the web content (crbug.com/734500).
     * */
    private void hideSplashScreenOnDrawingFinished(
            final Tab tab, final @SplashHidesReason int reason) {
        if (mSplashScreen == null) return;

        if (mCompositorViewHolder == null) {
            animateHidingSplashScreen(tab, reason);
            return;
        }

        mCompositorViewHolder.getCompositorView().surfaceRedrawNeededAsync(
                () -> { animateHidingSplashScreen(tab, reason); });
    }

    /** Performs the splash screen hiding animation. */
    private void animateHidingSplashScreen(final Tab tab, final @SplashHidesReason int reason) {
        if (mSplashScreen == null) return;

        mSplashScreen.animate().alpha(0f).withEndAction(new Runnable() {
            @Override
            public void run() {
                if (mSplashScreen == null) return;
                mParentView.removeView(mSplashScreen);
                finishSplashscreenTraceEvents();
                tab.removeObserver(WebappSplashScreenController.this);
                mSplashScreen = null;
                mCompositorViewHolder = null;
                notifySplashscreenHidden(reason);
            }
        });
    }

    private static class SingleShotOnDrawListener implements ViewTreeObserver.OnDrawListener {
        private final View mView;
        private final Runnable mAction;
        private boolean mHasRun;

        public static void install(View view, Runnable action) {
            SingleShotOnDrawListener listener = new SingleShotOnDrawListener(view, action);
            view.getViewTreeObserver().addOnDrawListener(listener);
        }

        private SingleShotOnDrawListener(View view, Runnable action) {
            mView = view;
            mAction = action;
        }

        @Override
        public void onDraw() {
            if (mHasRun) return;
            mHasRun = true;
            mAction.run();
            // Cannot call removeOnDrawListener within OnDraw, so do on next tick.
            mView.post(() -> mView.getViewTreeObserver().removeOnDrawListener(this));
        }
    };

    private void startSplashscreenTraceEvents() {
        TraceEvent.startAsync("WebappSplashScreen", hashCode());
        SingleShotOnDrawListener.install(mParentView,
                () -> { TraceEvent.startAsync("WebappSplashScreen.visible", hashCode()); });
    }

    private void finishSplashscreenTraceEvents() {
        TraceEvent.finishAsync("WebappSplashScreen", hashCode());
        SingleShotOnDrawListener.install(mParentView,
                () -> { TraceEvent.finishAsync("WebappSplashScreen.visible", hashCode()); });
    }

    /**
     * Register an observer for the splashscreen hidden/visible events.
     */
    public void addObserver(SplashscreenObserver observer) {
        mObservers.addObserver(observer);
    }

    /**
     * Deegister an observer for the splashscreen hidden/visible events.
     */
    public void removeObserver(SplashscreenObserver observer) {
        mObservers.removeObserver(observer);
    }

    private void notifySplashscreenVisible() {
        for (SplashscreenObserver observer : mObservers) {
            observer.onSplashscreenShown();
        }
    }

    private void notifySplashscreenHidden(@SplashHidesReason int reason) {
        for (SplashscreenObserver observer : mObservers) {
            observer.onSplashscreenHidden(reason);
        }
        mObservers.clear();
    }
}
