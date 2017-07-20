// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr_shell;

import static org.chromium.chrome.browser.vr_shell.VrTestRule.PAGE_LOAD_TIMEOUT_S;
import static org.chromium.chrome.browser.vr_shell.VrTestRule.POLL_CHECK_INTERVAL_SHORT_MS;
import static org.chromium.chrome.browser.vr_shell.VrTestRule.POLL_TIMEOUT_LONG_MS;
import static org.chromium.chrome.browser.vr_shell.VrTestRule.POLL_TIMEOUT_SHORT_MS;
import static org.chromium.chrome.test.util.ChromeRestriction.RESTRICTION_TYPE_VIEWER_DAYDREAM;
import static org.chromium.chrome.test.util.ChromeRestriction.RESTRICTION_TYPE_VIEWER_NON_DAYDREAM;

import android.os.Build;
import android.support.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.base.test.util.Restriction;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.vr_shell.util.CardboardUtils;
import org.chromium.chrome.browser.vr_shell.util.VrTransitionUtils;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

/**
 * End-to-end tests for sending input while using WebVR.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        ChromeActivityTestRule.DISABLE_NETWORK_PREDICTION_FLAG, "enable-webvr"})
@MinAndroidSdkLevel(Build.VERSION_CODES.KITKAT) // WebVR is only supported on K+
public class WebVrInputTest {
    @Rule
    public VrTestRule mVrTestRule = new VrTestRule();

    /**
     * Tests that screen touches are not registered when the viewer is a Daydream View.
     */
    @Test
    @MediumTest
    @Restriction(RESTRICTION_TYPE_VIEWER_DAYDREAM)
    public void testScreenTapsNotRegisteredOnDaydream() throws InterruptedException {
        mVrTestRule.loadUrlAndAwaitInitialization(
                VrTestRule.getHtmlTestFile("test_screen_taps_not_registered_on_daydream"),
                PAGE_LOAD_TIMEOUT_S);
        mVrTestRule.executeStepAndWait(
                "stepVerifyNoInitialTaps()", mVrTestRule.getFirstTabWebContents());
        VrTransitionUtils.enterPresentationAndWait(
                mVrTestRule.getFirstTabCvc(), mVrTestRule.getFirstTabWebContents());
        // Wait on VrShellImpl to say that its parent consumed the touch event
        // Set to 2 because there's an ACTION_DOWN followed by ACTION_UP
        final CountDownLatch touchRegisteredLatch = new CountDownLatch(2);
        ((VrShellImpl) VrShellDelegate.getVrShellForTesting())
                .setOnDispatchTouchEventForTesting(new OnDispatchTouchEventCallback() {
                    @Override
                    public void onDispatchTouchEvent(boolean parentConsumed) {
                        if (!parentConsumed) Assert.fail("Parent did not consume event");
                        touchRegisteredLatch.countDown();
                    }
                });
        CardboardUtils.sendCardboardClick(mVrTestRule.getActivity());
        Assert.assertTrue("VrShellImpl dispatched touches",
                touchRegisteredLatch.await(POLL_TIMEOUT_SHORT_MS, TimeUnit.MILLISECONDS));
        mVrTestRule.executeStepAndWait(
                "stepVerifyNoAdditionalTaps()", mVrTestRule.getFirstTabWebContents());
        mVrTestRule.endTest(mVrTestRule.getFirstTabWebContents());
    }

    /**
     * Tests that Daydream controller clicks are registered as screen taps when the viewer is a
     * Daydream View.
     */
    @Test
    @MediumTest
    @Restriction(RESTRICTION_TYPE_VIEWER_DAYDREAM)
    public void testControllerClicksRegisteredAsTapsOnDaydream() throws InterruptedException {
        EmulatedVrController controller = new EmulatedVrController(mVrTestRule.getActivity());
        mVrTestRule.loadUrlAndAwaitInitialization(
                VrTestRule.getHtmlTestFile("test_screen_taps_registered"), PAGE_LOAD_TIMEOUT_S);
        mVrTestRule.executeStepAndWait(
                "stepVerifyNoInitialTaps()", mVrTestRule.getFirstTabWebContents());
        // Wait to enter VR
        VrTransitionUtils.enterPresentationAndWait(
                mVrTestRule.getFirstTabCvc(), mVrTestRule.getFirstTabWebContents());
        // Send a controller click and wait for JavaScript to receive it
        controller.pressReleaseTouchpadButton();
        mVrTestRule.waitOnJavaScriptStep(mVrTestRule.getFirstTabWebContents());
        mVrTestRule.endTest(mVrTestRule.getFirstTabWebContents());
    }

    /**
     * Tests that screen touches are still registered when the viewer is Cardboard.
     */
    @Test
    @MediumTest
    @Restriction(RESTRICTION_TYPE_VIEWER_NON_DAYDREAM)
    @DisableIf.Build(message = "Flaky on L crbug.com/713781",
            sdk_is_greater_than = Build.VERSION_CODES.KITKAT,
            sdk_is_less_than = Build.VERSION_CODES.M)
    public void testScreenTapsRegisteredOnCardboard() throws InterruptedException {
        mVrTestRule.loadUrlAndAwaitInitialization(
                VrTestRule.getHtmlTestFile("test_screen_taps_registered"), PAGE_LOAD_TIMEOUT_S);
        mVrTestRule.executeStepAndWait(
                "stepVerifyNoInitialTaps()", mVrTestRule.getFirstTabWebContents());
        // Wait to enter VR
        VrTransitionUtils.enterPresentationAndWait(
                mVrTestRule.getFirstTabCvc(), mVrTestRule.getFirstTabWebContents());
        // Tap and wait for JavaScript to receive it
        CardboardUtils.sendCardboardClickAndWait(
                mVrTestRule.getActivity(), mVrTestRule.getFirstTabWebContents());
        mVrTestRule.endTest(mVrTestRule.getFirstTabWebContents());
    }

    /**
     * Tests that focus is locked to the presenting display for purposes of VR input.
     */
    @Test
    @MediumTest
    public void testPresentationLocksFocus() throws InterruptedException {
        mVrTestRule.loadUrlAndAwaitInitialization(
                VrTestRule.getHtmlTestFile("test_presentation_locks_focus"), PAGE_LOAD_TIMEOUT_S);
        VrTransitionUtils.enterPresentationAndWait(
                mVrTestRule.getFirstTabCvc(), mVrTestRule.getFirstTabWebContents());
        mVrTestRule.endTest(mVrTestRule.getFirstTabWebContents());
    }

    /**
     * Verifies that pressing the Daydream controller's 'app' button causes the user to exit
     * WebVR presentation.
     */
    @Test
    @MediumTest
    @Restriction(RESTRICTION_TYPE_VIEWER_DAYDREAM)
    @RetryOnFailure(message = "Very rarely, button press not registered (race condition?)")
    public void testAppButtonExitsPresentation() throws InterruptedException {
        mVrTestRule.loadUrlAndAwaitInitialization(
                mVrTestRule.getHtmlTestFile("generic_webvr_page"), PAGE_LOAD_TIMEOUT_S);
        VrTransitionUtils.enterPresentationOrFail(mVrTestRule.getFirstTabCvc());
        EmulatedVrController controller = new EmulatedVrController(mVrTestRule.getActivity());
        controller.pressReleaseAppButton();
        Assert.assertTrue("App button exited WebVR presentation",
                mVrTestRule.pollJavaScriptBoolean("!vrDisplay.isPresenting", POLL_TIMEOUT_SHORT_MS,
                        mVrTestRule.getFirstTabWebContents()));
    }

    /**
     * Tests that focus loss updates synchronously.
     */
    @Test
    @MediumTest
    @Restriction(RESTRICTION_TYPE_VIEWER_DAYDREAM)
    public void testFocusUpdatesSynchronously() throws InterruptedException {
        mVrTestRule.loadUrlAndAwaitInitialization(
                VrTestRule.getHtmlTestFile("generic_webvr_page_with_activate_listener"),
                PAGE_LOAD_TIMEOUT_S);

        CriteriaHelper.pollUiThread(new Criteria("DisplayActivate was never registered.") {
            @Override
            public boolean isSatisfied() {
                return VrShellDelegate.getInstanceForTesting().isListeningForWebVrActivate();
            }
        }, POLL_TIMEOUT_LONG_MS, POLL_CHECK_INTERVAL_SHORT_MS);
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mVrTestRule.getActivity().getCurrentContentViewCore().onPause();
                Assert.assertTrue(VrShellDelegate.getInstanceForTesting().isClearActivatePending());
            }
        });
    }
}
