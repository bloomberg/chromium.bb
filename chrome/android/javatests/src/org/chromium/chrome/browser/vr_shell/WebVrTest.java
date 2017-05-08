// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr_shell;

import static org.chromium.chrome.browser.vr_shell.VrUtils.POLL_TIMEOUT_SHORT_MS;
import static org.chromium.chrome.test.util.ChromeRestriction.RESTRICTION_TYPE_VIEWER_DAYDREAM;
import static org.chromium.chrome.test.util.ChromeRestriction.RESTRICTION_TYPE_VIEWER_NON_DAYDREAM;
import static org.chromium.chrome.test.util.ChromeRestriction.RESTRICTION_TYPE_WEBVR_SUPPORTED;

import android.os.Build;
import android.support.test.filters.LargeTest;
import android.support.test.filters.MediumTest;
import android.support.test.filters.SmallTest;
import android.widget.TextView;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.R;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

/**
 * End-to-end tests for WebVR using the WebVR test framework from VrTestBase.
 */
@CommandLineFlags.Add("enable-webvr")
@Restriction(RESTRICTION_TYPE_WEBVR_SUPPORTED)
public class WebVrTest extends VrTestBase {
    private static final String TAG = "WebVrTest";

    /**
     * Tests that a successful requestPresent call actually enters VR
     */
    @SmallTest
    public void testRequestPresentEntersVr() throws InterruptedException {
        String testName = "test_requestPresent_enters_vr";
        loadUrl(getHtmlTestFile(testName), PAGE_LOAD_TIMEOUT_S);
        assertTrue("VRDisplay found", vrDisplayFound(mWebContents));
        enterVrTapAndWait(mWebContents);
        assertTrue("VrShellDelegate is in VR", VrShellDelegate.isInVr());
        endTest(mWebContents);
    }

    /**
     * Tests that scanning the Daydream View NFC tag on supported devices fires the
     * vrdisplayactivate event.
     */
    @SmallTest
    @Restriction(RESTRICTION_TYPE_VIEWER_DAYDREAM)
    public void testNfcFiresVrdisplayactivate() throws InterruptedException {
        String testName = "test_nfc_fires_vrdisplayactivate";
        loadUrl(getHtmlTestFile(testName), PAGE_LOAD_TIMEOUT_S);
        simNfcScanAndWait(mWebContents);
        endTest(mWebContents);
    }

    /**
     * Tests that screen touches are not registered when the viewer is a Daydream View.
     */
    @LargeTest
    @Restriction(RESTRICTION_TYPE_VIEWER_DAYDREAM)
    public void testScreenTapsNotRegisteredOnDaydream() throws InterruptedException {
        String testName = "test_screen_taps_not_registered_on_daydream";
        loadUrl(getHtmlTestFile(testName), PAGE_LOAD_TIMEOUT_S);
        assertTrue("VRDisplay found", vrDisplayFound(mWebContents));
        executeStepAndWait("stepVerifyNoInitialTaps()", mWebContents);
        enterVrTapAndWait(mWebContents);
        // Wait on VrShellImpl to say that its parent consumed the touch event
        // Set to 2 because there's an ACTION_DOWN followed by ACTION_UP
        final CountDownLatch touchRegisteredLatch = new CountDownLatch(2);
        ((VrShellImpl) VrShellDelegate.getVrShellForTesting())
                .setOnDispatchTouchEventForTesting(new OnDispatchTouchEventCallback() {
                    @Override
                    public void onDispatchTouchEvent(
                            boolean parentConsumed, boolean cardboardTriggered) {
                        if (!parentConsumed) fail("Parent did not consume event");
                        if (cardboardTriggered) fail("Cardboard event triggered");
                        touchRegisteredLatch.countDown();
                    }
                });
        enterVrTap();
        assertTrue("VrShellImpl dispatched touches",
                touchRegisteredLatch.await(POLL_TIMEOUT_SHORT_MS, TimeUnit.MILLISECONDS));
        executeStepAndWait("stepVerifyNoAdditionalTaps()", mWebContents);
        endTest(mWebContents);
    }

