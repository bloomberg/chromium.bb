// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import org.junit.Assert;

import org.chromium.chrome.browser.vr.rules.VrTestRule;
import org.chromium.chrome.browser.vr.util.PermissionUtils;
import org.chromium.chrome.browser.vr.util.VrShellDelegateUtils;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.content_public.browser.WebContents;

/**
 * Extension of VrTestFramework containing WebXR for VR-specific functionality.
 */
public class WebXrVrTestFramework extends WebXrTestFramework {
    public WebXrVrTestFramework(ChromeActivityTestRule rule) {
        super(rule);
        if (!TestVrShellDelegate.isOnStandalone()) {
            Assert.assertFalse("Test started in VR", VrShellDelegate.isInVr());
        }
    }

    /**
     * VR-specific implementation of enterSessionWithUserGesture that includes a workaround for
     * receiving broadcasts late.
     *
     * @param webContents The WebContents for the tab to enter the VR session in.
     */
    @Override
    public void enterSessionWithUserGesture(WebContents webContents) {
        // TODO(https://crbug.com/762724): Remove this workaround when the issue with being resumed
        // before receiving the VR broadcast is fixed on VrCore's end.
        // However, we don't want to enable the workaround if the DON flow is enabled, as that
        // causes issues.
        if (!((VrTestRule) getRule()).isDonEnabled()) {
            VrShellDelegateUtils.getDelegateInstance().setExpectingBroadcast();
        }
        super.enterSessionWithUserGesture(webContents);
    }

    /**
     * WebXR for VR-specific implementation of enterSessionWithUserGestureOrFail.
     *
     * @param webContents The WebContents of the tab to enter the immersive session in.
     */
    @Override
    public void enterSessionWithUserGestureOrFail(WebContents webContents) {
        runJavaScriptOrFail(
                "sessionTypeToRequest = sessionTypes.IMMERSIVE", POLL_TIMEOUT_LONG_MS, webContents);
        enterSessionWithUserGesture(webContents);

        PermissionUtils.waitForConsentPrompt(getRule().getActivity());
        PermissionUtils.acceptConsentPrompt(getRule().getActivity());

        pollJavaScriptBooleanOrFail("sessionInfos[sessionTypes.IMMERSIVE].currentSession != null",
                POLL_TIMEOUT_LONG_MS, webContents);
        Assert.assertTrue("Immersive session started, but VR Shell not in presentation mode",
                TestVrShellDelegate.getVrShellForTesting().getWebVrModeEnabled());
    }

    /**
     * Ends a WebXR immersive session.
     *
     * @param webContents The WebContents for the tab to end the session in.
     */
    @Override
    public void endSession(WebContents webContents) {
        runJavaScriptOrFail("sessionInfos[sessionTypes.IMMERSIVE].currentSession.end()",
                POLL_TIMEOUT_SHORT_MS, webContents);
    }
}
