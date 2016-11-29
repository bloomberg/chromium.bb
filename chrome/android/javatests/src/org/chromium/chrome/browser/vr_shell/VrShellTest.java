// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr_shell;

import static org.chromium.chrome.test.util.ChromeRestriction.RESTRICTION_TYPE_DAYDREAM;
import static org.chromium.chrome.test.util.ChromeRestriction.RESTRICTION_TYPE_NON_DAYDREAM;

import android.content.pm.ActivityInfo;
import android.test.suitebuilder.annotation.MediumTest;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.test.ChromeTabbedActivityTestBase;
import org.chromium.chrome.test.util.RenderUtils.ViewRenderer;

import java.io.IOException;

/**
 * Instrumentation tests for VR Shell (Chrome VR)
 */
public class VrShellTest extends ChromeTabbedActivityTestBase {
    private static final String GOLDEN_DIR =
            "chrome/test/data/android/render_tests";
    private VrShellDelegate mDelegate;
    private ViewRenderer mViewRenderer;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mDelegate = getActivity().getVrShellDelegate();
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityOnBlankPage();
        mViewRenderer = new ViewRenderer(getActivity(),
                GOLDEN_DIR, this.getClass().getSimpleName());
    }

    private void forceEnterVr() {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mDelegate.enterVRIfNecessary();
            }
        });
    }

    private void forceExitVr() {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mDelegate.exitVRIfNecessary(false);
            }
        });
    }

    private void testEnterExitVrMode(boolean supported) {
        forceEnterVr();
        getInstrumentation().waitForIdleSync();
        if (supported) {
            assertTrue(mDelegate.isInVR());
        } else {
            assertFalse(mDelegate.isInVR());
        }
        forceExitVr();
        assertFalse(mDelegate.isInVR());
    }

    private void testEnterExitVrModeImage(boolean supported) throws IOException {
        int prevOrientation = getActivity().getRequestedOrientation();
        getActivity().setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
        getInstrumentation().waitForIdleSync();
        mViewRenderer.renderAndCompare(
                getActivity().getWindow().getDecorView().getRootView(),
                "blank_page");

        forceEnterVr();
        getInstrumentation().waitForIdleSync();
        // Currently, screenshots only show the static UI overlay, not the
        // actual content. Thus, 1:1 pixel checking is reliable until a
        // way to take screenshots of VR content is added, in which case
        // % similarity or some other method will need to be used. We're
        // assuming that if the UI overlay is visible, then the device has
        // successfully entered VR mode.
        if (supported) {
            mViewRenderer.renderAndCompare(
                    getActivity().getWindow().getDecorView().getRootView(),
                    "vr_entered");
        } else {
            mViewRenderer.renderAndCompare(
                    getActivity().getWindow().getDecorView().getRootView(),
                    "blank_page");
        }

        forceExitVr();
        getActivity().setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
        getInstrumentation().waitForIdleSync();
        mViewRenderer.renderAndCompare(
                getActivity().getWindow().getDecorView().getRootView(),
                "blank_page");

        getActivity().setRequestedOrientation(prevOrientation);
    }

    /**
     * Verifies that browser successfully enters and exits VR mode when told to
     * on Daydream-ready devices. Requires that the phone is unlocked.
     */
    @Restriction(RESTRICTION_TYPE_DAYDREAM)
    @MediumTest
    public void testEnterExitVrModeSupported() {
        testEnterExitVrMode(true);
    }

    /**
     * Verifies that browser does not enter VR mode on Non-Daydream-ready devices.
     */
    @Restriction(RESTRICTION_TYPE_NON_DAYDREAM)
    @MediumTest
    public void testEnterExitVrModeUnsupported() {
        testEnterExitVrMode(false);
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
        testEnterExitVrModeImage(true);
    }

    /**
     * Verifies that browser does not enter VR mode on Non-Daydream-ready devices
     * via a screendiffing check. Requires that the phone is unlocked.
     */
    @Restriction(RESTRICTION_TYPE_NON_DAYDREAM)
    @Feature("RenderTest")
    @MediumTest
    public void testEnterExitVrModeUnsupportedImage() throws IOException {
        testEnterExitVrModeImage(false);
    }
}
