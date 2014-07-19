// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.test.util.Feature;

/**
 * The tests for content postMessage API.
 */
public class PostMessageTest extends ContentViewTestBase {

    private static final String URL1 =
        "<!DOCTYPE html><html><body>" +
            "<script type=\"text/javascript\">" +
                "onmessage = function (e) {" +
                    "messageObject.setMessageParams(e.data, e.origin);" +
                "}" +
            "</script>" +
        "</body></html>";

    private static final String MESSAGE = "Foo";
    private static final String SOURCE_ORIGIN = "android_webview";

    // Inject to the page to verify received messages.
    private static class MessageObject {
        // Timeout to failure, in milliseconds
        private static final int TIMEOUT = 5000;

        private boolean mReady;
        private String mData;
        private String mOrigin;
        private Object mLock = new Object();

        public void setMessageParams(String data, String origin) {
            synchronized (mLock) {
                mData = data;
                mOrigin = origin;
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
    }

    private MessageObject mMessageObject;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mMessageObject = new MessageObject();
        setUpContentView(mMessageObject, "messageObject");
    }

    @SmallTest
    @Feature({"AndroidWebView", "Android-PostMessage"})
    public void testPostMessageToMainFrame() throws Throwable {
        ContentViewCore contentViewCore = getContentViewCore();
        loadDataSync(contentViewCore, URL1, "text/html", false);
        contentViewCore.postMessageToFrame(null, MESSAGE, SOURCE_ORIGIN, "*");
        mMessageObject.waitForMessage();
        assertEquals(MESSAGE, mMessageObject.getData());
        assertEquals(SOURCE_ORIGIN, mMessageObject.getOrigin());
    }
}
