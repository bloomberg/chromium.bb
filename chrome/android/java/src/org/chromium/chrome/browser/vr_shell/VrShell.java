// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr_shell;

import android.widget.FrameLayout;

import org.chromium.chrome.browser.tab.Tab;

/**
 * Abstracts away the VrShell class, which may or may not be present at runtime depending on
 * compile flags.
 */
public interface VrShell {
    /**
     * Performs native VrShell initialization.
     */
    void initializeNative(Tab currentTab, VrShellDelegate delegate, boolean forWebVR);

    /**
     * Swaps to the designated tab.
     */
    void swapTab(Tab currentTab);

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
     * Returns the GVRLayout as a FrameLayout.
     */
    FrameLayout getContainer();

    /**
     * Sets a callback to be run when the close button is tapped.
     */
    void setCloseButtonListener(Runnable runner);

    /**
     * Handles a change in page load progress.
     */
    void onLoadProgressChanged(double progress);

    /**
     * Passes a list of tabs to the VR UI.
     */
    void onTabListCreated(Tab[] mainTabs, Tab[] incognitoTabs);

    /**
     * Handles updating the UI for a tab that's had its title changed.
     */
    void onTabUpdated(boolean incognito, int id, String title);

    /**
     * Handles updating the UI for a tab that's been removed.
     */
    void onTabRemoved(boolean incognito, int id);
}
