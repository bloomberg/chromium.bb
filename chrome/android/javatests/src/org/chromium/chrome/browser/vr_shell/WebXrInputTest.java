// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr_shell;

import static org.chromium.chrome.browser.vr_shell.VrTestFramework.PAGE_LOAD_TIMEOUT_S;
import static org.chromium.chrome.browser.vr_shell.VrTestFramework.POLL_TIMEOUT_SHORT_MS;
import static org.chromium.chrome.test.util.ChromeRestriction.RESTRICTION_TYPE_VIEWER_DAYDREAM;

import android.os.Build;
import android.support.test.filters.MediumTest;

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
import org.chromium.chrome.browser.vr_shell.mock.MockVrDaydreamApi;
import org.chromium.chrome.browser.vr_shell.rules.VrActivityRestriction;
import org.chromium.chrome.browser.vr_shell.util.VrShellDelegateUtils;
import org.chromium.chrome.browser.vr_shell.util.VrTestRuleUtils;
import org.chromium.chrome.browser.vr_shell.util.XrTransitionUtils;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.content.browser.test.util.TouchCommon;

import java.util.List;
import java.util.concurrent.Callable;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeUnit;

/**
 * End-to-end tests for sending input while using WebXR.
 */
@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE, "enable-features=WebXR"})
@MinAndroidSdkLevel(Build.VERSION_CODES.KITKAT) // WebXR is only supported on K+
public class WebXrInputTest {
    @ClassParameter
    private static List<ParameterSet> sClassParams =
            VrTestRuleUtils.generateDefaultVrTestRuleParameters();
    @Rule
    public RuleChain mRuleChain;

    private ChromeActivityTestRule mVrTestRule;
    private XrTestFramework mXrTestFramework;

    public WebXrInputTest(Callable<ChromeActivityTestRule> callable) throws Exception {
        mVrTestRule = callable.call();
        mRuleChain = VrTestRuleUtils.wrapRuleInVrActivityRestrictionRule(mVrTestRule);
    }

    @Before
    public void setUp() throws Exception {
        mXrTestFramework = new XrTestFramework(mVrTestRule);
    }

    /**
     * Tests that screen touches are not registered when in an exclusive session.
     */
    @Test
    @MediumTest
    @DisableIf.
    Build(message = "Flaky on K/L crbug.com/762126", sdk_is_less_than = Build.VERSION_CODES.M)
    @VrActivityRestriction({VrActivityRestriction.SupportedActivity.ALL})
    public void testScreenTapsNotRegistered() throws InterruptedException {
        mXrTestFramework.loadUrlAndAwaitInitialization(
                XrTestFramework.getHtmlTestFile("webxr_test_screen_taps_not_registered"),
                PAGE_LOAD_TIMEOUT_S);
        XrTestFramework.executeStepAndWait(
                "stepVerifyNoInitialTaps()", mXrTestFramework.getFirstTabWebContents());
        XrTransitionUtils.enterPresentationOrFail(mXrTestFramework.getFirstTabCvc());
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
        TouchCommon.singleClickView(mVrTestRule.getActivity().getWindow().getDecorView());
        Assert.assertTrue("VrShellImpl dispatched touches",
                touchRegisteredLatch.await(POLL_TIMEOUT_SHORT_MS, TimeUnit.MILLISECONDS));
        XrTestFramework.executeStepAndWait(
                "stepVerifyNoAdditionalTaps()", mXrTestFramework.getFirstTabWebContents());
        XrTestFramework.endTest(mXrTestFramework.getFirstTabWebContents());
    }

    /**
     * Tests that focus is locked to the device with an exclusive session for the purposes of
     * VR input.
     */
    @Test
    @MediumTest
    @VrActivityRestriction({VrActivityRestriction.SupportedActivity.ALL})
    public void testPresentationLocksFocus() throws InterruptedException {
        mXrTestFramework.loadUrlAndAwaitInitialization(
                XrTestFramework.getHtmlTestFile("webxr_test_presentation_locks_focus"),
                PAGE_LOAD_TIMEOUT_S);
        XrTransitionUtils.enterPresentationOrFail(mXrTestFramework.getFirstTabCvc());
        XrTestFramework.executeStepAndWait(
                "stepSetupFocusLoss()", mXrTestFramework.getFirstTabWebContents());
        XrTestFramework.endTest(mXrTestFramework.getFirstTabWebContents());
    }

