// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import static org.chromium.chrome.browser.vr.XrTestFramework.PAGE_LOAD_TIMEOUT_S;

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
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.vr.rules.XrActivityRestriction;
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
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE, "enable-webvr"})
@MinAndroidSdkLevel(Build.VERSION_CODES.KITKAT) // WebVR is only supported on K+
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
        mRuleChain = VrTestRuleUtils.wrapRuleInXrActivityRestrictionRule(mTestRule);
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
    @CommandLineFlags.Add("enable-features=WebXROrientationSensorDevice")
    @XrActivityRestriction({XrActivityRestriction.SupportedActivity.ALL})
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
}
