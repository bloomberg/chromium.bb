// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr_shell;

import static org.chromium.chrome.browser.vr_shell.VrTestRule.PAGE_LOAD_TIMEOUT_S;

import android.os.Build;
import android.support.test.filters.MediumTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;

/**
 * End-to-end tests for WebVR's behavior when multiple tabs are involved.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        ChromeActivityTestRule.DISABLE_NETWORK_PREDICTION_FLAG, "enable-webvr"})
@MinAndroidSdkLevel(Build.VERSION_CODES.KITKAT) // WebVR is only supported on K+
public class WebVrTabTest {
    @Rule
    public VrTestRule mVrTestRule = new VrTestRule();

    /**
     * Tests that non-focused tabs cannot get pose information.
     */
    @Test
    @MediumTest
    public void testPoseDataUnfocusedTab() throws InterruptedException {
        mVrTestRule.loadUrlAndAwaitInitialization(
                VrTestRule.getHtmlTestFile("test_pose_data_unfocused_tab"), PAGE_LOAD_TIMEOUT_S);
        mVrTestRule.executeStepAndWait(
                "stepCheckFrameDataWhileFocusedTab()", mVrTestRule.getFirstTabWebContents());

        mVrTestRule.loadUrlInNewTab("about:blank");

        mVrTestRule.executeStepAndWait(
                "stepCheckFrameDataWhileNonFocusedTab()", mVrTestRule.getFirstTabWebContents());
        mVrTestRule.endTest(mVrTestRule.getFirstTabWebContents());
    }
}
