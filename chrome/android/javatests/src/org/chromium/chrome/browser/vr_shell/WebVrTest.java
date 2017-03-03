// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr_shell;

import static org.chromium.chrome.test.util.ChromeRestriction.RESTRICTION_TYPE_DAYDREAM_VIEW;
import static org.chromium.chrome.test.util.ChromeRestriction.RESTRICTION_TYPE_WEBVR_SUPPORTED;

import android.support.test.filters.SmallTest;

import org.chromium.base.Log;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Restriction;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.test.ChromeTabbedActivityTestBase;
import org.chromium.content.browser.test.util.ClickUtils;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.JavaScriptUtils;
import org.chromium.content_public.browser.WebContents;

import java.util.concurrent.Callable;
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
 *   - Trigger the next Javascript test step and wait for it to finish
 *
 * The Javascript code will automatically process test results once all
 * testharness.js tests are done, just like in layout tests. Once the results
 * are processed, the Javascript code will automatically signal the Java code,
 * which can then grab the results and pass/fail the instrumentation test.
 */
@CommandLineFlags.Add("enable-webvr")
@Restriction(RESTRICTION_TYPE_WEBVR_SUPPORTED)
public class WebVrTest extends ChromeTabbedActivityTestBase {
    private static final String TAG = "WebVrTest";
    private static final String TEST_DIR = "chrome/test/data/android/webvr_instrumentation";
    private static final int POLL_TIMEOUT_SHORT = 1000;
    private static final int POLL_TIMEOUT_LONG = 10000;

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
     * @param webContents The WebContents to run the Javascript through
     * @return Whether a VRDisplay was found
     */
    private boolean vrDisplayFound(WebContents webContents) {
        pollJavascriptBoolean(POLL_TIMEOUT_SHORT, "vrDisplayPromiseDone", webContents);
        String result = "null";
        try {
            result = JavaScriptUtils.executeJavaScriptAndWaitForResult(
                    webContents, "vrDisplay", POLL_TIMEOUT_SHORT, TimeUnit.MILLISECONDS);
        } catch (InterruptedException | TimeoutException e) {
            return false;
        }
        return !result.equals("null");
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
     * Taps in the middle of the screen then waits for the Javascript step to finish.
     * @param webContents The WebContents for the tab the Javascript step is in
     */
    private void enterVrTapAndWait(WebContents webContents) {
        enterVrTap();
        waitOnJavascriptStep(webContents);
    }

    /**
     * Use to simulate a Daydream View NFC scan without blocking afterwards
     */
    private void simNfcScan() {
        VrUtils.simNfc(getActivity());
    }

    /**
     * Simulate an NFC scan and wait for the Javascript code in the given
     * WebContents to signal that it is done with the step.
     * @param webContents The WebContents for the Javascript that will be polled
     */
    private void simNfcScanAndWait(WebContents webContents) {
        simNfcScan();
        waitOnJavascriptStep(webContents);
    }

    /**
     * Ends the test harness test and checks whether there it passed
     * @param webContents The WebContents for the tab to check results in
     * @return "Passed" if test passed, String with failure reason otherwise
     */
    private String checkResults(WebContents webContents) {
        String result = "false";
        try {
            result = JavaScriptUtils.executeJavaScriptAndWaitForResult(
                    webContents, "testPassed", 50, TimeUnit.MILLISECONDS);
        } catch (InterruptedException | TimeoutException e) {
            // Do nothing - if it times out, the test will be marked as failed
        }
        if (result.equals("true")) {
            return "Passed";
        }

        try {
            result = JavaScriptUtils.executeJavaScriptAndWaitForResult(
                    webContents, "resultString", 50, TimeUnit.MILLISECONDS);
        } catch (InterruptedException | TimeoutException e) {
            result = "Unable to retrieve failure reason";
        }
        return result;
    }

    /**
     * Helper function to end the test harness test and assert that it passed,
     * setting the failure reason as the description if it didn't.
     * @param webContents The WebContents for the tab to check test results in
     */
    private void endTest(WebContents webContents) {
        String result = checkResults(webContents);
        assertEquals("Passed", result);
    }

    /**
     * Polls the provided Javascript boolean until the timeout is reached or
     * the boolean is true.
     * @param timeoutMs The polling timeout in milliseconds
     * @param boolName The name of the Javascript boolean to poll
     * @param webContents The WebContents to run the Javascript through
     * @return True if the boolean evaluated to true, false if timed out
     */
    private boolean pollJavascriptBoolean(
            int timeoutMs, final String boolName, final WebContents webContents) {
        try {
            CriteriaHelper.pollInstrumentationThread(Criteria.equals(true, new Callable<Boolean>() {
                @Override
                public Boolean call() {
                    String result = "false";
                    try {
                        result = JavaScriptUtils.executeJavaScriptAndWaitForResult(
                                webContents, boolName, 50, TimeUnit.MILLISECONDS);
                    } catch (InterruptedException | TimeoutException e) {
                        // Expected to happen regularly, do nothing
                    }
                    return Boolean.parseBoolean(result);
                }
            }), timeoutMs, 100);
        } catch (AssertionError e) {
            Log.d(TAG, "pollJavascriptBoolean() timed out");
            return false;
        }
        return true;
    }

    /**
     * Waits for a Javascript step to finish, asserting that the step finished
     * instead of timing out.
     * @param webContents The WebContents for the tab the Javascript step is in
     */
    private void waitOnJavascriptStep(WebContents webContents) {
        assertTrue("Polling Javascript boolean javascriptDone succeeded",
                pollJavascriptBoolean(POLL_TIMEOUT_LONG, "javascriptDone", webContents));
        // Reset the synchronization boolean
        JavaScriptUtils.executeJavaScript(webContents, "javascriptDone = false");
    }

    /**
     * Executes a Javascript step function using the given WebContents.
     * @param stepFunction The Javascript step function to call
     * @param webContents The WebContents for the tab the Javascript is in
     */
    private void executeStepAndWait(String stepFunction, WebContents webContents) {
        // Run the step and block
        JavaScriptUtils.executeJavaScript(webContents, stepFunction);
        waitOnJavascriptStep(webContents);
    }

    /**
     * Tests that a successful requestPresent call actually enters VR
     */
    @SmallTest
    public void testRequestPresentEntersVr() throws InterruptedException {
        String testName = "test_requestPresent_enters_vr";
        loadUrl(getHtmlTestFile(testName), 10);
        assertTrue("VRDisplay found", vrDisplayFound(mWebContents));
        enterVrTapAndWait(mWebContents);
        assertTrue("VrShellDelegate is in VR", VrShellDelegate.isInVR());
        endTest(mWebContents);
    }

    /**
     * Tests that scanning the Daydream View NFC tag on supported devices
     * fires the onvrdisplayactivate event.
     */
    @SmallTest
    @Restriction(RESTRICTION_TYPE_DAYDREAM_VIEW)
    public void testNfcFiresOnvrdisplayactivate() throws InterruptedException {
        String testName = "test_nfc_fires_onvrdisplayactivate";
        loadUrl(getHtmlTestFile(testName), 10);
        simNfcScanAndWait(mWebContents);
        endTest(mWebContents);
    }

    /**
     * Tests that non-focused tabs cannot get pose information.
     */
    @SmallTest
    public void testPoseDataUnfocusedTab() throws InterruptedException {
        String testName = "test_pose_data_unfocused_tab";
        loadUrl(getHtmlTestFile(testName), 10);
        assertTrue("VRDisplay found", vrDisplayFound(mWebContents));
        executeStepAndWait("stepCheckFrameDataWhileFocusedTab()", mWebContents);

        loadUrlInNewTab("about:blank");

        executeStepAndWait("stepCheckFrameDataWhileNonFocusedTab()", mWebContents);
        endTest(mWebContents);
    }
}
