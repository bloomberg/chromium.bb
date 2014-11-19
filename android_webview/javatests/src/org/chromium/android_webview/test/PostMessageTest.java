// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.test.suitebuilder.annotation.SmallTest;
import android.webkit.ValueCallback;

import static org.chromium.base.test.util.ScalableTimeout.scaleTimeout;
import static org.chromium.content.browser.test.util.TestCallbackHelperContainer.OnPageFinishedHelper;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.MessageChannel;
import org.chromium.base.test.util.Feature;

/**
 * The tests for content postMessage API.
 */
public class PostMessageTest extends AwTestBase {

    private static final String TEST_PAGE =
            "<!DOCTYPE html><html><body>"
            + "    <script type=\"text/javascript\">"
            + "        onmessage = function (e) {"
            + "            messageObject.setMessageParams(e.data, e.origin, e.ports);"
            + "       }"
            + "   </script>"
            + "</body></html>";

    private static final String MESSAGE = "Foo";
    private static final String SOURCE_ORIGIN = "android_webview";

    // Inject to the page to verify received messages.
    private static class MessageObject {
        // Timeout to failure, in milliseconds
        private static final long TIMEOUT = scaleTimeout(5000);

        private boolean mReady;
        private String mData;
        private String mOrigin;
        private int[] mPorts;
        private Object mLock = new Object();

        public void setMessageParams(String data, String origin, int[] ports) {
            synchronized (mLock) {
                mData = data;
                mOrigin = origin;
                mPorts = ports;
                mReady = true;
                mLock.notify();
            }
        }

        public void waitForMessage() throws InterruptedException {
            synchronized (mLock) {
                if (!mReady) mLock.wait(TIMEOUT);
            }
        }

        public String getData() {
            return mData;
        }

        public String getOrigin() {
            return mOrigin;
        }

        public int[] getPorts() {
            return mPorts;
        }
    }

    private MessageObject mMessageObject;
    private TestAwContentsClient mContentsClient;
    private AwTestContainerView mTestContainerView;
    private AwContents mAwContents;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mMessageObject = new MessageObject();
        mContentsClient = new TestAwContentsClient();
        mTestContainerView = createAwTestContainerViewOnMainSync(mContentsClient);
        mAwContents = mTestContainerView.getAwContents();
        enableJavaScriptOnUiThread(mAwContents);

        try {
            runTestOnUiThread(new Runnable() {
                @Override
                public void run() {
                    mAwContents.addPossiblyUnsafeJavascriptInterface(mMessageObject,
                            "messageObject", null);
                }
            });
        } catch (Throwable t) {
            throw new RuntimeException(t);
        }
        OnPageFinishedHelper onPageFinishedHelper = mContentsClient.getOnPageFinishedHelper();
        int currentCallCount = onPageFinishedHelper.getCallCount();
        // Load test page
        loadDataSync(mAwContents, mContentsClient.getOnPageFinishedHelper(),
                TEST_PAGE, "text/html", false);
        onPageFinishedHelper.waitForCallback(currentCallCount);
    }

    @SmallTest
    @Feature({"AndroidWebView", "Android-PostMessage"})
    public void testPostMessageToMainFrame() throws Throwable {
        runTestOnUiThread(new Runnable() {
            @Override
            public void run() {
                mAwContents.postMessageToFrame(null, MESSAGE, SOURCE_ORIGIN, "*", null);
            }
        });
        mMessageObject.waitForMessage();
        assertEquals(MESSAGE, mMessageObject.getData());
        assertEquals(SOURCE_ORIGIN, mMessageObject.getOrigin());
    }

    // TODO(sgurun) This test verifies a channel is created by posting one of the
    // ports of the channel to a MessagePort and verifying one port is received.
    // in a next CL we will update the JS to post messages back to Webview so
    // we could do a more thorough verification.
    @SmallTest
    @Feature({"AndroidWebView", "Android-PostMessage"})
    public void testCreateChannel() throws Throwable {
        runTestOnUiThread(new Runnable() {
            @Override
            public void run() {
                ValueCallback<MessageChannel> callback = new ValueCallback<MessageChannel>() {
                    @Override
                    public void onReceiveValue(MessageChannel channel) {
                        mAwContents.postMessageToFrame(null, MESSAGE, SOURCE_ORIGIN, "*",
                                new int[]{channel.port2()});
                    }
                };
                mAwContents.createMessageChannel(callback);
            }
        });
        mMessageObject.waitForMessage();
        assertEquals(MESSAGE, mMessageObject.getData());
        assertEquals(SOURCE_ORIGIN, mMessageObject.getOrigin());
        // verify that one message port is received.
        assertEquals(1, mMessageObject.getPorts().length);
    }
}
