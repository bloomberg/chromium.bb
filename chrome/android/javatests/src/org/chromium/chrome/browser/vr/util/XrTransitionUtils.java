// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr.util;

import static org.chromium.chrome.browser.vr.XrTestFramework.POLL_TIMEOUT_LONG_MS;
import static org.chromium.chrome.browser.vr.XrTestFramework.POLL_TIMEOUT_SHORT_MS;

import android.content.DialogInterface;

import org.junit.Assert;

import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.permissions.PermissionDialogController;
import org.chromium.chrome.browser.vr.TestVrShellDelegate;
import org.chromium.chrome.browser.vr.XrTestFramework;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.WebContents;

/**
 * Class containing utility functions for transitioning between different
 * states in WebXR, e.g. entering immersive or AR sessions.
 */
public class XrTransitionUtils extends TransitionUtils {
    /**
     * WebXR version of enterPresentationOrFail since the condition to check is different between
     * the two APIs.
     */
    public static void enterPresentationOrFail(WebContents webContents) {
        XrTestFramework.runJavaScriptOrFail(
                "sessionTypeToRequest = sessionTypes.IMMERSIVE", POLL_TIMEOUT_LONG_MS, webContents);
        enterPresentation(webContents);
        Assert.assertTrue(XrTestFramework.pollJavaScriptBoolean(
                "sessionInfos[sessionTypes.IMMERSIVE].currentSession != null", POLL_TIMEOUT_LONG_MS,
                webContents));
        Assert.assertTrue(TestVrShellDelegate.getVrShellForTesting().getWebVrModeEnabled());
    }

    /**
     * Requests that WebXR start an AR session, failing the test if it does not succeed.
     * @param webContents The WebContents to run start the AR session in.
     */
    public static void enterArSessionOrFail(WebContents webContents) {
        XrTestFramework.runJavaScriptOrFail(
                "sessionTypeToRequest = sessionTypes.AR", POLL_TIMEOUT_LONG_MS, webContents);
        // Requesting an AR session for the first time on a page will always prompt for camera
        // permissions, but not on subsequent requests, so check to see if we'll need to accept it
        // after requesting the session.
        boolean expectPermissionPrompt = arSessionRequestWouldTriggerPermissionPrompt(webContents);
        // TODO(bsheedy): Rename enterPresentation since it's used for both presentation and AR?
        enterPresentation(webContents);
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
        Assert.assertTrue(XrTestFramework.pollJavaScriptBoolean(
                "sessionInfos[sessionTypes.AR].currentSession != null", POLL_TIMEOUT_LONG_MS,
                webContents));
    }

    /**
     * Ends the current AR session.
     * @param webContents The WebContents to end the AR session in.
     */
    public static void endArSession(WebContents webContents) {
        XrTestFramework.runJavaScriptOrFail("sessionInfos[sessionTypes.AR].currentSession.end()",
                POLL_TIMEOUT_SHORT_MS, webContents);
    }

    /**
     * Checks whether an AR session request would trigger a Camera permission prompt.
     * @param webContents The WebContents to check permissions in.
     * @return True if an AR session request will cause a permission prompt, false otherwise.
     */
    public static boolean arSessionRequestWouldTriggerPermissionPrompt(WebContents webContents) {
        XrTestFramework.runJavaScriptOrFail("checkIfArSessionWouldTriggerPermissionPrompt()",
                POLL_TIMEOUT_SHORT_MS, webContents);
        Assert.assertTrue(XrTestFramework.pollJavaScriptBoolean(
                "arSessionRequestWouldTriggerPermissionPrompt !== null", POLL_TIMEOUT_SHORT_MS,
                webContents));
        return Boolean.valueOf(
                XrTestFramework.runJavaScriptOrFail("arSessionRequestWouldTriggerPermissionPrompt",
                        POLL_TIMEOUT_SHORT_MS, webContents));
    }
}
