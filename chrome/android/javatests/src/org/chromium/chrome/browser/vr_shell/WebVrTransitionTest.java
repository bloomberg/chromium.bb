// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr_shell;

import static org.chromium.chrome.browser.vr_shell.VrTestRule.PAGE_LOAD_TIMEOUT_S;
import static org.chromium.chrome.test.util.ChromeRestriction.RESTRICTION_TYPE_VIEWER_DAYDREAM;

import android.os.Build;
import android.support.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.vr_shell.util.NfcSimUtils;
import org.chromium.chrome.browser.vr_shell.util.VrTransitionUtils;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;

/**
 * End-to-end tests for transitioning between WebVR's magic window and
 * presentation modes.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        ChromeActivityTestRule.DISABLE_NETWORK_PREDICTION_FLAG, "enable-webvr"})
@MinAndroidSdkLevel(Build.VERSION_CODES.KITKAT) // WebVR is only supported on K+
public class WebVrTransitionTest {
    @Rule
    public VrTestRule mVrTestRule = new VrTestRule();

    /**
     * Tests that a successful requestPresent call actually enters VR
     */
    @Test
    @MediumTest
    public void testRequestPresentEntersVr() throws InterruptedException {
        mVrTestRule.loadUrlAndAwaitInitialization(
                VrTestRule.getHtmlTestFile("test_requestPresent_enters_vr"), PAGE_LOAD_TIMEOUT_S);
        VrTransitionUtils.enterPresentationAndWait(
                mVrTestRule.getFirstTabCvc(), mVrTestRule.getFirstTabWebContents());
        Assert.assertTrue("VrShellDelegate is in VR", VrShellDelegate.isInVr());
        mVrTestRule.endTest(mVrTestRule.getFirstTabWebContents());
    }

    /**
     * Tests that scanning the Daydream View NFC tag on supported devices fires the
     * vrdisplayactivate event.
     */
    @Test
    @MediumTest
    @Restriction(RESTRICTION_TYPE_VIEWER_DAYDREAM)
    public void testNfcFiresVrdisplayactivate() throws InterruptedException {
        mVrTestRule.loadUrlAndAwaitInitialization(
                VrTestRule.getHtmlTestFile("test_nfc_fires_vrdisplayactivate"),
                PAGE_LOAD_TIMEOUT_S);
        NfcSimUtils.simNfcScan(mVrTestRule.getActivity());
        mVrTestRule.waitOnJavaScriptStep(mVrTestRule.getFirstTabWebContents());
        mVrTestRule.endTest(mVrTestRule.getFirstTabWebContents());
    }
}
