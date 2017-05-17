// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr_shell;

import static org.chromium.chrome.browser.vr_shell.VrUtils.POLL_CHECK_INTERVAL_LONG_MS;
import static org.chromium.chrome.browser.vr_shell.VrUtils.POLL_CHECK_INTERVAL_SHORT_MS;
import static org.chromium.chrome.browser.vr_shell.VrUtils.POLL_TIMEOUT_LONG_MS;
import static org.chromium.chrome.browser.vr_shell.VrUtils.POLL_TIMEOUT_SHORT_MS;

import android.app.Activity;
import android.support.test.InstrumentationRegistry;

import org.junit.Assert;
import org.junit.rules.TestRule;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import org.chromium.base.Log;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.content.browser.test.util.ClickUtils;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.DOMUtils;
import org.chromium.content.browser.test.util.JavaScriptUtils;
import org.chromium.content_public.browser.WebContents;

import java.util.concurrent.Callable;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

/**
 * Rule containing the test framework for WebVR testing, which requires back-and-forth
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
public class VrTestRule implements TestRule {
    private static final String TAG = "VrTestRule";
    static final String TEST_DIR = "chrome/test/data/android/webvr_instrumentation";
    static final int PAGE_LOAD_TIMEOUT_S = 10;

    @Override
    public Statement apply(final Statement base, Description desc) {
        return base;
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
     * Blocks until the promise returned by nagivator.getVRDisplays() resolves,
     * then checks whether a VRDisplay was actually found.
     * @param webContents The WebContents to run the JavaScript through.
     * @return Whether a VRDisplay was found.
     */
    public boolean vrDisplayFound(WebContents webContents) {
        pollJavaScriptBoolean("vrDisplayPromiseDone", POLL_TIMEOUT_SHORT_MS, webContents);
        return !runJavaScriptOrFail("vrDisplay", POLL_TIMEOUT_SHORT_MS, webContents).equals("null");
    }

    /**
     * Use to simulate a cardboard click by sending a touch event to the device.
     * @param activity The activity to send the cardboard click to.
     */
    public void sendCardboardClick(Activity activity) {
        ClickUtils.mouseSingleClickView(InstrumentationRegistry.getInstrumentation(),
                activity.getWindow().getDecorView().getRootView());
    }

    /**
     * Sends a cardboard click then waits for the JavaScript step to finish.
     * @param activity The activity to send the cardboard click to.
     * @param webContents The WebContents for the tab the JavaScript step is in.
     */
    public void sendCardboardClickAndWait(Activity activity, WebContents webContents) {
        sendCardboardClick(activity);
        waitOnJavaScriptStep(webContents);
    }

    /**
     * Sends a click event directly to the WebGL canvas, triggering its onclick
     * that by default calls WebVR's requestPresent. Will have a similar result to
     * sendCardboardClick if not already presenting, but less prone to errors, e.g.
     * if there's dialog in the center of the screen blocking the canvas.
     * @param cvc The ContentViewCore for the tab the canvas is in.
     */
    public void enterPresentation(ContentViewCore cvc) {
        try {
            DOMUtils.clickNode(cvc, "webgl-canvas");
        } catch (InterruptedException | TimeoutException e) {
            Assert.fail("Failed to click canvas to enter presentation: " + e.toString());
        }
    }

    /**
     * Sends a click event directly to the WebGL canvas then waits for the
     * JavaScript step to finish.
     * @param cvc The ContentViewCore for the tab the canvas is in.
     * @param webContents The WebContents for the tab the JavaScript step is in.
     */
    public void enterPresentationAndWait(ContentViewCore cvc, WebContents webContents) {
        enterPresentation(cvc);
        waitOnJavaScriptStep(webContents);
    }

    /**
     * Simulate an NFC scan and wait for the JavaScript code in the given
     * WebContents to signal that it is done with the step.
     * @param webContents The WebContents for the JavaScript that will be polled.
     */
    public void simNfcScanAndWait(ChromeTabbedActivity activity, WebContents webContents) {
        VrUtils.simNfcScan(activity);
        waitOnJavaScriptStep(webContents);
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
    public String runJavaScriptOrFail(String js, int timeout, WebContents webContents) {
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
    public String checkResults(WebContents webContents) {
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
    public void endTest(WebContents webContents) {
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
    public boolean pollJavaScriptBoolean(
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
    public void waitOnJavaScriptStep(WebContents webContents) {
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
    public void executeStepAndWait(String stepFunction, WebContents webContents) {
        // Run the step and block
        JavaScriptUtils.executeJavaScript(webContents, stepFunction);
        waitOnJavaScriptStep(webContents);
    }
}
