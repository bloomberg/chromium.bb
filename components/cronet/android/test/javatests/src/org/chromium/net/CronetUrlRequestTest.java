// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import android.os.ConditionVariable;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.test.util.Feature;
import org.chromium.net.TestUrlRequestCallback.FailureType;
import org.chromium.net.TestUrlRequestCallback.ResponseStep;
import org.chromium.net.test.FailurePhase;

import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * Test functionality of CronetUrlRequest.
 */
public class CronetUrlRequestTest extends CronetTestBase {
    // URL used for base tests.
    private static final String TEST_URL = "http://127.0.0.1:8000";

    private CronetTestFramework mTestFramework;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mTestFramework = startCronetTestFramework();
        assertTrue(NativeTestServer.startNativeTestServer(getContext()));
        // Add url interceptors after native application context is initialized.
        MockUrlRequestJobFactory.setUp();
    }

    @Override
    protected void tearDown() throws Exception {
        NativeTestServer.shutdownNativeTestServer();
        mTestFramework.mCronetEngine.shutdown();
        super.tearDown();
    }

    private TestUrlRequestCallback startAndWaitForComplete(String url) throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        // Create request.
        UrlRequest.Builder builder = new UrlRequest.Builder(
                url, callback, callback.getExecutor(), mTestFramework.mCronetEngine);
        UrlRequest urlRequest = builder.build();
        urlRequest.start();
        callback.blockForDone();
        assertTrue(urlRequest.isDone());
        return callback;
    }

    private void checkResponseInfo(UrlResponseInfo responseInfo, String expectedUrl,
            int expectedHttpStatusCode, String expectedHttpStatusText) {
        assertEquals(expectedUrl, responseInfo.getUrl());
        assertEquals(
                expectedUrl, responseInfo.getUrlChain().get(responseInfo.getUrlChain().size() - 1));
        assertEquals(expectedHttpStatusCode, responseInfo.getHttpStatusCode());
        assertEquals(expectedHttpStatusText, responseInfo.getHttpStatusText());
        assertFalse(responseInfo.wasCached());
        assertTrue(responseInfo.toString().length() > 0);
    }

    private void checkResponseInfoHeader(
            UrlResponseInfo responseInfo, String headerName, String headerValue) {
        Map<String, List<String>> responseHeaders =
                responseInfo.getAllHeaders();
        List<String> header = responseHeaders.get(headerName);
        assertNotNull(header);
        assertTrue(header.contains(headerValue));
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testBuilderChecks() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        try {
            new UrlRequest.Builder(
                    null, callback, callback.getExecutor(), mTestFramework.mCronetEngine);
            fail("URL not null-checked");
        } catch (NullPointerException e) {
            assertEquals("URL is required.", e.getMessage());
        }
        try {
            new UrlRequest.Builder(NativeTestServer.getRedirectURL(), null, callback.getExecutor(),
                    mTestFramework.mCronetEngine);
            fail("Callback not null-checked");
        } catch (NullPointerException e) {
            assertEquals("Callback is required.", e.getMessage());
        }
        try {
            new UrlRequest.Builder(NativeTestServer.getRedirectURL(), callback, null,
                    mTestFramework.mCronetEngine);
            fail("Executor not null-checked");
        } catch (NullPointerException e) {
            assertEquals("Executor is required.", e.getMessage());
        }
        try {
            new UrlRequest.Builder(
                    NativeTestServer.getRedirectURL(), callback, callback.getExecutor(), null);
            fail("CronetEngine not null-checked");
        } catch (NullPointerException e) {
            assertEquals("CronetEngine is required.", e.getMessage());
        }
        // Verify successful creation doesn't throw.
        new UrlRequest.Builder(NativeTestServer.getRedirectURL(), callback, callback.getExecutor(),
                mTestFramework.mCronetEngine);
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testSimpleGet() throws Exception {
        String url = NativeTestServer.getEchoMethodURL();
        TestUrlRequestCallback callback = startAndWaitForComplete(url);
        assertEquals(200, callback.mResponseInfo.getHttpStatusCode());
        // Default method is 'GET'.
        assertEquals("GET", callback.mResponseAsString);
        assertEquals(0, callback.mRedirectCount);
        assertEquals(callback.mResponseStep, ResponseStep.ON_SUCCEEDED);
        assertEquals(String.format("UrlResponseInfo[%s]: urlChain = [%s], httpStatus = 200 OK, "
                                     + "headers = [Connection=close, Content-Length=3, "
                                     + "Content-Type=text/plain], wasCached = false, "
                                     + "negotiatedProtocol = unknown, proxyServer= :0, "
                                     + "receivedBytesCount = 86",
                             url, url),
                callback.mResponseInfo.toString());
        checkResponseInfo(callback.mResponseInfo, NativeTestServer.getEchoMethodURL(), 200, "OK");
    }

    /**
     * Tests a redirect by running it step-by-step. Also tests that delaying a
     * request works as expected. To make sure there are no unexpected pending
     * messages, does a GET between UrlRequest.Callback callbacks.
     */
    @SmallTest
    @Feature({"Cronet"})
    public void testRedirectAsync() throws Exception {
        // Start the request and wait to see the redirect.
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        callback.setAutoAdvance(false);
        UrlRequest.Builder builder = new UrlRequest.Builder(NativeTestServer.getRedirectURL(),
                callback, callback.getExecutor(), mTestFramework.mCronetEngine);
        UrlRequest urlRequest = builder.build();
        urlRequest.start();
        callback.waitForNextStep();

        // Check the redirect.
        assertEquals(ResponseStep.ON_RECEIVED_REDIRECT, callback.mResponseStep);
        assertEquals(1, callback.mRedirectResponseInfoList.size());
        checkResponseInfo(callback.mRedirectResponseInfoList.get(0),
                NativeTestServer.getRedirectURL(), 302, "Found");
        assertEquals(1, callback.mRedirectResponseInfoList.get(0).getUrlChain().size());
        assertEquals(NativeTestServer.getSuccessURL(), callback.mRedirectUrlList.get(0));
        checkResponseInfoHeader(
                callback.mRedirectResponseInfoList.get(0), "redirect-header", "header-value");

        assertEquals(String.format("UrlResponseInfo[%s]: urlChain = [%s], httpStatus = 302 Found, "
                                     + "headers = [Location=/success.txt, "
                                     + "redirect-header=header-value], wasCached = false, "
                                     + "negotiatedProtocol = unknown, proxyServer= :0, "
                                     + "receivedBytesCount = 74",
                             NativeTestServer.getRedirectURL(), NativeTestServer.getRedirectURL()),
                callback.mRedirectResponseInfoList.get(0).toString());

        // Wait for an unrelated request to finish. The request should not
        // advance until followRedirect is invoked.
        testSimpleGet();
        assertEquals(ResponseStep.ON_RECEIVED_REDIRECT, callback.mResponseStep);
        assertEquals(1, callback.mRedirectResponseInfoList.size());

        // Follow the redirect and wait for the next set of headers.
        urlRequest.followRedirect();
        callback.waitForNextStep();

        assertEquals(ResponseStep.ON_RESPONSE_STARTED, callback.mResponseStep);
        assertEquals(1, callback.mRedirectResponseInfoList.size());
        assertEquals(200, callback.mResponseInfo.getHttpStatusCode());
        checkResponseInfo(callback.mResponseInfo, NativeTestServer.getSuccessURL(), 200, "OK");
        assertEquals(2, callback.mResponseInfo.getUrlChain().size());
        assertEquals(
                NativeTestServer.getRedirectURL(), callback.mResponseInfo.getUrlChain().get(0));
        assertEquals(NativeTestServer.getSuccessURL(), callback.mResponseInfo.getUrlChain().get(1));

        // Wait for an unrelated request to finish. The request should not
        // advance until read is invoked.
        testSimpleGet();
        assertEquals(ResponseStep.ON_RESPONSE_STARTED, callback.mResponseStep);

        // One read should get all the characters, but best not to depend on
        // how much is actually read from the socket at once.
        while (!callback.isDone()) {
            callback.startNextRead(urlRequest);
            callback.waitForNextStep();
            String response = callback.mResponseAsString;
            ResponseStep step = callback.mResponseStep;
            if (!callback.isDone()) {
                assertEquals(ResponseStep.ON_READ_COMPLETED, step);
            }
            // Should not receive any messages while waiting for another get,
            // as the next read has not been started.
            testSimpleGet();
            assertEquals(response, callback.mResponseAsString);
            assertEquals(step, callback.mResponseStep);
        }
        assertEquals(ResponseStep.ON_SUCCEEDED, callback.mResponseStep);
        assertEquals(NativeTestServer.SUCCESS_BODY, callback.mResponseAsString);

        assertEquals(String.format("UrlResponseInfo[%s]: urlChain = [%s, %s], httpStatus = 200 OK, "
                                     + "headers = [Content-Type=text/plain, "
                                     + "Access-Control-Allow-Origin=*, header-name=header-value, "
                                     + "multi-header-name=header-value1, "
                                     + "multi-header-name=header-value2], wasCached = false, "
                                     + "negotiatedProtocol = unknown, proxyServer= :0, "
                                     + "receivedBytesCount = 260",
                             NativeTestServer.getSuccessURL(), NativeTestServer.getRedirectURL(),
                             NativeTestServer.getSuccessURL()),
                callback.mResponseInfo.toString());

        // Make sure there are no other pending messages, which would trigger
        // asserts in TestUrlRequestCallback.
        testSimpleGet();
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testNotFound() throws Exception {
        String url = NativeTestServer.getFileURL("/notfound.html");
        TestUrlRequestCallback callback = startAndWaitForComplete(url);
        checkResponseInfo(callback.mResponseInfo, url, 404, "Not Found");
        assertEquals("<!DOCTYPE html>\n<html>\n<head>\n<title>Not found</title>\n"
                        + "<p>Test page loaded.</p>\n</head>\n</html>\n",
                callback.mResponseAsString);
        assertEquals(0, callback.mRedirectCount);
        assertEquals(callback.mResponseStep, ResponseStep.ON_SUCCEEDED);
    }

    // Checks that UrlRequest.Callback.onFailed is only called once in the case
    // of ERR_CONTENT_LENGTH_MISMATCH, which has an unusual failure path.
    // See http://crbug.com/468803.
    @SmallTest
    @Feature({"Cronet"})
    public void testContentLengthMismatchFailsOnce() throws Exception {
        String url = NativeTestServer.getFileURL(
                "/content_length_mismatch.html");
        TestUrlRequestCallback callback = startAndWaitForComplete(url);
        assertEquals(200, callback.mResponseInfo.getHttpStatusCode());
        // The entire response body will be read before the error is returned.
        // This is because the network stack returns data as it's read from the
        // socket, and the socket close message which tiggers the error will
        // only be passed along after all data has been read.
        assertEquals("Response that lies about content length.", callback.mResponseAsString);
        assertNotNull(callback.mError);
        assertEquals("Exception in CronetUrlRequest: net::ERR_CONTENT_LENGTH_MISMATCH",
                callback.mError.getMessage());
        // Wait for a couple round trips to make sure there are no pending
        // onFailed messages. This test relies on checks in
        // TestUrlRequestCallback catching a second onFailed call.
        testSimpleGet();
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testSetHttpMethod() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        String methodName = "HEAD";
        UrlRequest.Builder builder = new UrlRequest.Builder(NativeTestServer.getEchoMethodURL(),
                callback, callback.getExecutor(), mTestFramework.mCronetEngine);
        // Try to set 'null' method.
        try {
            builder.setHttpMethod(null);
            fail("Exception not thrown");
        } catch (NullPointerException e) {
            assertEquals("Method is required.", e.getMessage());
        }

        builder.setHttpMethod(methodName);
        builder.build().start();
        callback.blockForDone();
        assertEquals(200, callback.mResponseInfo.getHttpStatusCode());
        assertEquals(0, callback.mHttpResponseDataLength);
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testBadMethod() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder builder = new UrlRequest.Builder(
                TEST_URL, callback, callback.getExecutor(), mTestFramework.mCronetEngine);
        try {
            builder.setHttpMethod("bad:method!");
            builder.build().start();
            fail("IllegalArgumentException not thrown.");
        } catch (IllegalArgumentException e) {
            assertEquals("Invalid http method bad:method!",
                    e.getMessage());
        }
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testBadHeaderName() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder builder = new UrlRequest.Builder(
                TEST_URL, callback, callback.getExecutor(), mTestFramework.mCronetEngine);
        try {
            builder.addHeader("header:name", "headervalue");
            builder.build().start();
            fail("IllegalArgumentException not thrown.");
        } catch (IllegalArgumentException e) {
            assertEquals("Invalid header header:name=headervalue",
                    e.getMessage());
        }
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testBadHeaderValue() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder builder = new UrlRequest.Builder(
                TEST_URL, callback, callback.getExecutor(), mTestFramework.mCronetEngine);
        try {
            builder.addHeader("headername", "bad header\r\nvalue");
            builder.build().start();
            fail("IllegalArgumentException not thrown.");
        } catch (IllegalArgumentException e) {
            assertEquals("Invalid header headername=bad header\r\nvalue",
                    e.getMessage());
        }
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testAddHeader() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        String headerName = "header-name";
        String headerValue = "header-value";
        UrlRequest.Builder builder =
                new UrlRequest.Builder(NativeTestServer.getEchoHeaderURL(headerName), callback,
                        callback.getExecutor(), mTestFramework.mCronetEngine);

        builder.addHeader(headerName, headerValue);
        builder.build().start();
        callback.blockForDone();
        assertEquals(200, callback.mResponseInfo.getHttpStatusCode());
        assertEquals(headerValue, callback.mResponseAsString);
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testMultiRequestHeaders() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        String headerName = "header-name";
        String headerValue1 = "header-value1";
        String headerValue2 = "header-value2";
        UrlRequest.Builder builder = new UrlRequest.Builder(NativeTestServer.getEchoAllHeadersURL(),
                callback, callback.getExecutor(), mTestFramework.mCronetEngine);
        builder.addHeader(headerName, headerValue1);
        builder.addHeader(headerName, headerValue2);
        builder.build().start();
        callback.blockForDone();
        assertEquals(200, callback.mResponseInfo.getHttpStatusCode());
        String headers = callback.mResponseAsString;
        Pattern pattern = Pattern.compile(headerName + ":\\s(.*)\\r\\n");
        Matcher matcher = pattern.matcher(headers);
        List<String> actualValues = new ArrayList<String>();
        while (matcher.find()) {
            actualValues.add(matcher.group(1));
        }
        assertEquals(1, actualValues.size());
        assertEquals("header-value2", actualValues.get(0));
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testCustomUserAgent() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        String userAgentName = "User-Agent";
        String userAgentValue = "User-Agent-Value";
        UrlRequest.Builder builder =
                new UrlRequest.Builder(NativeTestServer.getEchoHeaderURL(userAgentName), callback,
                        callback.getExecutor(), mTestFramework.mCronetEngine);
        builder.addHeader(userAgentName, userAgentValue);
        builder.build().start();
        callback.blockForDone();
        assertEquals(200, callback.mResponseInfo.getHttpStatusCode());
        assertEquals(userAgentValue, callback.mResponseAsString);
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testDefaultUserAgent() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        String headerName = "User-Agent";
        UrlRequest.Builder builder =
                new UrlRequest.Builder(NativeTestServer.getEchoHeaderURL(headerName), callback,
                        callback.getExecutor(), mTestFramework.mCronetEngine);
        builder.build().start();
        callback.blockForDone();
        assertEquals(200, callback.mResponseInfo.getHttpStatusCode());
        assertTrue("Default User-Agent should contain Cronet/n.n.n.n but is "
                        + callback.mResponseAsString,
                Pattern.matches(
                        ".+Cronet/\\d+\\.\\d+\\.\\d+\\.\\d+.+", callback.mResponseAsString));
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testMockSuccess() throws Exception {
        TestUrlRequestCallback callback = startAndWaitForComplete(NativeTestServer.getSuccessURL());
        assertEquals(200, callback.mResponseInfo.getHttpStatusCode());
        assertEquals(0, callback.mRedirectResponseInfoList.size());
        assertTrue(callback.mHttpResponseDataLength != 0);
        assertEquals(callback.mResponseStep, ResponseStep.ON_SUCCEEDED);
        Map<String, List<String>> responseHeaders = callback.mResponseInfo.getAllHeaders();
        assertEquals("header-value", responseHeaders.get("header-name").get(0));
        List<String> multiHeader = responseHeaders.get("multi-header-name");
        assertEquals(2, multiHeader.size());
        assertEquals("header-value1", multiHeader.get(0));
        assertEquals("header-value2", multiHeader.get(1));
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testResponseHeadersList() throws Exception {
        TestUrlRequestCallback callback = startAndWaitForComplete(NativeTestServer.getSuccessURL());
        assertEquals(200, callback.mResponseInfo.getHttpStatusCode());
        List<Map.Entry<String, String>> responseHeaders =
                callback.mResponseInfo.getAllHeadersAsList();
        assertEquals(5, responseHeaders.size());
        assertEquals("Content-Type", responseHeaders.get(0).getKey());
        assertEquals("text/plain", responseHeaders.get(0).getValue());
        assertEquals("Access-Control-Allow-Origin", responseHeaders.get(1).getKey());
        assertEquals("*", responseHeaders.get(1).getValue());
        assertEquals("header-name", responseHeaders.get(2).getKey());
        assertEquals("header-value", responseHeaders.get(2).getValue());
        assertEquals("multi-header-name", responseHeaders.get(3).getKey());
        assertEquals("header-value1", responseHeaders.get(3).getValue());
        assertEquals("multi-header-name", responseHeaders.get(4).getKey());
        assertEquals("header-value2", responseHeaders.get(4).getValue());
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testMockMultiRedirect() throws Exception {
        TestUrlRequestCallback callback =
                startAndWaitForComplete(NativeTestServer.getMultiRedirectURL());
        UrlResponseInfo mResponseInfo = callback.mResponseInfo;
        assertEquals(2, callback.mRedirectCount);
        assertEquals(200, mResponseInfo.getHttpStatusCode());
        assertEquals(2, callback.mRedirectResponseInfoList.size());

        // Check first redirect (multiredirect.html -> redirect.html)
        UrlResponseInfo firstRedirectResponseInfo = callback.mRedirectResponseInfoList.get(0);
        assertEquals(1, firstRedirectResponseInfo.getUrlChain().size());
        assertEquals(NativeTestServer.getMultiRedirectURL(),
                firstRedirectResponseInfo.getUrlChain().get(0));
        checkResponseInfo(
                firstRedirectResponseInfo, NativeTestServer.getMultiRedirectURL(), 302, "Found");
        checkResponseInfoHeader(firstRedirectResponseInfo,
                "redirect-header0", "header-value");
        assertEquals(77, firstRedirectResponseInfo.getReceivedBytesCount());

        // Check second redirect (redirect.html -> success.txt)
        UrlResponseInfo secondRedirectResponseInfo = callback.mRedirectResponseInfoList.get(1);
        assertEquals(2, secondRedirectResponseInfo.getUrlChain().size());
        assertEquals(NativeTestServer.getMultiRedirectURL(),
                secondRedirectResponseInfo.getUrlChain().get(0));
        assertEquals(
                NativeTestServer.getRedirectURL(), secondRedirectResponseInfo.getUrlChain().get(1));
        checkResponseInfo(
                secondRedirectResponseInfo, NativeTestServer.getRedirectURL(), 302, "Found");
        checkResponseInfoHeader(secondRedirectResponseInfo,
                "redirect-header", "header-value");
        assertEquals(151, secondRedirectResponseInfo.getReceivedBytesCount());

        // Check final response (success.txt).
        assertEquals(NativeTestServer.getSuccessURL(), mResponseInfo.getUrl());
        assertEquals(3, mResponseInfo.getUrlChain().size());
        assertEquals(NativeTestServer.getMultiRedirectURL(), mResponseInfo.getUrlChain().get(0));
        assertEquals(NativeTestServer.getRedirectURL(), mResponseInfo.getUrlChain().get(1));
        assertEquals(NativeTestServer.getSuccessURL(), mResponseInfo.getUrlChain().get(2));
        assertTrue(callback.mHttpResponseDataLength != 0);
        assertEquals(2, callback.mRedirectCount);
        assertEquals(callback.mResponseStep, ResponseStep.ON_SUCCEEDED);
        assertEquals(337, mResponseInfo.getReceivedBytesCount());
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testMockNotFound() throws Exception {
        TestUrlRequestCallback callback =
                startAndWaitForComplete(NativeTestServer.getNotFoundURL());
        assertEquals(404, callback.mResponseInfo.getHttpStatusCode());
        assertEquals(121, callback.mResponseInfo.getReceivedBytesCount());
        assertTrue(callback.mHttpResponseDataLength != 0);
        assertEquals(0, callback.mRedirectCount);
        assertFalse(callback.mOnErrorCalled);
        assertEquals(callback.mResponseStep, ResponseStep.ON_SUCCEEDED);
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testMockStartAsyncError() throws Exception {
        final int arbitraryNetError = -3;
        TestUrlRequestCallback callback =
                startAndWaitForComplete(MockUrlRequestJobFactory.getMockUrlWithFailure(
                        FailurePhase.START, arbitraryNetError));
        assertNull(callback.mResponseInfo);
        assertNotNull(callback.mError);
        assertEquals(arbitraryNetError, callback.mError.netError());
        assertEquals(0, callback.mRedirectCount);
        assertTrue(callback.mOnErrorCalled);
        assertEquals(callback.mResponseStep, ResponseStep.NOTHING);
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testMockReadDataSyncError() throws Exception {
        final int arbitraryNetError = -4;
        TestUrlRequestCallback callback =
                startAndWaitForComplete(MockUrlRequestJobFactory.getMockUrlWithFailure(
                        FailurePhase.READ_SYNC, arbitraryNetError));
        assertEquals(200, callback.mResponseInfo.getHttpStatusCode());
        assertEquals(0, callback.mResponseInfo.getReceivedBytesCount());
        assertNotNull(callback.mError);
        assertEquals(arbitraryNetError, callback.mError.netError());
        assertEquals(0, callback.mRedirectCount);
        assertTrue(callback.mOnErrorCalled);
        assertEquals(callback.mResponseStep, ResponseStep.ON_RESPONSE_STARTED);
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testMockReadDataAsyncError() throws Exception {
        final int arbitraryNetError = -5;
        TestUrlRequestCallback callback =
                startAndWaitForComplete(MockUrlRequestJobFactory.getMockUrlWithFailure(
                        FailurePhase.READ_ASYNC, arbitraryNetError));
        assertEquals(200, callback.mResponseInfo.getHttpStatusCode());
        assertEquals(0, callback.mResponseInfo.getReceivedBytesCount());
        assertNotNull(callback.mError);
        assertEquals(arbitraryNetError, callback.mError.netError());
        assertEquals(0, callback.mRedirectCount);
        assertTrue(callback.mOnErrorCalled);
        assertEquals(callback.mResponseStep, ResponseStep.ON_RESPONSE_STARTED);
    }

    /**
     * Tests that request continues when client certificate is requested.
     */
    @SmallTest
    @Feature({"Cronet"})
    public void testMockClientCertificateRequested() throws Exception {
        TestUrlRequestCallback callback = startAndWaitForComplete(
                MockUrlRequestJobFactory.getMockUrlForClientCertificateRequest());
        assertNotNull(callback.mResponseInfo);
        assertEquals(200, callback.mResponseInfo.getHttpStatusCode());
        assertEquals("data", callback.mResponseAsString);
        assertEquals(0, callback.mRedirectCount);
        assertNull(callback.mError);
        assertFalse(callback.mOnErrorCalled);
    }

    /**
     * Tests that an SSL cert error will be reported via {@link UrlRequest#onFailed}.
     */
    @SmallTest
    @Feature({"Cronet"})
    public void testMockSSLCertificateError() throws Exception {
        TestUrlRequestCallback callback = startAndWaitForComplete(
                MockUrlRequestJobFactory.getMockUrlForSSLCertificateError());
        assertNull(callback.mResponseInfo);
        assertNotNull(callback.mError);
        assertTrue(callback.mOnErrorCalled);
        assertEquals(-201, callback.mError.netError());
        assertEquals("Exception in CronetUrlRequest: net::ERR_CERT_DATE_INVALID",
                callback.mError.getMessage());
        assertEquals(callback.mResponseStep, ResponseStep.NOTHING);
    }

    /**
     * Checks that the buffer is updated correctly, when starting at an offset,
     * when using legacy read() API.
     */
    @SmallTest
    @Feature({"Cronet"})
    @SuppressWarnings("deprecation")
    public void testLegacySimpleGetBufferUpdates() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        callback.mLegacyReadByteBufferAdjustment = true;
        callback.setAutoAdvance(false);
        // Since the default method is "GET", the expected response body is also
        // "GET".
        UrlRequest.Builder builder = new UrlRequest.Builder(NativeTestServer.getEchoMethodURL(),
                callback, callback.getExecutor(), mTestFramework.mCronetEngine);
        UrlRequest urlRequest = builder.build();
        urlRequest.start();
        callback.waitForNextStep();

        ByteBuffer readBuffer = ByteBuffer.allocateDirect(5);
        readBuffer.put("FOR".getBytes());
        assertEquals(3, readBuffer.position());

        // Read first two characters of the response ("GE"). It's theoretically
        // possible to need one read per character, though in practice,
        // shouldn't happen.
        while (callback.mResponseAsString.length() < 2) {
            assertFalse(callback.isDone());
            urlRequest.read(readBuffer);
            callback.waitForNextStep();
        }

        // Make sure the two characters were read.
        assertEquals("GE", callback.mResponseAsString);

        // Check the contents of the entire buffer. The first 3 characters
        // should not have been changed, and the last two should be the first
        // two characters from the response.
        assertEquals("FORGE", bufferContentsToString(readBuffer, 0, 5));
        // The limit should now be 5. Position could be either 3 or 4.
        assertEquals(5, readBuffer.limit());

        assertEquals(ResponseStep.ON_READ_COMPLETED, callback.mResponseStep);

        // Start reading from position 3. Since the only remaining character
        // from the response is a "T", when the read completes, the buffer
        // should contain "FORTE", with a position() of 3 and a limit() of 4.
        readBuffer.position(3);
        urlRequest.read(readBuffer);
        callback.waitForNextStep();

        // Make sure all three characters of the response have now been read.
        assertEquals("GET", callback.mResponseAsString);

        // Check the entire contents of the buffer. Only the third character
        // should have been modified.
        assertEquals("FORTE", bufferContentsToString(readBuffer, 0, 5));

        // Make sure position and limit were updated correctly.
        assertEquals(3, readBuffer.position());
        assertEquals(4, readBuffer.limit());

        assertEquals(ResponseStep.ON_READ_COMPLETED, callback.mResponseStep);

        // One more read attempt. The request should complete.
        readBuffer.position(1);
        readBuffer.limit(5);
        urlRequest.read(readBuffer);
        callback.waitForNextStep();

        assertEquals(200, callback.mResponseInfo.getHttpStatusCode());
        assertEquals("GET", callback.mResponseAsString);
        checkResponseInfo(callback.mResponseInfo, NativeTestServer.getEchoMethodURL(), 200, "OK");

        // Check that buffer contents were not modified.
        assertEquals("FORTE", bufferContentsToString(readBuffer, 0, 5));

        // Buffer limit should be set to original position, and position should
        // not have been modified, since nothing was read.
        assertEquals(1, readBuffer.position());
        assertEquals(1, readBuffer.limit());

        assertEquals(ResponseStep.ON_SUCCEEDED, callback.mResponseStep);

        // Make sure there are no other pending messages, which would trigger
        // asserts in TestUrlRequestCallback.
        testSimpleGet();
    }

    /**
     * Checks that the buffer is updated correctly, when starting at an offset.
     */
    @SmallTest
    @Feature({"Cronet"})
    public void testSimpleGetBufferUpdates() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        callback.setAutoAdvance(false);
        // Since the default method is "GET", the expected response body is also
        // "GET".
        UrlRequest.Builder builder = new UrlRequest.Builder(NativeTestServer.getEchoMethodURL(),
                callback, callback.getExecutor(), mTestFramework.mCronetEngine);
        UrlRequest urlRequest = builder.build();
        urlRequest.start();
        callback.waitForNextStep();

        ByteBuffer readBuffer = ByteBuffer.allocateDirect(5);
        readBuffer.put("FOR".getBytes());
        assertEquals(3, readBuffer.position());

        // Read first two characters of the response ("GE"). It's theoretically
        // possible to need one read per character, though in practice,
        // shouldn't happen.
        while (callback.mResponseAsString.length() < 2) {
            assertFalse(callback.isDone());
            callback.startNextRead(urlRequest, readBuffer);
            callback.waitForNextStep();
        }

        // Make sure the two characters were read.
        assertEquals("GE", callback.mResponseAsString);

        // Check the contents of the entire buffer. The first 3 characters
        // should not have been changed, and the last two should be the first
        // two characters from the response.
        assertEquals("FORGE", bufferContentsToString(readBuffer, 0, 5));
        // The limit and position should be 5.
        assertEquals(5, readBuffer.limit());
        assertEquals(5, readBuffer.position());

        assertEquals(ResponseStep.ON_READ_COMPLETED, callback.mResponseStep);

        // Start reading from position 3. Since the only remaining character
        // from the response is a "T", when the read completes, the buffer
        // should contain "FORTE", with a position() of 4 and a limit() of 5.
        readBuffer.position(3);
        callback.startNextRead(urlRequest, readBuffer);
        callback.waitForNextStep();

        // Make sure all three characters of the response have now been read.
        assertEquals("GET", callback.mResponseAsString);

        // Check the entire contents of the buffer. Only the third character
        // should have been modified.
        assertEquals("FORTE", bufferContentsToString(readBuffer, 0, 5));

        // Make sure position and limit were updated correctly.
        assertEquals(4, readBuffer.position());
        assertEquals(5, readBuffer.limit());

        assertEquals(ResponseStep.ON_READ_COMPLETED, callback.mResponseStep);

        // One more read attempt. The request should complete.
        readBuffer.position(1);
        readBuffer.limit(5);
        callback.startNextRead(urlRequest, readBuffer);
        callback.waitForNextStep();

        assertEquals(200, callback.mResponseInfo.getHttpStatusCode());
        assertEquals("GET", callback.mResponseAsString);
        checkResponseInfo(callback.mResponseInfo, NativeTestServer.getEchoMethodURL(), 200, "OK");

        // Check that buffer contents were not modified.
        assertEquals("FORTE", bufferContentsToString(readBuffer, 0, 5));

        // Position should not have been modified, since nothing was read.
        assertEquals(1, readBuffer.position());
        // Limit should be unchanged as always.
        assertEquals(5, readBuffer.limit());

        assertEquals(ResponseStep.ON_SUCCEEDED, callback.mResponseStep);

        // Make sure there are no other pending messages, which would trigger
        // asserts in TestUrlRequestCallback.
        testSimpleGet();
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testBadBuffers() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        callback.setAutoAdvance(false);
        UrlRequest.Builder builder = new UrlRequest.Builder(NativeTestServer.getEchoMethodURL(),
                callback, callback.getExecutor(), mTestFramework.mCronetEngine);
        UrlRequest urlRequest = builder.build();
        urlRequest.start();
        callback.waitForNextStep();

        // Try to read using a full buffer.
        try {
            ByteBuffer readBuffer = ByteBuffer.allocateDirect(4);
            readBuffer.put("full".getBytes());
            urlRequest.readNew(readBuffer);
            fail("Exception not thrown");
        } catch (IllegalArgumentException e) {
            assertEquals("ByteBuffer is already full.",
                    e.getMessage());
        }

        // Try to read using a non-direct buffer.
        try {
            ByteBuffer readBuffer = ByteBuffer.allocate(5);
            urlRequest.readNew(readBuffer);
            fail("Exception not thrown");
        } catch (IllegalArgumentException e) {
            assertEquals("byteBuffer must be a direct ByteBuffer.",
                    e.getMessage());
        }

        // Finish the request with a direct ByteBuffer.
        callback.setAutoAdvance(true);
        ByteBuffer readBuffer = ByteBuffer.allocateDirect(5);
        urlRequest.readNew(readBuffer);
        callback.blockForDone();
        assertEquals(200, callback.mResponseInfo.getHttpStatusCode());
        assertEquals("GET", callback.mResponseAsString);
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testUnexpectedReads() throws Exception {
        final TestUrlRequestCallback callback = new TestUrlRequestCallback();
        callback.setAutoAdvance(false);
        final UrlRequest urlRequest =
                new UrlRequest.Builder(NativeTestServer.getRedirectURL(), callback,
                                      callback.getExecutor(), mTestFramework.mCronetEngine)
                        .build();

        // Try to read before starting request.
        try {
            callback.startNextRead(urlRequest);
            fail("Exception not thrown");
        } catch (IllegalStateException e) {
            assertEquals("Unexpected read attempt.",
                    e.getMessage());
        }

        // Verify reading right after start throws an assertion. Both must be
        // invoked on the Executor thread, to prevent receiving data until after
        // startNextRead has been invoked.
        Runnable startAndRead = new Runnable() {
            @Override
            public void run() {
                urlRequest.start();
                try {
                    callback.startNextRead(urlRequest);
                    fail("Exception not thrown");
                } catch (IllegalStateException e) {
                    assertEquals("Unexpected read attempt.",
                            e.getMessage());
                }
            }
        };
        callback.getExecutor().execute(startAndRead);
        callback.waitForNextStep();

        assertEquals(callback.mResponseStep, ResponseStep.ON_RECEIVED_REDIRECT);
        // Try to read after the redirect.
        try {
            callback.startNextRead(urlRequest);
            fail("Exception not thrown");
        } catch (IllegalStateException e) {
            assertEquals("Unexpected read attempt.",
                    e.getMessage());
        }
        urlRequest.followRedirect();
        callback.waitForNextStep();

        assertEquals(callback.mResponseStep, ResponseStep.ON_RESPONSE_STARTED);
        assertEquals(200, callback.mResponseInfo.getHttpStatusCode());

        while (!callback.isDone()) {
            Runnable readTwice = new Runnable() {
                @Override
                public void run() {
                    callback.startNextRead(urlRequest);
                    // Try to read again before the last read completes.
                    try {
                        callback.startNextRead(urlRequest);
                        fail("Exception not thrown");
                    } catch (IllegalStateException e) {
                        assertEquals("Unexpected read attempt.",
                                e.getMessage());
                    }
                }
            };
            callback.getExecutor().execute(readTwice);
            callback.waitForNextStep();
        }

        assertEquals(callback.mResponseStep, ResponseStep.ON_SUCCEEDED);
        assertEquals(NativeTestServer.SUCCESS_BODY, callback.mResponseAsString);

        // Try to read after request is complete.
        try {
            callback.startNextRead(urlRequest);
            fail("Exception not thrown");
        } catch (IllegalStateException e) {
            assertEquals("Unexpected read attempt.",
                    e.getMessage());
        }
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testUnexpectedFollowRedirects() throws Exception {
        final TestUrlRequestCallback callback = new TestUrlRequestCallback();
        callback.setAutoAdvance(false);
        final UrlRequest urlRequest =
                new UrlRequest.Builder(NativeTestServer.getRedirectURL(), callback,
                                      callback.getExecutor(), mTestFramework.mCronetEngine)
                        .build();

        // Try to follow a redirect before starting the request.
        try {
            urlRequest.followRedirect();
            fail("Exception not thrown");
        } catch (IllegalStateException e) {
            assertEquals("No redirect to follow.",
                    e.getMessage());
        }

        // Try to follow a redirect just after starting the request. Has to be
        // done on the executor thread to avoid a race.
        Runnable startAndRead = new Runnable() {
            @Override
            public void run() {
                urlRequest.start();
                try {
                    urlRequest.followRedirect();
                    fail("Exception not thrown");
                } catch (IllegalStateException e) {
                    assertEquals("No redirect to follow.",
                            e.getMessage());
                }
            }
        };
        callback.getExecutor().execute(startAndRead);
        callback.waitForNextStep();

        assertEquals(callback.mResponseStep, ResponseStep.ON_RECEIVED_REDIRECT);
        // Try to follow the redirect twice. Second attempt should fail.
        Runnable followRedirectTwice = new Runnable() {
            @Override
            public void run() {
                urlRequest.followRedirect();
                try {
                    urlRequest.followRedirect();
                    fail("Exception not thrown");
                } catch (IllegalStateException e) {
                    assertEquals("No redirect to follow.",
                            e.getMessage());
                }
            }
        };
        callback.getExecutor().execute(followRedirectTwice);
        callback.waitForNextStep();

        assertEquals(callback.mResponseStep, ResponseStep.ON_RESPONSE_STARTED);
        assertEquals(200, callback.mResponseInfo.getHttpStatusCode());

        while (!callback.isDone()) {
            try {
                urlRequest.followRedirect();
                fail("Exception not thrown");
            } catch (IllegalStateException e) {
                assertEquals("No redirect to follow.",
                        e.getMessage());
            }
            callback.startNextRead(urlRequest);
            callback.waitForNextStep();
        }

        assertEquals(callback.mResponseStep, ResponseStep.ON_SUCCEEDED);
        assertEquals(NativeTestServer.SUCCESS_BODY, callback.mResponseAsString);

        // Try to follow redirect after request is complete.
        try {
            urlRequest.followRedirect();
            fail("Exception not thrown");
        } catch (IllegalStateException e) {
            assertEquals("No redirect to follow.",
                    e.getMessage());
        }
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testUploadSetDataProvider() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder builder = new UrlRequest.Builder(NativeTestServer.getEchoBodyURL(),
                callback, callback.getExecutor(), mTestFramework.mCronetEngine);

        try {
            builder.setUploadDataProvider(null, callback.getExecutor());
            fail("Exception not thrown");
        } catch (NullPointerException e) {
            assertEquals("Invalid UploadDataProvider.", e.getMessage());
        }

        TestUploadDataProvider dataProvider = new TestUploadDataProvider(
                TestUploadDataProvider.SuccessCallbackMode.SYNC, callback.getExecutor());
        builder.setUploadDataProvider(dataProvider, callback.getExecutor());
        try {
            builder.build().start();
            fail("Exception not thrown");
        } catch (IllegalArgumentException e) {
            assertEquals("Requests with upload data must have a Content-Type.", e.getMessage());
        }
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testUploadEmptyBodySync() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder builder = new UrlRequest.Builder(NativeTestServer.getEchoBodyURL(),
                callback, callback.getExecutor(), mTestFramework.mCronetEngine);

        TestUploadDataProvider dataProvider = new TestUploadDataProvider(
                TestUploadDataProvider.SuccessCallbackMode.SYNC, callback.getExecutor());
        builder.setUploadDataProvider(dataProvider, callback.getExecutor());
        builder.addHeader("Content-Type", "useless/string");
        builder.build().start();
        callback.blockForDone();

        assertEquals(0, dataProvider.getLength());
        assertEquals(0, dataProvider.getNumReadCalls());
        assertEquals(0, dataProvider.getNumRewindCalls());

        assertEquals(200, callback.mResponseInfo.getHttpStatusCode());
        assertEquals("", callback.mResponseAsString);
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testUploadSync() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder builder = new UrlRequest.Builder(NativeTestServer.getEchoBodyURL(),
                callback, callback.getExecutor(), mTestFramework.mCronetEngine);

        TestUploadDataProvider dataProvider = new TestUploadDataProvider(
                TestUploadDataProvider.SuccessCallbackMode.SYNC, callback.getExecutor());
        dataProvider.addRead("test".getBytes());
        builder.setUploadDataProvider(dataProvider, callback.getExecutor());
        builder.addHeader("Content-Type", "useless/string");
        builder.build().start();
        callback.blockForDone();

        assertEquals(4, dataProvider.getLength());
        assertEquals(1, dataProvider.getNumReadCalls());
        assertEquals(0, dataProvider.getNumRewindCalls());

        assertEquals(200, callback.mResponseInfo.getHttpStatusCode());
        assertEquals("test", callback.mResponseAsString);
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testUploadMultiplePiecesSync() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder builder = new UrlRequest.Builder(NativeTestServer.getEchoBodyURL(),
                callback, callback.getExecutor(), mTestFramework.mCronetEngine);

        TestUploadDataProvider dataProvider = new TestUploadDataProvider(
                TestUploadDataProvider.SuccessCallbackMode.SYNC, callback.getExecutor());
        dataProvider.addRead("Y".getBytes());
        dataProvider.addRead("et ".getBytes());
        dataProvider.addRead("another ".getBytes());
        dataProvider.addRead("test".getBytes());

        builder.setUploadDataProvider(dataProvider, callback.getExecutor());
        builder.addHeader("Content-Type", "useless/string");
        builder.build().start();
        callback.blockForDone();

        assertEquals(16, dataProvider.getLength());
        assertEquals(4, dataProvider.getNumReadCalls());
        assertEquals(0, dataProvider.getNumRewindCalls());

        assertEquals(200, callback.mResponseInfo.getHttpStatusCode());
        assertEquals("Yet another test", callback.mResponseAsString);
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testUploadMultiplePiecesAsync() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder builder = new UrlRequest.Builder(NativeTestServer.getEchoBodyURL(),
                callback, callback.getExecutor(), mTestFramework.mCronetEngine);

        TestUploadDataProvider dataProvider = new TestUploadDataProvider(
                TestUploadDataProvider.SuccessCallbackMode.ASYNC, callback.getExecutor());
        dataProvider.addRead("Y".getBytes());
        dataProvider.addRead("et ".getBytes());
        dataProvider.addRead("another ".getBytes());
        dataProvider.addRead("test".getBytes());

        builder.setUploadDataProvider(dataProvider, callback.getExecutor());
        builder.addHeader("Content-Type", "useless/string");
        builder.build().start();
        callback.blockForDone();

        assertEquals(16, dataProvider.getLength());
        assertEquals(4, dataProvider.getNumReadCalls());
        assertEquals(0, dataProvider.getNumRewindCalls());

        assertEquals(200, callback.mResponseInfo.getHttpStatusCode());
        assertEquals("Yet another test", callback.mResponseAsString);
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testUploadChangesDefaultMethod() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder builder = new UrlRequest.Builder(NativeTestServer.getEchoMethodURL(),
                callback, callback.getExecutor(), mTestFramework.mCronetEngine);

        TestUploadDataProvider dataProvider = new TestUploadDataProvider(
                TestUploadDataProvider.SuccessCallbackMode.SYNC, callback.getExecutor());
        dataProvider.addRead("test".getBytes());
        builder.setUploadDataProvider(dataProvider, callback.getExecutor());
        builder.addHeader("Content-Type", "useless/string");
        builder.build().start();
        callback.blockForDone();

        assertEquals(200, callback.mResponseInfo.getHttpStatusCode());
        assertEquals("POST", callback.mResponseAsString);
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testUploadWithSetMethod() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder builder = new UrlRequest.Builder(NativeTestServer.getEchoMethodURL(),
                callback, callback.getExecutor(), mTestFramework.mCronetEngine);

        final String method = "PUT";
        builder.setHttpMethod(method);

        TestUploadDataProvider dataProvider = new TestUploadDataProvider(
                TestUploadDataProvider.SuccessCallbackMode.SYNC, callback.getExecutor());
        dataProvider.addRead("test".getBytes());
        builder.setUploadDataProvider(dataProvider, callback.getExecutor());
        builder.addHeader("Content-Type", "useless/string");
        builder.build().start();
        callback.blockForDone();

        assertEquals(200, callback.mResponseInfo.getHttpStatusCode());
        assertEquals("PUT", callback.mResponseAsString);
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testUploadRedirectSync() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder builder =
                new UrlRequest.Builder(NativeTestServer.getRedirectToEchoBody(), callback,
                        callback.getExecutor(), mTestFramework.mCronetEngine);

        TestUploadDataProvider dataProvider = new TestUploadDataProvider(
                TestUploadDataProvider.SuccessCallbackMode.SYNC, callback.getExecutor());
        dataProvider.addRead("test".getBytes());
        builder.setUploadDataProvider(dataProvider, callback.getExecutor());
        builder.addHeader("Content-Type", "useless/string");
        builder.build().start();
        callback.blockForDone();

        // 1 read call before the rewind, 1 after.
        assertEquals(2, dataProvider.getNumReadCalls());
        assertEquals(1, dataProvider.getNumRewindCalls());

        assertEquals(200, callback.mResponseInfo.getHttpStatusCode());
        assertEquals("test", callback.mResponseAsString);
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testUploadRedirectAsync() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder builder =
                new UrlRequest.Builder(NativeTestServer.getRedirectToEchoBody(), callback,
                        callback.getExecutor(), mTestFramework.mCronetEngine);

        TestUploadDataProvider dataProvider = new TestUploadDataProvider(
                TestUploadDataProvider.SuccessCallbackMode.ASYNC, callback.getExecutor());
        dataProvider.addRead("test".getBytes());
        builder.setUploadDataProvider(dataProvider, callback.getExecutor());
        builder.addHeader("Content-Type", "useless/string");
        builder.build().start();
        callback.blockForDone();

        // 1 read call before the rewind, 1 after.
        assertEquals(2, dataProvider.getNumReadCalls());
        assertEquals(1, dataProvider.getNumRewindCalls());

        assertEquals(200, callback.mResponseInfo.getHttpStatusCode());
        assertEquals("test", callback.mResponseAsString);
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testUploadReadFailSync() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder builder = new UrlRequest.Builder(NativeTestServer.getEchoBodyURL(),
                callback, callback.getExecutor(), mTestFramework.mCronetEngine);

        TestUploadDataProvider dataProvider = new TestUploadDataProvider(
                TestUploadDataProvider.SuccessCallbackMode.SYNC, callback.getExecutor());
        dataProvider.setReadFailure(0, TestUploadDataProvider.FailMode.CALLBACK_SYNC);
        // This will never be read, but if the length is 0, read may never be
        // called.
        dataProvider.addRead("test".getBytes());
        builder.setUploadDataProvider(dataProvider, callback.getExecutor());
        builder.addHeader("Content-Type", "useless/string");
        builder.build().start();
        callback.blockForDone();

        assertEquals(1, dataProvider.getNumReadCalls());
        assertEquals(0, dataProvider.getNumRewindCalls());

        assertEquals("Exception received from UploadDataProvider", callback.mError.getMessage());
        assertEquals("Sync read failure", callback.mError.getCause().getMessage());
        assertEquals(null, callback.mResponseInfo);
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testUploadReadFailAsync() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder builder = new UrlRequest.Builder(NativeTestServer.getEchoBodyURL(),
                callback, callback.getExecutor(), mTestFramework.mCronetEngine);

        TestUploadDataProvider dataProvider = new TestUploadDataProvider(
                TestUploadDataProvider.SuccessCallbackMode.SYNC, callback.getExecutor());
        dataProvider.setReadFailure(0, TestUploadDataProvider.FailMode.CALLBACK_ASYNC);
        // This will never be read, but if the length is 0, read may never be
        // called.
        dataProvider.addRead("test".getBytes());
        builder.setUploadDataProvider(dataProvider, callback.getExecutor());
        builder.addHeader("Content-Type", "useless/string");
        builder.build().start();
        callback.blockForDone();

        assertEquals(1, dataProvider.getNumReadCalls());
        assertEquals(0, dataProvider.getNumRewindCalls());

        assertEquals("Exception received from UploadDataProvider", callback.mError.getMessage());
        assertEquals("Async read failure", callback.mError.getCause().getMessage());
        assertEquals(null, callback.mResponseInfo);
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testUploadReadFailThrown() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder builder = new UrlRequest.Builder(NativeTestServer.getEchoBodyURL(),
                callback, callback.getExecutor(), mTestFramework.mCronetEngine);

        TestUploadDataProvider dataProvider = new TestUploadDataProvider(
                TestUploadDataProvider.SuccessCallbackMode.SYNC, callback.getExecutor());
        dataProvider.setReadFailure(0, TestUploadDataProvider.FailMode.THROWN);
        // This will never be read, but if the length is 0, read may never be
        // called.
        dataProvider.addRead("test".getBytes());
        builder.setUploadDataProvider(dataProvider, callback.getExecutor());
        builder.addHeader("Content-Type", "useless/string");
        builder.build().start();
        callback.blockForDone();

        assertEquals(1, dataProvider.getNumReadCalls());
        assertEquals(0, dataProvider.getNumRewindCalls());

        assertEquals("Exception received from UploadDataProvider", callback.mError.getMessage());
        assertEquals("Thrown read failure", callback.mError.getCause().getMessage());
        assertEquals(null, callback.mResponseInfo);
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testUploadRewindFailSync() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder builder =
                new UrlRequest.Builder(NativeTestServer.getRedirectToEchoBody(), callback,
                        callback.getExecutor(), mTestFramework.mCronetEngine);

        TestUploadDataProvider dataProvider = new TestUploadDataProvider(
                TestUploadDataProvider.SuccessCallbackMode.SYNC, callback.getExecutor());
        dataProvider.setRewindFailure(TestUploadDataProvider.FailMode.CALLBACK_SYNC);
        dataProvider.addRead("test".getBytes());
        builder.setUploadDataProvider(dataProvider, callback.getExecutor());
        builder.addHeader("Content-Type", "useless/string");
        builder.build().start();
        callback.blockForDone();

        assertEquals(1, dataProvider.getNumReadCalls());
        assertEquals(1, dataProvider.getNumRewindCalls());

        assertEquals("Exception received from UploadDataProvider", callback.mError.getMessage());
        assertEquals("Sync rewind failure", callback.mError.getCause().getMessage());
        assertEquals(null, callback.mResponseInfo);
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testUploadRewindFailAsync() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder builder =
                new UrlRequest.Builder(NativeTestServer.getRedirectToEchoBody(), callback,
                        callback.getExecutor(), mTestFramework.mCronetEngine);

        TestUploadDataProvider dataProvider = new TestUploadDataProvider(
                TestUploadDataProvider.SuccessCallbackMode.ASYNC, callback.getExecutor());
        dataProvider.setRewindFailure(TestUploadDataProvider.FailMode.CALLBACK_ASYNC);
        dataProvider.addRead("test".getBytes());
        builder.setUploadDataProvider(dataProvider, callback.getExecutor());
        builder.addHeader("Content-Type", "useless/string");
        builder.build().start();
        callback.blockForDone();

        assertEquals(1, dataProvider.getNumReadCalls());
        assertEquals(1, dataProvider.getNumRewindCalls());

        assertEquals("Exception received from UploadDataProvider", callback.mError.getMessage());
        assertEquals("Async rewind failure", callback.mError.getCause().getMessage());
        assertEquals(null, callback.mResponseInfo);
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testUploadRewindFailThrown() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder builder =
                new UrlRequest.Builder(NativeTestServer.getRedirectToEchoBody(), callback,
                        callback.getExecutor(), mTestFramework.mCronetEngine);

        TestUploadDataProvider dataProvider = new TestUploadDataProvider(
                TestUploadDataProvider.SuccessCallbackMode.SYNC, callback.getExecutor());
        dataProvider.setRewindFailure(TestUploadDataProvider.FailMode.THROWN);
        dataProvider.addRead("test".getBytes());
        builder.setUploadDataProvider(dataProvider, callback.getExecutor());
        builder.addHeader("Content-Type", "useless/string");
        builder.build().start();
        callback.blockForDone();

        assertEquals(1, dataProvider.getNumReadCalls());
        assertEquals(1, dataProvider.getNumRewindCalls());

        assertEquals("Exception received from UploadDataProvider", callback.mError.getMessage());
        assertEquals("Thrown rewind failure", callback.mError.getCause().getMessage());
        assertEquals(null, callback.mResponseInfo);
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testUploadChunked() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder builder = new UrlRequest.Builder(NativeTestServer.getEchoBodyURL(),
                callback, callback.getExecutor(), mTestFramework.mCronetEngine);

        TestUploadDataProvider dataProvider = new TestUploadDataProvider(
                TestUploadDataProvider.SuccessCallbackMode.SYNC, callback.getExecutor());
        dataProvider.addRead("test hello".getBytes());
        dataProvider.setChunked(true);
        builder.setUploadDataProvider(dataProvider, callback.getExecutor());
        builder.addHeader("Content-Type", "useless/string");

        assertEquals(-1, dataProvider.getLength());

        builder.build().start();
        callback.blockForDone();

        // 1 read call for one data chunk.
        assertEquals(1, dataProvider.getNumReadCalls());
        assertEquals("test hello", callback.mResponseAsString);
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testUploadChunkedLastReadZeroLengthBody() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder builder = new UrlRequest.Builder(NativeTestServer.getEchoBodyURL(),
                callback, callback.getExecutor(), mTestFramework.mCronetEngine);

        TestUploadDataProvider dataProvider = new TestUploadDataProvider(
                TestUploadDataProvider.SuccessCallbackMode.SYNC, callback.getExecutor());
        // Add 3 reads. The last read has a 0-length body.
        dataProvider.addRead("hello there".getBytes());
        dataProvider.addRead("!".getBytes());
        dataProvider.addRead("".getBytes());
        dataProvider.setChunked(true);
        builder.setUploadDataProvider(dataProvider, callback.getExecutor());
        builder.addHeader("Content-Type", "useless/string");

        assertEquals(-1, dataProvider.getLength());

        builder.build().start();
        callback.blockForDone();

        // 2 read call for the first two data chunks, and 1 for final chunk.
        assertEquals(3, dataProvider.getNumReadCalls());
        assertEquals("hello there!", callback.mResponseAsString);
    }

    // Test where an upload fails without ever initializing the
    // UploadDataStream, because it can't connect to the server.
    @SmallTest
    @Feature({"Cronet"})
    public void testUploadFailsWithoutInitializingStream() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder builder = new UrlRequest.Builder(NativeTestServer.getEchoBodyURL(),
                callback, callback.getExecutor(), mTestFramework.mCronetEngine);
        // Shut down the test server, so connecting to it fails. Note that
        // calling shutdown again during teardown is safe.
        NativeTestServer.shutdownNativeTestServer();

        TestUploadDataProvider dataProvider = new TestUploadDataProvider(
                TestUploadDataProvider.SuccessCallbackMode.SYNC, callback.getExecutor());
        dataProvider.addRead("test".getBytes());
        builder.setUploadDataProvider(dataProvider, callback.getExecutor());
        builder.addHeader("Content-Type", "useless/string");
        builder.build().start();
        callback.blockForDone();

        assertNull(callback.mResponseInfo);
        assertEquals("Exception in CronetUrlRequest: net::ERR_CONNECTION_REFUSED",
                callback.mError.getMessage());
    }

    private void throwOrCancel(FailureType failureType, ResponseStep failureStep,
            boolean expectResponseInfo, boolean expectError) {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        callback.setFailure(failureType, failureStep);
        UrlRequest.Builder builder = new UrlRequest.Builder(NativeTestServer.getRedirectURL(),
                callback, callback.getExecutor(), mTestFramework.mCronetEngine);
        UrlRequest urlRequest = builder.build();
        urlRequest.start();
        callback.blockForDone();
        assertEquals(1, callback.mRedirectCount);
        assertEquals(callback.mResponseStep, failureStep);
        assertTrue(urlRequest.isDone());
        assertEquals(expectResponseInfo, callback.mResponseInfo != null);
        assertEquals(expectError, callback.mError != null);
        assertEquals(expectError, callback.mOnErrorCalled);
        assertEquals(failureType == FailureType.CANCEL_SYNC
                        || failureType == FailureType.CANCEL_ASYNC
                        || failureType == FailureType.CANCEL_ASYNC_WITHOUT_PAUSE,
                callback.mOnCanceledCalled);
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testFailures() throws Exception {
        throwOrCancel(FailureType.CANCEL_SYNC, ResponseStep.ON_RECEIVED_REDIRECT,
                false, false);
        throwOrCancel(FailureType.CANCEL_ASYNC, ResponseStep.ON_RECEIVED_REDIRECT,
                false, false);
        throwOrCancel(FailureType.CANCEL_ASYNC_WITHOUT_PAUSE, ResponseStep.ON_RECEIVED_REDIRECT,
                false, false);
        throwOrCancel(FailureType.THROW_SYNC, ResponseStep.ON_RECEIVED_REDIRECT,
                false, true);

        throwOrCancel(FailureType.CANCEL_SYNC, ResponseStep.ON_RESPONSE_STARTED,
                true, false);
        throwOrCancel(FailureType.CANCEL_ASYNC, ResponseStep.ON_RESPONSE_STARTED,
                true, false);
        throwOrCancel(FailureType.CANCEL_ASYNC_WITHOUT_PAUSE, ResponseStep.ON_RESPONSE_STARTED,
                true, false);
        throwOrCancel(FailureType.THROW_SYNC, ResponseStep.ON_RESPONSE_STARTED,
                true, true);

        throwOrCancel(FailureType.CANCEL_SYNC, ResponseStep.ON_READ_COMPLETED,
                true, false);
        throwOrCancel(FailureType.CANCEL_ASYNC, ResponseStep.ON_READ_COMPLETED,
                true, false);
        throwOrCancel(FailureType.CANCEL_ASYNC_WITHOUT_PAUSE, ResponseStep.ON_READ_COMPLETED,
                true, false);
        throwOrCancel(FailureType.THROW_SYNC, ResponseStep.ON_READ_COMPLETED,
                true, true);
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testThrowON_SUCCEEDED() {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        callback.setFailure(FailureType.THROW_SYNC, ResponseStep.ON_SUCCEEDED);
        UrlRequest.Builder builder = new UrlRequest.Builder(NativeTestServer.getRedirectURL(),
                callback, callback.getExecutor(), mTestFramework.mCronetEngine);
        UrlRequest urlRequest = builder.build();
        urlRequest.start();
        callback.blockForDone();
        assertEquals(1, callback.mRedirectCount);
        assertEquals(callback.mResponseStep, ResponseStep.ON_SUCCEEDED);
        assertTrue(urlRequest.isDone());
        assertNotNull(callback.mResponseInfo);
        assertNull(callback.mError);
        assertFalse(callback.mOnErrorCalled);
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testExecutorShutdown() {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();

        callback.setAutoAdvance(false);
        UrlRequest.Builder builder = new UrlRequest.Builder(NativeTestServer.getEchoBodyURL(),
                callback, callback.getExecutor(), mTestFramework.mCronetEngine);
        CronetUrlRequest urlRequest = (CronetUrlRequest) builder.build();
        urlRequest.start();
        callback.waitForNextStep();
        assertFalse(callback.isDone());
        assertFalse(urlRequest.isDone());

        final ConditionVariable requestDestroyed = new ConditionVariable(false);
        urlRequest.setOnDestroyedCallbackForTests(new Runnable() {
            @Override
            public void run() {
                requestDestroyed.open();
            }
        });

        // Shutdown the executor, so posting the task will throw an exception.
        callback.shutdownExecutor();
        ByteBuffer readBuffer = ByteBuffer.allocateDirect(5);
        urlRequest.readNew(readBuffer);
        // Callback will never be called again because executor is shutdown,
        // but request will be destroyed from network thread.
        requestDestroyed.block();

        assertFalse(callback.isDone());
        assertTrue(urlRequest.isDone());
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testUploadExecutorShutdown() throws Exception {
        class HangingUploadDataProvider extends UploadDataProvider {
            UploadDataSink mUploadDataSink;
            ByteBuffer mByteBuffer;
            ConditionVariable mReadCalled = new ConditionVariable(false);

            @Override
            public long getLength() {
                return 69;
            }

            @Override
            public void read(final UploadDataSink uploadDataSink,
                             final ByteBuffer byteBuffer) {
                mUploadDataSink = uploadDataSink;
                mByteBuffer = byteBuffer;
                mReadCalled.open();
            }

            @Override
            public void rewind(final UploadDataSink uploadDataSink) {
            }
        }

        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder builder = new UrlRequest.Builder(NativeTestServer.getEchoBodyURL(),
                callback, callback.getExecutor(), mTestFramework.mCronetEngine);

        ExecutorService uploadExecutor = Executors.newSingleThreadExecutor();
        HangingUploadDataProvider dataProvider = new HangingUploadDataProvider();
        builder.setUploadDataProvider(dataProvider, uploadExecutor);
        builder.addHeader("Content-Type", "useless/string");
        UrlRequest urlRequest = builder.build();
        urlRequest.start();
        // Wait for read to be called on executor.
        dataProvider.mReadCalled.block();
        // Shutdown the executor, so posting next task will throw an exception.
        uploadExecutor.shutdown();
        // Continue the upload.
        dataProvider.mByteBuffer.putInt(42);
        dataProvider.mUploadDataSink.onReadSucceeded(false);
        // Callback.onFailed will be called on request executor even though upload
        // executor is shutdown.
        callback.blockForDone();
        assertTrue(callback.isDone());
        assertTrue(callback.mOnErrorCalled);
        assertEquals("Exception received from UploadDataProvider", callback.mError.getMessage());
        assertTrue(urlRequest.isDone());
    }

    // Returns the contents of byteBuffer, from its position() to its limit(),
    // as a String. Does not modify byteBuffer's position().
    private String bufferContentsToString(ByteBuffer byteBuffer, int start,
            int end) {
        // Use a duplicate to avoid modifying byteBuffer.
        ByteBuffer duplicate = byteBuffer.duplicate();
        duplicate.position(start);
        duplicate.limit(end);
        byte[] contents = new byte[duplicate.remaining()];
        duplicate.get(contents);
        return new String(contents);
    }
}
