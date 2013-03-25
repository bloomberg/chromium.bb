// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.test.suitebuilder.annotation.SmallTest;
import android.util.Log;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwWebContentsDelegate;
import org.chromium.base.test.util.Feature;

/**
 * Tests for the ContentViewClient.addMessageToConsole() method.
 */
public class ClientAddMessageToConsoleTest extends AwTestBase {

    // Line number at which the console message is logged in the page returned by the
    // getLogMessageJavaScriptData method.
    private static final int LOG_MESSAGE_JAVASCRIPT_DATA_LINE_NUMBER = 4;

    private static final String TEST_MESSAGE_ONE = "Test message one.";
    private static final String TEST_MESSAGE_TWO = "The second test message.";

    private TestAwContentsClient mContentsClient;
    private AwContents mAwContents;

    @Override
    public void setUp() throws Exception {
        super.setUp();
        mContentsClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
                createAwTestContainerViewOnMainSync(mContentsClient);
        mAwContents = testContainerView.getAwContents();

        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                mAwContents.getContentViewCore().getContentSettings().setJavaScriptEnabled(true);
            }
        });
    }

    private static String getLogMessageJavaScriptData(String consoleLogMethod, String message) {
        // The %0A sequence is an encoded newline and is needed to test the source line number.
        return "<html>%0A" +
                  "<body>%0A" +
                    "<script>%0A" +
                      "console." + consoleLogMethod + "('" + message + "');%0A" +
                    "</script>%0A" +
                    "<div>%0A" +
                      "Logging the message [" + message + "] using console." + consoleLogMethod +
                      " method. " +
                    "</div>%0A" +
                 "</body>%0A" +
               "</html>";
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testAddMessageToConsoleCalledWithCorrectLevel() throws Throwable {
        TestAwContentsClient.AddMessageToConsoleHelper addMessageToConsoleHelper =
                mContentsClient.getAddMessageToConsoleHelper();

        int callCount = addMessageToConsoleHelper.getCallCount();
        loadDataSync(mAwContents, mContentsClient.getOnPageFinishedHelper(),
                     getLogMessageJavaScriptData("error", "msg"),
                     "text/html", false);
        addMessageToConsoleHelper.waitForCallback(callCount);
        assertEquals(AwWebContentsDelegate.LOG_LEVEL_ERROR ,
                addMessageToConsoleHelper.getLevel());

        callCount = addMessageToConsoleHelper.getCallCount();
        loadDataSync(mAwContents, mContentsClient.getOnPageFinishedHelper(),
                     getLogMessageJavaScriptData("warn", "msg"),
                     "text/html", false);
        addMessageToConsoleHelper.waitForCallback(callCount);
        assertEquals(AwWebContentsDelegate.LOG_LEVEL_WARNING ,
                addMessageToConsoleHelper.getLevel());

        callCount = addMessageToConsoleHelper.getCallCount();
        loadDataSync(mAwContents, mContentsClient.getOnPageFinishedHelper(),
                     getLogMessageJavaScriptData("log", "msg"),
                     "text/html", false);
        addMessageToConsoleHelper.waitForCallback(callCount);
        assertEquals(AwWebContentsDelegate.LOG_LEVEL_LOG ,
                addMessageToConsoleHelper.getLevel());

        // Can't test LOG_LEVEL_TIP as there's no way to generate a message at that log level
        // directly using JavaScript.
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testAddMessageToConsoleCalledWithCorrectMessage() throws Throwable {
        TestAwContentsClient.AddMessageToConsoleHelper addMessageToConsoleHelper =
                mContentsClient.getAddMessageToConsoleHelper();

        int callCount = addMessageToConsoleHelper.getCallCount();
        loadDataSync(mAwContents, mContentsClient.getOnPageFinishedHelper(),
                     getLogMessageJavaScriptData("log", TEST_MESSAGE_ONE),
                     "text/html", false);
        Log.w("test", getLogMessageJavaScriptData("log", TEST_MESSAGE_ONE));
        addMessageToConsoleHelper.waitForCallback(callCount);
        assertEquals(TEST_MESSAGE_ONE, addMessageToConsoleHelper.getMessage());

        callCount = addMessageToConsoleHelper.getCallCount();
        loadDataSync(mAwContents, mContentsClient.getOnPageFinishedHelper(),
                     getLogMessageJavaScriptData("log", TEST_MESSAGE_TWO),
                     "text/html", false);
        addMessageToConsoleHelper.waitForCallback(callCount);
        assertEquals(TEST_MESSAGE_TWO, addMessageToConsoleHelper.getMessage());
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testAddMessageToConsoleCalledWithCorrectLineAndSource() throws Throwable {
        TestAwContentsClient.AddMessageToConsoleHelper addMessageToConsoleHelper =
                mContentsClient.getAddMessageToConsoleHelper();

        int callCount = addMessageToConsoleHelper.getCallCount();
        String data = getLogMessageJavaScriptData("log", TEST_MESSAGE_ONE);
        loadDataSync(mAwContents, mContentsClient.getOnPageFinishedHelper(),
                     data, "text/html", false);
        addMessageToConsoleHelper.waitForCallback(callCount);
        assertTrue("Url [" + addMessageToConsoleHelper.getSourceId() + "] expected to end with [" +
                   data + "].", addMessageToConsoleHelper.getSourceId().endsWith(data));
        assertEquals(LOG_MESSAGE_JAVASCRIPT_DATA_LINE_NUMBER,
                     addMessageToConsoleHelper.getLineNumber());
    }
}
