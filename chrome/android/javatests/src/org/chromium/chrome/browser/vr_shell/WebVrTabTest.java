// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr_shell;

import static org.chromium.chrome.browser.vr_shell.VrTestFramework.PAGE_LOAD_TIMEOUT_S;

import android.os.Build;
import android.support.test.filters.MediumTest;

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
import org.chromium.chrome.browser.vr_shell.util.VrTestRuleUtils;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;

import java.util.List;
import java.util.concurrent.Callable;

/**
 * End-to-end tests for WebVR's behavior when multiple tabs are involved.
 */
@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        ChromeActivityTestRule.DISABLE_NETWORK_PREDICTION_FLAG, "enable-webvr"})
@MinAndroidSdkLevel(Build.VERSION_CODES.KITKAT) // WebVR is only supported on K+
public class WebVrTabTest {
    @ClassParameter
    private static List<ParameterSet> sClassParams =
            VrTestRuleUtils.generateDefaultVrTestRuleParameters();
    @Rule
    public RuleChain mRuleChain;

    private ChromeActivityTestRule mVrTestRule;
    private VrTestFramework mVrTestFramework;

    public WebVrTabTest(Callable<ChromeActivityTestRule> callable) throws Exception {
        mVrTestRule = callable.call();
        mRuleChain = VrTestRuleUtils.wrapRuleInVrActivityRestrictionRule(mVrTestRule);
    }

    @Before
    public void setUp() throws Exception {
        mVrTestFramework = new VrTestFramework(mVrTestRule);
    }

    /**
     * Tests that non-focused tabs cannot get pose information.
     */
    @Test
    @MediumTest
    public void testPoseDataUnfocusedTab() throws InterruptedException {
        mVrTestFramework.loadUrlAndAwaitInitialization(
                VrTestFramework.getHtmlTestFile("test_pose_data_unfocused_tab"),
                PAGE_LOAD_TIMEOUT_S);
        VrTestFramework.executeStepAndWait(
                "stepCheckFrameDataWhileFocusedTab()", mVrTestFramework.getFirstTabWebContents());

        mVrTestRule.loadUrlInNewTab("about:blank");

        VrTestFramework.executeStepAndWait("stepCheckFrameDataWhileNonFocusedTab()",
                mVrTestFramework.getFirstTabWebContents());
        VrTestFramework.endTest(mVrTestFramework.getFirstTabWebContents());
    }
}