    /**
     * Tests that pressing the Daydream controller's 'app' button causes the user to exit a
     * WebXR exclusive session.
     */
    @Test
    @MediumTest
    @Restriction(RESTRICTION_TYPE_VIEWER_DAYDREAM)
    @RetryOnFailure(message = "Very rarely, button press not registered (race condition?)")
    public void testAppButtonExitsPresentation() throws InterruptedException {
        mXrTestFramework.loadUrlAndAwaitInitialization(
                XrTestFramework.getHtmlTestFile("generic_webxr_page"), PAGE_LOAD_TIMEOUT_S);
        XrTransitionUtils.enterPresentationOrFail(mXrTestFramework.getFirstTabCvc());
        EmulatedVrController controller = new EmulatedVrController(mVrTestRule.getActivity());
        controller.pressReleaseAppButton();
        Assert.assertTrue("App button exited WebVR presentation",
                XrTestFramework.pollJavaScriptBoolean("exclusiveSession == null",
                        POLL_TIMEOUT_SHORT_MS, mXrTestFramework.getFirstTabWebContents()));
    }

    /**
     * Verifies that pressing the Daydream controller's 'app' button does not cause the user to exit
     * a WebXR exclusive session when VR browsing is disabled.
     */
    @Test
    @MediumTest
    @Restriction(RESTRICTION_TYPE_VIEWER_DAYDREAM)
    @VrActivityRestriction({VrActivityRestriction.SupportedActivity.ALL})
    @CommandLineFlags.Add({"disable-features=" + ChromeFeatureList.VR_BROWSING})
    public void testAppButtonNoopsWhenBrowsingDisabled()
            throws InterruptedException, ExecutionException {
        appButtonNoopsTestImpl();
    }

    /**
     * Verifies that pressing the Daydream controller's 'app' button does not cause the user to exit
     * a WebXR exclusive session when VR browsing isn't supported by the Activity.
     */
    @Test
    @MediumTest
    @Restriction(RESTRICTION_TYPE_VIEWER_DAYDREAM)
    @VrActivityRestriction({VrActivityRestriction.SupportedActivity.WAA,
            VrActivityRestriction.SupportedActivity.CCT})
    public void
    testAppButtonNoopsWhenBrowsingNotSupported() throws InterruptedException, ExecutionException {
        appButtonNoopsTestImpl();
    }

    private void appButtonNoopsTestImpl() throws InterruptedException, ExecutionException {
        mXrTestFramework.loadUrlAndAwaitInitialization(
                XrTestFramework.getHtmlTestFile("generic_webxr_page"), PAGE_LOAD_TIMEOUT_S);
        XrTransitionUtils.enterPresentationOrFail(mXrTestFramework.getFirstTabCvc());

        MockVrDaydreamApi mockApi = new MockVrDaydreamApi();
        VrShellDelegateUtils.getDelegateInstance().overrideDaydreamApiForTesting(mockApi);

        EmulatedVrController controller = new EmulatedVrController(mVrTestRule.getActivity());
        controller.pressReleaseAppButton();
        Assert.assertFalse("App button left Chrome",
                ThreadUtils.runOnUiThreadBlocking(new Callable<Boolean>() {
                    @Override
                    public Boolean call() throws Exception {
                        return mockApi.getExitFromVrCalled()
                                || mockApi.getLaunchVrHomescreenCalled();
                    }
                }));
        Assert.assertFalse("App button exited WebVR presentation",
                XrTestFramework.pollJavaScriptBoolean("exclusiveSession == null",
                        POLL_TIMEOUT_SHORT_MS, mXrTestFramework.getFirstTabWebContents()));
    }

    /**
     * Verifies that pressing the Daydream controller's 'app' button causes the user to exit
     * a WebXR presentation even when the page is not submitting frames.
     */
    @Test
    @MediumTest
    @Restriction(RESTRICTION_TYPE_VIEWER_DAYDREAM)
    @RetryOnFailure(message = "Very rarely, button press not registered (race condition?)")
    public void testAppButtonAfterPageStopsSubmitting() throws InterruptedException {
        mXrTestFramework.loadUrlAndAwaitInitialization(
                XrTestFramework.getHtmlTestFile("webxr_page_submits_once"), PAGE_LOAD_TIMEOUT_S);
        XrTransitionUtils.enterPresentationOrFail(mXrTestFramework.getFirstTabCvc());
        // Wait for page to stop submitting frames.
        XrTestFramework.waitOnJavaScriptStep(mXrTestFramework.getFirstTabWebContents());
        EmulatedVrController controller = new EmulatedVrController(mVrTestRule.getActivity());
        controller.pressReleaseAppButton();
        Assert.assertTrue("App button exited WebVR presentation",
                XrTestFramework.pollJavaScriptBoolean("exclusiveSession == null",
                        POLL_TIMEOUT_SHORT_MS, mXrTestFramework.getFirstTabWebContents()));
    }
}