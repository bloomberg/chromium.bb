// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr_shell;

import static org.chromium.chrome.browser.vr_shell.VrTestRule.PAGE_LOAD_TIMEOUT_S;
import static org.chromium.chrome.browser.vr_shell.VrTestRule.POLL_CHECK_INTERVAL_LONG_MS;
import static org.chromium.chrome.browser.vr_shell.VrTestRule.POLL_TIMEOUT_LONG_MS;
import static org.chromium.chrome.test.util.ChromeRestriction.RESTRICTION_TYPE_DEVICE_DAYDREAM;
import static org.chromium.chrome.test.util.ChromeRestriction.RESTRICTION_TYPE_DEVICE_NON_DAYDREAM;
import static org.chromium.chrome.test.util.ChromeRestriction.RESTRICTION_TYPE_VIEWER_DAYDREAM;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;
import android.support.test.filters.SmallTest;
import android.view.ViewGroup;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Restriction;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.compositor.CompositorViewHolder;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.vr_shell.mock.MockVrDaydreamApi;
import org.chromium.chrome.browser.vr_shell.util.NfcSimUtils;
import org.chromium.chrome.browser.vr_shell.util.VrShellDelegateUtils;
import org.chromium.chrome.browser.vr_shell.util.VrTransitionUtils;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;

import java.util.concurrent.Callable;
import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicReference;

/**
 * End-to-end tests for VR browsing, aka "VR Shell". This may require
 * interacting with WebVR in addition to the VR browser, so inherit from
 * VrTestBase for the WebVR test framework.
 */
@RunWith(ChromeJUnit4ClassRunner.class)

