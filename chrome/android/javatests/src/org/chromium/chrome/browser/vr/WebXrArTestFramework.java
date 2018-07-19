// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import android.content.DialogInterface;

import org.junit.Assert;

import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.permissions.PermissionDialogController;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.WebContents;

/**
 * WebXR for AR-specific implementation of the WebXrTestFramework.
 */
public class WebXrArTestFramework extends WebXrTestFramework {
    /**
     * Checks whether an AR session request would prompt the user for Camera permissions.
     * @param webContents The WebContents to check for the permission in
     * @return True if an AR session request will cause a permission prompt, false otherwise.
     */
    public static boolean arSessionRequestWouldTriggerPermissionPrompt(WebContents webContents) {
        runJavaScriptOrFail("checkIfArSessionWouldTriggerPermissionPrompt()", POLL_TIMEOUT_SHORT_MS,
                webContents);
        Assert.assertTrue(
                pollJavaScriptBoolean("arSessionRequestWouldTriggerPermissionPrompt !== null",
                        POLL_TIMEOUT_SHORT_MS, webContents));
        return Boolean.valueOf(runJavaScriptOrFail("arSessionRequestWouldTriggerPermissionPrompt",
                POLL_TIMEOUT_SHORT_MS, webContents));
    }

    /**
     * Must be constructed after the rule has been applied (e.g. in whatever method is
     * tagged with @Before)
     */
    public WebXrArTestFramework(ChromeActivityTestRule rule) {
        super(rule);
    }

    /**
     * Requests an AR session, automatically accepting the Camera permission prompt if necessary.
     * Causes a test failure if it is unable to do so.
     * @param webContents The Webcontents to start the AR session in.
     */
    @Override
    public void enterSessionWithUserGestureOrFail(WebContents webContents) {
        runJavaScriptOrFail(
                "sessionTypeToRequest = sessionTypes.AR", POLL_TIMEOUT_LONG_MS, webContents);
        // Requesting an AR session for the first time on a page will always prompt for camera
        // permissions, but not on subsequent requests, so check to see if we'll need to accept it
        // after requesting the session.
        boolean expectPermissionPrompt = arSessionRequestWouldTriggerPermissionPrompt(webContents);
        // TODO(bsheedy): Rename enterPresentation since it's used for both presentation and AR?
        enterSessionWithUserGesture(webContents);
        if (expectPermissionPrompt) {
            // Wait for the permission prompt to appear.
            CriteriaHelper.pollUiThread(new Criteria() {
                @Override
                public boolean isSatisfied() {
                    return PermissionDialogController.getInstance().getCurrentDialogForTesting()
                            != null;
                }
            });
            // Accept the permission prompt.
            ThreadUtils.runOnUiThreadBlocking(() -> {
                PermissionDialogController.getInstance()
                        .getCurrentDialogForTesting()
                        .getButton(DialogInterface.BUTTON_POSITIVE)
                        .performClick();
            });
        }
        Assert.assertTrue(
                pollJavaScriptBoolean("sessionInfos[sessionTypes.AR].currentSession != null",
                        POLL_TIMEOUT_LONG_MS, webContents));
    }

    /**
     * Helper function to run arSessionRequestWouldTriggerPermissionPrompt with the first tab's
     * WebContents.
     * @return True if an AR session request will cause a permission prompt, false otherwise
     */
    public boolean arSessionRequestWouldTriggerPermissionPrompt() {
        return arSessionRequestWouldTriggerPermissionPrompt(mFirstTabWebContents);
    }

    /**
     * Exits a WebXR AR session.
     * @param webcontents The WebContents to exit the AR session in
     */
    @Override
    public void endSession(WebContents webContents) {
        runJavaScriptOrFail("sessionInfos[sessionTypes.AR].currentSession.end()",
                POLL_TIMEOUT_SHORT_MS, webContents);
    }
}