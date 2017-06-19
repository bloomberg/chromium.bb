// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr_shell;

import static org.chromium.chrome.browser.vr_shell.VrTestRule.PAGE_LOAD_TIMEOUT_S;
import static org.chromium.chrome.browser.vr_shell.VrTestRule.POLL_TIMEOUT_LONG_MS;
import static org.chromium.chrome.browser.vr_shell.VrTestRule.POLL_TIMEOUT_SHORT_MS;
import static org.chromium.chrome.test.util.ChromeRestriction.RESTRICTION_TYPE_DEVICE_DAYDREAM;
import static org.chromium.chrome.test.util.ChromeRestriction.RESTRICTION_TYPE_VIEWER_DAYDREAM;

import android.support.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.vr_shell.util.VrInfoBarUtils;
import org.chromium.chrome.browser.vr_shell.util.VrShellDelegateUtils;
import org.chromium.chrome.browser.vr_shell.util.VrTransitionUtils;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ChromeTabUtils;

import java.util.concurrent.TimeoutException;

/**
 * Tests for the infobar that prompts the user to enter feedback on their VR browsing experience.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        ChromeActivityTestRule.DISABLE_NETWORK_PREDICTION_FLAG, "enable-features=VrShell",
        "enable-webvr"})
@Restriction(RESTRICTION_TYPE_DEVICE_DAYDREAM)

public class VrFeedbackInfoBarTest {
    @Rule
    public VrTestRule mVrTestRule = new VrTestRule();

    private static final String TEST_PAGE_2D_URL =
            VrTestRule.getHtmlTestFile("test_navigation_2d_page");
    private static final String TEST_PAGE_WEBVR_URL =
            VrTestRule.getHtmlTestFile("test_requestPresent_enters_vr");

    @Before
    public void setUp() throws Exception {
        Assert.assertFalse(VrFeedbackStatus.getFeedbackOptOut());
    }

    private void assertState(boolean isInVr, boolean isInfobarVisible) {
        Assert.assertEquals("Browser is in VR", isInVr, VrShellDelegate.isInVr());
        VrInfoBarUtils.expectInfoBarPresent(mVrTestRule, isInfobarVisible);
    }

    private void enterThenExitVr() {
        VrTransitionUtils.forceEnterVr();
        VrTransitionUtils.waitForVrEntry(POLL_TIMEOUT_LONG_MS);
        assertState(true /* isInVr */, false /* isInfobarVisible  */);
        VrTransitionUtils.forceExitVr();
    }

    /**
     * Tests that we respect the feedback frequency when showing the feedback prompt.
     */
    @Test
    @MediumTest
    public void testFeedbackFrequency() throws InterruptedException, TimeoutException {
        mVrTestRule.loadUrlAndAwaitInitialization(TEST_PAGE_2D_URL, PAGE_LOAD_TIMEOUT_S);
        // Set frequency of infobar to every 2nd time.
        VrShellDelegateUtils.getDelegateInstance().setFeedbackFrequencyForTesting(2);

        // Verify that the Feedback infobar is visible when exiting VR.
        enterThenExitVr();
        assertState(false /* isInVr */, true /* isInfobarVisible  */);
        VrInfoBarUtils.clickInfobarCloseButton(mVrTestRule);

        // Feedback infobar shouldn't show up this time.
        enterThenExitVr();
        assertState(false /* isInVr */, false /* isInfobarVisible  */);

        // Feedback infobar should show up again.
        enterThenExitVr();
        assertState(false /* isInVr */, true /* isInfobarVisible  */);
        VrInfoBarUtils.clickInfobarCloseButton(mVrTestRule);
    }

    /**
     * Tests that we don't show the feedback prompt when the user has opted-out.
     */
    @Test
    @MediumTest
    public void testFeedbackOptOut() throws InterruptedException, TimeoutException {
        mVrTestRule.loadUrlAndAwaitInitialization(TEST_PAGE_2D_URL, PAGE_LOAD_TIMEOUT_S);

        // Show infobar every time.
        VrShellDelegateUtils.getDelegateInstance().setFeedbackFrequencyForTesting(1);

        // The infobar should show the first time.
        enterThenExitVr();
        assertState(false /* isInVr */, true /* isInfobarVisible  */);

        // Opt-out of seeing the infobar.
        VrInfoBarUtils.clickInfoBarButton(VrInfoBarUtils.Button.SECONDARY, mVrTestRule);
        Assert.assertTrue(VrFeedbackStatus.getFeedbackOptOut());

        // The infobar should not show because the user opted out.
        enterThenExitVr();
        assertState(false /* isInVr */, false /* isInfobarVisible  */);
    }

    /**
     * Tests that we only show the feedback prompt when the user has actually used the VR browser.
     */
    @Test
    @MediumTest
    public void testFeedbackOnlyOnVrBrowsing() throws InterruptedException, TimeoutException {
        // Enter VR presentation mode.
        mVrTestRule.loadUrlAndAwaitInitialization(TEST_PAGE_WEBVR_URL, PAGE_LOAD_TIMEOUT_S);
        Assert.assertTrue("VRDisplay found",
                mVrTestRule.vrDisplayFound(mVrTestRule.getFirstTabWebContents()));
        VrTransitionUtils.enterPresentationAndWait(
                mVrTestRule.getFirstTabCvc(), mVrTestRule.getFirstTabWebContents());
        assertState(true /* isInVr */, false /* isInfobarVisible  */);
        Assert.assertTrue(VrShellDelegate.getVrShellForTesting().getWebVrModeEnabled());

        // Exiting VR should not prompt for feedback since the no VR browsing was performed.
        VrTransitionUtils.forceExitVr();
        assertState(false /* isInVr */, false /* isInfobarVisible  */);
    }

    /**
     * Tests that we show the prompt if the VR browser is used after exiting presentation mode.
     */
    @Test
    @MediumTest
    @Restriction(RESTRICTION_TYPE_VIEWER_DAYDREAM)
    public void testExitPresentationInVr() throws InterruptedException, TimeoutException {
        // Enter VR presentation mode.
        mVrTestRule.loadUrlAndAwaitInitialization(TEST_PAGE_WEBVR_URL, PAGE_LOAD_TIMEOUT_S);
        Assert.assertTrue("VRDisplay found",
                mVrTestRule.vrDisplayFound(mVrTestRule.getFirstTabWebContents()));
        VrTransitionUtils.enterPresentationAndWait(
                mVrTestRule.getFirstTabCvc(), mVrTestRule.getFirstTabWebContents());
        assertState(true /* isInVr */, false /* isInfobarVisible  */);
        Assert.assertTrue(VrShellDelegate.getVrShellForTesting().getWebVrModeEnabled());

        // Exit presentation mode by navigating to a different url.
        ChromeTabUtils.waitForTabPageLoaded(
                mVrTestRule.getActivity().getActivityTab(), new Runnable() {
                    @Override
                    public void run() {
                        mVrTestRule.runJavaScriptOrFail(
                                "window.location.href = '" + TEST_PAGE_2D_URL + "';",
                                POLL_TIMEOUT_SHORT_MS, mVrTestRule.getFirstTabWebContents());
                    }
                }, POLL_TIMEOUT_LONG_MS);

        // Exiting VR should prompt for feedback since 2D browsing was performed after.
        VrTransitionUtils.forceExitVr();
        assertState(false /* isInVr */, true /* isInfobarVisible  */);
    }
}
