// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import org.chromium.chrome.browser.UrlConstants;
import org.chromium.chrome.browser.device.DeviceClassManager;
import org.chromium.chrome.browser.util.AccessibilityUtil;
import org.chromium.components.security_state.ConnectionSecurityLevel;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.content_public.browser.WebContents;

/**
 * A delegate to determine visibility of the browser controls.
 */
public class BrowserControlsVisibilityDelegate {
    protected final Tab mTab;

    /**
     * Basic constructor.
     * @param tab The associated {@link Tab}.
     */
    public BrowserControlsVisibilityDelegate(Tab tab) {
        mTab = tab;
    }

    /**
     * @return Whether hiding browser controls is enabled or not.
     */
    public boolean isHidingBrowserControlsEnabled() {
        WebContents webContents = mTab.getWebContents();
        if (webContents == null || webContents.isDestroyed()) return false;

        String url = mTab.getUrl();
        boolean enableHidingBrowserControls = url != null;
        enableHidingBrowserControls &= !url.startsWith(UrlConstants.CHROME_SCHEME);
        enableHidingBrowserControls &= !url.startsWith(UrlConstants.CHROME_NATIVE_SCHEME);

        int securityState = mTab.getSecurityLevel();
        enableHidingBrowserControls &= (securityState != ConnectionSecurityLevel.DANGEROUS
                && securityState != ConnectionSecurityLevel.SECURITY_WARNING);

        enableHidingBrowserControls &=
                !AccessibilityUtil.isAccessibilityEnabled(mTab.getApplicationContext());

        ContentViewCore cvc = mTab.getContentViewCore();
        enableHidingBrowserControls &= cvc == null || !cvc.isFocusedNodeEditable();
        enableHidingBrowserControls &= !mTab.isShowingErrorPage();
        enableHidingBrowserControls &= !webContents.isShowingInterstitialPage();
        enableHidingBrowserControls &= (mTab.getFullscreenManager() != null);
        enableHidingBrowserControls &= DeviceClassManager.enableFullscreen();
        enableHidingBrowserControls &= !mTab.isFullscreenWaitingForLoad();

        return enableHidingBrowserControls;
    }

    /**
     * @return Whether showing browser controls is enabled or not.
     */
    public boolean isShowingBrowserControlsEnabled() {
        if (mTab.getFullscreenManager() == null) return true;
        return !mTab.getFullscreenManager().getPersistentFullscreenMode();
    }
}
