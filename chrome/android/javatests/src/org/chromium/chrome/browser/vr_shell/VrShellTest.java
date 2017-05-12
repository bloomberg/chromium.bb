// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr_shell;

import static org.chromium.chrome.browser.vr_shell.VrUtils.POLL_CHECK_INTERVAL_LONG_MS;
import static org.chromium.chrome.browser.vr_shell.VrUtils.POLL_TIMEOUT_LONG_MS;
import static org.chromium.chrome.test.util.ChromeRestriction.RESTRICTION_TYPE_DEVICE_DAYDREAM;
import static org.chromium.chrome.test.util.ChromeRestriction.RESTRICTION_TYPE_DEVICE_NON_DAYDREAM;
import static org.chromium.chrome.test.util.ChromeRestriction.RESTRICTION_TYPE_VIEWER_DAYDREAM;

import android.content.pm.ActivityInfo;
import android.support.test.filters.MediumTest;
import android.support.test.filters.SmallTest;
import android.view.ViewGroup;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.CompositorViewHolder;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.test.util.RenderUtils.ViewRenderer;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;

import java.io.IOException;
import java.util.concurrent.Callable;
import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicReference;

/**
 * End-to-end tests for VR browsing, aka "VR Shell". This may require
 * interacting with WebVR in addition to the VR browser, so inherit from
 * VrTestBase for the WebVR test framework.
 */
@CommandLineFlags.Add("enable-features=VrShell")
public class VrShellTest extends VrTestBase {
    private static final String GOLDEN_DIR =
            "chrome/test/data/android/render_tests";

