// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr_shell;

import org.junit.Assert;

import org.chromium.base.Log;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.JavaScriptUtils;
import org.chromium.content_public.browser.WebContents;

import java.util.concurrent.Callable;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

/**
 * Class containing the test framework for WebVR testing, which requires back-and-forth
 * communication with JavaScript running in the browser.
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
public class VrTestFramework {
    public static final int PAGE_LOAD_TIMEOUT_S = 10;
    public static final int POLL_CHECK_INTERVAL_SHORT_MS = 50;
    public static final int POLL_CHECK_INTERVAL_LONG_MS = 100;
    public static final int POLL_TIMEOUT_SHORT_MS = 1000;
    public static final int POLL_TIMEOUT_LONG_MS = 10000;

    private static final String TAG = "VrTestFramework";
    static final String TEST_DIR = "chrome/test/data/android/webvr_instrumentation";

    private ChromeActivityTestRule mRule;
    private WebContents mFirstTabWebContents;
    private ContentViewCore mFirstTabCvc;

    /**
     * Must be constructed after the rule has been applied (e.g. in whatever method is
     * tagged with @Before)
     */
    public VrTestFramework(ChromeActivityTestRule rule) {
        mRule = rule;
        mFirstTabWebContents = mRule.getActivity().getActivityTab().getWebContents();
        mFirstTabCvc = mRule.getActivity().getActivityTab().getContentViewCore();
        Assert.assertFalse("Test did not start in VR", VrShellDelegate.isInVr());
    }

    public WebContents getFirstTabWebContents() {
        return mFirstTabWebContents;
    }

    public ContentViewCore getFirstTabCvc() {
        return mFirstTabCvc;
    }

    public ChromeActivityTestRule getRule() {
        return mRule;
    }

    /**
     * Gets the file:// URL to the test file
     * @param testName The name of the test whose file will be retrieved.
     * @return The file:// URL to the specified test file.
     */
    public static String getHtmlTestFile(String testName) {
        return "file://" + UrlUtils.getIsolatedTestFilePath(TEST_DIR) + "/html/" + testName
                + ".html";
    }

    /**
     * Loads the given URL with the given timeout then waits for JavaScript to
     * signal that it's ready for testing.
     * @param url The URL of the page to load.
     * @param timeoutSec The timeout of the page load in seconds.
     * @return The return value of ChromeActivityTestRule.loadUrl()
     */
    public int loadUrlAndAwaitInitialization(String url, int timeoutSec)
            throws InterruptedException {
        int result = mRule.loadUrl(url, timeoutSec);
        Assert.assertTrue("JavaScript initialization successful",
                pollJavaScriptBoolean("isInitializationComplete()", POLL_TIMEOUT_SHORT_MS,
                        mRule.getActivity().getActivityTab().getWebContents()));
        return result;
    }

    /**
     * Checks whether a VRDisplay was actually found.
     * @param webContents The WebContents to run the JavaScript through.
     * @return Whether a VRDisplay was found.
     */
    public static boolean vrDisplayFound(WebContents webContents) {
        return !runJavaScriptOrFail("vrDisplay", POLL_TIMEOUT_SHORT_MS, webContents).equals("null");
    }

    /**
     * Helper function to run the given JavaScript, return the return value,
     * and fail if a timeout/interrupt occurs so we don't have to catch or
     * declare exceptions all the time.
     * @param js The JavaScript to run.
     * @param timeout The timeout in milliseconds before a failure.
     * @param webContents The WebContents object to run the JavaScript in.
     * @return The return value of the JavaScript.
     */
    public static String runJavaScriptOrFail(String js, int timeout, WebContents webContents) {
        try {
            return JavaScriptUtils.executeJavaScriptAndWaitForResult(
                    webContents, js, timeout, TimeUnit.MILLISECONDS);
        } catch (InterruptedException | TimeoutException e) {
            Assert.fail("Fatal interruption or timeout running JavaScript: " + js);
        }
        return "Not reached";
    }

    /**
     * Ends the test harness test and checks whether there it passed.
     * @param webContents The WebContents for the tab to check results in.
     * @return "Passed" if test passed, String with failure reason otherwise.
     */
    public static String checkResults(WebContents webContents) {
        if (runJavaScriptOrFail("testPassed", POLL_TIMEOUT_SHORT_MS, webContents).equals("true")) {
            return "Passed";
        }
        return runJavaScriptOrFail("resultString", POLL_TIMEOUT_SHORT_MS, webContents);
    }

    /**
     * Helper function to end the test harness test and assert that it passed,
     * setting the failure reason as the description if it didn't.
     * @param webContents The WebContents for the tab to check test results in.
     */
    public static void endTest(WebContents webContents) {
        Assert.assertEquals("Passed", checkResults(webContents));
    }

    /**
     * Polls the provided JavaScript boolean until the timeout is reached or
     * the boolean is true.
     * @param boolName The name of the JavaScript boolean or expression to poll.
     * @param timeoutMs The polling timeout in milliseconds.
     * @param webContents The WebContents to run the JavaScript through.
     * @return True if the boolean evaluated to true, false if timed out.
     */
    public static boolean pollJavaScriptBoolean(
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
     * @param webContents The WebContents for the tab the JavaScript step is in.
     */
    public static void waitOnJavaScriptStep(WebContents webContents) {
        Assert.assertTrue("Polling JavaScript boolean javascriptDone timed out",
                pollJavaScriptBoolean("javascriptDone", POLL_TIMEOUT_LONG_MS, webContents));
        // Reset the synchronization boolean
        runJavaScriptOrFail("javascriptDone = false", POLL_TIMEOUT_SHORT_MS, webContents);
    }

    /**
     * Executes a JavaScript step function using the given WebContents.
     * @param stepFunction The JavaScript step function to call.
     * @param webContents The WebContents for the tab the JavaScript is in.
     */
    public static void executeStepAndWait(String stepFunction, WebContents webContents) {
        // Run the step and block
        JavaScriptUtils.executeJavaScript(webContents, stepFunction);
        waitOnJavaScriptStep(webContents);
    }
}
