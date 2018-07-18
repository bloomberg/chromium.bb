// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import org.junit.Assert;

import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.content_public.browser.WebContents;

/**
 * Extension of WebXrVrTestFramework containing WebVR-specific functionality.
 */
public class WebVrTestFramework extends WebXrVrTestFramework {
    public WebVrTestFramework(ChromeActivityTestRule rule) {
        super(rule);
    }

    /**
     * Checks whether a VRDisplay was actually found. Keeps the "xrDeviceFound" naming instead of
     * "vrDisplayFound" since WebVR is being deprecated and maintaining consistency with WebXR
     * naming is more important in the long run.
     * @param webContents The WebContents to run the JavaScript through.
     * @return Whether a VRDisplay was found.
     */
    @Override
    public boolean xrDeviceFound(WebContents webContents) {
        return !runJavaScriptOrFail("vrDisplay", POLL_TIMEOUT_SHORT_MS, webContents).equals("null");
    }

    /**
     * WebVR-specific implementation of enterSessionWithUserGestureOrFail.
     * @param webContents The WebContents of the tab to enter WebVR presentation in.
     */
    @Override
    public void enterSessionWithUserGestureOrFail(WebContents webContents) {
        enterSessionWithUserGesture(webContents);
        Assert.assertTrue(
                pollJavaScriptBoolean("vrDisplay.isPresenting", POLL_TIMEOUT_LONG_MS, webContents));
        Assert.assertTrue(TestVrShellDelegate.getVrShellForTesting().getWebVrModeEnabled());
    }

    /**
     * Exits WebVR presentation.
     * @param webContents The WebContents of the tab to exit WebVR presentation in.
     */
    @Override
    public void endSession(WebContents webContents) {
        runJavaScriptOrFail("vrDisplay.exitPresent()", POLL_TIMEOUT_SHORT_MS, webContents);
    }
}
