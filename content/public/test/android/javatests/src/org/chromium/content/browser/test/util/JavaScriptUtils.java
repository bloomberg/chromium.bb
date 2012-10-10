// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.test.util;

import android.test.InstrumentationTestCase;

import junit.framework.Assert;

import org.chromium.content.browser.ContentView;

/**
 * Collection of JavaScript utilities.
 */
public class JavaScriptUtils {
    private static int mRequestId;

    /**
     * Executes the given snippet of JavaScript code within the given ContentView. Returns the
     * result of its execution in JSON format.
     */
    public static String executeJavaScriptAndWaitForResult(InstrumentationTestCase test,
            final ContentView view, TestCallbackHelperContainer viewClient, final String code)
                    throws Throwable {
        TestCallbackHelperContainer.OnEvaluateJavaScriptResultHelper
                onEvaluateJavaScriptResultHelper = viewClient.getOnEvaluateJavaScriptResultHelper();
        int currentCallCount = onEvaluateJavaScriptResultHelper.getCallCount();
        test.runTestOnUiThread(new Runnable() {
            @Override
            public void run() {
                mRequestId = view.evaluateJavaScript(code);
            }
        });
        onEvaluateJavaScriptResultHelper.waitForCallback(currentCallCount);
        Assert.assertEquals("Response ID mismatch when evaluating JavaScript.",
                mRequestId, onEvaluateJavaScriptResultHelper.getId());
        return onEvaluateJavaScriptResultHelper.getJsonResult();
    }

    /**
     * Executes the given snippet of JavaScript code but does not wait for the result.
     */
    public static void executeJavaScript(InstrumentationTestCase test, final ContentView view,
            final String code) throws Throwable {
        test.runTestOnUiThread(new Runnable() {
            @Override
            public void run() {
                view.evaluateJavaScript(code);
            }
        });
    }
}
