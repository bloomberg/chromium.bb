// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr_shell;

import static org.chromium.chrome.browser.vr_shell.VrTestFramework.PAGE_LOAD_TIMEOUT_S;
import static org.chromium.chrome.browser.vr_shell.VrTestFramework.POLL_CHECK_INTERVAL_SHORT_MS;
import static org.chromium.chrome.browser.vr_shell.VrTestFramework.POLL_TIMEOUT_LONG_MS;
import static org.chromium.chrome.browser.vr_shell.VrTestFramework.POLL_TIMEOUT_SHORT_MS;
import static org.chromium.chrome.test.util.ChromeRestriction.RESTRICTION_TYPE_VIEWER_DAYDREAM;
import static org.chromium.chrome.test.util.ChromeRestriction.RESTRICTION_TYPE_VIEWER_NON_DAYDREAM;

import android.os.Build;
import android.os.SystemClock;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;
import android.view.MotionEvent;
import android.view.View;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.params.ParameterAnnotations.ClassParameter;
import org.chromium.base.test.params.ParameterAnnotations.UseRunnerDelegate;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.base.test.util.Restriction;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.vr_shell.util.VrShellDelegateUtils;
import org.chromium.chrome.browser.vr_shell.util.VrTestRuleUtils;
import org.chromium.chrome.browser.vr_shell.util.VrTransitionUtils;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.content.browser.test.util.ClickUtils;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;

import java.util.List;
import java.util.concurrent.Callable;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

/**
 * End-to-end tests for sending input while using WebVR.
 */
@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        ChromeActivityTestRule.DISABLE_NETWORK_PREDICTION_FLAG, "enable-webvr",
        "enable-gamepad-extensions"})
@MinAndroidSdkLevel(Build.VERSION_CODES.KITKAT) // WebVR is only supported on K+
public class WebVrInputTest {
    @ClassParameter
    private static List<ParameterSet> sClassParams =
            VrTestRuleUtils.generateDefaultVrTestRuleParameters();
    @Rule
    public RuleChain mRuleChain;

    private ChromeActivityTestRule mVrTestRule;
    private VrTestFramework mVrTestFramework;

    public WebVrInputTest(Callable<ChromeActivityTestRule> callable) throws Exception {
        mVrTestRule = callable.call();
        mRuleChain = VrTestRuleUtils.wrapRuleInVrActivityRestrictionRule(mVrTestRule);
    }

    @Before
    public void setUp() throws Exception {
        mVrTestFramework = new VrTestFramework(mVrTestRule);
    }

    /**
     * Tests that screen touches are not registered when in VR.
     */
    @Test
    @MediumTest
    @DisableIf.Build(message = "Flaky on K/L crbug.com/762126",
            sdk_is_less_than = Build.VERSION_CODES.M)
    public void testScreenTapsNotRegistered() throws InterruptedException {
        mVrTestFramework.loadUrlAndAwaitInitialization(
                VrTestFramework.getHtmlTestFile("test_screen_taps_not_registered"),
                PAGE_LOAD_TIMEOUT_S);
        VrTestFramework.executeStepAndWait(
                "stepVerifyNoInitialTaps()", mVrTestFramework.getFirstTabWebContents());
        VrTransitionUtils.enterPresentationAndWait(
                mVrTestFramework.getFirstTabCvc(), mVrTestFramework.getFirstTabWebContents());
        // Wait on VrShellImpl to say that its parent consumed the touch event
        // Set to 2 because there's an ACTION_DOWN followed by ACTION_UP
        final CountDownLatch touchRegisteredLatch = new CountDownLatch(2);
        ((VrShellImpl) TestVrShellDelegate.getVrShellForTesting())
                .setOnDispatchTouchEventForTesting(new OnDispatchTouchEventCallback() {
                    @Override
                    public void onDispatchTouchEvent(boolean parentConsumed) {
                        if (!parentConsumed) Assert.fail("Parent did not consume event");
                        touchRegisteredLatch.countDown();
                    }
                });
        ClickUtils.mouseSingleClickView(InstrumentationRegistry.getInstrumentation(),
                mVrTestRule.getActivity().getWindow().getDecorView().getRootView());
        Assert.assertTrue("VrShellImpl dispatched touches",
                touchRegisteredLatch.await(POLL_TIMEOUT_SHORT_MS, TimeUnit.MILLISECONDS));
        VrTestFramework.executeStepAndWait(
                "stepVerifyNoAdditionalTaps()", mVrTestFramework.getFirstTabWebContents());
        VrTestFramework.endTest(mVrTestFramework.getFirstTabWebContents());
    }

