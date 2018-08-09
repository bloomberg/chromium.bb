// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices;

import org.chromium.chrome.browser.fullscreen.BrowserStateBrowserControlsVisibilityDelegate;
import org.chromium.chrome.browser.tab.BrowserControlsVisibilityDelegate;
import org.chromium.chrome.browser.tab.Tab;

/**
 * Class to handle the state and logic for CustomTabActivity to do with Trusted Web Activities.
 */
public class TrustedWebActivityUi {
    private boolean mInTrustedWebActivity;

    /**
     * A {@link BrowserControlsVisibilityDelegate} that disallows showing the Browser Controls when
     * we are in a Trusted Web Activity.
     */
    private final BrowserControlsVisibilityDelegate mInTwaVisibilityDelegate =
            new BrowserControlsVisibilityDelegate() {
                @Override
                public boolean canShowBrowserControls() {
                    return !mInTrustedWebActivity;
                }

                @Override
                public boolean canAutoHideBrowserControls() {
                    return true;
                }
            };

    /**
     * Gets a {@link BrowserControlsVisibilityDelegate} that will hide/show the Custom Tab toolbar
     * on verification/leaving the verified origin.
     */
    public BrowserControlsVisibilityDelegate getBrowserControlsVisibilityDelegate() {
        return mInTwaVisibilityDelegate;
    }

    /**
     * Updates the UI appropriately for whether or not Trusted Web Activity mode is enabled.
     */
    public void setTrustedWebActivityMode(boolean enabled, Tab tab,
            BrowserStateBrowserControlsVisibilityDelegate delegate) {
        mInTrustedWebActivity = enabled;

        // Force showing the controls for a bit when leaving Trusted Web Activity mode.
        if (!enabled) delegate.showControlsTransient();

        // Reflect the browser controls update in the Tab.
        tab.updateFullscreenEnabledState();
    }
}