    private VrShellDelegate mDelegate;
    private ViewRenderer mViewRenderer;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mDelegate = VrUtils.getVrShellDelegateInstance();
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityOnBlankPage();
        mViewRenderer = new ViewRenderer(getActivity(),
                GOLDEN_DIR, this.getClass().getSimpleName());
    }

    private void enterExitVrMode(boolean supported) {
        MockVrDaydreamApi mockApi = new MockVrDaydreamApi();
        if (!supported) {
            mDelegate.overrideDaydreamApiForTesting(mockApi);
        }
        VrUtils.forceEnterVr();
        if (supported) {
            VrUtils.waitForVrSupported(POLL_TIMEOUT_LONG_MS);
            assertTrue(VrShellDelegate.isInVr());
        } else {
            assertFalse(mockApi.getLaunchInVrCalled());
            assertFalse(VrShellDelegate.isInVr());
        }
        VrUtils.forceExitVr(mDelegate);
        assertFalse(VrShellDelegate.isInVr());
    }

    private void enterExitVrModeImage(boolean supported) throws IOException {
        int prevOrientation = getActivity().getRequestedOrientation();
        getActivity().setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
        getInstrumentation().waitForIdleSync();
        mViewRenderer.renderAndCompare(
                getActivity().getWindow().getDecorView().getRootView(),
                "blank_page");

        VrUtils.forceEnterVr();
        // Currently, screenshots only show the static UI overlay, not the
        // actual content. Thus, 1:1 pixel checking is reliable until a
        // way to take screenshots of VR content is added, in which case
        // % similarity or some other method will need to be used. We're
        // assuming that if the UI overlay is visible, then the device has
        // successfully entered VR mode.
        if (supported) {
            VrUtils.waitForVrSupported(POLL_TIMEOUT_LONG_MS);
            mViewRenderer.renderAndCompare(
                    getActivity().getWindow().getDecorView().getRootView(),
                    "vr_entered");
        } else {
            // TODO(bsheedy): Find a good way to wait before taking a screenshot
            // when running on an unsupported device
            mViewRenderer.renderAndCompare(
                    getActivity().getWindow().getDecorView().getRootView(),
                    "blank_page");
        }

        VrUtils.forceExitVr(mDelegate);
        getActivity().setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
        getInstrumentation().waitForIdleSync();
        mViewRenderer.renderAndCompare(
                getActivity().getWindow().getDecorView().getRootView(),
                "blank_page");

        getActivity().setRequestedOrientation(prevOrientation);
    }

    private void enterVrModeNfc(boolean supported) {
        getInstrumentation().waitForIdleSync();
        VrUtils.simNfc(getActivity());
        if (supported) {
            VrUtils.waitForVrSupported(POLL_TIMEOUT_LONG_MS);
            assertTrue(VrShellDelegate.isInVr());
        } else {
            assertFalse(VrShellDelegate.isInVr());
        }
        VrUtils.forceExitVr(mDelegate);
        // TODO(bsheedy): Figure out why NFC tests cause the next test to fail
        // to enter VR unless we sleep for some amount of time after exiting VR
        // in the NFC test
    }

    /**
     * Verifies that browser successfully enters VR mode when Daydream headset
     * NFC tag is scanned on a Daydream-ready device. Requires that the phone
     * is unlocked.
     */
    @Restriction({RESTRICTION_TYPE_DEVICE_DAYDREAM, RESTRICTION_TYPE_VIEWER_DAYDREAM})
    @MediumTest
    public void testSimNfcSupported() {
        enterVrModeNfc(true);
    }

    /**
     * Verifies that the browser does not enter VR mode on Non-Daydream-ready
     * devices when the Daydream headset NFC tag is scanned.
     */
    @Restriction(RESTRICTION_TYPE_DEVICE_NON_DAYDREAM)
    @SmallTest
    public void testSimNfcUnsupported() {
        enterVrModeNfc(false);
    }

    /**
     * Verifies that browser successfully enters and exits VR mode when told to
     * on Daydream-ready devices. Requires that the phone is unlocked.
     */
    @Restriction(RESTRICTION_TYPE_DEVICE_DAYDREAM)
    @SmallTest
    public void testEnterExitVrModeSupported() {
        enterExitVrMode(true);
    }

    /**
     * Verifies that browser does not enter VR mode on Non-Daydream-ready devices.
     */
    @Restriction(RESTRICTION_TYPE_DEVICE_NON_DAYDREAM)
    @SmallTest
    public void testEnterExitVrModeUnsupported() {
        enterExitVrMode(false);
    }

    /**
     * Verifies that browser successfully enters and exits VR mode when told to
     * on Daydream-ready devices via a screendiffing check.
     * Requires that the phone is unlocked.
     */
    @Restriction(RESTRICTION_TYPE_DEVICE_DAYDREAM)
    @Feature("RenderTest")
    @MediumTest
    public void testEnterExitVrModeSupportedImage() throws IOException {
        enterExitVrModeImage(true);
    }

    /**
     * Verifies that browser does not enter VR mode on Non-Daydream-ready devices
     * via a screendiffing check. Requires that the phone is unlocked.
     */
    @Restriction(RESTRICTION_TYPE_DEVICE_NON_DAYDREAM)
    @Feature("RenderTest")
    @MediumTest
    public void testEnterExitVrModeUnsupportedImage() throws IOException {
        enterExitVrModeImage(false);
    }

    /**
     * Verifies that swiping up/down on the Daydream controller's touchpad scrolls
     * the webpage while in the VR browser.
     */
    @Restriction({RESTRICTION_TYPE_DEVICE_DAYDREAM, RESTRICTION_TYPE_VIEWER_DAYDREAM})
    @MediumTest
    public void testControllerScrolling() throws InterruptedException {
        // Load page in VR and make sure the controller is pointed at the content quad
        loadUrl("chrome://credits", PAGE_LOAD_TIMEOUT_S);
        VrUtils.forceEnterVr();
        VrUtils.waitForVrSupported(POLL_TIMEOUT_LONG_MS);
        EmulatedVrController controller = new EmulatedVrController(getActivity());
        final ContentViewCore cvc = getActivity().getActivityTab().getActiveContentViewCore();
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
        assertTrue("Controller was able to scroll down", startScrollY < endScrollY);

        // Test that scrolling up works
        startScrollY = endScrollY;
        controller.scrollUp(scrollSteps, scrollSpeed);
        endScrollY = cvc.getNativeScrollYForTest();
        assertTrue("Controller was able to scroll up", startScrollY > endScrollY);
    }

    /**
     * Verify that resizing the CompositorViewHolder does not cause the current tab to resize while
     * the CompositorViewHolder is detached from the TabModelSelector. See crbug.com/680240.
     * @throws InterruptedException
     * @throws TimeoutException
     */
    @SmallTest
    @RetryOnFailure
    public void testResizeWithCompositorViewHolderDetached()
            throws InterruptedException, TimeoutException {
        final AtomicReference<TabModelSelector> selector = new AtomicReference<>();
        final AtomicReference<Integer> oldWidth = new AtomicReference<>();
        final int testWidth = 123;
        final ContentViewCore cvc = getActivity().getActivityTab().getActiveContentViewCore();

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                CompositorViewHolder compositorViewHolder = (CompositorViewHolder)
                        getActivity().findViewById(R.id.compositor_view_holder);
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
                return getActivity().findViewById(R.id.compositor_view_holder).getMeasuredWidth();
            }
        }));

        assertEquals("Viewport width should not have changed when resizing a detached "
                + "CompositorViewHolder",
                cvc.getViewportWidthPix(),
                oldWidth.get().intValue());

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                CompositorViewHolder compositorViewHolder = (CompositorViewHolder) getActivity()
                        .findViewById(R.id.compositor_view_holder);
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
