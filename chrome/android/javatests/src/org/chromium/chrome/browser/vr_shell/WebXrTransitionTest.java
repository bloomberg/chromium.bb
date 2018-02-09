// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr_shell;

import static org.chromium.chrome.browser.vr_shell.TestFramework.PAGE_LOAD_TIMEOUT_S;
import static org.chromium.chrome.browser.vr_shell.XrTestFramework.PAGE_LOAD_TIMEOUT_S;
import static org.chromium.chrome.browser.vr_shell.XrTestFramework.POLL_CHECK_INTERVAL_SHORT_MS;
import static org.chromium.chrome.browser.vr_shell.XrTestFramework.POLL_TIMEOUT_LONG_MS;
import static org.chromium.chrome.browser.vr_shell.XrTestFramework.POLL_TIMEOUT_SHORT_MS;
import static org.chromium.chrome.test.util.ChromeRestriction.RESTRICTION_TYPE_DON_ENABLED;
import static org.chromium.chrome.test.util.ChromeRestriction.RESTRICTION_TYPE_VIEWER_DAYDREAM;

import android.os.Build;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;
import android.support.test.uiautomator.UiDevice;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.runner.RunWith;

import org.chromium.base.test.params.ParameterAnnotations.ClassParameter;
import org.chromium.base.test.params.ParameterAnnotations.UseRunnerDelegate;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.vr_shell.rules.VrActivityRestriction;
import org.chromium.chrome.browser.vr_shell.util.VrTestRuleUtils;
import org.chromium.chrome.browser.vr_shell.util.XrTransitionUtils;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;

import java.util.List;
import java.util.concurrent.Callable;
import java.util.concurrent.TimeoutException;

/**
 * end-to-end tests for transitioning between WebXR's magic window (non-exclusive session) and
 * presentation (exclusive session) modes.
 */
@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE, "enable-features=WebXR"})
@MinAndroidSdkLevel(Build.VERSION_CODES.KITKAT) // WebXR is only supported on K+
public class WebXrTransitionTest {
    @ClassParameter
    private static List<ParameterSet> sClassParams =
            VrTestRuleUtils.generateDefaultVrTestRuleParameters();
    @Rule
    public RuleChain mRuleChain;

    private ChromeActivityTestRule mVrTestRule;
    private XrTestFramework mXrTestFramework;

    public WebXrTransitionTest(Callable<ChromeActivityTestRule> callable) throws Exception {
        mVrTestRule = callable.call();
        mRuleChain = VrTestRuleUtils.wrapRuleInVrActivityRestrictionRule(mVrTestRule);
    }

    @Before
    public void setUp() throws Exception {
        mXrTestFramework = new XrTestFramework(mVrTestRule);
    }

    /**
     * Tests that a successful request for an exclusive session actually enters VR.
     */
    @Test
    @MediumTest
    @VrActivityRestriction({VrActivityRestriction.SupportedActivity.ALL})
    public void testRequestSessionEntersVr() throws InterruptedException {
        mXrTestFramework.loadUrlAndAwaitInitialization(
                XrTestFramework.getHtmlTestFile("generic_webxr_page"), PAGE_LOAD_TIMEOUT_S);
        XrTransitionUtils.enterPresentationOrFail(mXrTestFramework.getFirstTabCvc());
        Assert.assertTrue("VrShellDelegate is in VR", VrShellDelegate.isInVr());
    }

    /**
     * Tests that WebXR is not exposed if the flag is not on and the page does
     * not have an origin trial token.
     */
    @Test
    @MediumTest
    @CommandLineFlags.Remove({"enable-features=WebXR"})
    @VrActivityRestriction({VrActivityRestriction.SupportedActivity.ALL})
    public void testWebXrDisabledWithoutFlagSet() throws InterruptedException {
        // TODO(bsheedy): Remove this test once WebXR is on by default without
        // requiring an origin trial.
        mXrTestFramework.loadUrlAndAwaitInitialization(
                XrTestFramework.getHtmlTestFile("test_webxr_disabled_without_flag_set"),
                PAGE_LOAD_TIMEOUT_S);
        XrTestFramework.waitOnJavaScriptStep(mXrTestFramework.getFirstTabWebContents());
        XrTestFramework.endTest(mXrTestFramework.getFirstTabWebContents());
    }

    /**
     * Tests that the exclusive session promise doesn't resolve if the DON flow is
     * not completed.
     */
    @Test
    @MediumTest
    @Restriction({RESTRICTION_TYPE_VIEWER_DAYDREAM, RESTRICTION_TYPE_DON_ENABLED})
    @VrActivityRestriction({VrActivityRestriction.SupportedActivity.ALL})
    public void testPresentationPromiseUnresolvedDuringDon() throws InterruptedException {
        mXrTestFramework.loadUrlAndAwaitInitialization(
                XrTestFramework.getHtmlTestFile(
                        "webxr_test_presentation_promise_unresolved_during_don"),
                PAGE_LOAD_TIMEOUT_S);
        XrTransitionUtils.enterPresentationAndWait(
                mXrTestFramework.getFirstTabCvc(), mXrTestFramework.getFirstTabWebContents());
        XrTestFramework.endTest(mXrTestFramework.getFirstTabWebContents());
    }

