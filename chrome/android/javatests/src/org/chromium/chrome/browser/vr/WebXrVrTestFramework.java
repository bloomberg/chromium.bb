// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import org.junit.Assert;

import org.chromium.base.CommandLine;
import org.chromium.chrome.browser.vr.util.VrShellDelegateUtils;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.content_public.browser.WebContents;

/**
 * Extension of VrTestFramework containing WebXR for VR-specific functionality.
 */
public class WebXrVrTestFramework extends WebXrTestFramework {
    public WebXrVrTestFramework(ChromeActivityTestRule rule) {
        super(rule);
        Assert.assertFalse("Test did not start in VR", VrShellDelegate.isInVr());
    }

    /**
     * VR-specific implementation of enterSessionWithUserGesture that includes a workaround for
     * receiving broadcasts late.
     * @param webContents The WebContents for the tab to enter the VR session in.
     */
    @Override
    public void enterSessionWithUserGesture(WebContents webContents) {
        // TODO(https://crbug.com/762724): Remove this workaround when the issue with being resumed
        // before receiving the VR broadcast is fixed on VrCore's end.
        // However, we don't want to enable the workaround if the DON flow is enabled, as that
        // causes issues. Since we don't have a way of actually checking whether the DON flow is
        // enabled, check for the presence of the flag that's passed to tests when the DON flow is
        // enabled.
        if (!CommandLine.getInstance().hasSwitch("don-enabled")) {
            VrShellDelegateUtils.getDelegateInstance().setExpectingBroadcast();
        }
        super.enterSessionWithUserGesture(webContents);
    }

    /**
     * WebXR for VR-specific implementation of enterSessionWithUserGestureOrFail.
     * @param webContents The WebContents of the tab to enter the immersive session in.
     */
    @Override
    public void enterSessionWithUserGestureOrFail(WebContents webContents) {
        runJavaScriptOrFail(
                "sessionTypeToRequest = sessionTypes.IMMERSIVE", POLL_TIMEOUT_LONG_MS, webContents);
        enterSessionWithUserGesture(webContents);
        Assert.assertTrue(
                pollJavaScriptBoolean("sessionInfos[sessionTypes.IMMERSIVE].currentSession != null",
                        POLL_TIMEOUT_LONG_MS, webContents));
        Assert.assertTrue(TestVrShellDelegate.getVrShellForTesting().getWebVrModeEnabled());
    }

    /**
     * Ends a WebXR immersive session.
     * @param webContents The WebContents for the tab to end the session in
     */
    @Override
    public void endSession(WebContents webContents) {
        runJavaScriptOrFail("sessionInfos[sessionTypes.IMMERSIVE].currentSession.end()",
                POLL_TIMEOUT_SHORT_MS, webContents);
    }
}
