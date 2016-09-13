// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr_shell;

import android.widget.FrameLayout;

/**
 * Abstracts away the VrShell class, which may or may not be present at runtime depending on
 * compile flags.
 */
public interface VrShellInterface {
    /**
     * Performs native VrShell initialization.
     */
    void onNativeLibraryReady();

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
     * Sets Android VR Mode to |enabled|.
     */
    void setVrModeEnabled(boolean enabled);

    /**
     * Returns the GVRLayout as a FrameLayout.
     */
    FrameLayout getContainer();
}
