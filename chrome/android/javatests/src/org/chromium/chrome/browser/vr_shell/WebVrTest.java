// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr_shell;

import static org.chromium.chrome.browser.vr_shell.VrUtils.POLL_CHECK_INTERVAL_LONG_MS;
import static org.chromium.chrome.browser.vr_shell.VrUtils.POLL_CHECK_INTERVAL_SHORT_MS;
import static org.chromium.chrome.browser.vr_shell.VrUtils.POLL_TIMEOUT_LONG_MS;
import static org.chromium.chrome.browser.vr_shell.VrUtils.POLL_TIMEOUT_SHORT_MS;
import static org.chromium.chrome.test.util.ChromeRestriction.RESTRICTION_TYPE_VIEWER_DAYDREAM;
import static org.chromium.chrome.test.util.ChromeRestriction.RESTRICTION_TYPE_VIEWER_NON_DAYDREAM;
import static org.chromium.chrome.test.util.ChromeRestriction.RESTRICTION_TYPE_WEBVR_SUPPORTED;

import android.os.Build;
import android.support.test.filters.MediumTest;
import android.support.test.filters.SmallTest;
import android.widget.TextView;

import org.chromium.base.Log;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Restriction;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.test.ChromeTabbedActivityTestBase;
import org.chromium.content.browser.test.util.ClickUtils;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.JavaScriptUtils;
import org.chromium.content_public.browser.WebContents;

import java.util.concurrent.Callable;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

/**
 * This is a workaround for testing aspects of WebVR that aren't testable with
 * WebVR's mocked layout tests, such as E2E tests.
 *
 * The general test flow is:
 * - Load the HTML file containing the test, which:
 *   - Loads the WebVR boilerplate code and some test functions
 *   - Sets up common elements like the canvas and synchronization variable
 *   - Sets up any steps that need to be triggered by the Java code
 * - Check if any VRDisplay objects were found and fail the test if it doesn't
 *       match what we expect for that test
 * - Repeat:
 *   - Run any necessary Java-side code, e.g. trigger a user action
 *   - Trigger the next JavaScript test step and wait for it to finish
 *
 * The JavaScript code will automatically process test results once all
 * testharness.js tests are done, just like in layout tests. Once the results
 * are processed, the JavaScript code will automatically signal the Java code,
 * which can then grab the results and pass/fail the instrumentation test.
 */
@CommandLineFlags.Add("enable-webvr")
@Restriction(RESTRICTION_TYPE_WEBVR_SUPPORTED)
public class WebVrTest extends ChromeTabbedActivityTestBase {
    private static final String TAG = "WebVrTest";
    private static final String TEST_DIR = "chrome/test/data/android/webvr_instrumentation";
    private static final int PAGE_LOAD_TIMEOUT_S = 10;