    /**
     * Tests that Daydream controller clicks are registered as screen taps when the viewer is a
     * Daydream View.
     */
    @LargeTest
    @Restriction(RESTRICTION_TYPE_VIEWER_DAYDREAM)
    public void testControllerClicksRegisteredAsTapsOnDaydream() throws InterruptedException {
        EmulatedVrController controller = new EmulatedVrController(getActivity());
        String testName = "test_screen_taps_registered";
        loadUrl(getHtmlTestFile(testName), PAGE_LOAD_TIMEOUT_S);
        assertTrue("VRDisplay found", vrDisplayFound(mWebContents));
        executeStepAndWait("stepVerifyNoInitialTaps()", mWebContents);
        // Tap and wait to enter VR
        enterVrTapAndWait(mWebContents);
        // Send a controller click and wait for JavaScript to receive it
        controller.pressReleaseTouchpadButton();
        waitOnJavaScriptStep(mWebContents);
        endTest(mWebContents);
    }

    /**
     * Tests that screen touches are still registered when the viewer is Cardboard.
     */
    @MediumTest
    @Restriction(RESTRICTION_TYPE_VIEWER_NON_DAYDREAM)
    @DisableIf.Build(message = "Flaky on L crbug.com/713781",
            sdk_is_greater_than = Build.VERSION_CODES.KITKAT,
            sdk_is_less_than = Build.VERSION_CODES.M)
    public void testScreenTapsRegisteredOnCardboard() throws InterruptedException {
        String testName = "test_screen_taps_registered";
        loadUrl(getHtmlTestFile(testName), PAGE_LOAD_TIMEOUT_S);
        assertTrue("VRDisplay found", vrDisplayFound(mWebContents));
        executeStepAndWait("stepVerifyNoInitialTaps()", mWebContents);
        // Tap and wait to enter VR
        enterVrTapAndWait(mWebContents);
        // Tap and wait for JavaScript to receive it
        enterVrTapAndWait(mWebContents);
        endTest(mWebContents);
    }

    /**
     * Tests that non-focused tabs cannot get pose information.
     */
    @SmallTest
    public void testPoseDataUnfocusedTab() throws InterruptedException {
        String testName = "test_pose_data_unfocused_tab";
        loadUrl(getHtmlTestFile(testName), PAGE_LOAD_TIMEOUT_S);
        assertTrue("VRDisplay found", vrDisplayFound(mWebContents));
        executeStepAndWait("stepCheckFrameDataWhileFocusedTab()", mWebContents);

        loadUrlInNewTab("about:blank");

        executeStepAndWait("stepCheckFrameDataWhileNonFocusedTab()", mWebContents);
        endTest(mWebContents);
    }

