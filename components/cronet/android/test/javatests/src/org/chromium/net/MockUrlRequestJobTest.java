// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import android.test.suitebuilder.annotation.LargeTest;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.test.util.Feature;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.util.HashMap;
import java.util.List;
import java.util.concurrent.Executors;

/**
 * Tests that use mock URLRequestJobs to simulate URL requests.
 */
public class MockUrlRequestJobTest extends CronetTestBase {
    private static final String TAG = "MockURLRequestJobTest";

    private CronetTestActivity mActivity;
    private MockUrlRequestJobFactory mMockUrlRequestJobFactory;

    // Helper function to create a HttpUrlRequest with the specified url.
    private TestHttpUrlRequestListener createRequestAndWaitForComplete(
            String url, boolean disableRedirects) {
        HashMap<String, String> headers = new HashMap<String, String>();
        TestHttpUrlRequestListener listener = new TestHttpUrlRequestListener();

        HttpUrlRequest request = mActivity.mRequestFactory.createRequest(
                url,
                HttpUrlRequest.REQUEST_PRIORITY_MEDIUM,
                headers,
                listener);
        if (disableRedirects) {
            request.disableRedirects();
        }
        request.start();
        listener.blockForComplete();
        return listener;
    }

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mActivity = launchCronetTestApp();
        mMockUrlRequestJobFactory = new MockUrlRequestJobFactory(
                getInstrumentation().getTargetContext());
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testSuccessURLRequest() throws Exception {
        TestHttpUrlRequestListener listener = createRequestAndWaitForComplete(
                MockUrlRequestJobFactory.SUCCESS_URL, false);
        assertEquals(MockUrlRequestJobFactory.SUCCESS_URL, listener.mUrl);
        assertEquals(200, listener.mHttpStatusCode);
        assertEquals("OK", listener.mHttpStatusText);
        assertEquals("this is a text file\n",
                new String(listener.mResponseAsBytes));
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testRedirectURLRequest() throws Exception {
        TestHttpUrlRequestListener listener = createRequestAndWaitForComplete(
                MockUrlRequestJobFactory.REDIRECT_URL, false);

        // Currently Cronet does not expose the url after redirect.
        assertEquals(MockUrlRequestJobFactory.REDIRECT_URL, listener.mUrl);
        assertEquals(200, listener.mHttpStatusCode);
        assertEquals("OK", listener.mHttpStatusText);
        // Expect that the request is redirected to success.txt.
        assertEquals("this is a text file\n",
                new String(listener.mResponseAsBytes));
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testNotFoundURLRequest() throws Exception {
        TestHttpUrlRequestListener listener = createRequestAndWaitForComplete(
                MockUrlRequestJobFactory.NOTFOUND_URL, false);
        assertEquals(MockUrlRequestJobFactory.NOTFOUND_URL, listener.mUrl);
        assertEquals(404, listener.mHttpStatusCode);
        assertEquals("Not Found", listener.mHttpStatusText);
        assertEquals(
                "<!DOCTYPE html>\n<html>\n<head>\n<title>Not found</title>\n"
                + "<p>Test page loaded.</p>\n</head>\n</html>\n",
                new String(listener.mResponseAsBytes));
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testFailedURLRequest() throws Exception {
        TestHttpUrlRequestListener listener = createRequestAndWaitForComplete(
                MockUrlRequestJobFactory.FAILED_URL, false);

        assertEquals(MockUrlRequestJobFactory.FAILED_URL, listener.mUrl);
        assertEquals(null, listener.mHttpStatusText);
        assertEquals(0, listener.mHttpStatusCode);
    }

    @SmallTest
    @Feature({"Cronet"})
    // Test that redirect can be disabled for a request.
    public void testDisableRedirects() throws Exception {
        TestHttpUrlRequestListener listener = createRequestAndWaitForComplete(
                MockUrlRequestJobFactory.REDIRECT_URL, true);
        // Currently Cronet does not expose the url after redirect.
        assertEquals(MockUrlRequestJobFactory.REDIRECT_URL, listener.mUrl);
        assertEquals(302, listener.mHttpStatusCode);
        // Expect that the request is not redirected to success.txt.
        assertNotNull(listener.mResponseHeaders);
        List<String> entry = listener.mResponseHeaders.get("redirect-header");
        assertEquals(1, entry.size());
        assertEquals("header-value", entry.get(0));
        List<String> location = listener.mResponseHeaders.get("Location");
        assertEquals(1, location.size());
        assertEquals("/success.txt", location.get(0));
        assertEquals("Request failed because there were too many redirects or "
                         + "redirects have been disabled",
                     listener.mException.getMessage());
    }

    /**
     * TestByteChannel is used for making sure write is not called after the
     * channel has been closed. Can synchronously cancel a request when write is
     * called.
     */
    static class TestByteChannel extends ChunkedWritableByteChannel {
        HttpUrlRequest mRequestToCancelOnWrite;

        @Override
        public int write(ByteBuffer byteBuffer) throws IOException {
            assertTrue(isOpen());
            if (mRequestToCancelOnWrite != null) {
                assertFalse(mRequestToCancelOnWrite.isCanceled());
                mRequestToCancelOnWrite.cancel();
                mRequestToCancelOnWrite = null;
            }
            return super.write(byteBuffer);
        }

        @Override
        public void close() {
            assertTrue(isOpen());
            super.close();
        }

        /**
         * Set request that will be synchronously canceled when write is called.
         */
        public void setRequestToCancelOnWrite(HttpUrlRequest request) {
            mRequestToCancelOnWrite = request;
        }
    }

    @LargeTest
    @Feature({"Cronet"})
    public void testNoWriteAfterCancelOnAnotherThread() throws Exception {
        CronetTestActivity activity = launchCronetTestApp();

        // This test verifies that WritableByteChannel.write is not called after
        // WritableByteChannel.close if request is canceled from another
        // thread.
        for (int i = 0; i < 100; ++i) {
            HashMap<String, String> headers = new HashMap<String, String>();
            TestByteChannel channel = new TestByteChannel();
            TestHttpUrlRequestListener listener =
                    new TestHttpUrlRequestListener();

            // Create request.
            final HttpUrlRequest request =
                    activity.mRequestFactory.createRequest(
                            MockUrlRequestJobFactory.SUCCESS_URL,
                            HttpUrlRequest.REQUEST_PRIORITY_LOW, headers,
                            channel, listener);
            request.start();
            listener.blockForStart();
            Runnable cancelTask = new Runnable() {
                public void run() {
                    request.cancel();
                }
            };
            Executors.newCachedThreadPool().execute(cancelTask);
            listener.blockForComplete();
            assertFalse(channel.isOpen());
        }
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testNoWriteAfterSyncCancel() throws Exception {
        CronetTestActivity activity = launchCronetTestApp();

        HashMap<String, String> headers = new HashMap<String, String>();
        TestByteChannel channel = new TestByteChannel();
        TestHttpUrlRequestListener listener = new TestHttpUrlRequestListener();

        String data = "MyBigFunkyData";
        int dataLength = data.length();
        int repeatCount = 10000;
        String mockUrl = mMockUrlRequestJobFactory.getMockUrlForData(data,
                repeatCount);

        // Create request.
        final HttpUrlRequest request =
                activity.mRequestFactory.createRequest(
                        mockUrl,
                        HttpUrlRequest.REQUEST_PRIORITY_LOW, headers,
                        channel, listener);
        // Channel will cancel the request from the network thread during the
        // first write.
        channel.setRequestToCancelOnWrite(request);
        request.start();
        listener.blockForComplete();
        assertTrue(request.isCanceled());
        assertFalse(channel.isOpen());
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testBigDataSyncReadRequest() throws Exception {
        String data = "MyBigFunkyData";
        int dataLength = data.length();
        int repeatCount = 100000;
        String mockUrl = mMockUrlRequestJobFactory.getMockUrlForData(data,
                repeatCount);
        TestHttpUrlRequestListener listener = createRequestAndWaitForComplete(
                mockUrl, false);
        assertEquals(mockUrl, listener.mUrl);
        String responseData = new String(listener.mResponseAsBytes);
        for (int i = 0; i < repeatCount; ++i) {
            assertEquals(data, responseData.substring(dataLength * i,
                    dataLength * (i + 1)));
        }
    }
}
