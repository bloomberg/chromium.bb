// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import static org.chromium.chrome.browser.vr.XrTestFramework.PAGE_LOAD_TIMEOUT_S;
import static org.chromium.chrome.browser.vr.XrTestFramework.POLL_TIMEOUT_SHORT_MS;
import static org.chromium.chrome.test.util.ChromeRestriction.RESTRICTION_TYPE_SVR;
import static org.chromium.chrome.test.util.ChromeRestriction.RESTRICTION_TYPE_VIEWER_DAYDREAM;

import android.graphics.PointF;
import android.os.Build;
import android.support.test.filters.MediumTest;

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
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.vr.rules.XrActivityRestriction;
import org.chromium.chrome.browser.vr.util.NativeUiUtils;
import org.chromium.chrome.browser.vr.util.NfcSimUtils;
import org.chromium.chrome.browser.vr.util.VrShellDelegateUtils;
import org.chromium.chrome.browser.vr.util.VrTestRuleUtils;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;

import java.util.List;
import java.util.concurrent.Callable;

/**
 * End-to-end tests for WebVR where the choice of test device has a greater
 * impact than the usual Daydream-ready vs. non-Daydream-ready effect.
 */
@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        "enable-features=LogJsConsoleMessages", "enable-webvr"})
@MinAndroidSdkLevel(Build.VERSION_CODES.LOLLIPOP) // WebVR is only supported on L+
public class WebXrVrDeviceTest {
    @ClassParameter
    private static List<ParameterSet> sClassParams =
            VrTestRuleUtils.generateDefaultTestRuleParameters();
    @Rule
    public RuleChain mRuleChain;

    private ChromeActivityTestRule mTestRule;
    private WebXrVrTestFramework mWebXrVrTestFramework;
    private WebVrTestFramework mWebVrTestFramework;

    public WebXrVrDeviceTest(Callable<ChromeActivityTestRule> callable) throws Exception {
        mTestRule = callable.call();
        mRuleChain = VrTestRuleUtils.wrapRuleInActivityRestrictionRule(mTestRule);
    }

    @Before
    public void setUp() throws Exception {
        mWebXrVrTestFramework = new WebXrVrTestFramework(mTestRule);
        mWebVrTestFramework = new WebVrTestFramework(mTestRule);
    }

    /**
     * Tests that the reported WebVR capabilities match expectations on the devices the WebVR tests
     * are run on continuously.
     */
    @Test
    @MediumTest
    @XrActivityRestriction({XrActivityRestriction.SupportedActivity.ALL})
    public void testDeviceCapabilitiesMatchExpectations() throws InterruptedException {
        mWebVrTestFramework.loadUrlAndAwaitInitialization(
                WebVrTestFramework.getFileUrlForHtmlTestFile(
                        "test_device_capabilities_match_expectations"),
                PAGE_LOAD_TIMEOUT_S);
        mWebVrTestFramework.executeStepAndWait(
                "stepCheckDeviceCapabilities('" + Build.DEVICE + "')");
        mWebVrTestFramework.endTest();
    }

    /**
     * Tests that the magic-window-only GVR-less implementation causes a VRDisplay to be present
     * when GVR isn't present and has expected capabilities.
     */
    @Test
    @MediumTest
    @XrActivityRestriction({XrActivityRestriction.SupportedActivity.ALL})
    @Restriction(RESTRICTION_TYPE_SVR)
    public void testGvrlessMagicWindowCapabilities() throws InterruptedException {
        // Make Chrome think that VrCore is not installed
        VrShellDelegateUtils.setVrCoreCompatibility(VrCoreCompatibility.VR_NOT_AVAILABLE);

        mWebVrTestFramework.loadUrlAndAwaitInitialization(
                WebVrTestFramework.getFileUrlForHtmlTestFile(
                        "test_device_capabilities_match_expectations"),
                PAGE_LOAD_TIMEOUT_S);
        Assert.assertTrue(mWebVrTestFramework.xrDeviceFound());
        mWebVrTestFramework.executeStepAndWait(
                "stepCheckDeviceCapabilities('VR Orientation Device')");
        mWebVrTestFramework.endTest();
        VrShellDelegateUtils.getDelegateInstance().overrideVrCoreVersionCheckerForTesting(null);
    }

    /**
     * Tests that reported WebXR capabilities match expectations.
     */
    @Test
    @MediumTest
    @CommandLineFlags
            .Remove({"enable-webvr"})
            @CommandLineFlags.Add({"enable-features=WebXR"})
            @XrActivityRestriction({XrActivityRestriction.SupportedActivity.ALL})
            public void testWebXrCapabilities() throws InterruptedException {
        mWebXrVrTestFramework.loadUrlAndAwaitInitialization(
                WebXrVrTestFramework.getFileUrlForHtmlTestFile("test_webxr_capabilities"),
                PAGE_LOAD_TIMEOUT_S);
        mWebXrVrTestFramework.executeStepAndWait("stepCheckCapabilities('Daydream')");
        mWebXrVrTestFramework.endTest();
    }

