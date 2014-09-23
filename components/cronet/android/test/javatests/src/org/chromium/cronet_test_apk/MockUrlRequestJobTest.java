// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.cronet_test_apk;

import android.os.ConditionVariable;
import android.test.suitebuilder.annotation.SmallTest;
import android.util.Log;

import org.chromium.base.JNINamespace;
import org.chromium.base.test.util.Feature;
import org.chromium.net.HttpUrlRequest;
import org.chromium.net.HttpUrlRequestListener;

import java.util.HashMap;

// Tests that use mock URLRequestJobs to simulate URL requests.
@JNINamespace("cronet")
public class MockUrlRequestJobTest extends CronetTestBase {
    private static final String TAG = "MockURLRequestJobTest";
    private static final String MOCK_CRONET_TEST_SUCCESS_URL =
            "http://mock.http/success.txt";
    private static final String MOCK_CRONET_TEST_REDIRECT_URL =
            "http://mock.http/redirect.html";
    private static final String MOCK_CRONET_TEST_NOTFOUND_URL =
            "http://mock.http/notfound.html";
    private static final String MOCK_CRONET_TEST_FAILED_URL =
            "http://mock.failed.request/-2";

    class MockHttpUrlRequestListener implements HttpUrlRequestListener {
        ConditionVariable mComplete = new ConditionVariable();
        public int mHttpStatusCode = 0;
        public String mUrl;
        public byte[] mResponseAsBytes;

        public MockHttpUrlRequestListener() {
        }

        @Override
        public void onResponseStarted(HttpUrlRequest request) {
            Log.i(TAG, "****** Response Started, content length is " +
                    request.getContentLength());
            Log.i(TAG, "*** Headers Are *** " + request.getAllHeaders());
            mHttpStatusCode = request.getHttpStatusCode();
        }

        public void blockForComplete() {
            mComplete.block();
        }

        @Override
        public void onRequestComplete(HttpUrlRequest request) {
            mUrl = request.getUrl();
            mResponseAsBytes = request.getResponseAsBytes();
            mComplete.open();
            Log.i(TAG, "****** Request Complete, status code is " +
                    request.getHttpStatusCode());
        }
    }

    // Helper function to create a HttpUrlRequest with the specified url.
    private MockHttpUrlRequestListener createUrlRequestAndWaitForComplete(
            String url) {
        CronetTestActivity activity = launchCronetTestApp();
        assertNotNull(activity);
        // AddUrlInterceptors() after native application context is initialized.
        nativeAddUrlInterceptors();

        HashMap<String, String> headers = new HashMap<String, String>();
        MockHttpUrlRequestListener listener = new MockHttpUrlRequestListener();

        HttpUrlRequest request = activity.mChromiumRequestFactory.createRequest(
                url,
                HttpUrlRequest.REQUEST_PRIORITY_MEDIUM,
                headers,
                listener);
        request.start();
        listener.blockForComplete();
        return listener;
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testSuccessURLRequest() throws Exception {
        MockHttpUrlRequestListener listener =
            createUrlRequestAndWaitForComplete(MOCK_CRONET_TEST_SUCCESS_URL);
        assertEquals(MOCK_CRONET_TEST_SUCCESS_URL, listener.mUrl);
        assertEquals(200, listener.mHttpStatusCode);
        assertEquals("this is a text file\n",
                     new String(listener.mResponseAsBytes));
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testRedirectURLRequest() throws Exception {
        MockHttpUrlRequestListener listener =
            createUrlRequestAndWaitForComplete(MOCK_CRONET_TEST_REDIRECT_URL);

        // Currently Cronet does not expose the url after redirect.
        assertEquals(MOCK_CRONET_TEST_REDIRECT_URL, listener.mUrl);
        assertEquals(200, listener.mHttpStatusCode);
        // Expect that the request is redirected to success.txt.
        assertEquals("this is a text file\n",
                     new String(listener.mResponseAsBytes));
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testNotFoundURLRequest() throws Exception {
        MockHttpUrlRequestListener listener =
            createUrlRequestAndWaitForComplete(MOCK_CRONET_TEST_NOTFOUND_URL);
        assertEquals(MOCK_CRONET_TEST_NOTFOUND_URL, listener.mUrl);
        assertEquals(404, listener.mHttpStatusCode);
        assertEquals(
            "<!DOCTYPE html>\n<html>\n<head>\n<title>Not found</title>\n" +
                "<p>Test page loaded.</p>\n</head>\n</html>\n",
            new String(listener.mResponseAsBytes));
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testFailedURLRequest() throws Exception {
        MockHttpUrlRequestListener listener =
            createUrlRequestAndWaitForComplete(MOCK_CRONET_TEST_FAILED_URL);
        assertEquals(MOCK_CRONET_TEST_FAILED_URL, listener.mUrl);
        assertEquals(0, listener.mHttpStatusCode);
    }

    private native void nativeAddUrlInterceptors();
}
