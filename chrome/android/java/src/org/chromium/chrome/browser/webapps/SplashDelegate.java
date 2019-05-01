// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.view.View;
import android.view.ViewGroup;

import org.chromium.chrome.browser.tab.Tab;

/** Delegate for {@link SplashController}. */
interface SplashDelegate {
    /** Shows the splash screen. */
    void showSplash(ViewGroup parentView, WebappInfo webappInfo);

    /** Hides the splash screen. Runs the callback once the splash screen is hidden. */
    void hideSplash(Tab tab, Runnable finishedHidingCallback);

    /** Returns whether the splash screen is visible and not in the process of hiding. */
    boolean isSplashVisible();

    /**
     * Returns the {@link View} containing the splash screen if it is a direct child of
     * the passed-in view.
     */
    View getSplashViewIfChildOf(ViewGroup parent);

    /** Returns whether to wait for a subsequent page load to hide the splash screen. */
    boolean shouldWaitForSubsequentPageLoadToHideSplash();
}
