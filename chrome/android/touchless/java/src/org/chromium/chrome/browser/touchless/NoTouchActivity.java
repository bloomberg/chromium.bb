// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touchless;

import static org.chromium.chrome.browser.UrlConstants.NTP_URL;

import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.os.SystemClock;
import android.view.KeyEvent;
import android.view.ViewGroup;

import org.chromium.base.VisibleForTesting;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.AppHooks;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeInactivityTracker;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.IntentHandler.IntentHandlerDelegate;
import org.chromium.chrome.browser.IntentHandler.TabOpenType;
import org.chromium.chrome.browser.SingleTabActivity;
import org.chromium.chrome.browser.UrlConstants;
import org.chromium.chrome.browser.compositor.layouts.LayoutManager;
import org.chromium.chrome.browser.fullscreen.ChromeFullscreenManager;
import org.chromium.chrome.browser.preferences.ChromePreferenceManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabRedirectHandler;
import org.chromium.chrome.browser.tab.TabState;
import org.chromium.chrome.browser.touchless.dialog.TouchlessDialogPresenter;
import org.chromium.chrome.browser.touchless.ui.iph.KeyFunctionsIPHCoordinator;
import org.chromium.chrome.browser.touchless.ui.progressbar.ProgressBarCoordinator;
import org.chromium.chrome.browser.touchless.ui.progressbar.ProgressBarView;
import org.chromium.chrome.browser.touchless.ui.tooltip.TooltipView;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.common.Referrer;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;

/**
 * An Activity used to display WebContents on devices that don't support touch.
 */
public class NoTouchActivity extends SingleTabActivity {
    public static final String DINOSAUR_GAME_INTENT =
            "org.chromium.chrome.browser.touchless.DinoActivity";

    @VisibleForTesting
    static final String LAST_BACKGROUNDED_TIME_MS_PREF = "NoTouchActivity.BackgroundTimeMs";

    // Time at which an intent was received and handled.
    private long mIntentHandlingTimeMs;

    private KeyFunctionsIPHCoordinator mKeyFunctionsIPHCoordinator;
    private ProgressBarCoordinator mProgressBarCoordinator;
    private TooltipView mTooltipView;
    private ProgressBarView mProgressBarView;

    /** The class that enables zooming for all websites and handles touchless zooming. */
    private TouchlessZoomHelper mTouchlessZoomHelper;

    /** The class that controls the UI for touchless devices. */
    private TouchlessUiController mUiController;

    /** The class that finishes this activity after a timeout. */
    private ChromeInactivityTracker mInactivityTracker;

    /** Tab observer that tracks media state. */
    private TouchlessTabObserver mTabObserver;

    /**
     * Internal class which performs the intent handling operations delegated by IntentHandler.
     */
    private class InternalIntentDelegate implements IntentHandler.IntentHandlerDelegate {
        /**
         * Processes a url view intent.
         *
         * @param url The url from the intent.
         */
        @Override
        public void processUrlViewIntent(String url, String referer, String headers,
                @TabOpenType int tabOpenType, String externalAppId, int tabIdToBringToFront,
                boolean hasUserGesture, Intent intent) {
            // TODO(mthiesse): ChromeTabbedActivity records a user Action here, we should do the
            // same.
            assert getActivityTab() != null;

            switch (tabOpenType) {
                case TabOpenType.REUSE_URL_MATCHING_TAB_ELSE_NEW_TAB:
                    if (getActivityTab().getUrl().contentEquals(url)) break;
                    // fall through
                case TabOpenType.BRING_TAB_TO_FRONT: // fall through
                case TabOpenType.REUSE_APP_ID_MATCHING_TAB_ELSE_NEW_TAB: // fall through
                case TabOpenType.OPEN_NEW_TAB: // fall through
                case TabOpenType.CLOBBER_CURRENT_TAB:
                    // TODO(mthiesse): For now, let's just clobber current tab always. Are the other
                    // tab open types meaningful when we only have a single tab?

                    // When we get a view intent, create a new tab to reset history state so that
                    // back returns you to the sender.
                    if (tabOpenType != TabOpenType.BRING_TAB_TO_FRONT) createAndShowTab();
                    Tab currentTab = getActivityTab();
                    TabRedirectHandler.from(currentTab).updateIntent(intent);
                    int transitionType = PageTransition.LINK | PageTransition.FROM_API;
                    LoadUrlParams loadUrlParams = new LoadUrlParams(url);
                    loadUrlParams.setIntentReceivedTimestamp(mIntentHandlingTimeMs);
                    loadUrlParams.setHasUserGesture(hasUserGesture);
                    loadUrlParams.setTransitionType(
                            IntentHandler.getTransitionTypeFromIntent(intent, transitionType));
                    if (referer != null) {
                        loadUrlParams.setReferrer(new Referrer(
                                referer, IntentHandler.getReferrerPolicyFromIntent(intent)));
                    }
                    currentTab.loadUrl(loadUrlParams);
                    break;
                case TabOpenType.OPEN_NEW_INCOGNITO_TAB:
                    // Incognito is unsupported for this Activity.
                    assert false;
                    break;
                default:
                    assert false : "Unknown TabOpenType: " + tabOpenType;
                    break;
            }
        }