    /**
     * Tests that WebXR returns null poses while in VR browsing mode, and valid ones otherwise.
     * Specific steps:
     *   * Enter immersive mode by clicking on 'Enter VR' button displayed on a VR content page
     *   * Check for non-null poses
     *   * Enter inline VR mode by clicking the 'app' button on the controller
     *   * Check for null poses
     */
    @Test
    @MediumTest
    @CommandLineFlags
            .Remove({"enable-webvr"})
            @CommandLineFlags.Add({"enable-features=WebXR"})
            @Restriction(RESTRICTION_TYPE_VIEWER_DAYDREAM)
            public void testForNullPosesInInlineVrPostImmersive() throws InterruptedException {
        mWebXrVrTestFramework.loadUrlAndAwaitInitialization(
                WebXrVrTestFramework.getFileUrlForHtmlTestFile("test_inline_vr_poses"),
                PAGE_LOAD_TIMEOUT_S);
        mWebXrVrTestFramework.enterSessionWithUserGestureOrFail();
        Assert.assertTrue("Browser did not enter VR", VrShellDelegate.isInVr());

        mWebXrVrTestFramework.executeStepAndWait("posesTurnedOnStep()");

        mWebXrVrTestFramework.executeStepAndWait("resetCounters()");
        NativeUiUtils.clickAppButton(UserFriendlyElementName.NONE, new PointF());
        mWebXrVrTestFramework.pollJavaScriptBoolean(
                "sessionInfos[sessionTypes.IMMERSIVE].currentSession == null",
                POLL_TIMEOUT_SHORT_MS);

        mWebXrVrTestFramework.executeStepAndWait("posesTurnedOffStep()");

        mWebXrVrTestFramework.executeStepAndWait("done()");
        mWebXrVrTestFramework.endTest();
    }

    /**
     * Tests that WebXR returns null poses while in VR browsing mode, and valid ones otherwise.
     * Specific steps:
     *   * Enter inline VR mode using NFC scan while browser points at a VR content page
     *   * Check for null poses
     *   * Enter immersive mode by clicking the 'Enter VR' button on the page
     *   * Check for non-null poses
     */
    @Test
    @MediumTest
    @CommandLineFlags
            .Remove({"enable-webvr"})
            @CommandLineFlags.Add({"enable-features=WebXR"})
            @Restriction(RESTRICTION_TYPE_VIEWER_DAYDREAM)
            public void testForNullPosesInInlineVrFromNfc() throws InterruptedException {
        mWebXrVrTestFramework.loadUrlAndAwaitInitialization(
                WebXrVrTestFramework.getFileUrlForHtmlTestFile("test_inline_vr_poses"),
                PAGE_LOAD_TIMEOUT_S);
        NfcSimUtils.simNfcScanUntilVrEntry(mTestRule.getActivity());

        mWebXrVrTestFramework.executeStepAndWait("posesTurnedOffStep()");

        mWebXrVrTestFramework.executeStepAndWait("resetCounters()");
        mWebXrVrTestFramework.enterSessionWithUserGestureOrFail();
        Assert.assertTrue("Browser did not enter VR", VrShellDelegate.isInVr());

        mWebXrVrTestFramework.executeStepAndWait("posesTurnedOnStep()");

        mWebXrVrTestFramework.executeStepAndWait("done()");
        mWebXrVrTestFramework.endTest();
    }

    /**
     * Tests that WebXR returns null poses while in VR browsing mode, and valid ones otherwise.
     * Specific steps:
     *   * Enter inline VR mode using NFC scan while browser points at a blank page
     *   * Point the browser at a page with VR content
     *   * Check for null poses
     *   * Enter immersive mode by clicking the 'Enter VR' button on the page
     *   * Check for non-null poses
     */
    @Test
    @MediumTest
    @CommandLineFlags
            .Remove({"enable-webvr"})
            @CommandLineFlags.Add({"enable-features=WebXR"})
            @Restriction(RESTRICTION_TYPE_VIEWER_DAYDREAM)
            public void testForNullPosesInInlineVrOnNavigation() throws InterruptedException {
        NfcSimUtils.simNfcScanUntilVrEntry(mTestRule.getActivity());
        mWebXrVrTestFramework.loadUrlAndAwaitInitialization(
                WebXrVrTestFramework.getFileUrlForHtmlTestFile("test_inline_vr_poses"),
                PAGE_LOAD_TIMEOUT_S);

        mWebXrVrTestFramework.executeStepAndWait("posesTurnedOffStep()");

        mWebXrVrTestFramework.executeStepAndWait("resetCounters()");
        mWebXrVrTestFramework.enterSessionWithUserGestureOrFail();
        Assert.assertTrue("Browser did not enter VR", VrShellDelegate.isInVr());

        mWebXrVrTestFramework.executeStepAndWait("posesTurnedOnStep()");

        mWebXrVrTestFramework.executeStepAndWait("done()");
        mWebXrVrTestFramework.endTest();
    }
}
