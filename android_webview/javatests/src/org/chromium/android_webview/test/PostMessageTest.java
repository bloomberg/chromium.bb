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
import org.chromium.android_webview.test.util.CommonResources;
import org.chromium.base.test.util.Feature;
import org.chromium.net.test.util.TestWebServer;

/**
 * The tests for content postMessage API.
 */
public class PostMessageTest extends AwTestBase {

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
    private TestWebServer mWebServer;

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
        mWebServer = TestWebServer.start();
    }

    @Override
    protected void tearDown() throws Exception {
        mWebServer.shutdown();
        super.tearDown();
    }

    private static final String WEBVIEW_MESSAGE = "from_webview";

    private static final String TEST_PAGE =
            "<!DOCTYPE html><html><body>"
            + "    <script type=\"text/javascript\">"
            + "        onmessage = function (e) {"
            + "            messageObject.setMessageParams(e.data, e.origin, e.ports);"
            + "        }"
            + "   </script>"
            + "</body></html>";

    private void loadPage(String page) throws Throwable {
        final String url = mWebServer.setResponse("/test.html", page,
                CommonResources.getTextHtmlHeaders(true));
        OnPageFinishedHelper onPageFinishedHelper = mContentsClient.getOnPageFinishedHelper();
        int currentCallCount = onPageFinishedHelper.getCallCount();
        loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), url);
        onPageFinishedHelper.waitForCallback(currentCallCount);
    }

    @SmallTest
    @Feature({"AndroidWebView", "Android-PostMessage"})
    public void testPostMessageToMainFrame() throws Throwable {
        loadPage(TEST_PAGE);
        runTestOnUiThread(new Runnable() {
            @Override
            public void run() {
                mAwContents.postMessageToFrame(null, WEBVIEW_MESSAGE, SOURCE_ORIGIN,
                        mWebServer.getBaseUrl(), null);
            }
        });
        mMessageObject.waitForMessage();
        assertEquals(WEBVIEW_MESSAGE, mMessageObject.getData());
        assertEquals(SOURCE_ORIGIN, mMessageObject.getOrigin());
    }

    // TODO(sgurun) This test verifies a channel is created by posting one of the
    // ports of the channel to a MessagePort and verifying one port is received.
    // in a next CL we will update the JS to post messages back to Webview so
    // we could do a more thorough verification.
    @SmallTest
    @Feature({"AndroidWebView", "Android-PostMessage"})
    public void testCreateChannel() throws Throwable {
        loadPage(TEST_PAGE);
        runTestOnUiThread(new Runnable() {
            @Override
            public void run() {
                ValueCallback<MessageChannel> callback = new ValueCallback<MessageChannel>() {
                    @Override
                    public void onReceiveValue(MessageChannel channel) {
                        mAwContents.postMessageToFrame(null, WEBVIEW_MESSAGE, SOURCE_ORIGIN,
                                mWebServer.getBaseUrl(), new int[]{channel.port2()});
                    }
                };
                mAwContents.createMessageChannel(callback);
            }
        });
        mMessageObject.waitForMessage();
        assertEquals(WEBVIEW_MESSAGE, mMessageObject.getData());
        assertEquals(SOURCE_ORIGIN, mMessageObject.getOrigin());
        // verify that one message port is received.
        assertEquals(1, mMessageObject.getPorts().length);
    }

    private static final String WORKER_MESSAGE = "from_worker";

    // Listen for messages. Pass port 1 to worker and use port 2 to receive messages from
    // from worker.
    private static final String TEST_PAGE_FOR_PORT_TRANSFER =
            "<!DOCTYPE html><html><body>"
            + "    <script type=\"text/javascript\">"
            + "        var worker = new Worker(\"worker.js\");"
            + "        onmessage = function (e) {"
            + "            if (e.data == \"" + WEBVIEW_MESSAGE + "\") {"
            + "                worker.postMessage(\"worker_port\", [e.ports[0]]);"
            + "                var messageChannelPort = e.ports[1];"
            + "                messageChannelPort.onmessage = receiveWorkerMessage;"
            + "            }"
            + "        };"
            + "        function receiveWorkerMessage(e) {"
            + "            if (e.data == \"" + WORKER_MESSAGE + "\") {"
            + "                messageObject.setMessageParams(e.data, e.origin, e.ports);"
            + "            }"
            + "        };"
            + "   </script>"
            + "</body></html>";

    private static final String WORKER_SCRIPT =
            "onmessage = function(e) {"
            + "    if (e.data == \"worker_port\") {"
            + "        var toWindow = e.ports[0];"
            + "        toWindow.postMessage(\"" + WORKER_MESSAGE + "\");"
            + "        toWindow.start();"
            + "    }"
            + "}";

    @SmallTest
    @Feature({"AndroidWebView", "Android-PostMessage"})
    public void testTransferPortsToWorker() throws Throwable {
        mWebServer.setResponse("/worker.js", WORKER_SCRIPT,
                CommonResources.getTextJavascriptHeaders(true));
        loadPage(TEST_PAGE_FOR_PORT_TRANSFER);
        runTestOnUiThread(new Runnable() {
            @Override
            public void run() {
                ValueCallback<MessageChannel> callback = new ValueCallback<MessageChannel>() {
                    @Override
                    public void onReceiveValue(MessageChannel channel) {
                        mAwContents.postMessageToFrame(null, WEBVIEW_MESSAGE, SOURCE_ORIGIN,
                                mWebServer.getBaseUrl(),
                                new int[]{channel.port1(), channel.port2()});
                    }
                };
                mAwContents.createMessageChannel(callback);
            }
        });
        mMessageObject.waitForMessage();
        assertEquals(WORKER_MESSAGE, mMessageObject.getData());
    }
}