        @Override
        public void processWebSearchIntent(String query) {
            assert false;
        }
    }

    @Override
    public void finishNativeInitialization() {
        initializeCompositorContent(new LayoutManager(getCompositorViewHolder()), null /* urlBar */,
                (ViewGroup) findViewById(android.R.id.content), null /* controlContainer */);

        getFullscreenManager().setTab(getActivityTab());

        mUiController = AppHooks.get().createTouchlessUiController(this);
        AppHooks.get().attachTouchlessMenuCoordinator(this);
        super.finishNativeInitialization();
    }

    @Override
    public void initializeState() {
        mInactivityTracker = new ChromeInactivityTracker(
                LAST_BACKGROUNDED_TIME_MS_PREF, this.getLifecycleDispatcher());
        boolean launchNtpDueToInactivity = shouldForceNTPDueToInactivity();

        // SingleTabActivity#initializeState creates a tab based on #getSavedInstanceState(), so if
        // we need to clear it due to inactivity, we should do it before calling
        // super#initializeState.
        if (launchNtpDueToInactivity) resetSavedInstanceState();
        super.initializeState();

        mKeyFunctionsIPHCoordinator =
                new KeyFunctionsIPHCoordinator(mTooltipView, getActivityTabProvider());
        mProgressBarCoordinator =
                new ProgressBarCoordinator(mProgressBarView, getActivityTabProvider());
        mTouchlessZoomHelper = new TouchlessZoomHelper(getActivityTabProvider());

        // By this point if we were going to restore a URL from savedInstanceState we would already
        // have done so.
        if (getActivityTab().getUrl().isEmpty()) {
            boolean intentWithEffect = false;
            Intent intent = getIntent();
            mIntentHandlingTimeMs = SystemClock.uptimeMillis();
            if (!launchNtpDueToInactivity && intent != null) {
                // Treat Dino intent action like a url request for chrome://dino
                if (DINOSAUR_GAME_INTENT.equals(intent.getComponent().getClassName())) {
                    intent.setData(Uri.parse(UrlConstants.CHROME_DINO_URL));
                }

                if (!mIntentHandler.shouldIgnoreIntent(intent)) {
                    intentWithEffect = mIntentHandler.onNewIntent(intent);
                }
            }
            if (!intentWithEffect) getTabCreator(false).launchNTP();
        }
        resetSavedInstanceState();
    }

    private boolean shouldForceNTPDueToInactivity() {
        if (mInactivityTracker == null) return false;
        if (mTabObserver == null) return false;

        return !mTabObserver.isPlayingMedia() && mInactivityTracker.inactivityThresholdPassed();
    }

