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
    void initializeNativeOnUI(Tab currentTab, VrShellDelegate delegate, boolean forWebVR);

    /**
     * Pauses VrShell.
     */
    void pauseOnUI();

    /**
     * Resumes VrShell.
     */
    void resumeOnUI();

    /**
     * Destroys VrShell.
     */
    void teardownOnUI();

    /**
     * Sets whether we're presenting WebVR content or not.
     */
    void setWebVrModeEnabledOnUI(boolean enabled);

    /**
     * Returns the GVRLayout as a FrameLayout.
     */
    FrameLayout getContainerOnUI();

    /**
     * Sets a callback to be run when the close button is tapped.
     */
    void setCloseButtonListenerOnUI(Runnable runner);
}
