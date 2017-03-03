// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr_shell;

import static org.chromium.chrome.test.util.ChromeRestriction.RESTRICTION_TYPE_DAYDREAM;
import static org.chromium.chrome.test.util.ChromeRestriction.RESTRICTION_TYPE_DAYDREAM_VIEW;
import static org.chromium.chrome.test.util.ChromeRestriction.RESTRICTION_TYPE_NON_DAYDREAM;

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
import org.chromium.chrome.test.ChromeTabbedActivityTestBase;
import org.chromium.chrome.test.util.RenderUtils.ViewRenderer;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;

import java.io.IOException;
import java.util.concurrent.Callable;
import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicReference;

/**
 * Instrumentation tests for VR Shell (Chrome VR)
 */
@CommandLineFlags.Add("enable-features=VrShell")
public class VrShellTest extends ChromeTabbedActivityTestBase {
    private static final String GOLDEN_DIR =
            "chrome/test/data/android/render_tests";

    private VrShellDelegate mDelegate;
    private ViewRenderer mViewRenderer;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mDelegate = VrShellDelegate.getInstanceForTesting();
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
            VrUtils.waitForVrSupported();
            assertTrue(VrShellDelegate.isInVR());
        } else {
            assertFalse(mockApi.getLaunchInVrCalled());
            assertFalse(VrShellDelegate.isInVR());
        }
        VrUtils.forceExitVr(mDelegate);
        assertFalse(VrShellDelegate.isInVR());
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
            VrUtils.waitForVrSupported();
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
            VrUtils.waitForVrSupported();
            assertTrue(VrShellDelegate.isInVR());
        } else {
            assertFalse(VrShellDelegate.isInVR());
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
    @Restriction({RESTRICTION_TYPE_DAYDREAM, RESTRICTION_TYPE_DAYDREAM_VIEW})
    @MediumTest
    public void testSimNfcSupported() {
        enterVrModeNfc(true);
    }

    /**
     * Verifies that the browser does not enter VR mode on Non-Daydream-ready
     * devices when the Daydream headset NFC tag is scanned.
     */
    @Restriction(RESTRICTION_TYPE_NON_DAYDREAM)
    @SmallTest
    public void testSimNfcUnsupported() {
        enterVrModeNfc(false);
    }

    /**
     * Verifies that browser successfully enters and exits VR mode when told to
     * on Daydream-ready devices. Requires that the phone is unlocked.
     */
    @Restriction(RESTRICTION_TYPE_DAYDREAM)
    @SmallTest
    public void testEnterExitVrModeSupported() {
        enterExitVrMode(true);
    }

    /**
     * Verifies that browser does not enter VR mode on Non-Daydream-ready devices.
     */
    @Restriction(RESTRICTION_TYPE_NON_DAYDREAM)
    @SmallTest
    public void testEnterExitVrModeUnsupported() {
        enterExitVrMode(false);
    }

    /**
     * Verifies that browser successfully enters and exits VR mode when told to
     * on Daydream-ready devices via a screendiffing check.
     * Requires that the phone is unlocked.
     */
    @Restriction(RESTRICTION_TYPE_DAYDREAM)
    @Feature("RenderTest")
    @MediumTest
    public void testEnterExitVrModeSupportedImage() throws IOException {
        enterExitVrModeImage(true);
    }

    /**
     * Verifies that browser does not enter VR mode on Non-Daydream-ready devices
     * via a screendiffing check. Requires that the phone is unlocked.
     */
    @Restriction(RESTRICTION_TYPE_NON_DAYDREAM)
    @Feature("RenderTest")
    @MediumTest
    public void testEnterExitVrModeUnsupportedImage() throws IOException {
        enterExitVrModeImage(false);
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
                selector.set(compositorViewHolder.detachForVR());
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
                compositorViewHolder.onExitVR(selector.get());
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