    /**
     * Helper function to run the tests checking for the upgrade/install InfoBar being present since
     * all that differs is the value returned by VrCoreVersionChecker and a couple asserts.
     *
     * @param checkerReturnValue The value to have the VrCoreVersionChecker return
     */
    private void infoBarTestHelper(int checkerReturnValue) throws InterruptedException {
        MockVrCoreVersionCheckerImpl mockChecker = new MockVrCoreVersionCheckerImpl();
        mockChecker.setMockReturnValue(checkerReturnValue);
        VrUtils.getVrShellDelegateInstance().overrideVrCoreVersionCheckerForTesting(mockChecker);
        String testName = "generic_webvr_page";
        loadUrl(getHtmlTestFile(testName), PAGE_LOAD_TIMEOUT_S);
        String displayFound = "VRDisplay Found";
        String barPresent = "InfoBar present";
        if (checkerReturnValue == VrCoreVersionChecker.VR_READY) {
            assertTrue(displayFound, vrDisplayFound(mWebContents));
            assertFalse(barPresent,
                    VrUtils.isUpdateInstallInfoBarPresent(
                            getActivity().getWindow().getDecorView()));
        } else if (checkerReturnValue == VrCoreVersionChecker.VR_OUT_OF_DATE
                || checkerReturnValue == VrCoreVersionChecker.VR_NOT_AVAILABLE) {
            // Out of date and missing cases are the same, but with different text
            String expectedMessage, expectedButton;
            if (checkerReturnValue == VrCoreVersionChecker.VR_OUT_OF_DATE) {
                expectedMessage =
                        getActivity().getString(R.string.vr_services_check_infobar_update_text);
                expectedButton =
                        getActivity().getString(R.string.vr_services_check_infobar_update_button);
            } else {
                expectedMessage =
                        getActivity().getString(R.string.vr_services_check_infobar_install_text);
                expectedButton =
                        getActivity().getString(R.string.vr_services_check_infobar_install_button);
            }
            assertFalse(displayFound, vrDisplayFound(mWebContents));
            assertTrue(barPresent,
                    VrUtils.isUpdateInstallInfoBarPresent(
                            getActivity().getWindow().getDecorView()));
            TextView tempView = (TextView) getActivity().getWindow().getDecorView().findViewById(
                    R.id.infobar_message);
            assertEquals(expectedMessage, tempView.getText().toString());
            tempView = (TextView) getActivity().getWindow().getDecorView().findViewById(
                    R.id.button_primary);
            assertEquals(expectedButton, tempView.getText().toString());
        } else if (checkerReturnValue == VrCoreVersionChecker.VR_NOT_SUPPORTED) {
            assertFalse(displayFound, vrDisplayFound(mWebContents));
            assertFalse(barPresent,
                    VrUtils.isUpdateInstallInfoBarPresent(
                            getActivity().getWindow().getDecorView()));
        } else {
            fail("Invalid VrCoreVersionChecker value: " + String.valueOf(checkerReturnValue));
        }
        assertEquals(checkerReturnValue, mockChecker.getLastReturnValue());
    }

    /**
     * Tests that the upgrade/install VR Services InfoBar is not present when VR Services is
     * installed and up to date.
     */
    @MediumTest
    public void testInfoBarNotPresentWhenVrServicesCurrent() throws InterruptedException {
        infoBarTestHelper(VrCoreVersionChecker.VR_READY);
    }

    /**
     * Tests that the upgrade VR Services InfoBar is present when VR Services is outdated.
     */
    @MediumTest
    public void testInfoBarPresentWhenVrServicesOutdated() throws InterruptedException {
        infoBarTestHelper(VrCoreVersionChecker.VR_OUT_OF_DATE);
    }

    /**
     * Tests that the install VR Services InfoBar is present when VR Services is missing.
     */
    @MediumTest
    public void testInfoBarPresentWhenVrServicesMissing() throws InterruptedException {
        infoBarTestHelper(VrCoreVersionChecker.VR_NOT_AVAILABLE);
    }

    /**
     * Tests that the install VR Services InfoBar is not present when VR is not supported on the
     * device.
     */
    @MediumTest
    public void testInfoBarNotPresentWhenVrServicesNotSupported() throws InterruptedException {
        infoBarTestHelper(VrCoreVersionChecker.VR_NOT_SUPPORTED);
    }

    /**
     * Tests that the reported WebVR capabilities match expectations on the devices the WebVR tests
     * are run on continuously.
     */
    @MediumTest
    public void testDeviceCapabilitiesMatchExpectations() throws InterruptedException {
        String testName = "test_device_capabilities_match_expectations";
        loadUrl(getHtmlTestFile(testName), PAGE_LOAD_TIMEOUT_S);
        assertTrue("VRDisplayFound", vrDisplayFound(mWebContents));
        executeStepAndWait("stepCheckDeviceCapabilities('" + Build.DEVICE + "')", mWebContents);
        endTest(mWebContents);
    }

    /**
     * Tests that focus is locked to the presenting display for purposes of VR input.
     */
    @MediumTest
    @DisableIf.Build(message = "Flaky on L crbug.com/713781",
            sdk_is_greater_than = Build.VERSION_CODES.KITKAT,
            sdk_is_less_than = Build.VERSION_CODES.M)
    public void testPresentationLocksFocus() throws InterruptedException {
        String testName = "test_presentation_locks_focus";
        loadUrl(getHtmlTestFile(testName), PAGE_LOAD_TIMEOUT_S);
        enterVrTapAndWait(mWebContents);
        waitOnJavaScriptStep(mWebContents);
        endTest(mWebContents);
    }
}
