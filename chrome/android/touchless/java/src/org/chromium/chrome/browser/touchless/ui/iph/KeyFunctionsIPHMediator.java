// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touchless.ui.iph;

import org.chromium.base.task.PostTask;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.native_page.NativePageFactory;
import org.chromium.chrome.browser.preferences.ChromePreferenceManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.feature_engagement.Tracker.DisplayLockHandle;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.touchless.CursorObserver;
import org.chromium.ui.touchless.TouchlessEventHandler;

import java.util.concurrent.FutureTask;

/**
 * Controls the UI model for key functions IPH.
 */
public class KeyFunctionsIPHMediator implements CursorObserver {
    private final PropertyModel mModel;
    private final KeyFunctionsIPHTabObserver mKeyFunctionsIPHTabObserver;
    private FutureTask mHideTask;
    private int mPageLoadCount;

    private static final long DISPLAY_DURATION_MS = 3000;

    // For the first INTRODUCTORY_SESSIONS sessions, show the IPH every INTRODUCTORY_PAGE_LOAD_CYCLE
    // page loads.
    private static final int INTRODUCTORY_SESSIONS = 6;
    private static final int INTRODUCTORY_PAGE_LOAD_CYCLE = 3;

    KeyFunctionsIPHMediator(PropertyModel model, ActivityTabProvider activityTabProvider) {
        mModel = model;
        mKeyFunctionsIPHTabObserver = new KeyFunctionsIPHTabObserver(activityTabProvider);
        TouchlessEventHandler.addCursorObserver(this);
    }

    @Override
    public void onCursorVisibilityChanged(boolean isCursorVisible) {}

    @Override
    public void onFallbackCursorModeToggled(boolean isOn) {
        show(isOn, false);
    }

    private void show(boolean isFallbackCursorModeOn, boolean fromPageLoadStarted) {
        // TODO(crbug.com/942665): Populate this.
        boolean pageOptimizedForMobile = true;
        if (fromPageLoadStarted && pageOptimizedForMobile) {
            int totalSessionCount = ChromePreferenceManager.getInstance().readInt(
                    ChromePreferenceManager.TOUCHLESS_BROWSING_SESSION_COUNT);
            if (totalSessionCount <= INTRODUCTORY_SESSIONS
                    && mPageLoadCount % INTRODUCTORY_PAGE_LOAD_CYCLE != 1) {
                return;
            }
            if (totalSessionCount > INTRODUCTORY_SESSIONS && mPageLoadCount > 1) return;
        }

        // This ensures that no other in-product help UI is currently shown.
        DisplayLockHandle displayLockHandle =
                TrackerFactory.getTrackerForProfile(Profile.getLastUsedProfile())
                        .acquireDisplayLock();
        if (displayLockHandle == null) return;

        if (mHideTask != null) mHideTask.cancel(false);
        mHideTask = new FutureTask<Void>(() -> {
            mModel.set(KeyFunctionsIPHProperties.IS_VISIBLE, false);
            displayLockHandle.release();
            return null;
        });
        PostTask.postDelayedTask(UiThreadTaskTraits.DEFAULT, mHideTask, DISPLAY_DURATION_MS);
        mModel.set(KeyFunctionsIPHProperties.IS_CURSOR_VISIBLE, isFallbackCursorModeOn);
        mModel.set(KeyFunctionsIPHProperties.IS_VISIBLE, true);
    }

    void destroy() {
        TouchlessEventHandler.removeCursorObserver(this);
        mKeyFunctionsIPHTabObserver.destroy();
        if (mHideTask != null) mHideTask.cancel(false);
    }

    private class KeyFunctionsIPHTabObserver extends ActivityTabProvider.ActivityTabTabObserver {
        KeyFunctionsIPHTabObserver(ActivityTabProvider tabProvider) {
            super(tabProvider);
        }

        @Override
        public void onPageLoadStarted(Tab tab, String url) {
            if (NativePageFactory.isNativePageUrl(url, tab.isIncognito())) return;

            mPageLoadCount++;
            show(false, true);
        }
    }
}
