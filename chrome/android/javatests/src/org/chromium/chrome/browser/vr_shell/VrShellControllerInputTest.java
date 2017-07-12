// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr_shell;

import static org.chromium.chrome.browser.vr_shell.VrTestRule.PAGE_LOAD_TIMEOUT_S;
import static org.chromium.chrome.browser.vr_shell.VrTestRule.POLL_CHECK_INTERVAL_LONG_MS;
import static org.chromium.chrome.browser.vr_shell.VrTestRule.POLL_TIMEOUT_LONG_MS;
import static org.chromium.chrome.test.util.ChromeRestriction.RESTRICTION_TYPE_DEVICE_DAYDREAM;
import static org.chromium.chrome.test.util.ChromeRestriction.RESTRICTION_TYPE_VIEWER_DAYDREAM;

import android.support.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.vr_shell.util.VrTransitionUtils;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;

/**
 * End-to-end tests for Daydream controller input while in the VR browser.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        ChromeActivityTestRule.DISABLE_NETWORK_PREDICTION_FLAG, "enable-features=VrShell"})
public class VrShellControllerInputTest {
    @Rule
    public VrTestRule mVrTestRule = new VrTestRule();

    /**
     * Verifies that swiping up/down/left/right on the Daydream controller's
     * touchpad scrolls the webpage while in the VR browser.
     */
    @Test
    @Restriction({RESTRICTION_TYPE_DEVICE_DAYDREAM, RESTRICTION_TYPE_VIEWER_DAYDREAM})
    @MediumTest
    public void testControllerScrolling() throws InterruptedException {
        // Load page in VR and make sure the controller is pointed at the content quad
        mVrTestRule.loadUrl(
                mVrTestRule.getHtmlTestFile("test_controller_scrolling"), PAGE_LOAD_TIMEOUT_S);
        VrTransitionUtils.forceEnterVr();
        VrTransitionUtils.waitForVrEntry(POLL_TIMEOUT_LONG_MS);
        EmulatedVrController controller = new EmulatedVrController(mVrTestRule.getActivity());
        final ContentViewCore cvc =
                mVrTestRule.getActivity().getActivityTab().getActiveContentViewCore();
        controller.recenterView();

        // Wait for the page to be scrollable
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return cvc.computeVerticalScrollRange() > cvc.getContainerView().getHeight()
                        && cvc.computeHorizontalScrollRange() > cvc.getContainerView().getWidth();
            }
        }, POLL_TIMEOUT_LONG_MS, POLL_CHECK_INTERVAL_LONG_MS);

        // Test that scrolling down works
        int startScrollPoint = cvc.getNativeScrollYForTest();
        // Arbitrary, but valid values to scroll smoothly
        int scrollSteps = 20;
        int scrollSpeed = 60;
        controller.scroll(EmulatedVrController.ScrollDirection.DOWN, scrollSteps, scrollSpeed);
        // We need this second scroll down, otherwise the horizontal scrolling becomes flaky
        // TODO(bsheedy): Figure out why this is the case
        controller.scroll(EmulatedVrController.ScrollDirection.DOWN, scrollSteps, scrollSpeed);
        int endScrollPoint = cvc.getNativeScrollYForTest();
        Assert.assertTrue("Controller was able to scroll down", startScrollPoint < endScrollPoint);

        // Test that scrolling up works
        startScrollPoint = endScrollPoint;
        controller.scroll(EmulatedVrController.ScrollDirection.UP, scrollSteps, scrollSpeed);
        endScrollPoint = cvc.getNativeScrollYForTest();
        Assert.assertTrue("Controller was able to scroll up", startScrollPoint > endScrollPoint);

        // Test that scrolling right works
        startScrollPoint = cvc.getNativeScrollXForTest();
        controller.scroll(EmulatedVrController.ScrollDirection.RIGHT, scrollSteps, scrollSpeed);
        endScrollPoint = cvc.getNativeScrollXForTest();
        Assert.assertTrue("Controller was able to scroll right", startScrollPoint < endScrollPoint);

        // Test that scrolling left works
        startScrollPoint = endScrollPoint;
        controller.scroll(EmulatedVrController.ScrollDirection.LEFT, scrollSteps, scrollSpeed);
        endScrollPoint = cvc.getNativeScrollXForTest();
        Assert.assertTrue("Controller was able to scroll left", startScrollPoint > endScrollPoint);
    }
}
