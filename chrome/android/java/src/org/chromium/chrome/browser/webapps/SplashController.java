// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.os.SystemClock;
import android.support.annotation.IntDef;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewTreeObserver;

import org.chromium.base.ObserverList;
import org.chromium.base.TraceEvent;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.WarmupManager;
import org.chromium.chrome.browser.compositor.CompositorView;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserverRegistrar;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Shows and hides splash screen. */
public class SplashController extends EmptyTabObserver {
    private static class SingleShotOnDrawListener implements ViewTreeObserver.OnDrawListener {
        private final View mView;
        private final Runnable mAction;
        private boolean mHasRun;

        public static void install(View view, Runnable action) {
            view.getViewTreeObserver().addOnDrawListener(
                    new SingleShotOnDrawListener(view, action));
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

    public static final String HISTOGRAM_SPLASHSCREEN_DURATION = "Webapp.Splashscreen.Duration";
    public static final String HISTOGRAM_SPLASHSCREEN_HIDES = "Webapp.Splashscreen.Hides";

    private final TabObserverRegistrar mTabObserverRegistrar;

    private SplashDelegate mDelegate;

    /** View to which the splash screen is added. */
    private ViewGroup mParentView;

    private View mSplashView;

    /** Whether the splash hide animation was started. */
    private boolean mWasSplashHideAnimationStarted;

    /** Time that the splash screen was shown. */
    private long mSplashShownTimestamp;

    private ObserverList<SplashscreenObserver> mObservers;

    public SplashController(TabObserverRegistrar tabObserverRegistrar) {
        mTabObserverRegistrar = tabObserverRegistrar;
        mTabObserverRegistrar.registerTabObserver(this);
        mObservers = new ObserverList<>();
    }

    /** Shows the splash screen. */
    public void showSplash(
            SplashDelegate delegate, ViewGroup parentView, final WebappInfo webappInfo) {
        mDelegate = delegate;
        mParentView = parentView;
        mSplashShownTimestamp = SystemClock.elapsedRealtime();

        mSplashView = mDelegate.buildSplashView(webappInfo);
        mParentView.addView(mSplashView);

        notifySplashscreenVisible(mSplashShownTimestamp);
    }

    /**
     * Transfers a {@param viewHierarchy} to the splashscreen's parent view while keeping the
     * splashscreen on top.
     */
    public void setViewHierarchyBelowSplashscreen(ViewGroup viewHierarchy) {
        WarmupManager.transferViewHeirarchy(viewHierarchy, mParentView);
        if (mSplashView != null) {
            mParentView.bringChildToFront(mSplashView);
        }
    }

    @VisibleForTesting
    View getSplashScreenForTests() {
        return mSplashView;
    }

    @Override
    public void didFirstVisuallyNonEmptyPaint(Tab tab) {
        if (canHideSplashScreen()) {
            hideSplash(tab, SplashHidesReason.PAINT);
        }
    }

    @Override
    public void onPageLoadFinished(Tab tab, String url) {
        if (canHideSplashScreen()) {
            hideSplash(tab, SplashHidesReason.LOAD_FINISHED);
        }
    }

    @Override
    public void onPageLoadFailed(Tab tab, int errorCode) {
        if (canHideSplashScreen()) {
            hideSplash(tab, SplashHidesReason.LOAD_FAILED);
        }
    }

    @Override
    public void onCrash(Tab tab) {
        hideSplash(tab, SplashHidesReason.CRASH);
    }

    protected boolean canHideSplashScreen() {
        return !mDelegate.shouldWaitForSubsequentPageLoadToHideSplash();
    }

    /** Hides the splash screen. */
    private void hideSplash(final Tab tab, final @SplashHidesReason int reason) {
        if (reason == SplashHidesReason.LOAD_FAILED || reason == SplashHidesReason.CRASH) {
            animateHideSplash(tab, reason);
            return;
        }
        // Delay hiding the splash screen till the compositor has finished drawing the next frame.
        // Without this callback we were seeing a short flash of white between the splash screen and
        // the web content (crbug.com/734500).
        CompositorView compositorView =
                tab.getActivity().getCompositorViewHolder().getCompositorView();
        compositorView.surfaceRedrawNeededAsync(() -> { animateHideSplash(tab, reason); });
    }

    private void animateHideSplash(final Tab tab, final @SplashHidesReason int reason) {
        if (mWasSplashHideAnimationStarted) return;

        mWasSplashHideAnimationStarted = true;
        mTabObserverRegistrar.unregisterTabObserver(this);
        tab.removeObserver(this);

        recordTraceEventsStartedHidingSplash();

        mSplashView.animate().alpha(0f).withEndAction(() -> { hideSplashNow(tab, reason); });
    }

    private void hideSplashNow(Tab tab, @SplashHidesReason int reason) {
        mParentView.removeView(mSplashView);

        long splashHiddenTimestamp = SystemClock.elapsedRealtime();
        recordTraceEventsFinishedHidingSplash();

        mDelegate.onSplashHidden(tab);
        recordSplashHiddenUma(reason, splashHiddenTimestamp);
        notifySplashscreenHidden(splashHiddenTimestamp);

        mDelegate = null;
        mSplashView = null;
    }

    /** Called once the splash screen is hidden to record UMA metrics. */
    private void recordSplashHiddenUma(@SplashHidesReason int reason, long splashHiddenTimestamp) {
        RecordHistogram.recordEnumeratedHistogram(
                HISTOGRAM_SPLASHSCREEN_HIDES, reason, SplashHidesReason.NUM_ENTRIES);

        assert mSplashShownTimestamp != 0;
        RecordHistogram.recordMediumTimesHistogram(
                HISTOGRAM_SPLASHSCREEN_DURATION, splashHiddenTimestamp - mSplashShownTimestamp);
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

    private void notifySplashscreenVisible(long timestamp) {
        for (SplashscreenObserver observer : mObservers) {
            observer.onSplashscreenShown(timestamp);
        }
    }

    private void notifySplashscreenHidden(long timestamp) {
        for (SplashscreenObserver observer : mObservers) {
            observer.onSplashscreenHidden(timestamp);
        }
        mObservers.clear();
    }

    private void recordTraceEventsShowedSplash() {
        SingleShotOnDrawListener.install(
                mParentView, () -> { TraceEvent.startAsync("SplashScreen.visible", hashCode()); });
    }

    private void recordTraceEventsStartedHidingSplash() {
        TraceEvent.startAsync("SplashScreen.hidingAnimation", hashCode());
    }

    private void recordTraceEventsFinishedHidingSplash() {
        TraceEvent.finishAsync("SplashScreen.hidingAnimation", hashCode());
        SingleShotOnDrawListener.install(mParentView,
                () -> { TraceEvent.finishAsync("WebappSplashScreen.visible", hashCode()); });
    }
}
