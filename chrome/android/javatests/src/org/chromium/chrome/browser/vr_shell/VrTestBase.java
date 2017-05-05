// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr_shell;

import static org.chromium.chrome.browser.vr_shell.VrUtils.POLL_CHECK_INTERVAL_LONG_MS;
import static org.chromium.chrome.browser.vr_shell.VrUtils.POLL_CHECK_INTERVAL_SHORT_MS;
import static org.chromium.chrome.browser.vr_shell.VrUtils.POLL_TIMEOUT_LONG_MS;
import static org.chromium.chrome.browser.vr_shell.VrUtils.POLL_TIMEOUT_SHORT_MS;

import org.chromium.base.Log;
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
 *   - Trigger the next JavaScript test step and wait for it to finish
 *
 * The JavaScript code will automatically process test results once all
 * testharness.js tests are done, just like in layout tests. Once the results
 * are processed, the JavaScript code will automatically signal the Java code,
 * which can then grab the results and pass/fail the instrumentation test.
 */
public class VrTestBase extends ChromeTabbedActivityTestBase {
    private static final String TAG = "VrTestBase";
    protected static final String TEST_DIR = "chrome/test/data/android/webvr_instrumentation";
    protected static final int PAGE_LOAD_TIMEOUT_S = 10;

    protected WebContents mWebContents;

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
    protected static String getHtmlTestFile(String testName) {
        return "file://" + UrlUtils.getIsolatedTestFilePath(TEST_DIR) + "/html/" + testName
                + ".html";
    }

    /**
     * Blocks until the promise returned by nagivator.getVRDisplays() resolves,
     * then checks whether a VRDisplay was actually found.
     * @param webContents The WebContents to run the JavaScript through
     * @return Whether a VRDisplay was found
     */
    protected boolean vrDisplayFound(WebContents webContents) {
        pollJavaScriptBoolean("vrDisplayPromiseDone", POLL_TIMEOUT_SHORT_MS, webContents);
        return !runJavaScriptOrFail("vrDisplay", POLL_TIMEOUT_SHORT_MS, webContents).equals("null");
    }

    /**
     * Use to tap in the middle of the screen, triggering the canvas' onclick
     * to fulfil WebVR's gesture requirement for presenting.
     */
    protected void enterVrTap() {
        ClickUtils.mouseSingleClickView(
                getInstrumentation(), getActivity().getWindow().getDecorView().getRootView());
    }

    /**
     * Taps in the middle of the screen then waits for the JavaScript step to finish.
     * @param webContents The WebContents for the tab the JavaScript step is in
     */
    protected void enterVrTapAndWait(WebContents webContents) {
        enterVrTap();
        waitOnJavaScriptStep(webContents);
    }

    /**
     * Use to simulate a Daydream View NFC scan without blocking afterwards
     */
    protected void simNfcScan() {
        VrUtils.simNfc(getActivity());
    }

    /**
     * Simulate an NFC scan and wait for the JavaScript code in the given
     * WebContents to signal that it is done with the step.
     * @param webContents The WebContents for the JavaScript that will be polled
     */
    protected void simNfcScanAndWait(WebContents webContents) {
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
    protected String runJavaScriptOrFail(String js, int timeout, WebContents webContents) {
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
    protected String checkResults(WebContents webContents) {
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
    protected void endTest(WebContents webContents) {
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
    protected boolean pollJavaScriptBoolean(
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
    protected void waitOnJavaScriptStep(WebContents webContents) {
        assertTrue("Polling JavaScript boolean javascriptDone timed out",
                pollJavaScriptBoolean("javascriptDone", POLL_TIMEOUT_LONG_MS, webContents));
        // Reset the synchronization boolean
        runJavaScriptOrFail("javascriptDone = false", POLL_TIMEOUT_SHORT_MS, webContents);
    }

    /**
     * Executes a JavaScript step function using the given WebContents.
     * @param stepFunction The JavaScript step function to call
     * @param webContents The WebContents for the tab the JavaScript is in
     */
    protected void executeStepAndWait(String stepFunction, WebContents webContents) {
        // Run the step and block
        JavaScriptUtils.executeJavaScript(webContents, stepFunction);
        waitOnJavaScriptStep(webContents);
    }
}
