// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.os.Build;
import android.test.suitebuilder.annotation.SmallTest;
import android.webkit.ValueCallback;

import static org.chromium.base.test.util.ScalableTimeout.scaleTimeout;
import static org.chromium.content.browser.test.util.TestCallbackHelperContainer.OnPageFinishedHelper;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.MessageChannel;
import org.chromium.android_webview.MessagePort;
import org.chromium.android_webview.test.util.CommonResources;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.net.test.util.TestWebServer;

import java.util.concurrent.CountDownLatch;

/**
 * The tests for content postMessage API.
 */
@MinAndroidSdkLevel(Build.VERSION_CODES.KITKAT)
public class PostMessageTest extends AwTestBase {

    private static final String SOURCE_ORIGIN = "android_webview";
    // Timeout to failure, in milliseconds
    private static final long TIMEOUT = scaleTimeout(5000);

    // Inject to the page to verify received messages.
    private static class MessageObject {
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
    private static final String JS_MESSAGE = "from_js";

    private static final String TEST_PAGE =
            "<!DOCTYPE html><html><body>"
            + "    <script type=\"text/javascript\">"
            + "        onmessage = function (e) {"
            + "            messageObject.setMessageParams(e.data, e.origin, e.ports);"
            + "            if (e.ports != null && e.ports.length > 0) {"
            + "               e.ports[0].postMessage(\"" + JS_MESSAGE + "\");"
            + "            }"
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

    @SmallTest
    @Feature({"AndroidWebView", "Android-PostMessage"})
    public void testTransferringSamePortTwiceNotAllowed() throws Throwable {
        loadPage(TEST_PAGE);
        final CountDownLatch latch = new CountDownLatch(1);
        runTestOnUiThread(new Runnable() {
            @Override
            public void run() {
                ValueCallback<MessageChannel> callback = new ValueCallback<MessageChannel>() {
                    @Override
                    public void onReceiveValue(MessageChannel channel) {
                        mAwContents.postMessageToFrame(null, "1", SOURCE_ORIGIN,
                                mWebServer.getBaseUrl(),
                                new MessagePort[]{channel.port2()});
                        // retransfer the port. This should fail with an exception
                        try {
                            mAwContents.postMessageToFrame(null, "2", SOURCE_ORIGIN,
                                    mWebServer.getBaseUrl(),
                                    new MessagePort[]{channel.port2()});
                        } catch (IllegalStateException ex) {
                            latch.countDown();
                            return;
                        }
                        fail();
                    }
                };
                mAwContents.createMessageChannel(callback);
            }
        });
        boolean ignore = latch.await(TIMEOUT, java.util.concurrent.TimeUnit.MILLISECONDS);
    }

    private static class ChannelContainer {
        private boolean mReady;
        private MessageChannel mChannel;
        private Object mLock = new Object();
        private String mMessage;

        public void set(MessageChannel channel) {
            mChannel = channel;
        }
        public MessageChannel get() {
            return mChannel;
        }

        public void setMessage(String message) {
            synchronized (mLock) {
                mMessage = message;
                mReady = true;
                mLock.notify();
            }
        }

        public String getMessage() {
            return mMessage;
        }

        public void waitForMessage() throws InterruptedException {
            synchronized (mLock) {
                if (!mReady) mLock.wait(TIMEOUT);
            }
        }
    }

    // Verify that messages from JS can be waited on a UI thread.
    @SmallTest
    // TODO this test turned out to be flaky. When it fails, it always fails in IPC.
    // When a postmessage is received, an IPC message is sent from browser to renderer
    // to convert the postmessage from WebSerializedScriptValue to a string. The IPC is sent
    // and seems to be received by IPC in renderer, but then nothing else seems to happen.
    // The issue seems like blocking the UI thread causes a racing SYNC ipc from renderer
    // to browser to block waiting for UI thread, and this would in turn block renderer
    // doing the conversion.
    @DisabledTest
    @Feature({"AndroidWebView", "Android-PostMessage"})
    public void testReceiveMessageInBackgroundThread() throws Throwable {
        loadPage(TEST_PAGE);
        final ChannelContainer channelContainer = new ChannelContainer();
        runTestOnUiThread(new Runnable() {
            @Override
            public void run() {
                ValueCallback<MessageChannel> callback = new ValueCallback<MessageChannel>() {
                    @Override
                    public void onReceiveValue(MessageChannel channel) {
                        // verify communication from JS to Java.
                        channelContainer.set(channel);
                        channel.port1().setMessageHandler(new MessagePort.MessageHandler() {
                            @Override
                            public void onMessage(String message) {
                                channelContainer.setMessage(message);
                            }
                        });
                        mAwContents.postMessageToFrame(null, WEBVIEW_MESSAGE, SOURCE_ORIGIN,
                                mWebServer.getBaseUrl(), new MessagePort[]{channel.port2()});
                    }
                };
                mAwContents.createMessageChannel(callback);
            }
        });
        mMessageObject.waitForMessage();
        assertEquals(WEBVIEW_MESSAGE, mMessageObject.getData());
        assertEquals(SOURCE_ORIGIN, mMessageObject.getOrigin());
        // verify that one message port is received at the js side
        assertEquals(1, mMessageObject.getPorts().length);
        // wait until we receive a message from JS
        runTestOnUiThread(new Runnable() {
            @Override
            public void run() {
                try {
                    channelContainer.waitForMessage();
                } catch (InterruptedException e) {
                    // ignore.
                }
            }
        });
        assertEquals(JS_MESSAGE, channelContainer.getMessage());
    }

    private static final String ECHO_PAGE =
            "<!DOCTYPE html><html><body>"
            + "    <script type=\"text/javascript\">"
            + "        onmessage = function (e) {"
            + "            var myPort = e.ports[0];"
            + "            myPort.onmessage = function(e) {"
            + "                myPort.postMessage(e.data + \"" + JS_MESSAGE + "\"); }"
            + "        }"
            + "   </script>"
            + "</body></html>";

    // Verify that a channel can be created and basic full duplex communication
    // can happen on it. Do this by sending a message to JS and let it echo'ing
    // the message with some text prepended to it.
    @SmallTest
    @Feature({"AndroidWebView", "Android-PostMessage"})
    public void testMessageChannel() throws Throwable {
        final String hello = "HELLO";
        final ChannelContainer channelContainer = new ChannelContainer();
        loadPage(ECHO_PAGE);
        runTestOnUiThread(new Runnable() {
            @Override
            public void run() {
                ValueCallback<MessageChannel> callback = new ValueCallback<MessageChannel>() {
                    @Override
                    public void onReceiveValue(MessageChannel channel) {
                        channel.port1().setMessageHandler(new MessagePort.MessageHandler() {
                            @Override
                            public void onMessage(String message) {
                                channelContainer.setMessage(message);
                            }
                        });
                        mAwContents.postMessageToFrame(null, WEBVIEW_MESSAGE, SOURCE_ORIGIN,
                                mWebServer.getBaseUrl(), new MessagePort[]{channel.port2()});
                        channel.port1().postMessage(hello, null);
                    }
                };
                mAwContents.createMessageChannel(callback);
            }
        });
        // wait for the asynchronous response from JS
        channelContainer.waitForMessage();
        assertEquals(hello + JS_MESSAGE, channelContainer.getMessage());
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
                                new MessagePort[]{channel.port1(), channel.port2()});
                    }
                };
                mAwContents.createMessageChannel(callback);
            }
        });
        mMessageObject.waitForMessage();
        assertEquals(WORKER_MESSAGE, mMessageObject.getData());
    }
}
