// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr_shell.util;

import static org.chromium.chrome.browser.vr_shell.VrTestFramework.POLL_CHECK_INTERVAL_SHORT_MS;
import static org.chromium.chrome.browser.vr_shell.VrTestFramework.POLL_TIMEOUT_LONG_MS;

import android.app.Activity;
import android.content.Intent;
import android.net.Uri;

import com.google.vr.ndk.base.DaydreamApi;

import org.junit.Assert;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.vr_shell.TestVrShellDelegate;
import org.chromium.chrome.browser.vr_shell.VrClassesWrapperImpl;
import org.chromium.chrome.browser.vr_shell.VrIntentUtils;
import org.chromium.chrome.browser.vr_shell.VrShellDelegate;
import org.chromium.chrome.browser.vr_shell.VrTestFramework;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.DOMUtils;
import org.chromium.content_public.browser.WebContents;

import java.util.concurrent.Callable;
import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicBoolean;

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
                VrShellDelegateUtils.getDelegateInstance().shutdownVr(
                        true /* disableVrMode */, true /* stayingInChrome */);
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
                return VrShellDelegateUtils.getDelegateInstance().isVrEntryComplete();
            }
        }), timeout, POLL_CHECK_INTERVAL_SHORT_MS);
    }

    /**
     * Sends a click event directly to the WebGL canvas, triggering its onclick
     * that by default calls WebVR's requestPresent. Will have a similar result to
     * CardboardUtils.sendCardboardClick if not already presenting, but less prone
     * to errors, e.g. if there's dialog in the center of the screen blocking the canvas.
     *
     * Only meant to be used alongside the test framework from VrTestFramework.
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
     * Only meant to be used alongside the test framework from VrTestFramework.
     * @param cvc The ContentViewCore for the tab the canvas is in.
     * @param webContents The WebContents for the tab the JavaScript step is in.
     */
    public static void enterPresentationAndWait(ContentViewCore cvc, WebContents webContents) {
        enterPresentation(cvc);
        VrTestFramework.waitOnJavaScriptStep(webContents);
    }

    /**
     * Sends a click event directly to the WebGL canvas then waits for WebVR to
     * think that it is presenting, failing if this does not occur within the
     * allotted time.
     *
     * @param cvc The ContentViewCore for the tab the canvas is in.
     */
    public static void enterPresentationOrFail(ContentViewCore cvc) {
        enterPresentation(cvc);
        Assert.assertTrue(VrTestFramework.pollJavaScriptBoolean(
                "vrDisplay.isPresenting", POLL_TIMEOUT_LONG_MS, cvc.getWebContents()));
        Assert.assertTrue(TestVrShellDelegate.getVrShellForTesting().getWebVrModeEnabled());
    }

    /**
     * @return Whether the VR back button is enabled.
     */
    public static Boolean isBackButtonEnabled() {
        final AtomicBoolean isBackButtonEnabled = new AtomicBoolean();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                isBackButtonEnabled.set(
                        TestVrShellDelegate.getVrShellForTesting().isBackButtonEnabled());
            }
        });
        return isBackButtonEnabled.get();
    }

    /**
     * @return Whether the VR forward button is enabled.
     */
    public static Boolean isForwardButtonEnabled() {
        final AtomicBoolean isForwardButtonEnabled = new AtomicBoolean();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                isForwardButtonEnabled.set(
                        TestVrShellDelegate.getVrShellForTesting().isForwardButtonEnabled());
            }
        });
        return isForwardButtonEnabled.get();
    }

    /**
     * Navigates VrShell back.
     */
    public static void navigateBack() {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                TestVrShellDelegate.getVrShellForTesting().navigateBack();
            }
        });
    }

    /**
     * Navigates VrShell forward.
     */
    public static void navigateForward() {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                TestVrShellDelegate.getVrShellForTesting().navigateForward();
            }
        });
    }

    /**
     * Sends an intent to Chrome telling it to autopresent the given URL. This
     * is expected to fail unless the trusted intent check is disabled in VrShellDelegate.
     *
     * @param url String containing the URL to open
     * @param activity The activity to launch the intent from
     */
    public static void sendDaydreamAutopresentIntent(String url, final Activity activity) {
        // Create an intent that will launch Chrome at the specified URL with autopresent
        final Intent intent =
                new Intent(ContextUtils.getApplicationContext(), ChromeLauncherActivity.class);
        intent.setData(Uri.parse(url));
        intent.putExtra(VrIntentUtils.DAYDREAM_VR_EXTRA, true);
        DaydreamApi.setupVrIntent(intent);
        intent.removeCategory("com.google.intent.category.DAYDREAM");

        final VrClassesWrapperImpl wrapper = new VrClassesWrapperImpl();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                wrapper.createVrDaydreamApi(activity).launchInVr(intent);
            }
        });
    }
}