    @Override
    public void onNewIntent(Intent intent) {
        mIntentHandlingTimeMs = SystemClock.uptimeMillis();

        if (DINOSAUR_GAME_INTENT.equals(intent.getComponent().getClassName())) {
            intent.setData(Uri.parse(UrlConstants.CHROME_DINO_URL));
        }

        super.onNewIntent(intent);
    }

    @Override
    public void onNewIntentWithNative(Intent intent) {
        if (shouldForceNTPDueToInactivity()) {
            if (!mIntentHandler.shouldIgnoreIntent(intent)) {
                if (!NTP_URL.equals(getActivityTab().getUrl())) {
                    intent.setData(Uri.parse(NTP_URL));
                }
            }
        }
        super.onNewIntentWithNative(intent);
    }

    @Override
    protected IntentHandlerDelegate createIntentHandlerDelegate() {
        return new InternalIntentDelegate();
    }

    @Override
    public @ChromeActivity.ActivityType int getActivityType() {
        return ChromeActivity.ActivityType.NO_TOUCH;
    }

    @Override
    protected void initializeToolbar() {}

    @Override
    public ModalDialogManager createModalDialogManager() {
        return new ModalDialogManager(new TouchlessDialogPresenter(this), ModalDialogType.APP);
    }

    @Override
    protected ChromeFullscreenManager createFullscreenManager() {
        return new ChromeFullscreenManager(this, ChromeFullscreenManager.ControlsPosition.NONE);
    }

    @Override
    public void onSaveInstanceState(Bundle outState) {
        super.onSaveInstanceState(outState);
        saveTabState(outState);
    }

    private void saveTabState(Bundle outState) {
        Tab tab = getActivityTab();
        if (tab == null || tab.getUrl() == null || tab.getUrl().isEmpty()) return;
        long time = SystemClock.elapsedRealtime();
        outState.putInt(BUNDLE_TAB_ID, tab.getId());
        TabState.saveState(outState, TabState.from(tab));
        RecordHistogram.recordTimesHistogram("Android.StrictMode.NoTouchActivitySaveState",
                SystemClock.elapsedRealtime() - time);
    }

    @Override
    protected Tab createTab() {
        Tab tab = super.createTab();
        mTabObserver = new TouchlessTabObserver();
        tab.addObserver(mTabObserver);
        return tab;
    }

    @Override
    protected TabState restoreTabState(Bundle savedInstanceState, int tabId) {
        return TabState.restoreTabState(savedInstanceState);
    }

    @Override
    protected void doLayoutInflation() {
        super.doLayoutInflation();
        ViewGroup coordinatorLayout = (ViewGroup) findViewById(R.id.coordinator);
        mTooltipView = new TooltipView(this);
        mProgressBarView = new ProgressBarView(this);
        coordinatorLayout.addView(mTooltipView);
        coordinatorLayout.addView(mProgressBarView);
    }

    @Override
    public boolean dispatchKeyEvent(KeyEvent event) {
        if (mProgressBarCoordinator != null) mProgressBarCoordinator.onKeyEvent();

        boolean consumedEvent = mUiController != null ? mUiController.onKeyEvent(event) : false;

        return consumedEvent || super.dispatchKeyEvent(event);
    }

    @Override
    public void onStart() {
        super.onStart();
        ChromePreferenceManager.getInstance().incrementInt(
                ChromePreferenceManager.TOUCHLESS_BROWSING_SESSION_COUNT);
    }

    @Override
    public void onStopWithNative() {
        super.onStopWithNative();
        getFullscreenManager().exitPersistentFullscreenMode();
    }

    @Override
    protected void onDestroyInternal() {
        super.onDestroyInternal();
        if (mKeyFunctionsIPHCoordinator != null) mKeyFunctionsIPHCoordinator.destroy();
        if (mProgressBarCoordinator != null) mProgressBarCoordinator.destroy();
        if (mTouchlessZoomHelper != null) mTouchlessZoomHelper.destroy();
        if (mUiController != null) {
            mUiController.destroy();
            mUiController = null;
        }
    }

    /**
     * @return A UI controller implementation.
     */
    public TouchlessUiController getTouchlessUiController() {
        return mUiController;
    }
}
