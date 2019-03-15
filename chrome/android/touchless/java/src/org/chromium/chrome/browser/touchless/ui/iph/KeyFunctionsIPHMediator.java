// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touchless.ui.iph;

import org.chromium.base.task.PostTask;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.ui.base.CursorVisibilityObserver;
import org.chromium.ui.base.TouchlessEventHandler;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.concurrent.FutureTask;

/**
 * Controls the UI model for key functions IPH.
 */
public class KeyFunctionsIPHMediator implements CursorVisibilityObserver {
    private final PropertyModel mModel;
    private final KeyFunctionsIPHTabObserver mKeyFunctionsIPHTabObserver;
    private FutureTask mHideTask;
    private boolean mHasShownOnSpatNav;
    private boolean mHasShownOnFallback;

    private static final long DISPLAY_DURATION_MS = 1500;

    KeyFunctionsIPHMediator(PropertyModel model, ActivityTabProvider activityTabProvider) {
        mModel = model;
        mKeyFunctionsIPHTabObserver = new KeyFunctionsIPHTabObserver(activityTabProvider);
        TouchlessEventHandler.addCursorVisibilityObserver(this);
    }

    @Override
    public void onCursorVisibilityChanged(boolean isCursorVisible) {
        show(isCursorVisible);
    }

    private void show(boolean isCursorVisible) {
        // TODO(crbug.com/942665): Populate this.
        boolean pageOptimizedForMobile = true;
        if ((!isCursorVisible && mHasShownOnSpatNav && pageOptimizedForMobile)
                || (isCursorVisible && mHasShownOnFallback)) {
            return;
        }

        if (isCursorVisible) {
            mHasShownOnFallback = true;
        } else {
            mHasShownOnSpatNav = true;
        }
        if (mHideTask != null) mHideTask.cancel(false);
        mHideTask = new FutureTask<Void>(() -> {
            mModel.set(KeyFunctionsIPHProperties.IS_VISIBLE, false);
            return null;
        });
        PostTask.postDelayedTask(UiThreadTaskTraits.DEFAULT, mHideTask, DISPLAY_DURATION_MS);
        mModel.set(KeyFunctionsIPHProperties.IS_CURSOR_VISIBLE, isCursorVisible);
        mModel.set(KeyFunctionsIPHProperties.IS_VISIBLE, true);
    }

    void destroy() {
        TouchlessEventHandler.removeCursorVisibilityObserver(this);
        mKeyFunctionsIPHTabObserver.destroy();
        if (mHideTask != null) mHideTask.cancel(false);
    }

    private class KeyFunctionsIPHTabObserver extends ActivityTabProvider.ActivityTabTabObserver {
        KeyFunctionsIPHTabObserver(ActivityTabProvider tabProvider) {
            super(tabProvider);
        }

        @Override
        public void onPageLoadFinished(Tab tab, String url) {
            if (tab.isNativePage()) return;

            if (tab.isShowingErrorPage()) return;

            show(false);
        }
    }
}
