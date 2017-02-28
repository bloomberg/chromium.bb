// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import static org.chromium.base.test.util.ScalableTimeout.scaleTimeout;

import android.os.Message;
import android.support.test.filters.SmallTest;
import android.util.Base64;

import org.chromium.android_webview.AwContents;
import org.chromium.base.test.util.Feature;
import org.chromium.content.browser.test.util.TestCallbackHelperContainer;
import org.chromium.net.test.util.TestWebServer;

import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

/**
 * Tests if resubmission of post data is handled properly.
 */
public class AwContentsClientOnFormResubmissionTest extends AwTestBase {

    private static class TestAwContentsClient
            extends org.chromium.android_webview.test.TestAwContentsClient {

        // Number of times onFormResubmit is called.
        private int mResubmissions;
        // Whether to resubmit Post data on reload.
        private boolean mResubmit;

        public int getResubmissions() {
            return mResubmissions;
        }
        public void setResubmit(boolean resubmit) {
            mResubmit = resubmit;
        }
        @Override
        public void onFormResubmission(Message dontResend, Message resend) {
            mResubmissions++;
            if (mResubmit) {
                resend.sendToTarget();
            } else {
                dontResend.sendToTarget();
            }
        }
    }

    // Server responses for load and reload of posts.
    private static final String LOAD_RESPONSE =
            "<html><head><title>Load</title></head><body>HELLO</body></html>";
    private static final String RELOAD_RESPONSE =
            "<html><head><title>Reload</title></head><body>HELLO</body></html>";

    // Server timeout in seconds. Used to detect dontResend case.
    private static final long TIMEOUT = scaleTimeout(3);

    // The web server.
    private TestWebServer mServer;
    // The mock client.
    private TestAwContentsClient mContentsClient;
    private AwContents mAwContents;

    @Override
    public void setUp() throws Exception {
        super.setUp();
        mServer = TestWebServer.start();
        mContentsClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
                createAwTestContainerViewOnMainSync(mContentsClient);
        mAwContents = testContainerView.getAwContents();
    }

    @Override
    public void tearDown() throws Exception {
        mServer.shutdown();
        super.tearDown();
    }

    @SmallTest
    @Feature({"AndroidWebView", "Navigation"})
    public void testResend() throws Throwable {
        mContentsClient.setResubmit(true);
        doReload();
        assertEquals(1, mContentsClient.getResubmissions());
        assertEquals("Reload", getTitleOnUiThread(mAwContents));
    }

    @SmallTest
    @Feature({"AndroidWebView", "Navigation"})
    public void testDontResend() throws Throwable {
        mContentsClient.setResubmit(false);
        doReload();
        assertEquals(1, mContentsClient.getResubmissions());
        assertEquals("Load", getTitleOnUiThread(mAwContents));
    }

    protected void doReload() throws Throwable {
        String url = mServer.setResponse("/form", LOAD_RESPONSE, null);
        String postData = "content=blabla";
        byte[] data = Base64.encode(postData.getBytes("UTF-8"), Base64.DEFAULT);
        postUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), url, data);
        assertEquals(0, mContentsClient.getResubmissions());
        assertEquals("Load", getTitleOnUiThread(mAwContents));
        // Verify reload works as expected.
        mServer.setResponse("/form", RELOAD_RESPONSE, null);
        TestCallbackHelperContainer.OnPageFinishedHelper onPageFinishedHelper =
                mContentsClient.getOnPageFinishedHelper();
        int callCount = onPageFinishedHelper.getCallCount();
        // Run reload on UI thread.
        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                mAwContents.getNavigationController().reload(true);
            }
        });
        try {
            // Wait for page finished callback, or a timeout. A timeout is necessary
            // to detect a dontResend response.
            onPageFinishedHelper.waitForCallback(callCount, 1, TIMEOUT, TimeUnit.SECONDS);
        } catch (TimeoutException e) {
            // Exception expected from testDontResend case.
        }
    }
}