    /**
     * Tests that Daydream controller clicks are registered as gamepad button pressed.
     */
    @Test
    @MediumTest
    @Restriction(RESTRICTION_TYPE_VIEWER_DAYDREAM)
    public void testControllerClicksRegisteredOnDaydream() throws InterruptedException {
        EmulatedVrController controller = new EmulatedVrController(mVrTestRule.getActivity());
        mVrTestFramework.loadUrlAndAwaitInitialization(
                VrTestFramework.getHtmlTestFile("test_gamepad_button"), PAGE_LOAD_TIMEOUT_S);
        // Wait to enter VR
        VrTransitionUtils.enterPresentationAndWait(
                mVrTestFramework.getFirstTabCvc(), mVrTestFramework.getFirstTabWebContents());
        // Send a controller click and wait for JavaScript to receive it.
        controller.sendClickButtonToggleEvent();
        VrTestFramework.waitOnJavaScriptStep(mVrTestFramework.getFirstTabWebContents());
        controller.sendClickButtonToggleEvent();
        VrTestFramework.waitOnJavaScriptStep(mVrTestFramework.getFirstTabWebContents());
        VrTestFramework.endTest(mVrTestFramework.getFirstTabWebContents());
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
        mVrTestFramework.loadUrlAndAwaitInitialization(
                VrTestFramework.getHtmlTestFile("test_gamepad_button"), PAGE_LOAD_TIMEOUT_S);
        // Wait to enter VR
        VrTransitionUtils.enterPresentationAndWait(
                mVrTestFramework.getFirstTabCvc(), mVrTestFramework.getFirstTabWebContents());
        int x = mVrTestFramework.getFirstTabCvc().getViewportHeightPix() / 2;
        int y = mVrTestFramework.getFirstTabCvc().getViewportWidthPix() / 2;

        // TODO(mthiesse, crbug.com/758374): Injecting touch events into the root GvrLayout
        // (VrShellImpl) is flaky. Sometimes the events just don't get routed to the presentation
        // view for no apparent reason. We should figure out why this is and see if it's fixable.
        final View presentationView = ((VrShellImpl) TestVrShellDelegate.getVrShellForTesting())
                                              .getPresentationViewForTesting();
        long downTime = SystemClock.uptimeMillis();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                presentationView.dispatchTouchEvent(
                        MotionEvent.obtain(downTime, downTime, MotionEvent.ACTION_DOWN, x, y, 0));
            }
        });
        VrTestFramework.waitOnJavaScriptStep(mVrTestFramework.getFirstTabWebContents());
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                long now = SystemClock.uptimeMillis();
                presentationView.dispatchTouchEvent(
                        MotionEvent.obtain(downTime, now, MotionEvent.ACTION_UP, x, y, 0));
            }
        });
        VrTestFramework.waitOnJavaScriptStep(mVrTestFramework.getFirstTabWebContents());
        VrTestFramework.endTest(mVrTestFramework.getFirstTabWebContents());
    }

    /**
     * Tests that focus is locked to the presenting display for purposes of VR input.
     */
    @Test
    @MediumTest
    public void testPresentationLocksFocus() throws InterruptedException {
        mVrTestFramework.loadUrlAndAwaitInitialization(
                VrTestFramework.getHtmlTestFile("test_presentation_locks_focus"),
                PAGE_LOAD_TIMEOUT_S);
        VrTransitionUtils.enterPresentationAndWait(
                mVrTestFramework.getFirstTabCvc(), mVrTestFramework.getFirstTabWebContents());
        VrTestFramework.endTest(mVrTestFramework.getFirstTabWebContents());
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
        mVrTestFramework.loadUrlAndAwaitInitialization(
                VrTestFramework.getHtmlTestFile("generic_webvr_page"), PAGE_LOAD_TIMEOUT_S);
        VrTransitionUtils.enterPresentationOrFail(mVrTestFramework.getFirstTabCvc());
        EmulatedVrController controller = new EmulatedVrController(mVrTestRule.getActivity());
        controller.pressReleaseAppButton();
        Assert.assertTrue("App button exited WebVR presentation",
                VrTestFramework.pollJavaScriptBoolean("!vrDisplay.isPresenting",
                        POLL_TIMEOUT_SHORT_MS, mVrTestFramework.getFirstTabWebContents()));
    }

    /**
     * Verifies that pressing the Daydream controller's 'app' button does not cause the user to exit
     * WebVR presentation when VR browsing is disabled.
     */
    @Test
    @MediumTest
    @Restriction(RESTRICTION_TYPE_VIEWER_DAYDREAM)
    @CommandLineFlags.Add({"disable-features=" + ChromeFeatureList.VR_SHELL})
    public void testAppButtonNoopsWhenBrowsingDisabled() throws InterruptedException {
        mVrTestFramework.loadUrlAndAwaitInitialization(
                VrTestFramework.getHtmlTestFile("generic_webvr_page"), PAGE_LOAD_TIMEOUT_S);
        VrTransitionUtils.enterPresentationOrFail(mVrTestFramework.getFirstTabCvc());
        EmulatedVrController controller = new EmulatedVrController(mVrTestRule.getActivity());
        controller.pressReleaseAppButton();
        Assert.assertFalse("App button exited WebVR presentation",
                VrTestFramework.pollJavaScriptBoolean("!vrDisplay.isPresenting",
                        POLL_TIMEOUT_SHORT_MS, mVrTestFramework.getFirstTabWebContents()));
    }

    /**
     * Tests that focus loss updates synchronously.
     */
    @Test
    @MediumTest
    @Restriction(RESTRICTION_TYPE_VIEWER_DAYDREAM)
    public void testFocusUpdatesSynchronously() throws InterruptedException {
        mVrTestFramework.loadUrlAndAwaitInitialization(
                VrTestFramework.getHtmlTestFile("generic_webvr_page_with_activate_listener"),
                PAGE_LOAD_TIMEOUT_S);

        CriteriaHelper.pollUiThread(new Criteria("DisplayActivate was never registered.") {
            @Override
            public boolean isSatisfied() {
                return VrShellDelegateUtils.getDelegateInstance().isListeningForWebVrActivate();
            }
        }, POLL_TIMEOUT_LONG_MS, POLL_CHECK_INTERVAL_SHORT_MS);
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mVrTestRule.getActivity().getCurrentContentViewCore().onPause();
                Assert.assertTrue(
                        VrShellDelegateUtils.getDelegateInstance().isClearActivatePending());
            }
        });
    }

    /**
     * Verifies that pressing the Daydream controller's 'app' button causes the user to exit
     * WebVR presentation even when the page is not submitting frames.
     */
    @Test
    @MediumTest
    @Restriction(RESTRICTION_TYPE_VIEWER_DAYDREAM)
    @RetryOnFailure(message = "Very rarely, button press not registered (race condition?)")
    public void testAppButtonAfterPageStopsSubmitting() throws InterruptedException {
        mVrTestFramework.loadUrlAndAwaitInitialization(
                VrTestFramework.getHtmlTestFile("webvr_page_submits_once"), PAGE_LOAD_TIMEOUT_S);
        VrTransitionUtils.enterPresentationOrFail(mVrTestFramework.getFirstTabCvc());
        // Wait for page to stop submitting frames.
        VrTestFramework.waitOnJavaScriptStep(mVrTestFramework.getFirstTabWebContents());
        EmulatedVrController controller = new EmulatedVrController(mVrTestRule.getActivity());
        controller.pressReleaseAppButton();
        Assert.assertTrue("App button exited WebVR presentation",
                VrTestFramework.pollJavaScriptBoolean("!vrDisplay.isPresenting",
                        POLL_TIMEOUT_SHORT_MS, mVrTestFramework.getFirstTabWebContents()));
    }
}
