// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr_shell;

import static org.chromium.chrome.browser.vr_shell.VrTestFramework.PAGE_LOAD_TIMEOUT_S;
import static org.chromium.chrome.browser.vr_shell.VrTestFramework.POLL_CHECK_INTERVAL_SHORT_MS;
import static org.chromium.chrome.browser.vr_shell.VrTestFramework.POLL_TIMEOUT_LONG_MS;
import static org.chromium.chrome.browser.vr_shell.VrTestFramework.POLL_TIMEOUT_SHORT_MS;
import static org.chromium.chrome.test.util.ChromeRestriction.RESTRICTION_TYPE_DEVICE_DAYDREAM;
import static org.chromium.chrome.test.util.ChromeRestriction.RESTRICTION_TYPE_DEVICE_NON_DAYDREAM;
import static org.chromium.chrome.test.util.ChromeRestriction.RESTRICTION_TYPE_VIEWER_DAYDREAM;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Restriction;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.vr_shell.mock.MockVrDaydreamApi;
import org.chromium.chrome.browser.vr_shell.rules.ChromeTabbedActivityVrTestRule;
import org.chromium.chrome.browser.vr_shell.util.NfcSimUtils;
import org.chromium.chrome.browser.vr_shell.util.VrShellDelegateUtils;
import org.chromium.chrome.browser.vr_shell.util.VrTransitionUtils;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.DOMUtils;

import java.util.concurrent.TimeoutException;

