// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr_shell.util;

import static org.chromium.chrome.browser.vr_shell.VrTestRule.POLL_CHECK_INTERVAL_SHORT_MS;

import org.junit.Assert;

import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.vr_shell.VrShellDelegate;
import org.chromium.chrome.browser.vr_shell.VrTestRule;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.DOMUtils;
import org.chromium.content_public.browser.WebContents;

import java.util.concurrent.Callable;
import java.util.concurrent.TimeoutException;

/**
 * Class containing utility functions for transitioning between different
 * states in VR, such as fullscreen, WebVR presentation, and the VR browser.
 *
 * All the transitions in this class are performed directly through Chrome,
 * as opposed to NFC tag simulation which involves receiving an intent from
 * an outside application (VR Services).
 */
public class VrTransitionUtils {
    /**
     * Forces the browser into VR mode via a VrShellDelegate call.
     */
    public static void forceEnterVr() {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                VrShellDelegate.enterVrIfNecessary();
            }
        });
    }

    /**
     * Forces the browser out of VR mode via a VrShellDelegate call.
     */
    public static void forceExitVr() {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                VrShellDelegateUtils.getDelegateInstance().shutdownVr(true /* disableVrMode */,
                        false /* canReenter */, true /* stayingInChrome */);
            }
        });
    }

    /**
     * Waits until the given VrShellDelegate's isInVR() returns true. Should
     * only be used when VR support is expected.
     * @param timeout How long to wait before giving up, in milliseconds
     */
    public static void waitForVrEntry(final int timeout) {
        // If VR Shell is supported, mInVr should eventually go to true
        // Relatively long timeout because sometimes GVR takes a while to enter VR
        CriteriaHelper.pollUiThread(Criteria.equals(true, new Callable<Boolean>() {
            @Override
            public Boolean call() {
                return VrShellDelegate.isInVr();
            }
        }), timeout, POLL_CHECK_INTERVAL_SHORT_MS);
    }

    /**
     * Sends a click event directly to the WebGL canvas, triggering its onclick
     * that by default calls WebVR's requestPresent. Will have a similar result to
     * CardboardUtils.sendCardboardClick if not already presenting, but less prone
     * to errors, e.g. if there's dialog in the center of the screen blocking the canvas.
     *
     * Only meant to be used alongside the test framework from VrTestRule.
     * @param cvc The ContentViewCore for the tab the canvas is in.
     */
    public static void enterPresentation(ContentViewCore cvc) {
        try {
            DOMUtils.clickNode(cvc, "webgl-canvas");
        } catch (InterruptedException | TimeoutException e) {
            Assert.fail("Failed to click canvas to enter presentation: " + e.toString());
        }
    }

    /**
     * Sends a click event directly to the WebGL canvas then waits for the
     * JavaScript step to finish.
     *
     * Only meant to be used alongside the test framework from VrTestRule.
     * @param cvc The ContentViewCore for the tab the canvas is in.
     * @param webContents The WebContents for the tab the JavaScript step is in.
     */
    public static void enterPresentationAndWait(ContentViewCore cvc, WebContents webContents) {
        enterPresentation(cvc);
        VrTestRule.waitOnJavaScriptStep(webContents);
    }
}