    /**
     * Tests that the exclusive session promise is rejected if the DON flow is canceled.
     */
    @Test
    @MediumTest
    @Restriction({RESTRICTION_TYPE_VIEWER_DAYDREAM, RESTRICTION_TYPE_DON_ENABLED})
    @VrActivityRestriction({VrActivityRestriction.SupportedActivity.ALL})
    public void testPresentationPromiseRejectedIfDonCanceled() throws InterruptedException {
        mXrTestFramework.loadUrlAndAwaitInitialization(
                XrTestFramework.getHtmlTestFile(
                        "webxr_test_presentation_promise_rejected_if_don_canceled"),
                PAGE_LOAD_TIMEOUT_S);
        final UiDevice uiDevice =
                UiDevice.getInstance(InstrumentationRegistry.getInstrumentation());
        XrTransitionUtils.enterPresentation(mXrTestFramework.getFirstTabCvc());
        // Wait until the DON flow appears to be triggered
        // TODO(bsheedy): Make this less hacky if there's ever an explicit way to check if the
        // DON flow is currently active https://crbug.com/758296
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return uiDevice.getCurrentPackageName().equals("com.google.vr.vrcore");
            }
        }, POLL_TIMEOUT_LONG_MS, POLL_CHECK_INTERVAL_SHORT_MS);
        uiDevice.pressBack();
        TestFramework.waitOnJavaScriptStep(mXrTestFramework.getFirstTabWebContents());
        TestFramework.endTest(mXrTestFramework.getFirstTabWebContents());
    }

    /**
     * Tests that the omnibox reappears after exiting an exclusive session.
     */
    @Test
    @MediumTest
    public void testControlsVisibleAfterExitingVr() throws InterruptedException {
        mXrTestFramework.loadUrlAndAwaitInitialization(
                XrTestFramework.getHtmlTestFile("generic_webxr_page"), PAGE_LOAD_TIMEOUT_S);
        XrTransitionUtils.enterPresentationOrFail(mXrTestFramework.getFirstTabCvc());
        XrTransitionUtils.forceExitVr();
        // The hiding of the controls may only propagate after VR has exited, so give it a chance
        // to propagate. In the worst case this test will erroneously pass, but should never
        // erroneously fail, and should only be flaky if omnibox showing is broken.
        Thread.sleep(100);
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                ChromeActivity activity = mXrTestFramework.getRule().getActivity();
                return activity.getFullscreenManager().getBrowserControlHiddenRatio() == 0.0;
            }
        }, POLL_TIMEOUT_SHORT_MS, POLL_CHECK_INTERVAL_SHORT_MS);
    }

    /**
     * Tests that window.requestAnimationFrame stops firing while in a WebXR exclusive session, but
     * resumes afterwards.
     */
    @Test
    @MediumTest
    @VrActivityRestriction({VrActivityRestriction.SupportedActivity.ALL})
    public void testWindowRafStopsFiringWhilePresenting() throws InterruptedException {
        mXrTestFramework.loadUrlAndAwaitInitialization(
                XrTestFramework.getHtmlTestFile(
                        "webxr_test_window_raf_stops_firing_during_exclusive_session"),
                PAGE_LOAD_TIMEOUT_S);
        XrTestFramework.executeStepAndWait(
                "stepVerifyBeforePresent()", mXrTestFramework.getFirstTabWebContents());
        XrTransitionUtils.enterPresentationOrFail(mXrTestFramework.getFirstTabCvc());
        XrTestFramework.executeStepAndWait(
                "stepVerifyDuringPresent()", mXrTestFramework.getFirstTabWebContents());
        XrTransitionUtils.forceExitVr();
        XrTestFramework.executeStepAndWait(
                "stepVerifyAfterPresent()", mXrTestFramework.getFirstTabWebContents());
        XrTestFramework.endTest(mXrTestFramework.getFirstTabWebContents());
    }

    /**
     * Tests renderer crashes while in WebXR presentation stay in VR.
     */
    @Test
    @MediumTest
    @Restriction(RESTRICTION_TYPE_VIEWER_DAYDREAM)
    @VrActivityRestriction({VrActivityRestriction.SupportedActivity.CTA})
    public void testRendererKilledInWebXrStaysInVr()
            throws IllegalArgumentException, InterruptedException, TimeoutException {
        mXrTestFramework.loadUrlAndAwaitInitialization(
                VrTestFramework.getHtmlTestFile("generic_webxr_page"), PAGE_LOAD_TIMEOUT_S);
        XrTransitionUtils.enterPresentationOrFail(mXrTestFramework.getFirstTabCvc());

        mXrTestFramework.simulateRendererKilled();
        Assert.assertTrue("Browser is in VR", VrShellDelegate.isInVr());
    }
}
