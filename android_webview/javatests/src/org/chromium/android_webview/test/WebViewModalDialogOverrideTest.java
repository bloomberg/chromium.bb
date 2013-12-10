// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Test suite for displaying and functioning of modal dialogs.
 */

package org.chromium.android_webview.test;

import android.test.suitebuilder.annotation.MediumTest;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.JsPromptResultReceiver;
import org.chromium.android_webview.JsResultReceiver;
import org.chromium.base.test.util.Feature;
import org.chromium.content.browser.test.util.CallbackHelper;

import java.util.concurrent.atomic.AtomicBoolean;

public class WebViewModalDialogOverrideTest extends AwTestBase {
    private static final String EMPTY_PAGE =
            "<!doctype html>" +
            "<title>Modal Dialog Test</title><p>Testcase.</p>";
    private static final String BEFORE_UNLOAD_URL =
            "<!doctype html>" +
            "<head><script>window.onbeforeunload=function() {" +
            "return 'Are you sure?';" +
            "};</script></head></body>";

    /*
     * Verify that when the AwContentsClient calls handleJsAlert.
     */
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testOverrideAlertHandling() throws Throwable {
        final String ALERT_TEXT = "Hello World!";

        final AtomicBoolean callbackCalled = new AtomicBoolean(false);
        // Returning true from the callback should not show a dialog.
        TestAwContentsClient client = new TestAwContentsClient() {
            @Override
            public void handleJsAlert(String url, String message, JsResultReceiver res) {
                callbackCalled.set(true);
                res.confirm();
                assertEquals(ALERT_TEXT, message);
            }
        };
        AwTestContainerView view = createAwTestContainerViewOnMainSync(client);
        final AwContents awContents = view.getAwContents();

        enableJavaScriptOnUiThread(awContents);
        loadDataSync(awContents, client.getOnPageFinishedHelper(),
                EMPTY_PAGE, "text/html", false);
        executeJavaScriptAndWaitForResult(awContents, client,
                "alert('" + ALERT_TEXT + "')");
        assertTrue(callbackCalled.get());
    }

    /*
     * Verify that when the AwContentsClient calls handleJsPrompt.
     */
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testOverridePromptHandling() throws Throwable {
        final String PROMPT_TEXT = "How do you like your eggs in the morning?";
        final String PROMPT_DEFAULT = "Scrambled";
        final String PROMPT_RESULT = "I like mine with a kiss";

        final AtomicBoolean called = new AtomicBoolean(false);
        // Returning true from the callback should not show a dialog.
        final TestAwContentsClient client = new TestAwContentsClient() {
            @Override
            public void handleJsPrompt(String url, String message, String defaultValue,
                                      JsPromptResultReceiver res) {
                assertEquals(PROMPT_TEXT, message);
                assertEquals(PROMPT_DEFAULT, defaultValue);
                res.confirm(PROMPT_RESULT);
                called.set(true);
            }
        };
        AwTestContainerView view = createAwTestContainerViewOnMainSync(client);
        final AwContents awContents = view.getAwContents();

        enableJavaScriptOnUiThread(awContents);
        loadDataSync(awContents, client.getOnPageFinishedHelper(),
                EMPTY_PAGE, "text/html", false);
        String result = executeJavaScriptAndWaitForResult(awContents, client,
                "prompt('" + PROMPT_TEXT + "','" + PROMPT_DEFAULT + "')");
        assertTrue(called.get());
        assertEquals("\"" + PROMPT_RESULT + "\"", result);
    }

    /*
     * Verify that when the AwContentsClient calls handleJsConfirm and the client confirms.
     */
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testOverrideConfirmHandlingConfirmed() throws Throwable {
        final String CONFIRM_TEXT = "Would you like a cookie?";

        final AtomicBoolean called = new AtomicBoolean(false);
        // Returning true from the callback should not show a dialog.
        TestAwContentsClient client = new TestAwContentsClient() {
            @Override
            public void handleJsConfirm(String url, String message, JsResultReceiver res) {
                assertEquals(CONFIRM_TEXT, message);
                res.confirm();
                called.set(true);
            }
        };
        AwTestContainerView view = createAwTestContainerViewOnMainSync(client);
        final AwContents awContents = view.getAwContents();
        enableJavaScriptOnUiThread(awContents);

        loadDataSync(awContents, client.getOnPageFinishedHelper(),
                EMPTY_PAGE, "text/html", false);
        String result = executeJavaScriptAndWaitForResult(awContents, client,
                "confirm('" + CONFIRM_TEXT + "')");
        assertTrue(called.get());
        assertEquals("true", result);
    }

    /*
     * Verify that when the AwContentsClient calls handleJsConfirm and the client cancels.
     */
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testOverrideConfirmHandlingCancelled() throws Throwable {
        final String CONFIRM_TEXT = "Would you like a cookie?";

        final AtomicBoolean called = new AtomicBoolean(false);
        // Returning true from the callback should not show a dialog.
        TestAwContentsClient client = new TestAwContentsClient() {
            @Override
            public void handleJsConfirm(String url, String message, JsResultReceiver res) {
                assertEquals(CONFIRM_TEXT, message);
                res.cancel();
                called.set(true);
            }
        };
        AwTestContainerView view = createAwTestContainerViewOnMainSync(client);
        final AwContents awContents = view.getAwContents();
        enableJavaScriptOnUiThread(awContents);

        loadDataSync(awContents, client.getOnPageFinishedHelper(),
                EMPTY_PAGE, "text/html", false);
        String result = executeJavaScriptAndWaitForResult(awContents, client,
                "confirm('" + CONFIRM_TEXT + "')");
        assertTrue(called.get());
        assertEquals("false", result);
    }

    /*
     * Verify that when the AwContentsClient calls handleJsBeforeUnload
     */
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testOverrideBeforeUnloadHandling() throws Throwable {
        final CallbackHelper jsBeforeUnloadHelper = new CallbackHelper();
        TestAwContentsClient client = new TestAwContentsClient() {
            @Override
            public void handleJsBeforeUnload(String url, String message, JsResultReceiver res) {
                res.cancel();
                jsBeforeUnloadHelper.notifyCalled();
            }
        };
        AwTestContainerView view = createAwTestContainerViewOnMainSync(client);
        final AwContents awContents = view.getAwContents();
        enableJavaScriptOnUiThread(awContents);

        loadDataSync(awContents, client.getOnPageFinishedHelper(), BEFORE_UNLOAD_URL,
                "text/html", false);
        enableJavaScriptOnUiThread(awContents);

        // Don't wait synchronously because we don't leave the page.
        int currentCallCount = jsBeforeUnloadHelper.getCallCount();
        loadDataAsync(awContents, EMPTY_PAGE, "text/html", false);
        jsBeforeUnloadHelper.waitForCallback(currentCallCount);
    }
}