@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        ChromeActivityTestRule.DISABLE_NETWORK_PREDICTION_FLAG, "enable-features=VrShell"})
public class VrShellTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private VrShellDelegate mDelegate;

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();
        mDelegate = VrShellDelegateUtils.getDelegateInstance();
    }

    private void enterExitVrMode(boolean supported) {
        MockVrDaydreamApi mockApi = new MockVrDaydreamApi();
        if (!supported) {
            mDelegate.overrideDaydreamApiForTesting(mockApi);
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

    private void enterVrModeNfc(boolean supported) {
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        NfcSimUtils.simNfcScan(mActivityTestRule.getActivity());
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

    /**
     * Verifies that browser successfully enters VR mode when Daydream headset
     * NFC tag is scanned on a Daydream-ready device. Requires that the phone
     * is unlocked.
     */
    @Test
    @Restriction({RESTRICTION_TYPE_DEVICE_DAYDREAM, RESTRICTION_TYPE_VIEWER_DAYDREAM})
    @MediumTest
    public void testSimNfcSupported() {
        enterVrModeNfc(true);
    }

    /**
     * Verifies that the browser does not enter VR mode on Non-Daydream-ready
     * devices when the Daydream headset NFC tag is scanned.
     */
    @Test
    @Restriction(RESTRICTION_TYPE_DEVICE_NON_DAYDREAM)
    @SmallTest
    public void testSimNfcUnsupported() {
        enterVrModeNfc(false);
    }

    /**
     * Verifies that browser successfully enters and exits VR mode when told to
     * on Daydream-ready devices. Requires that the phone is unlocked.
     */
    @Test
    @Restriction(RESTRICTION_TYPE_DEVICE_DAYDREAM)
    @SmallTest
    public void testEnterExitVrModeSupported() {
        enterExitVrMode(true);
    }

    /**
     * Verifies that browser does not enter VR mode on Non-Daydream-ready devices.
     */
    @Test
    @Restriction(RESTRICTION_TYPE_DEVICE_NON_DAYDREAM)
    @SmallTest
    public void testEnterExitVrModeUnsupported() {
        enterExitVrMode(false);
    }

    /**
     * Verifies that swiping up/down on the Daydream controller's touchpad scrolls
     * the webpage while in the VR browser.
     */
    @Test
    @Restriction({RESTRICTION_TYPE_DEVICE_DAYDREAM, RESTRICTION_TYPE_VIEWER_DAYDREAM})
    @MediumTest
    public void testControllerScrolling() throws InterruptedException {
        // Load page in VR and make sure the controller is pointed at the content quad
        mActivityTestRule.loadUrl("chrome://credits", PAGE_LOAD_TIMEOUT_S);
        VrTransitionUtils.forceEnterVr();
        VrTransitionUtils.waitForVrEntry(POLL_TIMEOUT_LONG_MS);
        EmulatedVrController controller = new EmulatedVrController(mActivityTestRule.getActivity());
        final ContentViewCore cvc =
                mActivityTestRule.getActivity().getActivityTab().getActiveContentViewCore();
        controller.recenterView();

        // Wait for the page to be scrollable
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return cvc.computeVerticalScrollRange() > cvc.getContainerView().getHeight();
            }
        }, POLL_TIMEOUT_LONG_MS, POLL_CHECK_INTERVAL_LONG_MS);

        // Test that scrolling down works
        int startScrollY = cvc.getNativeScrollYForTest();
        // Arbitrary, but valid values to scroll smoothly
        int scrollSteps = 20;
        int scrollSpeed = 60;
        controller.scrollDown(scrollSteps, scrollSpeed);
        int endScrollY = cvc.getNativeScrollYForTest();
        Assert.assertTrue("Controller was able to scroll down", startScrollY < endScrollY);

        // Test that scrolling up works
        startScrollY = endScrollY;
        controller.scrollUp(scrollSteps, scrollSpeed);
        endScrollY = cvc.getNativeScrollYForTest();
        Assert.assertTrue("Controller was able to scroll up", startScrollY > endScrollY);
    }

    /**
     * Verify that resizing the CompositorViewHolder does not cause the current tab to resize while
     * the CompositorViewHolder is detached from the TabModelSelector. See crbug.com/680240.
     * @throws InterruptedException
     * @throws TimeoutException
     */
    @Test
    @SmallTest
    @RetryOnFailure
    public void testResizeWithCompositorViewHolderDetached()
            throws InterruptedException, TimeoutException {
        final AtomicReference<TabModelSelector> selector = new AtomicReference<>();
        final AtomicReference<Integer> oldWidth = new AtomicReference<>();
        final int testWidth = 123;
        final ContentViewCore cvc =
                mActivityTestRule.getActivity().getActivityTab().getActiveContentViewCore();

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                CompositorViewHolder compositorViewHolder =
                        (CompositorViewHolder) mActivityTestRule.getActivity().findViewById(
                                R.id.compositor_view_holder);
                selector.set(compositorViewHolder.detachForVr());
                oldWidth.set(cvc.getViewportWidthPix());

                ViewGroup.LayoutParams layoutParams = compositorViewHolder.getLayoutParams();
                layoutParams.width = testWidth;
                layoutParams.height = 456;
                compositorViewHolder.requestLayout();
            }
        });
        CriteriaHelper.pollUiThread(Criteria.equals(testWidth, new Callable<Integer>() {
            @Override
            public Integer call() {
                return mActivityTestRule.getActivity()
                        .findViewById(R.id.compositor_view_holder)
                        .getMeasuredWidth();
            }
        }));

        Assert.assertEquals("Viewport width should not have changed when resizing a detached "
                        + "CompositorViewHolder",
                cvc.getViewportWidthPix(), oldWidth.get().intValue());

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                CompositorViewHolder compositorViewHolder =
                        (CompositorViewHolder) mActivityTestRule.getActivity().findViewById(
                                R.id.compositor_view_holder);
                compositorViewHolder.onExitVr(selector.get());
            }
        });

        CriteriaHelper.pollUiThread(Criteria.equals(testWidth, new Callable<Integer>() {
            @Override
            public Integer call() {
                return cvc.getViewportWidthPix();
            }
        }));
    }
}
