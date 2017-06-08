// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr_shell;

import static org.chromium.chrome.browser.vr_shell.VrTestRule.PAGE_LOAD_TIMEOUT_S;
import static org.chromium.chrome.browser.vr_shell.util.VrUtils.POLL_TIMEOUT_SHORT_MS;
import static org.chromium.chrome.test.util.ChromeRestriction.RESTRICTION_TYPE_VIEWER_DAYDREAM;
import static org.chromium.chrome.test.util.ChromeRestriction.RESTRICTION_TYPE_VIEWER_NON_DAYDREAM;

import android.os.Build;
import android.support.test.filters.LargeTest;
import android.support.test.filters.MediumTest;
import android.support.test.filters.SmallTest;
import android.view.View;
import android.widget.TextView;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.vr_shell.mock.MockVrCoreVersionCheckerImpl;
import org.chromium.chrome.browser.vr_shell.util.VrUtils;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

/**
 * End-to-end tests for WebVR using the WebVR test framework from VrTestRule.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        ChromeActivityTestRule.DISABLE_NETWORK_PREDICTION_FLAG, "enable-webvr"})
@MinAndroidSdkLevel(Build.VERSION_CODES.KITKAT) // WebVR is only supported on K+
public class WebVrTest {
    @Rule
    public VrTestRule mVrTestRule = new VrTestRule();

    @Before
    public void setUp() throws InterruptedException {
        Assert.assertFalse("VrShellDelegate is in VR", VrShellDelegate.isInVr());
    }

    /**
     * Tests that a successful requestPresent call actually enters VR
     */
    @Test
    @SmallTest
    public void testRequestPresentEntersVr() throws InterruptedException {
        mVrTestRule.loadUrlAndAwaitInitialization(
                VrTestRule.getHtmlTestFile("test_requestPresent_enters_vr"), PAGE_LOAD_TIMEOUT_S);
        mVrTestRule.enterPresentationAndWait(
                mVrTestRule.getFirstTabCvc(), mVrTestRule.getFirstTabWebContents());
        Assert.assertTrue("VrShellDelegate is in VR", VrShellDelegate.isInVr());
        mVrTestRule.endTest(mVrTestRule.getFirstTabWebContents());
    }

    /**
     * Tests that scanning the Daydream View NFC tag on supported devices fires the
     * vrdisplayactivate event.
     */
    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_VIEWER_DAYDREAM)
    public void testNfcFiresVrdisplayactivate() throws InterruptedException {
        mVrTestRule.loadUrlAndAwaitInitialization(
                VrTestRule.getHtmlTestFile("test_nfc_fires_vrdisplayactivate"),
                PAGE_LOAD_TIMEOUT_S);
        mVrTestRule.simNfcScanAndWait(
                mVrTestRule.getActivity(), mVrTestRule.getFirstTabWebContents());
        mVrTestRule.endTest(mVrTestRule.getFirstTabWebContents());
    }

    /**
     * Tests that screen touches are not registered when the viewer is a Daydream View.
     */
    @Test
    @LargeTest
    @Restriction(RESTRICTION_TYPE_VIEWER_DAYDREAM)
    public void testScreenTapsNotRegisteredOnDaydream() throws InterruptedException {
        mVrTestRule.loadUrlAndAwaitInitialization(
                VrTestRule.getHtmlTestFile("test_screen_taps_not_registered_on_daydream"),
                PAGE_LOAD_TIMEOUT_S);
        mVrTestRule.executeStepAndWait(
                "stepVerifyNoInitialTaps()", mVrTestRule.getFirstTabWebContents());
        mVrTestRule.enterPresentationAndWait(
                mVrTestRule.getFirstTabCvc(), mVrTestRule.getFirstTabWebContents());
        // Wait on VrShellImpl to say that its parent consumed the touch event
        // Set to 2 because there's an ACTION_DOWN followed by ACTION_UP
        final CountDownLatch touchRegisteredLatch = new CountDownLatch(2);
        ((VrShellImpl) VrShellDelegate.getVrShellForTesting())
                .setOnDispatchTouchEventForTesting(new OnDispatchTouchEventCallback() {
                    @Override
                    public void onDispatchTouchEvent(boolean parentConsumed) {
                        if (!parentConsumed) Assert.fail("Parent did not consume event");
                        touchRegisteredLatch.countDown();
                    }
                });
        mVrTestRule.sendCardboardClick(mVrTestRule.getActivity());
        Assert.assertTrue("VrShellImpl dispatched touches",
                touchRegisteredLatch.await(POLL_TIMEOUT_SHORT_MS, TimeUnit.MILLISECONDS));
        mVrTestRule.executeStepAndWait(
                "stepVerifyNoAdditionalTaps()", mVrTestRule.getFirstTabWebContents());
        mVrTestRule.endTest(mVrTestRule.getFirstTabWebContents());
    }

    /**
     * Tests that Daydream controller clicks are registered as screen taps when the viewer is a
     * Daydream View.
     */
    @Test
    @LargeTest
    @Restriction(RESTRICTION_TYPE_VIEWER_DAYDREAM)
    public void testControllerClicksRegisteredAsTapsOnDaydream() throws InterruptedException {
        EmulatedVrController controller = new EmulatedVrController(mVrTestRule.getActivity());
        mVrTestRule.loadUrlAndAwaitInitialization(
                VrTestRule.getHtmlTestFile("test_screen_taps_registered"), PAGE_LOAD_TIMEOUT_S);
        mVrTestRule.executeStepAndWait(
                "stepVerifyNoInitialTaps()", mVrTestRule.getFirstTabWebContents());
        // Wait to enter VR
        mVrTestRule.enterPresentationAndWait(
                mVrTestRule.getFirstTabCvc(), mVrTestRule.getFirstTabWebContents());
        // Send a controller click and wait for JavaScript to receive it
        controller.pressReleaseTouchpadButton();
        mVrTestRule.waitOnJavaScriptStep(mVrTestRule.getFirstTabWebContents());
        mVrTestRule.endTest(mVrTestRule.getFirstTabWebContents());
    }

    /**
     * Tests that screen touches are still registered when the viewer is Cardboard.
     */
    @Test
    @MediumTest
    @Restriction(RESTRICTION_TYPE_VIEWER_NON_DAYDREAM)
    @DisableIf.Build(message = "Flaky on L crbug.com/713781",
            sdk_is_greater_than = Build.VERSION_CODES.KITKAT,
            sdk_is_less_than = Build.VERSION_CODES.M)
    public void testScreenTapsRegisteredOnCardboard() throws InterruptedException {
        mVrTestRule.loadUrlAndAwaitInitialization(
                VrTestRule.getHtmlTestFile("test_screen_taps_registered"), PAGE_LOAD_TIMEOUT_S);
        mVrTestRule.executeStepAndWait(
                "stepVerifyNoInitialTaps()", mVrTestRule.getFirstTabWebContents());
        // Wait to enter VR
        mVrTestRule.enterPresentationAndWait(
                mVrTestRule.getFirstTabCvc(), mVrTestRule.getFirstTabWebContents());
        // Tap and wait for JavaScript to receive it
        mVrTestRule.sendCardboardClickAndWait(
                mVrTestRule.getActivity(), mVrTestRule.getFirstTabWebContents());
        mVrTestRule.endTest(mVrTestRule.getFirstTabWebContents());
    }

    /**
     * Tests that non-focused tabs cannot get pose information.
     */
    @Test
    @SmallTest
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

    /**
     * Helper function to run the tests checking for the upgrade/install InfoBar being present since
     * all that differs is the value returned by VrCoreVersionChecker and a couple asserts.
     *
     * @param checkerReturnCompatibility The compatibility to have the VrCoreVersionChecker return
     */
    private void infoBarTestHelper(final int checkerReturnCompatibility) {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                MockVrCoreVersionCheckerImpl mockChecker = new MockVrCoreVersionCheckerImpl();
                mockChecker.setMockReturnValue(new VrCoreInfo(null, checkerReturnCompatibility));
                VrShellDelegate.getInstanceForTesting().overrideVrCoreVersionCheckerForTesting(
                        mockChecker);
                Assert.assertEquals(
                        checkerReturnCompatibility, mockChecker.getLastReturnValue().compatibility);
            }
        });
        View decorView = mVrTestRule.getActivity().getWindow().getDecorView();
        if (checkerReturnCompatibility == VrCoreCompatibility.VR_READY) {
            VrUtils.expectInfoBarPresent(decorView, false);
        } else if (checkerReturnCompatibility == VrCoreCompatibility.VR_OUT_OF_DATE
                || checkerReturnCompatibility == VrCoreCompatibility.VR_NOT_AVAILABLE) {
            // Out of date and missing cases are the same, but with different text
            String expectedMessage, expectedButton;
            if (checkerReturnCompatibility == VrCoreCompatibility.VR_OUT_OF_DATE) {
                expectedMessage = mVrTestRule.getActivity().getString(
                        R.string.vr_services_check_infobar_update_text);
                expectedButton = mVrTestRule.getActivity().getString(
                        R.string.vr_services_check_infobar_update_button);
            } else {
                expectedMessage = mVrTestRule.getActivity().getString(
                        R.string.vr_services_check_infobar_install_text);
                expectedButton = mVrTestRule.getActivity().getString(
                        R.string.vr_services_check_infobar_install_button);
            }
            VrUtils.expectInfoBarPresent(decorView, true);
            TextView tempView =
                    (TextView) mVrTestRule.getActivity().getWindow().getDecorView().findViewById(
                            R.id.infobar_message);
            Assert.assertEquals(expectedMessage, tempView.getText().toString());
            tempView = (TextView) mVrTestRule.getActivity().getWindow().getDecorView().findViewById(
                    R.id.button_primary);
            Assert.assertEquals(expectedButton, tempView.getText().toString());
        } else if (checkerReturnCompatibility == VrCoreCompatibility.VR_NOT_SUPPORTED) {
            VrUtils.expectInfoBarPresent(decorView, false);
        } else {
            Assert.fail("Invalid VrCoreVersionChecker compatibility: "
                    + String.valueOf(checkerReturnCompatibility));
        }
    }

    /**
     * Tests that the upgrade/install VR Services InfoBar is not present when VR Services is
     * installed and up to date.
     */
    @Test
    @MediumTest
    public void testInfoBarNotPresentWhenVrServicesCurrent() throws InterruptedException {
        infoBarTestHelper(VrCoreCompatibility.VR_READY);
    }

    /**
     * Tests that the upgrade VR Services InfoBar is present when VR Services is outdated.
     */
    @Test
    @MediumTest
    public void testInfoBarPresentWhenVrServicesOutdated() throws InterruptedException {
        infoBarTestHelper(VrCoreCompatibility.VR_OUT_OF_DATE);
    }

    /**
     * Tests that the install VR Services InfoBar is present when VR Services is missing.
     */
    @Test
    @MediumTest
    public void testInfoBarPresentWhenVrServicesMissing() throws InterruptedException {
        infoBarTestHelper(VrCoreCompatibility.VR_NOT_AVAILABLE);
    }

    /**
     * Tests that the install VR Services InfoBar is not present when VR is not supported on the
     * device.
     */
    @Test
    @MediumTest
    public void testInfoBarNotPresentWhenVrServicesNotSupported() throws InterruptedException {
        infoBarTestHelper(VrCoreCompatibility.VR_NOT_SUPPORTED);
    }

    /**
     * Tests that the reported WebVR capabilities match expectations on the devices the WebVR tests
     * are run on continuously.
     */
    @Test
    @MediumTest
    public void testDeviceCapabilitiesMatchExpectations() throws InterruptedException {
        mVrTestRule.loadUrlAndAwaitInitialization(
                VrTestRule.getHtmlTestFile("test_device_capabilities_match_expectations"),
                PAGE_LOAD_TIMEOUT_S);
        mVrTestRule.executeStepAndWait("stepCheckDeviceCapabilities('" + Build.DEVICE + "')",
                mVrTestRule.getFirstTabWebContents());
        mVrTestRule.endTest(mVrTestRule.getFirstTabWebContents());
    }

    /**
     * Tests that focus is locked to the presenting display for purposes of VR input.
     */
    @Test
    @MediumTest
    public void testPresentationLocksFocus() throws InterruptedException {
        mVrTestRule.loadUrlAndAwaitInitialization(
                VrTestRule.getHtmlTestFile("test_presentation_locks_focus"), PAGE_LOAD_TIMEOUT_S);
        mVrTestRule.enterPresentationAndWait(
                mVrTestRule.getFirstTabCvc(), mVrTestRule.getFirstTabWebContents());
        mVrTestRule.endTest(mVrTestRule.getFirstTabWebContents());
    }
}