    private WebContents mWebContents;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mWebContents = getActivity().getActivityTab().getWebContents();
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityOnBlankPage();
    }

    /**
     * Gets the file:// URL to the test file
     * @param testName The name of the test whose file will be retrieved
     * @return The file:// URL to the specified test file
     */
    private String getHtmlTestFile(String testName) {
        return "file://" + UrlUtils.getIsolatedTestFilePath(TEST_DIR) + "/html/" + testName
                + ".html";
    }

    /**
     * Blocks until the promise returned by nagivator.getVRDisplays() resolves,
     * then checks whether a VRDisplay was actually found.
     * @param webContents The WebContents to run the JavaScript through
     * @return Whether a VRDisplay was found
     */
    private boolean vrDisplayFound(WebContents webContents) {
        pollJavaScriptBoolean("vrDisplayPromiseDone", POLL_TIMEOUT_SHORT_MS, webContents);
        return !runJavaScriptOrFail("vrDisplay", POLL_TIMEOUT_SHORT_MS, webContents).equals("null");
    }

    /**
     * Use to tap in the middle of the screen, triggering the canvas' onclick
     * to fulfil WebVR's gesture requirement for presenting.
     */
    private void enterVrTap() {
        ClickUtils.mouseSingleClickView(
                getInstrumentation(), getActivity().getWindow().getDecorView().getRootView());
    }

    /**
     * Taps in the middle of the screen then waits for the JavaScript step to finish.
     * @param webContents The WebContents for the tab the JavaScript step is in
     */
    private void enterVrTapAndWait(WebContents webContents) {
        enterVrTap();
        waitOnJavaScriptStep(webContents);
    }

    /**
     * Use to simulate a Daydream View NFC scan without blocking afterwards
     */
    private void simNfcScan() {
        VrUtils.simNfc(getActivity());
    }

    /**
     * Simulate an NFC scan and wait for the JavaScript code in the given
     * WebContents to signal that it is done with the step.
     * @param webContents The WebContents for the JavaScript that will be polled
     */
    private void simNfcScanAndWait(WebContents webContents) {
        simNfcScan();
        waitOnJavaScriptStep(webContents);
    }

    /**
     * Helper function to run the given JavaScript, return the return value,
     * and fail if a timeout/interrupt occurs so we don't have to catch or
     * declare exceptions all the time.
     * @param js The JavaScript to run
     * @param timeout The timeout in milliseconds before a failure
     * @param webContents The WebContents object to run the JavaScript in
     * @return The return value of the JavaScript
     */
    private String runJavaScriptOrFail(String js, int timeout, WebContents webContents) {
        try {
            return JavaScriptUtils.executeJavaScriptAndWaitForResult(
                    webContents, js, timeout, TimeUnit.MILLISECONDS);
        } catch (InterruptedException | TimeoutException e) {
            fail("Fatal interruption or timeout running JavaScript: " + js);
        }
        return "Not reached";
    }

    /**
     * Ends the test harness test and checks whether there it passed
     * @param webContents The WebContents for the tab to check results in
     * @return "Passed" if test passed, String with failure reason otherwise
     */
    private String checkResults(WebContents webContents) {
        if (runJavaScriptOrFail("testPassed", POLL_TIMEOUT_SHORT_MS, webContents).equals("true")) {
            return "Passed";
        }
        return runJavaScriptOrFail("resultString", POLL_TIMEOUT_SHORT_MS, webContents);
    }

    /**
     * Helper function to end the test harness test and assert that it passed,
     * setting the failure reason as the description if it didn't.
     * @param webContents The WebContents for the tab to check test results in
     */
    private void endTest(WebContents webContents) {
        assertEquals("Passed", checkResults(webContents));
    }

    /**
     * Polls the provided JavaScript boolean until the timeout is reached or
     * the boolean is true.
     * @param boolName The name of the JavaScript boolean or expression to poll
     * @param timeoutMs The polling timeout in milliseconds
     * @param webContents The WebContents to run the JavaScript through
     * @return True if the boolean evaluated to true, false if timed out
     */
    private boolean pollJavaScriptBoolean(
            final String boolName, int timeoutMs, final WebContents webContents) {
        try {
            CriteriaHelper.pollInstrumentationThread(Criteria.equals(true, new Callable<Boolean>() {
                @Override
                public Boolean call() {
                    String result = "false";
                    try {
                        result = JavaScriptUtils.executeJavaScriptAndWaitForResult(webContents,
                                boolName, POLL_CHECK_INTERVAL_SHORT_MS, TimeUnit.MILLISECONDS);
                    } catch (InterruptedException | TimeoutException e) {
                        // Expected to happen regularly, do nothing
                    }
                    return Boolean.parseBoolean(result);
                }
            }), timeoutMs, POLL_CHECK_INTERVAL_LONG_MS);
        } catch (AssertionError e) {
            Log.d(TAG, "pollJavaScriptBoolean() timed out");
            return false;
        }
        return true;
    }

    /**
     * Waits for a JavaScript step to finish, asserting that the step finished
     * instead of timing out.
     * @param webContents The WebContents for the tab the JavaScript step is in
     */
    private void waitOnJavaScriptStep(WebContents webContents) {
        assertTrue("Polling JavaScript boolean javascriptDone succeeded",
                pollJavaScriptBoolean("javascriptDone", POLL_TIMEOUT_LONG_MS, webContents));
        // Reset the synchronization boolean
        runJavaScriptOrFail("javascriptDone = false", POLL_TIMEOUT_SHORT_MS, webContents);
    }

    /**
     * Executes a JavaScript step function using the given WebContents.
     * @param stepFunction The JavaScript step function to call
     * @param webContents The WebContents for the tab the JavaScript is in
     */
    private void executeStepAndWait(String stepFunction, WebContents webContents) {
        // Run the step and block
        JavaScriptUtils.executeJavaScript(webContents, stepFunction);
        waitOnJavaScriptStep(webContents);
    }

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
     * Tests that scanning the Daydream View NFC tag on supported devices
     * fires the vrdisplayactivate event.
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
     * Tests that screen touches are not registered when the viewer is a
     * Daydream View.
     */
    /*
    @LargeTest
    @Restriction(RESTRICTION_TYPE_VIEWER_DAYDREAM)
    */
    @DisabledTest(message = "crbug.com/713781")
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
     * Tests that screen touches are still registered when the viewer is
     * Cardboard.
     */
    @MediumTest
    @Restriction(RESTRICTION_TYPE_VIEWER_NON_DAYDREAM)
    public void testScreenTapsRegisteredOnCardboard() throws InterruptedException {
        String testName = "test_screen_taps_registered_on_cardboard";
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
     * Helper function to run the tests checking for the upgrade/install InfoBar
     * being present since all that differs is the value returned by VrCoreVersionChecker
     * and a couple asserts.
     * @param checkerReturnValue The value to have the VrCoreVersionChecker return
     */
    private void infoBarTestHelper(int checkerReturnValue) throws InterruptedException {
        MockVrCoreVersionCheckerImpl mockChecker = new MockVrCoreVersionCheckerImpl();
        mockChecker.setMockReturnValue(checkerReturnValue);
        VrShellDelegate.getInstanceForTesting().overrideVrCoreVersionCheckerForTesting(mockChecker);
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
     * Tests that the upgrade/install VR Services InfoBar is not present when
     * VR Services is installed and up to date.
     */
    @MediumTest
    public void testInfoBarNotPresentWhenVrServicesCurrent() throws InterruptedException {
        infoBarTestHelper(VrCoreVersionChecker.VR_READY);
    }

    /**
     * Tests that the upgrade VR Services InfoBar is present when
     * VR Services is outdated.
     */
    @MediumTest
    public void testInfoBarPresentWhenVrServicesOutdated() throws InterruptedException {
        infoBarTestHelper(VrCoreVersionChecker.VR_OUT_OF_DATE);
    }

    /**
     * Tests that the install VR Services InfoBar is present when VR
     * Services is missing.
     */
    @MediumTest
    public void testInfoBarPresentWhenVrServicesMissing() throws InterruptedException {
        infoBarTestHelper(VrCoreVersionChecker.VR_NOT_AVAILABLE);
    }

    /**
     * Tests that the install VR Services InfoBar is not present when VR
     * is not supported on the device.
     */
    @MediumTest
    public void testInfoBarNotPresentWhenVrServicesNotSupported() throws InterruptedException {
        infoBarTestHelper(VrCoreVersionChecker.VR_NOT_SUPPORTED);
    }

    /**
     * Tests that the reported WebVR capabilities match expectations on the
     * devices the WebVR tests are run on continuously.
     */
    @MediumTest
    public void testDeviceCapabilitiesMatchExpectations() throws InterruptedException {
        String testName = "test_device_capabilities_match_expectations";
        loadUrl(getHtmlTestFile(testName), PAGE_LOAD_TIMEOUT_S);
        assertTrue("VRDisplayFound", vrDisplayFound(mWebContents));
        executeStepAndWait("stepCheckDeviceCapabilities('" + Build.DEVICE + "')", mWebContents);
        endTest(mWebContents);
    }
}
