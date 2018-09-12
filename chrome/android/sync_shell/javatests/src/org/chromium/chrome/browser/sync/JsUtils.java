// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync;

import static org.chromium.base.test.util.ScalableTimeout.scaleTimeout;

import org.junit.Assert;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.content_public.browser.JavaScriptCallback;
import org.chromium.content_public.browser.WebContents;

import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

/**
 * Collection of utilities for Sync tests.
 */
public class JsUtils {
    private static final long EVALUATION_TIMEOUT_SECONDS = scaleTimeout(5);

    /**
     * Executes the given snippet of JavaScript code within the given ContentView.
     * Returns the result of its execution in JSON format.
     */
    public static String executeJavaScriptAndWaitForResult(WebContents webContents, String code)
            throws InterruptedException, TimeoutException {
        return executeJavaScriptAndWaitForResult(
                webContents, code, EVALUATION_TIMEOUT_SECONDS, TimeUnit.SECONDS);
    }

    /**
     * Executes the given snippet of JavaScript code within the given WebContents.
     * Does not depend on ContentView and TestCallbackHelperContainer.
     * Returns the result of its execution in JSON format.
     */
    public static String executeJavaScriptAndWaitForResult(final WebContents webContents,
            final String code, final long timeout, final TimeUnit timeoutUnits)
            throws InterruptedException, TimeoutException {
        final EvaluateJavaScriptResultHelper helper = new EvaluateJavaScriptResultHelper();
        // Calling this from the UI thread causes it to time-out: the UI thread being blocked won't
        // have a chance to process the JavaScript eval response).
        Assert.assertFalse("Executing JavaScript should be done from the test thread, "
                        + " not the UI thread",
                ThreadUtils.runningOnUiThread());
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                helper.evaluateJavaScriptForTests(webContents, code);
            }
        });
        helper.waitUntilHasValue(timeout, timeoutUnits);
        Assert.assertTrue("Failed to retrieve JavaScript evaluation results.", helper.hasValue());
        return helper.getJsonResultAndClear();
    }

    private static class EvaluateJavaScriptResultHelper extends CallbackHelper {
        private String mJsonResult;

        /**
         * Starts evaluation of a given JavaScript code on a given webContents.
         * @param webContents A WebContents instance to be used.
         * @param code A JavaScript code to be evaluated.
         */
        public void evaluateJavaScriptForTests(WebContents webContents, String code) {
            JavaScriptCallback callback = new JavaScriptCallback() {
                @Override
                public void handleJavaScriptResult(String jsonResult) {
                    notifyCalled(jsonResult);
                }
            };
            mJsonResult = null;
            nativeEvaluateJavaScript(webContents, code, callback);
        }

        /**
         * Returns true if the evaluation started by evaluateJavaScriptForTests() has completed.
         */
        public boolean hasValue() {
            return mJsonResult != null;
        }

        /**
         * Returns the JSON result of a previously completed JavaScript evaluation and
         * resets the helper to accept new evaluations.
         * @return String JSON result of a previously completed JavaScript evaluation.
         */
        public String getJsonResultAndClear() {
            assert hasValue();
            String result = mJsonResult;
            mJsonResult = null;
            return result;
        }

        /**
         * Waits till the JavaScript evaluation finishes and returns true if a value was returned,
         * false if it timed-out.
         */
        public boolean waitUntilHasValue(long timeout, TimeUnit unit)
                throws InterruptedException, TimeoutException {
            int count = getCallCount();
            // Reads and writes are atomic for reference variables in java, this is thread safe
            if (hasValue()) return true;
            waitForCallback(count, 1, timeout, unit);
            return hasValue();
        }

        public boolean waitUntilHasValue() throws InterruptedException, TimeoutException {
            return waitUntilHasValue(CallbackHelper.WAIT_TIMEOUT_SECONDS, TimeUnit.SECONDS);
        }

        public void notifyCalled(String jsonResult) {
            assert !hasValue();
            mJsonResult = jsonResult;
            notifyCalled();
        }
    }

    private static native void nativeEvaluateJavaScript(
            WebContents webContents, String script, JavaScriptCallback callback);
}
