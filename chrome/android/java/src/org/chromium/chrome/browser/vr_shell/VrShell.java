// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr_shell;

import android.graphics.Point;
import android.widget.FrameLayout;

/**
 * Abstracts away the VrShell class, which may or may not be present at runtime depending on
 * compile flags.
 */
public interface VrShell extends VrDialogManager, VrToastManager {
    /**
     * Performs native VrShell initialization.
     */
    void initializeNative(boolean forWebVr, boolean webVrAutopresentationExpected, boolean inCct);

    /**
     * Pauses VrShell.
     */
    void pause();

    /**
     * Resumes VrShell.
     */
    void resume();

    /**
     * Destroys VrShell.
     */
    void teardown();

    /**
     * Sets whether we're presenting WebVR content or not.
     */
    void setWebVrModeEnabled(boolean enabled);

    /**
     * Returns true if we're presenting WebVR content.
     */
    boolean getWebVrModeEnabled();

    /**
     * Returns true if our URL bar is showing a string.
     */
    boolean isDisplayingUrlForTesting();

    /**
     * Returns the GVRLayout as a FrameLayout.
     */
    FrameLayout getContainer();

    /**
     * Returns whether the back button is enabled.
     */
    Boolean isBackButtonEnabled();

    /**
     * Returns whether the forward button is enabled.
     */
    Boolean isForwardButtonEnabled();

    /**
     * Requests to exit VR.
     */
    void requestToExitVr(@UiUnsupportedMode int reason, boolean showExitPromptBeforeDoff);

    /**
     *  Triggers VrShell to navigate forward.
     */
    void navigateForward();

    /**
     *  Triggers VrShell to navigate backward.
     */
    void navigateBack();

    /**
     *  Asks VrShell to reload the current page.
     */
    void reloadTab();

    /**
     *  Asks VrShell to open a new tab.
     */
    void openNewTab(boolean incognito);

    /**
     *  Asks VrShell to close all incognito tabs.
     */
    void closeAllIncognitoTabs();

    /**
     * Simulates a user accepting the currently visible DOFF prompt.
     */
    void acceptDoffPromptForTesting();

    /**
     * Performs a UI action that doesn't require a position argument on a UI element.
     */
    void performUiActionForTesting(int elementName, int actionType, Point position);

    /**
     * @param topContentOffset The content offset (usually applied by the omnibox).
     */
    void rawTopContentOffsetChanged(float topContentOffset);
}