/**
 * End-to-end tests for state transitions in VR, e.g. exiting WebVR presentation
 * into the VR browser.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        ChromeActivityTestRule.DISABLE_NETWORK_PREDICTION_FLAG, "enable-features=VrShell"})
public class VrShellTransitionTest {
    // We explicitly instantiate a rule here instead of using parameterization since this class
    // only ever runs in ChromeTabbedActivity.
    @Rule
    public ChromeTabbedActivityVrTestRule mVrTestRule = new ChromeTabbedActivityVrTestRule();

    private VrTestFramework mVrTestFramework;

    @Before
    public void setUp() throws Exception {
        mVrTestFramework = new VrTestFramework(mVrTestRule);
    }

    private void enterVrShellNfc(boolean supported) {
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        NfcSimUtils.simNfcScan(mVrTestRule.getActivity());
        if (supported) {
            VrTransitionUtils.waitForVrEntry(POLL_TIMEOUT_LONG_MS);
            Assert.assertTrue(VrShellDelegate.isInVr());
        } else {
            Assert.assertFalse(VrShellDelegate.isInVr());
        }
        VrTransitionUtils.forceExitVr();
        // TODO(bsheedy): Figure out why NFC tests cause the next test to fail
        // to enter VR unless we sleep for some amount of time after exiting VR
        // in the NFC test
    }

    private void enterExitVrShell(boolean supported) {
        MockVrDaydreamApi mockApi = new MockVrDaydreamApi();
        if (!supported) {
            VrShellDelegateUtils.getDelegateInstance().overrideDaydreamApiForTesting(mockApi);
        }
        VrTransitionUtils.forceEnterVr();
        if (supported) {
            VrTransitionUtils.waitForVrEntry(POLL_TIMEOUT_LONG_MS);
            Assert.assertTrue(VrShellDelegate.isInVr());
        } else {
            Assert.assertFalse(mockApi.getLaunchInVrCalled());
            Assert.assertFalse(VrShellDelegate.isInVr());
        }
        VrTransitionUtils.forceExitVr();
        Assert.assertFalse(VrShellDelegate.isInVr());
    }

    /**
     * Verifies that browser successfully transitions from 2D Chrome to the VR
     * browser when the Daydream View NFC tag is scanned on a Daydream-ready device.
     */
    @Test
    @Restriction({RESTRICTION_TYPE_DEVICE_DAYDREAM, RESTRICTION_TYPE_VIEWER_DAYDREAM})
    @RetryOnFailure(message = "crbug.com/736527")
    @MediumTest
    public void test2dtoVrShellNfcSupported() {
        enterVrShellNfc(true /* supported */);
    }

    /**
     * Verifies that the browser does not transition from 2D chrome to the VR
     * browser when the Daydream View NFC tag is scanned on a non-Daydream-ready
     * device.
     */
    @Test
    @Restriction(RESTRICTION_TYPE_DEVICE_NON_DAYDREAM)
    @MediumTest
    public void test2dtoVrShellNfcUnsupported() {
        enterVrShellNfc(false /* supported */);
    }

    /**
     * Verifies that browser successfully transitions from 2D chrome to the VR
     * browser and back when the VrShellDelegate tells it to on a Daydream-ready
     * device.
     */
    @Test
    @Restriction(RESTRICTION_TYPE_DEVICE_DAYDREAM)
    @MediumTest
    public void test2dtoVrShellto2dSupported() {
        enterExitVrShell(true /* supported */);
    }

    /**
     * Verifies that browser does not enter VR mode on Non-Daydream-ready devices.
     */
    @Test
    @Restriction(RESTRICTION_TYPE_DEVICE_NON_DAYDREAM)
    @MediumTest
    public void test2dtoVrShellto2dUnsupported() {
        enterExitVrShell(false /* supported */);
    }

    /**
     * Tests that we exit fullscreen mode after exiting VR from cinema mode.
     */
    @Test
    @Restriction(RESTRICTION_TYPE_VIEWER_DAYDREAM)
    @MediumTest
    public void testExitFullscreenAfterExitingVrFromCinemaMode()
            throws InterruptedException, TimeoutException {
        VrTransitionUtils.forceEnterVr();
        VrTransitionUtils.waitForVrEntry(POLL_TIMEOUT_LONG_MS);
        mVrTestFramework.loadUrlAndAwaitInitialization(
                VrTestFramework.getHtmlTestFile("test_navigation_2d_page"), PAGE_LOAD_TIMEOUT_S);
        DOMUtils.clickNode(mVrTestFramework.getFirstTabCvc(), "fullscreen");
        VrTestFramework.waitOnJavaScriptStep(mVrTestFramework.getFirstTabWebContents());

        Assert.assertTrue(DOMUtils.isFullscreen(mVrTestFramework.getFirstTabWebContents()));
        VrTransitionUtils.forceExitVr();
        // The fullscreen exit from exiting VR isn't necessarily instantaneous, so give it
        // a bit of time.
        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                try {
                    return !DOMUtils.isFullscreen(mVrTestFramework.getFirstTabWebContents());
                } catch (InterruptedException | TimeoutException e) {
                    return false;
                }
            }
        }, POLL_TIMEOUT_SHORT_MS, POLL_CHECK_INTERVAL_SHORT_MS);
    }

    /**
     * Tests that the reported display dimensions are correct when exiting
     * from WebVR presentation to the VR browser.
     */
    @Test
    @CommandLineFlags.Add("enable-webvr")
    @Restriction(RESTRICTION_TYPE_VIEWER_DAYDREAM)
    @MediumTest
    public void testExitPresentationWebVrToVrShell()
            throws IllegalArgumentException, InterruptedException, TimeoutException {
        VrTransitionUtils.forceEnterVr();
        VrTransitionUtils.waitForVrEntry(POLL_TIMEOUT_LONG_MS);
        mVrTestFramework.loadUrlAndAwaitInitialization(
                VrTestFramework.getHtmlTestFile("test_navigation_webvr_page"), PAGE_LOAD_TIMEOUT_S);
        VrShellImpl vrShellImpl = (VrShellImpl) TestVrShellDelegate.getVrShellForTesting();
        float expectedWidth = vrShellImpl.getContentWidthForTesting();
        float expectedHeight = vrShellImpl.getContentHeightForTesting();
        VrTransitionUtils.enterPresentationOrFail(mVrTestFramework.getFirstTabCvc());

        // Validate our size is what we expect while in VR.
        String javascript = "Math.abs(screen.width - " + expectedWidth + ") <= 1 && "
                + "Math.abs(screen.height - " + expectedHeight + ") <= 1";
        Assert.assertTrue(VrTestFramework.pollJavaScriptBoolean(
                javascript, POLL_TIMEOUT_LONG_MS, mVrTestFramework.getFirstTabWebContents()));

        // Exit presentation through JavaScript.
        VrTestFramework.runJavaScriptOrFail("vrDisplay.exitPresent();", POLL_TIMEOUT_SHORT_MS,
                mVrTestFramework.getFirstTabWebContents());

        // We aren't comparing for equality because there is some rounding that occurs.
        Assert.assertTrue(VrTestFramework.pollJavaScriptBoolean(
                javascript, POLL_TIMEOUT_LONG_MS, mVrTestFramework.getFirstTabWebContents()));
    }
}
