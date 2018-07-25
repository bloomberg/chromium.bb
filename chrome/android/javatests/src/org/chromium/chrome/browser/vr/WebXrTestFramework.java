// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import org.junit.Assert;

import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.content.browser.test.util.DOMUtils;
import org.chromium.content_public.browser.WebContents;

import java.util.concurrent.TimeoutException;

/**
 * Extension of XrTestFramework meant for testing XR-related web APIs.
 */
public abstract class WebXrTestFramework extends XrTestFramework {
    /**
     * Must be constructed after the rule has been applied (e.g. in whatever method is
     * tagged with @Before).
     */
    public WebXrTestFramework(ChromeActivityTestRule rule) {
        super(rule);
    }

    /**
     * Checks whether an XRDevice was actually found. Needs to be non-static despite not using any
     * member variables in order for the WebContents-less helper version to work properly in
     * subclasses.
     *
     * @param webContents The WebContents to run the JavaScript through.
     * @return Whether an XRDevice was found.
     */
    public boolean xrDeviceFound(WebContents webContents) {
        return !runJavaScriptOrFail("xrDevice", POLL_TIMEOUT_SHORT_MS, webContents).equals("null");
    }

    /**
     * Helper function to run xrDeviceFound with the first tab's WebContents.
     *
     * @return Whether an XRDevice was found.
     */
    public boolean xrDeviceFound() {
        return xrDeviceFound(mFirstTabWebContents);
    }

    /**
     * Enters a WebXR or WebVR session of some kind by tapping on the canvas on the page. Needs to
     * be non-static despite not using any member variables in order for the WebContents-less helper
     * version to work properly in subclasses.
     *
     * @param webContents The WebContents for the tab the canvas is in.
     */
    public void enterSessionWithUserGesture(WebContents webContents) {
        try {
            DOMUtils.clickNode(webContents, "webgl-canvas", false /* goThroughRootAndroidView */);
        } catch (InterruptedException | TimeoutException e) {
            Assert.fail("Failed to click canvas to enter session: " + e.toString());
        }
    }

    /**
     * Helper function to run enterSessionWithUserGesture using the first tab's WebContents.
     */
    public void enterSessionWithUserGesture() {
        enterSessionWithUserGesture(mFirstTabWebContents);
    }

    /**
     * Enters a WebXR or WebVR session of some kind and waits until the page reports it is finished
     * with its JavaScript step. Needs to be non-static despite not using any member variables in
     * order for the WebContents-less helper version to work properly in subclasses.
     *
     * @param webContents The WebContents for the tab to enter the session in.
     */
    public void enterSessionWithUserGestureAndWait(WebContents webContents) {
        enterSessionWithUserGesture(webContents);
        waitOnJavaScriptStep(webContents);
    }

    /**
     * Helper function to run enterSessionWithUserGestureAndWait with the first tab's WebContents.
     */
    public void enterSessionWithUserGestureAndWait() {
        enterSessionWithUserGestureAndWait(mFirstTabWebContents);
    }

    /**
     * Attempts to enter a WebXR or WebVR session of some kind, failing if it is unable to.
     *
     * @param webContents The WebContents for the tab to enter the session in.
     */
    public abstract void enterSessionWithUserGestureOrFail(WebContents webContents);

    /**
     * Helper function to run enterSessionWithUserGestureOrFail with the first tab's WebContents.
     */
    public void enterSessionWithUserGestureOrFail() {
        enterSessionWithUserGestureOrFail(mFirstTabWebContents);
    }

    /**
     * Ends whatever type of session a subclass enters with enterSessionWithUserGesture.
     *
     * @param webContents The WebContents to end the session in
     */
    public abstract void endSession(WebContents webContents);

    /**
     * Helper function to run endSession with the first tab's WebContents.
     */
    public void endSession() {
        endSession(mFirstTabWebContents);
    }
}
