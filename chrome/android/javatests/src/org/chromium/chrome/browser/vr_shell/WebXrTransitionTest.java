// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr_shell;

import static org.chromium.chrome.browser.vr_shell.XrTestFramework.PAGE_LOAD_TIMEOUT_S;

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
import org.chromium.chrome.browser.vr_shell.rules.VrActivityRestriction;
import org.chromium.chrome.browser.vr_shell.util.VrTestRuleUtils;
import org.chromium.chrome.browser.vr_shell.util.XrTransitionUtils;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;

import java.util.List;
import java.util.concurrent.Callable;

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
}