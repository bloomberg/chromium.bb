// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import android.os.ConditionVariable;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.test.util.Feature;
import org.chromium.net.TestUrlRequestListener.FailureType;
import org.chromium.net.TestUrlRequestListener.ResponseStep;
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

    private CronetTestActivity mActivity;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mActivity = launchCronetTestApp();
        assertTrue(NativeTestServer.startNativeTestServer(
                getInstrumentation().getTargetContext()));
        // Add url interceptors after native application context is initialized.
        MockUrlRequestJobFactory.setUp();
    }

    @Override
    protected void tearDown() throws Exception {
        NativeTestServer.shutdownNativeTestServer();
        mActivity.mCronetEngine.shutdown();
        super.tearDown();
    }

    private TestUrlRequestListener startAndWaitForComplete(String url)
            throws Exception {
        TestUrlRequestListener listener = new TestUrlRequestListener();
        // Create request.
        UrlRequest.Builder builder = new UrlRequest.Builder(
                url, listener, listener.getExecutor(), mActivity.mCronetEngine);
        UrlRequest urlRequest = builder.build();
        urlRequest.start();
        listener.blockForDone();
        assertTrue(urlRequest.isDone());
        return listener;
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
        TestUrlRequestListener listener = new TestUrlRequestListener();
        try {
            new UrlRequest.Builder(null, listener, listener.getExecutor(), mActivity.mCronetEngine);
            fail("URL not null-checked");
        } catch (NullPointerException e) {
            assertEquals("URL is required.", e.getMessage());
        }
        try {
            new UrlRequest.Builder(NativeTestServer.getRedirectURL(), null, listener.getExecutor(),
                    mActivity.mCronetEngine);
            fail("Listener not null-checked");
        } catch (NullPointerException e) {
            assertEquals("Listener is required.", e.getMessage());
        }
        try {
            new UrlRequest.Builder(
                    NativeTestServer.getRedirectURL(), listener, null, mActivity.mCronetEngine);
            fail("Executor not null-checked");
        } catch (NullPointerException e) {
            assertEquals("Executor is required.", e.getMessage());
        }
        try {
            new UrlRequest.Builder(
                    NativeTestServer.getRedirectURL(), listener, listener.getExecutor(), null);
            fail("CronetEngine not null-checked");
        } catch (NullPointerException e) {
            assertEquals("CronetEngine is required.", e.getMessage());
        }
        // Verify successful creation doesn't throw.
        new UrlRequest.Builder(NativeTestServer.getRedirectURL(), listener, listener.getExecutor(),
                mActivity.mCronetEngine);
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testSimpleGet() throws Exception {
        String url = NativeTestServer.getEchoMethodURL();
        TestUrlRequestListener listener = startAndWaitForComplete(url);
        assertEquals(200, listener.mResponseInfo.getHttpStatusCode());
        // Default method is 'GET'.
        assertEquals("GET", listener.mResponseAsString);
        assertEquals(0, listener.mRedirectCount);
        assertEquals(listener.mResponseStep, ResponseStep.ON_SUCCEEDED);
        assertEquals(String.format("UrlResponseInfo[%s]: urlChain = [%s], httpStatus = 200 OK, "
                                     + "headers = [Connection=close, Content-Length=3, "
                                     + "Content-Type=text/plain], wasCached = false, "
                                     + "negotiatedProtocol = unknown, proxyServer= :0, "
                                     + "receivedBytesCount = 86",
                             url, url),
                listener.mResponseInfo.toString());
        checkResponseInfo(listener.mResponseInfo,
                NativeTestServer.getEchoMethodURL(), 200, "OK");
    }

    /**
     * Tests a redirect by running it step-by-step. Also tests that delaying a
     * request works as expected. To make sure there are no unexpected pending
     * messages, does a GET between UrlRequestListener callbacks.
     */
    @SmallTest
    @Feature({"Cronet"})
    public void testRedirectAsync() throws Exception {
        // Start the request and wait to see the redirect.
        TestUrlRequestListener listener = new TestUrlRequestListener();
        listener.setAutoAdvance(false);
        UrlRequest.Builder builder = new UrlRequest.Builder(NativeTestServer.getRedirectURL(),
                listener, listener.getExecutor(), mActivity.mCronetEngine);
        UrlRequest urlRequest = builder.build();
        urlRequest.start();
        listener.waitForNextStep();

        // Check the redirect.
        assertEquals(ResponseStep.ON_RECEIVED_REDIRECT, listener.mResponseStep);
        assertEquals(1, listener.mRedirectResponseInfoList.size());
        checkResponseInfo(listener.mRedirectResponseInfoList.get(0),
                NativeTestServer.getRedirectURL(), 302, "Found");
        assertEquals(1, listener.mRedirectResponseInfoList.get(0).getUrlChain().size());
        assertEquals(NativeTestServer.getSuccessURL(), listener.mRedirectUrlList.get(0));
        checkResponseInfoHeader(listener.mRedirectResponseInfoList.get(0),
                "redirect-header", "header-value");

        assertEquals(String.format("UrlResponseInfo[%s]: urlChain = [%s], httpStatus = 302 Found, "
                                     + "headers = [Location=/success.txt, "
                                     + "redirect-header=header-value], wasCached = false, "
                                     + "negotiatedProtocol = unknown, proxyServer= :0, "
                                     + "receivedBytesCount = 74",
                             NativeTestServer.getRedirectURL(), NativeTestServer.getRedirectURL()),
                listener.mRedirectResponseInfoList.get(0).toString());

        // Wait for an unrelated request to finish. The request should not
        // advance until followRedirect is invoked.
        testSimpleGet();
        assertEquals(ResponseStep.ON_RECEIVED_REDIRECT, listener.mResponseStep);
        assertEquals(1, listener.mRedirectResponseInfoList.size());

        // Follow the redirect and wait for the next set of headers.
        urlRequest.followRedirect();
        listener.waitForNextStep();

        assertEquals(ResponseStep.ON_RESPONSE_STARTED, listener.mResponseStep);
        assertEquals(1, listener.mRedirectResponseInfoList.size());
        assertEquals(200, listener.mResponseInfo.getHttpStatusCode());
        checkResponseInfo(listener.mResponseInfo, NativeTestServer.getSuccessURL(), 200, "OK");
        assertEquals(2, listener.mResponseInfo.getUrlChain().size());
        assertEquals(
                NativeTestServer.getRedirectURL(), listener.mResponseInfo.getUrlChain().get(0));
        assertEquals(NativeTestServer.getSuccessURL(), listener.mResponseInfo.getUrlChain().get(1));

        // Wait for an unrelated request to finish. The request should not
        // advance until read is invoked.
        testSimpleGet();
        assertEquals(ResponseStep.ON_RESPONSE_STARTED, listener.mResponseStep);

        // One read should get all the characters, but best not to depend on
        // how much is actually read from the socket at once.
        while (!listener.isDone()) {
            listener.startNextRead(urlRequest);
            listener.waitForNextStep();
            String response = listener.mResponseAsString;
            ResponseStep step = listener.mResponseStep;
            if (!listener.isDone()) {
                assertEquals(ResponseStep.ON_READ_COMPLETED, step);
            }
            // Should not receive any messages while waiting for another get,
            // as the next read has not been started.
            testSimpleGet();
            assertEquals(response, listener.mResponseAsString);
            assertEquals(step, listener.mResponseStep);
        }
        assertEquals(ResponseStep.ON_SUCCEEDED, listener.mResponseStep);
        assertEquals(NativeTestServer.SUCCESS_BODY, listener.mResponseAsString);

        assertEquals(String.format("UrlResponseInfo[%s]: urlChain = [%s, %s], httpStatus = 200 OK, "
                                     + "headers = [Content-Type=text/plain, "
                                     + "Access-Control-Allow-Origin=*, header-name=header-value, "
                                     + "multi-header-name=header-value1, "
                                     + "multi-header-name=header-value2], wasCached = false, "
                                     + "negotiatedProtocol = unknown, proxyServer= :0, "
                                     + "receivedBytesCount = 260",
                             NativeTestServer.getSuccessURL(), NativeTestServer.getRedirectURL(),
                             NativeTestServer.getSuccessURL()),
                listener.mResponseInfo.toString());

        // Make sure there are no other pending messages, which would trigger
        // asserts in TestURLRequestListener.
        testSimpleGet();
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testNotFound() throws Exception {
        String url = NativeTestServer.getFileURL("/notfound.html");
        TestUrlRequestListener listener = startAndWaitForComplete(url);
        checkResponseInfo(listener.mResponseInfo, url, 404, "Not Found");
        assertEquals(
                "<!DOCTYPE html>\n<html>\n<head>\n<title>Not found</title>\n"
                + "<p>Test page loaded.</p>\n</head>\n</html>\n",
                listener.mResponseAsString);
        assertEquals(0, listener.mRedirectCount);
        assertEquals(listener.mResponseStep, ResponseStep.ON_SUCCEEDED);
    }

    // Checks that UrlRequestListener.onFailed is only called once in the case
    // of ERR_CONTENT_LENGTH_MISMATCH, which has an unusual failure path.
    // See http://crbug.com/468803.
    @SmallTest
    @Feature({"Cronet"})
    public void testContentLengthMismatchFailsOnce() throws Exception {
        String url = NativeTestServer.getFileURL(
                "/content_length_mismatch.html");
        TestUrlRequestListener listener = startAndWaitForComplete(url);
        assertEquals(200, listener.mResponseInfo.getHttpStatusCode());
        // The entire response body will be read before the error is returned.
        // This is because the network stack returns data as it's read from the
        // socket, and the socket close message which tiggers the error will
        // only be passed along after all data has been read.
        assertEquals("Response that lies about content length.",
                listener.mResponseAsString);
        assertNotNull(listener.mError);
        assertEquals(
                "Exception in CronetUrlRequest: net::ERR_CONTENT_LENGTH_MISMATCH",
                listener.mError.getMessage());
        // Wait for a couple round trips to make sure there are no pending
        // onFailed messages. This test relies on checks in
        // TestUrlRequestListener catching a second onFailed call.
        testSimpleGet();
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testSetHttpMethod() throws Exception {
        TestUrlRequestListener listener = new TestUrlRequestListener();
        String methodName = "HEAD";
        UrlRequest.Builder builder = new UrlRequest.Builder(NativeTestServer.getEchoMethodURL(),
                listener, listener.getExecutor(), mActivity.mCronetEngine);
        // Try to set 'null' method.
        try {
            builder.setHttpMethod(null);
            fail("Exception not thrown");
        } catch (NullPointerException e) {
            assertEquals("Method is required.", e.getMessage());
        }

        builder.setHttpMethod(methodName);
        builder.build().start();
        listener.blockForDone();
        assertEquals(200, listener.mResponseInfo.getHttpStatusCode());
        assertEquals(0, listener.mHttpResponseDataLength);
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testBadMethod() throws Exception {
        TestUrlRequestListener listener = new TestUrlRequestListener();
        UrlRequest.Builder builder = new UrlRequest.Builder(
                TEST_URL, listener, listener.getExecutor(), mActivity.mCronetEngine);
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
        TestUrlRequestListener listener = new TestUrlRequestListener();
        UrlRequest.Builder builder = new UrlRequest.Builder(
                TEST_URL, listener, listener.getExecutor(), mActivity.mCronetEngine);
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
        TestUrlRequestListener listener = new TestUrlRequestListener();
        UrlRequest.Builder builder = new UrlRequest.Builder(
                TEST_URL, listener, listener.getExecutor(), mActivity.mCronetEngine);
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
        TestUrlRequestListener listener = new TestUrlRequestListener();
        String headerName = "header-name";
        String headerValue = "header-value";
        UrlRequest.Builder builder =
                new UrlRequest.Builder(NativeTestServer.getEchoHeaderURL(headerName), listener,
                        listener.getExecutor(), mActivity.mCronetEngine);

        builder.addHeader(headerName, headerValue);
        builder.build().start();
        listener.blockForDone();
        assertEquals(200, listener.mResponseInfo.getHttpStatusCode());
        assertEquals(headerValue, listener.mResponseAsString);
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testMultiRequestHeaders() throws Exception {
        TestUrlRequestListener listener = new TestUrlRequestListener();
        String headerName = "header-name";
        String headerValue1 = "header-value1";
        String headerValue2 = "header-value2";
        UrlRequest.Builder builder = new UrlRequest.Builder(NativeTestServer.getEchoAllHeadersURL(),
                listener, listener.getExecutor(), mActivity.mCronetEngine);
        builder.addHeader(headerName, headerValue1);
        builder.addHeader(headerName, headerValue2);
        builder.build().start();
        listener.blockForDone();
        assertEquals(200, listener.mResponseInfo.getHttpStatusCode());
        String headers = listener.mResponseAsString;
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
        TestUrlRequestListener listener = new TestUrlRequestListener();
        String userAgentName = "User-Agent";
        String userAgentValue = "User-Agent-Value";
        UrlRequest.Builder builder =
                new UrlRequest.Builder(NativeTestServer.getEchoHeaderURL(userAgentName), listener,
                        listener.getExecutor(), mActivity.mCronetEngine);
        builder.addHeader(userAgentName, userAgentValue);
        builder.build().start();
        listener.blockForDone();
        assertEquals(200, listener.mResponseInfo.getHttpStatusCode());
        assertEquals(userAgentValue, listener.mResponseAsString);
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testDefaultUserAgent() throws Exception {
        TestUrlRequestListener listener = new TestUrlRequestListener();
        String headerName = "User-Agent";
        UrlRequest.Builder builder =
                new UrlRequest.Builder(NativeTestServer.getEchoHeaderURL(headerName), listener,
                        listener.getExecutor(), mActivity.mCronetEngine);
        builder.build().start();
        listener.blockForDone();
        assertEquals(200, listener.mResponseInfo.getHttpStatusCode());
        assertTrue("Default User-Agent should contain Cronet/n.n.n.n but is "
                           + listener.mResponseAsString,
                   Pattern.matches(".+Cronet/\\d+\\.\\d+\\.\\d+\\.\\d+.+",
                           listener.mResponseAsString));
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testMockSuccess() throws Exception {
        TestUrlRequestListener listener = startAndWaitForComplete(NativeTestServer.getSuccessURL());
        assertEquals(200, listener.mResponseInfo.getHttpStatusCode());
        assertEquals(0, listener.mRedirectResponseInfoList.size());
        assertTrue(listener.mHttpResponseDataLength != 0);
        assertEquals(listener.mResponseStep, ResponseStep.ON_SUCCEEDED);
        Map<String, List<String>> responseHeaders =
                listener.mResponseInfo.getAllHeaders();
        assertEquals("header-value", responseHeaders.get("header-name").get(0));
        List<String> multiHeader = responseHeaders.get("multi-header-name");
        assertEquals(2, multiHeader.size());
        assertEquals("header-value1", multiHeader.get(0));
        assertEquals("header-value2", multiHeader.get(1));
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testResponseHeadersList() throws Exception {
        TestUrlRequestListener listener = startAndWaitForComplete(NativeTestServer.getSuccessURL());
        assertEquals(200, listener.mResponseInfo.getHttpStatusCode());
        List<Map.Entry<String, String>> responseHeaders =
                listener.mResponseInfo.getAllHeadersAsList();
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
        TestUrlRequestListener listener =
                startAndWaitForComplete(NativeTestServer.getMultiRedirectURL());
        UrlResponseInfo mResponseInfo = listener.mResponseInfo;
        assertEquals(2, listener.mRedirectCount);
        assertEquals(200, mResponseInfo.getHttpStatusCode());
        assertEquals(2, listener.mRedirectResponseInfoList.size());

        // Check first redirect (multiredirect.html -> redirect.html)
        UrlResponseInfo firstRedirectResponseInfo = listener.mRedirectResponseInfoList.get(0);
        assertEquals(1, firstRedirectResponseInfo.getUrlChain().size());
        assertEquals(NativeTestServer.getMultiRedirectURL(),
                firstRedirectResponseInfo.getUrlChain().get(0));
        checkResponseInfo(
                firstRedirectResponseInfo, NativeTestServer.getMultiRedirectURL(), 302, "Found");
        checkResponseInfoHeader(firstRedirectResponseInfo,
                "redirect-header0", "header-value");
        assertEquals(77, firstRedirectResponseInfo.getReceivedBytesCount());

        // Check second redirect (redirect.html -> success.txt)
        UrlResponseInfo secondRedirectResponseInfo = listener.mRedirectResponseInfoList.get(1);
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
        assertTrue(listener.mHttpResponseDataLength != 0);
        assertEquals(2, listener.mRedirectCount);
        assertEquals(listener.mResponseStep, ResponseStep.ON_SUCCEEDED);
        assertEquals(337, mResponseInfo.getReceivedBytesCount());
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testMockNotFound() throws Exception {
        TestUrlRequestListener listener =
                startAndWaitForComplete(NativeTestServer.getNotFoundURL());
        assertEquals(404, listener.mResponseInfo.getHttpStatusCode());
        assertEquals(121, listener.mResponseInfo.getReceivedBytesCount());
        assertTrue(listener.mHttpResponseDataLength != 0);
        assertEquals(0, listener.mRedirectCount);
        assertFalse(listener.mOnErrorCalled);
        assertEquals(listener.mResponseStep, ResponseStep.ON_SUCCEEDED);
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testMockStartAsyncError() throws Exception {
        final int arbitraryNetError = -3;
        TestUrlRequestListener listener =
                startAndWaitForComplete(MockUrlRequestJobFactory.getMockUrlWithFailure(
                        FailurePhase.START, arbitraryNetError));
        assertNull(listener.mResponseInfo);
        assertNotNull(listener.mError);
        assertEquals(arbitraryNetError, listener.mError.netError());
        assertEquals(0, listener.mRedirectCount);
        assertTrue(listener.mOnErrorCalled);
        assertEquals(listener.mResponseStep, ResponseStep.NOTHING);
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testMockReadDataSyncError() throws Exception {
        final int arbitraryNetError = -4;
        TestUrlRequestListener listener =
                startAndWaitForComplete(MockUrlRequestJobFactory.getMockUrlWithFailure(
                        FailurePhase.READ_SYNC, arbitraryNetError));
        assertEquals(200, listener.mResponseInfo.getHttpStatusCode());
        assertEquals(0, listener.mResponseInfo.getReceivedBytesCount());
        assertNotNull(listener.mError);
        assertEquals(arbitraryNetError, listener.mError.netError());
        assertEquals(0, listener.mRedirectCount);
        assertTrue(listener.mOnErrorCalled);
        assertEquals(listener.mResponseStep, ResponseStep.ON_RESPONSE_STARTED);
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testMockReadDataAsyncError() throws Exception {
        final int arbitraryNetError = -5;
        TestUrlRequestListener listener =
                startAndWaitForComplete(MockUrlRequestJobFactory.getMockUrlWithFailure(
                        FailurePhase.READ_ASYNC, arbitraryNetError));
        assertEquals(200, listener.mResponseInfo.getHttpStatusCode());
        assertEquals(0, listener.mResponseInfo.getReceivedBytesCount());
        assertNotNull(listener.mError);
        assertEquals(arbitraryNetError, listener.mError.netError());
        assertEquals(0, listener.mRedirectCount);
        assertTrue(listener.mOnErrorCalled);
        assertEquals(listener.mResponseStep, ResponseStep.ON_RESPONSE_STARTED);
    }

    /**
     * Tests that an SSL cert error will be reported via {@link UrlRequest#onFailed}.
     */
    @SmallTest
    @Feature({"Cronet"})
    public void testMockSSLCertificateError() throws Exception {
        TestUrlRequestListener listener = startAndWaitForComplete(
                MockUrlRequestJobFactory.getMockUrlForSSLCertificateError());
        assertNull(listener.mResponseInfo);
        assertNotNull(listener.mError);
        assertTrue(listener.mOnErrorCalled);
        assertEquals(-201, listener.mError.netError());
        assertEquals("Exception in CronetUrlRequest: net::ERR_CERT_DATE_INVALID",
                listener.mError.getMessage());
        assertEquals(listener.mResponseStep, ResponseStep.NOTHING);
    }

    /**
     * Checks that the buffer is updated correctly, when starting at an offset,
     * when using legacy read() API.
     */
    @SmallTest
    @Feature({"Cronet"})
    public void testLegacySimpleGetBufferUpdates() throws Exception {
        TestUrlRequestListener listener = new TestUrlRequestListener();
        listener.mLegacyReadByteBufferAdjustment = true;
        listener.setAutoAdvance(false);
        // Since the default method is "GET", the expected response body is also
        // "GET".
        UrlRequest.Builder builder = new UrlRequest.Builder(NativeTestServer.getEchoMethodURL(),
                listener, listener.getExecutor(), mActivity.mCronetEngine);
        UrlRequest urlRequest = builder.build();
        urlRequest.start();
        listener.waitForNextStep();

        ByteBuffer readBuffer = ByteBuffer.allocateDirect(5);
        readBuffer.put("FOR".getBytes());
        assertEquals(3, readBuffer.position());

        // Read first two characters of the response ("GE"). It's theoretically
        // possible to need one read per character, though in practice,
        // shouldn't happen.
        while (listener.mResponseAsString.length() < 2) {
            assertFalse(listener.isDone());
            urlRequest.read(readBuffer);
            listener.waitForNextStep();
        }

        // Make sure the two characters were read.
        assertEquals("GE", listener.mResponseAsString);

        // Check the contents of the entire buffer. The first 3 characters
        // should not have been changed, and the last two should be the first
        // two characters from the response.
        assertEquals("FORGE", bufferContentsToString(readBuffer, 0, 5));
        // The limit should now be 5. Position could be either 3 or 4.
        assertEquals(5, readBuffer.limit());

        assertEquals(ResponseStep.ON_READ_COMPLETED, listener.mResponseStep);

        // Start reading from position 3. Since the only remaining character
        // from the response is a "T", when the read completes, the buffer
        // should contain "FORTE", with a position() of 3 and a limit() of 4.
        readBuffer.position(3);
        urlRequest.read(readBuffer);
        listener.waitForNextStep();

        // Make sure all three characters of the response have now been read.
        assertEquals("GET", listener.mResponseAsString);

        // Check the entire contents of the buffer. Only the third character
        // should have been modified.
        assertEquals("FORTE", bufferContentsToString(readBuffer, 0, 5));

        // Make sure position and limit were updated correctly.
        assertEquals(3, readBuffer.position());
        assertEquals(4, readBuffer.limit());

        assertEquals(ResponseStep.ON_READ_COMPLETED, listener.mResponseStep);

        // One more read attempt. The request should complete.
        readBuffer.position(1);
        readBuffer.limit(5);
        urlRequest.read(readBuffer);
        listener.waitForNextStep();

        assertEquals(200, listener.mResponseInfo.getHttpStatusCode());
        assertEquals("GET", listener.mResponseAsString);
        checkResponseInfo(listener.mResponseInfo,
                NativeTestServer.getEchoMethodURL(), 200, "OK");

        // Check that buffer contents were not modified.
        assertEquals("FORTE", bufferContentsToString(readBuffer, 0, 5));

        // Buffer limit should be set to original position, and position should
        // not have been modified, since nothing was read.
        assertEquals(1, readBuffer.position());
        assertEquals(1, readBuffer.limit());

        assertEquals(ResponseStep.ON_SUCCEEDED, listener.mResponseStep);

        // Make sure there are no other pending messages, which would trigger
        // asserts in TestURLRequestListener.
        testSimpleGet();
    }

    /**
     * Checks that the buffer is updated correctly, when starting at an offset.
     */
    @SmallTest
    @Feature({"Cronet"})
    public void testSimpleGetBufferUpdates() throws Exception {
        TestUrlRequestListener listener = new TestUrlRequestListener();
        listener.setAutoAdvance(false);
        // Since the default method is "GET", the expected response body is also
        // "GET".
        UrlRequest.Builder builder = new UrlRequest.Builder(NativeTestServer.getEchoMethodURL(),
                listener, listener.getExecutor(), mActivity.mCronetEngine);
        UrlRequest urlRequest = builder.build();
        urlRequest.start();
        listener.waitForNextStep();

        ByteBuffer readBuffer = ByteBuffer.allocateDirect(5);
        readBuffer.put("FOR".getBytes());
        assertEquals(3, readBuffer.position());

        // Read first two characters of the response ("GE"). It's theoretically
        // possible to need one read per character, though in practice,
        // shouldn't happen.
        while (listener.mResponseAsString.length() < 2) {
            assertFalse(listener.isDone());
            listener.startNextRead(urlRequest, readBuffer);
            listener.waitForNextStep();
        }

        // Make sure the two characters were read.
        assertEquals("GE", listener.mResponseAsString);

        // Check the contents of the entire buffer. The first 3 characters
        // should not have been changed, and the last two should be the first
        // two characters from the response.
        assertEquals("FORGE", bufferContentsToString(readBuffer, 0, 5));
        // The limit and position should be 5.
        assertEquals(5, readBuffer.limit());
        assertEquals(5, readBuffer.position());

        assertEquals(ResponseStep.ON_READ_COMPLETED, listener.mResponseStep);

        // Start reading from position 3. Since the only remaining character
        // from the response is a "T", when the read completes, the buffer
        // should contain "FORTE", with a position() of 4 and a limit() of 5.
        readBuffer.position(3);
        listener.startNextRead(urlRequest, readBuffer);
        listener.waitForNextStep();

        // Make sure all three characters of the response have now been read.
        assertEquals("GET", listener.mResponseAsString);

        // Check the entire contents of the buffer. Only the third character
        // should have been modified.
        assertEquals("FORTE", bufferContentsToString(readBuffer, 0, 5));

        // Make sure position and limit were updated correctly.
        assertEquals(4, readBuffer.position());
        assertEquals(5, readBuffer.limit());

        assertEquals(ResponseStep.ON_READ_COMPLETED, listener.mResponseStep);

        // One more read attempt. The request should complete.
        readBuffer.position(1);
        readBuffer.limit(5);
        listener.startNextRead(urlRequest, readBuffer);
        listener.waitForNextStep();

        assertEquals(200, listener.mResponseInfo.getHttpStatusCode());
        assertEquals("GET", listener.mResponseAsString);
        checkResponseInfo(listener.mResponseInfo, NativeTestServer.getEchoMethodURL(), 200, "OK");

        // Check that buffer contents were not modified.
        assertEquals("FORTE", bufferContentsToString(readBuffer, 0, 5));

        // Position should not have been modified, since nothing was read.
        assertEquals(1, readBuffer.position());
        // Limit should be unchanged as always.
        assertEquals(5, readBuffer.limit());

        assertEquals(ResponseStep.ON_SUCCEEDED, listener.mResponseStep);

        // Make sure there are no other pending messages, which would trigger
        // asserts in TestURLRequestListener.
        testSimpleGet();
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testBadBuffers() throws Exception {
        TestUrlRequestListener listener = new TestUrlRequestListener();
        listener.setAutoAdvance(false);
        UrlRequest.Builder builder = new UrlRequest.Builder(NativeTestServer.getEchoMethodURL(),
                listener, listener.getExecutor(), mActivity.mCronetEngine);
        UrlRequest urlRequest = builder.build();
        urlRequest.start();
        listener.waitForNextStep();

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
        listener.setAutoAdvance(true);
        ByteBuffer readBuffer = ByteBuffer.allocateDirect(5);
        urlRequest.readNew(readBuffer);
        listener.blockForDone();
        assertEquals(200, listener.mResponseInfo.getHttpStatusCode());
        assertEquals("GET", listener.mResponseAsString);
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testUnexpectedReads() throws Exception {
        final TestUrlRequestListener listener = new TestUrlRequestListener();
        listener.setAutoAdvance(false);
        final UrlRequest urlRequest =
                new UrlRequest.Builder(NativeTestServer.getRedirectURL(), listener,
                                      listener.getExecutor(), mActivity.mCronetEngine)
                        .build();

        // Try to read before starting request.
        try {
            listener.startNextRead(urlRequest);
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
                    listener.startNextRead(urlRequest);
                    fail("Exception not thrown");
                } catch (IllegalStateException e) {
                    assertEquals("Unexpected read attempt.",
                            e.getMessage());
                }
            }
        };
        listener.getExecutor().execute(startAndRead);
        listener.waitForNextStep();

        assertEquals(listener.mResponseStep, ResponseStep.ON_RECEIVED_REDIRECT);
        // Try to read after the redirect.
        try {
            listener.startNextRead(urlRequest);
            fail("Exception not thrown");
        } catch (IllegalStateException e) {
            assertEquals("Unexpected read attempt.",
                    e.getMessage());
        }
        urlRequest.followRedirect();
        listener.waitForNextStep();

        assertEquals(listener.mResponseStep, ResponseStep.ON_RESPONSE_STARTED);
        assertEquals(200, listener.mResponseInfo.getHttpStatusCode());

        while (!listener.isDone()) {
            Runnable readTwice = new Runnable() {
                @Override
                public void run() {
                    listener.startNextRead(urlRequest);
                    // Try to read again before the last read completes.
                    try {
                        listener.startNextRead(urlRequest);
                        fail("Exception not thrown");
                    } catch (IllegalStateException e) {
                        assertEquals("Unexpected read attempt.",
                                e.getMessage());
                    }
                }
            };
            listener.getExecutor().execute(readTwice);
            listener.waitForNextStep();
        }

        assertEquals(listener.mResponseStep, ResponseStep.ON_SUCCEEDED);
        assertEquals(NativeTestServer.SUCCESS_BODY, listener.mResponseAsString);

        // Try to read after request is complete.
        try {
            listener.startNextRead(urlRequest);
            fail("Exception not thrown");
        } catch (IllegalStateException e) {
            assertEquals("Unexpected read attempt.",
                    e.getMessage());
        }
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testUnexpectedFollowRedirects() throws Exception {
        final TestUrlRequestListener listener = new TestUrlRequestListener();
        listener.setAutoAdvance(false);
        final UrlRequest urlRequest =
                new UrlRequest.Builder(NativeTestServer.getRedirectURL(), listener,
                                      listener.getExecutor(), mActivity.mCronetEngine)
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
        listener.getExecutor().execute(startAndRead);
        listener.waitForNextStep();

        assertEquals(listener.mResponseStep, ResponseStep.ON_RECEIVED_REDIRECT);
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
        listener.getExecutor().execute(followRedirectTwice);
        listener.waitForNextStep();

        assertEquals(listener.mResponseStep, ResponseStep.ON_RESPONSE_STARTED);
        assertEquals(200, listener.mResponseInfo.getHttpStatusCode());

        while (!listener.isDone()) {
            try {
                urlRequest.followRedirect();
                fail("Exception not thrown");
            } catch (IllegalStateException e) {
                assertEquals("No redirect to follow.",
                        e.getMessage());
            }
            listener.startNextRead(urlRequest);
            listener.waitForNextStep();
        }

        assertEquals(listener.mResponseStep, ResponseStep.ON_SUCCEEDED);
        assertEquals(NativeTestServer.SUCCESS_BODY, listener.mResponseAsString);

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
        TestUrlRequestListener listener = new TestUrlRequestListener();
        UrlRequest.Builder builder = new UrlRequest.Builder(NativeTestServer.getEchoBodyURL(),
                listener, listener.getExecutor(), mActivity.mCronetEngine);

        try {
            builder.setUploadDataProvider(null, listener.getExecutor());
            fail("Exception not thrown");
        } catch (NullPointerException e) {
            assertEquals("Invalid UploadDataProvider.", e.getMessage());
        }

        TestUploadDataProvider dataProvider = new TestUploadDataProvider(
                TestUploadDataProvider.SuccessCallbackMode.SYNC, listener.getExecutor());
        builder.setUploadDataProvider(dataProvider, listener.getExecutor());
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
        TestUrlRequestListener listener = new TestUrlRequestListener();
        UrlRequest.Builder builder = new UrlRequest.Builder(NativeTestServer.getEchoBodyURL(),
                listener, listener.getExecutor(), mActivity.mCronetEngine);

        TestUploadDataProvider dataProvider = new TestUploadDataProvider(
                TestUploadDataProvider.SuccessCallbackMode.SYNC, listener.getExecutor());
        builder.setUploadDataProvider(dataProvider, listener.getExecutor());
        builder.addHeader("Content-Type", "useless/string");
        builder.build().start();
        listener.blockForDone();

        assertEquals(0, dataProvider.getLength());
        assertEquals(0, dataProvider.getNumReadCalls());
        assertEquals(0, dataProvider.getNumRewindCalls());

        assertEquals(200, listener.mResponseInfo.getHttpStatusCode());
        assertEquals("", listener.mResponseAsString);
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testUploadSync() throws Exception {
        TestUrlRequestListener listener = new TestUrlRequestListener();
        UrlRequest.Builder builder = new UrlRequest.Builder(NativeTestServer.getEchoBodyURL(),
                listener, listener.getExecutor(), mActivity.mCronetEngine);

        TestUploadDataProvider dataProvider = new TestUploadDataProvider(
                TestUploadDataProvider.SuccessCallbackMode.SYNC, listener.getExecutor());
        dataProvider.addRead("test".getBytes());
        builder.setUploadDataProvider(dataProvider, listener.getExecutor());
        builder.addHeader("Content-Type", "useless/string");
        builder.build().start();
        listener.blockForDone();

        assertEquals(4, dataProvider.getLength());
        assertEquals(1, dataProvider.getNumReadCalls());
        assertEquals(0, dataProvider.getNumRewindCalls());

        assertEquals(200, listener.mResponseInfo.getHttpStatusCode());
        assertEquals("test", listener.mResponseAsString);
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testUploadMultiplePiecesSync() throws Exception {
        TestUrlRequestListener listener = new TestUrlRequestListener();
        UrlRequest.Builder builder = new UrlRequest.Builder(NativeTestServer.getEchoBodyURL(),
                listener, listener.getExecutor(), mActivity.mCronetEngine);

        TestUploadDataProvider dataProvider = new TestUploadDataProvider(
                TestUploadDataProvider.SuccessCallbackMode.SYNC, listener.getExecutor());
        dataProvider.addRead("Y".getBytes());
        dataProvider.addRead("et ".getBytes());
        dataProvider.addRead("another ".getBytes());
        dataProvider.addRead("test".getBytes());

        builder.setUploadDataProvider(dataProvider, listener.getExecutor());
        builder.addHeader("Content-Type", "useless/string");
        builder.build().start();
        listener.blockForDone();

        assertEquals(16, dataProvider.getLength());
        assertEquals(4, dataProvider.getNumReadCalls());
        assertEquals(0, dataProvider.getNumRewindCalls());

        assertEquals(200, listener.mResponseInfo.getHttpStatusCode());
        assertEquals("Yet another test", listener.mResponseAsString);
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testUploadMultiplePiecesAsync() throws Exception {
        TestUrlRequestListener listener = new TestUrlRequestListener();
        UrlRequest.Builder builder = new UrlRequest.Builder(NativeTestServer.getEchoBodyURL(),
                listener, listener.getExecutor(), mActivity.mCronetEngine);

        TestUploadDataProvider dataProvider = new TestUploadDataProvider(
                TestUploadDataProvider.SuccessCallbackMode.ASYNC, listener.getExecutor());
        dataProvider.addRead("Y".getBytes());
        dataProvider.addRead("et ".getBytes());
        dataProvider.addRead("another ".getBytes());
        dataProvider.addRead("test".getBytes());

        builder.setUploadDataProvider(dataProvider, listener.getExecutor());
        builder.addHeader("Content-Type", "useless/string");
        builder.build().start();
        listener.blockForDone();

        assertEquals(16, dataProvider.getLength());
        assertEquals(4, dataProvider.getNumReadCalls());
        assertEquals(0, dataProvider.getNumRewindCalls());

        assertEquals(200, listener.mResponseInfo.getHttpStatusCode());
        assertEquals("Yet another test", listener.mResponseAsString);
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testUploadChangesDefaultMethod() throws Exception {
        TestUrlRequestListener listener = new TestUrlRequestListener();
        UrlRequest.Builder builder = new UrlRequest.Builder(NativeTestServer.getEchoMethodURL(),
                listener, listener.getExecutor(), mActivity.mCronetEngine);

        TestUploadDataProvider dataProvider = new TestUploadDataProvider(
                TestUploadDataProvider.SuccessCallbackMode.SYNC, listener.getExecutor());
        dataProvider.addRead("test".getBytes());
        builder.setUploadDataProvider(dataProvider, listener.getExecutor());
        builder.addHeader("Content-Type", "useless/string");
        builder.build().start();
        listener.blockForDone();

        assertEquals(200, listener.mResponseInfo.getHttpStatusCode());
        assertEquals("POST", listener.mResponseAsString);
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testUploadWithSetMethod() throws Exception {
        TestUrlRequestListener listener = new TestUrlRequestListener();
        UrlRequest.Builder builder = new UrlRequest.Builder(NativeTestServer.getEchoMethodURL(),
                listener, listener.getExecutor(), mActivity.mCronetEngine);

        final String method = "PUT";
        builder.setHttpMethod(method);

        TestUploadDataProvider dataProvider = new TestUploadDataProvider(
                TestUploadDataProvider.SuccessCallbackMode.SYNC, listener.getExecutor());
        dataProvider.addRead("test".getBytes());
        builder.setUploadDataProvider(dataProvider, listener.getExecutor());
        builder.addHeader("Content-Type", "useless/string");
        builder.build().start();
        listener.blockForDone();

        assertEquals(200, listener.mResponseInfo.getHttpStatusCode());
        assertEquals("PUT", listener.mResponseAsString);
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testUploadRedirectSync() throws Exception {
        TestUrlRequestListener listener = new TestUrlRequestListener();
        UrlRequest.Builder builder =
                new UrlRequest.Builder(NativeTestServer.getRedirectToEchoBody(), listener,
                        listener.getExecutor(), mActivity.mCronetEngine);

        TestUploadDataProvider dataProvider = new TestUploadDataProvider(
                TestUploadDataProvider.SuccessCallbackMode.SYNC, listener.getExecutor());
        dataProvider.addRead("test".getBytes());
        builder.setUploadDataProvider(dataProvider, listener.getExecutor());
        builder.addHeader("Content-Type", "useless/string");
        builder.build().start();
        listener.blockForDone();

        // 1 read call before the rewind, 1 after.
        assertEquals(2, dataProvider.getNumReadCalls());
        assertEquals(1, dataProvider.getNumRewindCalls());

        assertEquals(200, listener.mResponseInfo.getHttpStatusCode());
        assertEquals("test", listener.mResponseAsString);
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testUploadRedirectAsync() throws Exception {
        TestUrlRequestListener listener = new TestUrlRequestListener();
        UrlRequest.Builder builder =
                new UrlRequest.Builder(NativeTestServer.getRedirectToEchoBody(), listener,
                        listener.getExecutor(), mActivity.mCronetEngine);

        TestUploadDataProvider dataProvider = new TestUploadDataProvider(
                TestUploadDataProvider.SuccessCallbackMode.ASYNC, listener.getExecutor());
        dataProvider.addRead("test".getBytes());
        builder.setUploadDataProvider(dataProvider, listener.getExecutor());
        builder.addHeader("Content-Type", "useless/string");
        builder.build().start();
        listener.blockForDone();

        // 1 read call before the rewind, 1 after.
        assertEquals(2, dataProvider.getNumReadCalls());
        assertEquals(1, dataProvider.getNumRewindCalls());

        assertEquals(200, listener.mResponseInfo.getHttpStatusCode());
        assertEquals("test", listener.mResponseAsString);
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testUploadReadFailSync() throws Exception {
        TestUrlRequestListener listener = new TestUrlRequestListener();
        UrlRequest.Builder builder = new UrlRequest.Builder(NativeTestServer.getEchoBodyURL(),
                listener, listener.getExecutor(), mActivity.mCronetEngine);

        TestUploadDataProvider dataProvider = new TestUploadDataProvider(
                TestUploadDataProvider.SuccessCallbackMode.SYNC, listener.getExecutor());
        dataProvider.setReadFailure(0, TestUploadDataProvider.FailMode.CALLBACK_SYNC);
        // This will never be read, but if the length is 0, read may never be
        // called.
        dataProvider.addRead("test".getBytes());
        builder.setUploadDataProvider(dataProvider, listener.getExecutor());
        builder.addHeader("Content-Type", "useless/string");
        builder.build().start();
        listener.blockForDone();

        assertEquals(1, dataProvider.getNumReadCalls());
        assertEquals(0, dataProvider.getNumRewindCalls());

        assertEquals("Exception received from UploadDataProvider", listener.mError.getMessage());
        assertEquals("Sync read failure", listener.mError.getCause().getMessage());
        assertEquals(null, listener.mResponseInfo);
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testUploadReadFailAsync() throws Exception {
        TestUrlRequestListener listener = new TestUrlRequestListener();
        UrlRequest.Builder builder = new UrlRequest.Builder(NativeTestServer.getEchoBodyURL(),
                listener, listener.getExecutor(), mActivity.mCronetEngine);

        TestUploadDataProvider dataProvider = new TestUploadDataProvider(
                TestUploadDataProvider.SuccessCallbackMode.SYNC, listener.getExecutor());
        dataProvider.setReadFailure(0, TestUploadDataProvider.FailMode.CALLBACK_ASYNC);
        // This will never be read, but if the length is 0, read may never be
        // called.
        dataProvider.addRead("test".getBytes());
        builder.setUploadDataProvider(dataProvider, listener.getExecutor());
        builder.addHeader("Content-Type", "useless/string");
        builder.build().start();
        listener.blockForDone();

        assertEquals(1, dataProvider.getNumReadCalls());
        assertEquals(0, dataProvider.getNumRewindCalls());

        assertEquals("Exception received from UploadDataProvider", listener.mError.getMessage());
        assertEquals("Async read failure", listener.mError.getCause().getMessage());
        assertEquals(null, listener.mResponseInfo);
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testUploadReadFailThrown() throws Exception {
        TestUrlRequestListener listener = new TestUrlRequestListener();
        UrlRequest.Builder builder = new UrlRequest.Builder(NativeTestServer.getEchoBodyURL(),
                listener, listener.getExecutor(), mActivity.mCronetEngine);

        TestUploadDataProvider dataProvider = new TestUploadDataProvider(
                TestUploadDataProvider.SuccessCallbackMode.SYNC, listener.getExecutor());
        dataProvider.setReadFailure(0, TestUploadDataProvider.FailMode.THROWN);
        // This will never be read, but if the length is 0, read may never be
        // called.
        dataProvider.addRead("test".getBytes());
        builder.setUploadDataProvider(dataProvider, listener.getExecutor());
        builder.addHeader("Content-Type", "useless/string");
        builder.build().start();
        listener.blockForDone();

        assertEquals(1, dataProvider.getNumReadCalls());
        assertEquals(0, dataProvider.getNumRewindCalls());

        assertEquals("Exception received from UploadDataProvider", listener.mError.getMessage());
        assertEquals("Thrown read failure", listener.mError.getCause().getMessage());
        assertEquals(null, listener.mResponseInfo);
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testUploadRewindFailSync() throws Exception {
        TestUrlRequestListener listener = new TestUrlRequestListener();
        UrlRequest.Builder builder =
                new UrlRequest.Builder(NativeTestServer.getRedirectToEchoBody(), listener,
                        listener.getExecutor(), mActivity.mCronetEngine);

        TestUploadDataProvider dataProvider = new TestUploadDataProvider(
                TestUploadDataProvider.SuccessCallbackMode.SYNC, listener.getExecutor());
        dataProvider.setRewindFailure(TestUploadDataProvider.FailMode.CALLBACK_SYNC);
        dataProvider.addRead("test".getBytes());
        builder.setUploadDataProvider(dataProvider, listener.getExecutor());
        builder.addHeader("Content-Type", "useless/string");
        builder.build().start();
        listener.blockForDone();

        assertEquals(1, dataProvider.getNumReadCalls());
        assertEquals(1, dataProvider.getNumRewindCalls());

        assertEquals("Exception received from UploadDataProvider", listener.mError.getMessage());
        assertEquals("Sync rewind failure", listener.mError.getCause().getMessage());
        assertEquals(null, listener.mResponseInfo);
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testUploadRewindFailAsync() throws Exception {
        TestUrlRequestListener listener = new TestUrlRequestListener();
        UrlRequest.Builder builder =
                new UrlRequest.Builder(NativeTestServer.getRedirectToEchoBody(), listener,
                        listener.getExecutor(), mActivity.mCronetEngine);

        TestUploadDataProvider dataProvider = new TestUploadDataProvider(
                TestUploadDataProvider.SuccessCallbackMode.ASYNC, listener.getExecutor());
        dataProvider.setRewindFailure(TestUploadDataProvider.FailMode.CALLBACK_ASYNC);
        dataProvider.addRead("test".getBytes());
        builder.setUploadDataProvider(dataProvider, listener.getExecutor());
        builder.addHeader("Content-Type", "useless/string");
        builder.build().start();
        listener.blockForDone();

        assertEquals(1, dataProvider.getNumReadCalls());
        assertEquals(1, dataProvider.getNumRewindCalls());

        assertEquals("Exception received from UploadDataProvider", listener.mError.getMessage());
        assertEquals("Async rewind failure", listener.mError.getCause().getMessage());
        assertEquals(null, listener.mResponseInfo);
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testUploadRewindFailThrown() throws Exception {
        TestUrlRequestListener listener = new TestUrlRequestListener();
        UrlRequest.Builder builder =
                new UrlRequest.Builder(NativeTestServer.getRedirectToEchoBody(), listener,
                        listener.getExecutor(), mActivity.mCronetEngine);

        TestUploadDataProvider dataProvider = new TestUploadDataProvider(
                TestUploadDataProvider.SuccessCallbackMode.SYNC, listener.getExecutor());
        dataProvider.setRewindFailure(TestUploadDataProvider.FailMode.THROWN);
        dataProvider.addRead("test".getBytes());
        builder.setUploadDataProvider(dataProvider, listener.getExecutor());
        builder.addHeader("Content-Type", "useless/string");
        builder.build().start();
        listener.blockForDone();

        assertEquals(1, dataProvider.getNumReadCalls());
        assertEquals(1, dataProvider.getNumRewindCalls());

        assertEquals("Exception received from UploadDataProvider", listener.mError.getMessage());
        assertEquals("Thrown rewind failure", listener.mError.getCause().getMessage());
        assertEquals(null, listener.mResponseInfo);
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testUploadChunked() throws Exception {
        TestUrlRequestListener listener = new TestUrlRequestListener();
        UrlRequest.Builder builder = new UrlRequest.Builder(NativeTestServer.getEchoBodyURL(),
                listener, listener.getExecutor(), mActivity.mCronetEngine);

        TestUploadDataProvider dataProvider = new TestUploadDataProvider(
                TestUploadDataProvider.SuccessCallbackMode.SYNC, listener.getExecutor());
        dataProvider.addRead("test hello".getBytes());
        dataProvider.setChunked(true);
        builder.setUploadDataProvider(dataProvider, listener.getExecutor());
        builder.addHeader("Content-Type", "useless/string");

        assertEquals(-1, dataProvider.getLength());

        builder.build().start();
        listener.blockForDone();

        // 1 read call for one data chunk.
        assertEquals(1, dataProvider.getNumReadCalls());
        assertEquals("test hello", listener.mResponseAsString);
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testUploadChunkedLastReadZeroLengthBody() throws Exception {
        TestUrlRequestListener listener = new TestUrlRequestListener();
        UrlRequest.Builder builder = new UrlRequest.Builder(NativeTestServer.getEchoBodyURL(),
                listener, listener.getExecutor(), mActivity.mCronetEngine);

        TestUploadDataProvider dataProvider = new TestUploadDataProvider(
                TestUploadDataProvider.SuccessCallbackMode.SYNC, listener.getExecutor());
        // Add 3 reads. The last read has a 0-length body.
        dataProvider.addRead("hello there".getBytes());
        dataProvider.addRead("!".getBytes());
        dataProvider.addRead("".getBytes());
        dataProvider.setChunked(true);
        builder.setUploadDataProvider(dataProvider, listener.getExecutor());
        builder.addHeader("Content-Type", "useless/string");

        assertEquals(-1, dataProvider.getLength());

        builder.build().start();
        listener.blockForDone();

        // 2 read call for the first two data chunks, and 1 for final chunk.
        assertEquals(3, dataProvider.getNumReadCalls());
        assertEquals("hello there!", listener.mResponseAsString);
    }

    // Test where an upload fails without ever initializing the
    // UploadDataStream, because it can't connect to the server.
    @SmallTest
    @Feature({"Cronet"})
    public void testUploadFailsWithoutInitializingStream() throws Exception {
        TestUrlRequestListener listener = new TestUrlRequestListener();
        UrlRequest.Builder builder = new UrlRequest.Builder(NativeTestServer.getEchoBodyURL(),
                listener, listener.getExecutor(), mActivity.mCronetEngine);
        // Shut down the test server, so connecting to it fails. Note that
        // calling shutdown again during teardown is safe.
        NativeTestServer.shutdownNativeTestServer();

        TestUploadDataProvider dataProvider = new TestUploadDataProvider(
                TestUploadDataProvider.SuccessCallbackMode.SYNC, listener.getExecutor());
        dataProvider.addRead("test".getBytes());
        builder.setUploadDataProvider(dataProvider, listener.getExecutor());
        builder.addHeader("Content-Type", "useless/string");
        builder.build().start();
        listener.blockForDone();

        assertNull(listener.mResponseInfo);
        assertEquals("Exception in CronetUrlRequest: net::ERR_CONNECTION_REFUSED",
                listener.mError.getMessage());
    }

    private void throwOrCancel(FailureType failureType, ResponseStep failureStep,
            boolean expectResponseInfo, boolean expectError) {
        TestUrlRequestListener listener = new TestUrlRequestListener();
        listener.setFailure(failureType, failureStep);
        UrlRequest.Builder builder = new UrlRequest.Builder(NativeTestServer.getRedirectURL(),
                listener, listener.getExecutor(), mActivity.mCronetEngine);
        UrlRequest urlRequest = builder.build();
        urlRequest.start();
        listener.blockForDone();
        assertEquals(1, listener.mRedirectCount);
        assertEquals(listener.mResponseStep, failureStep);
        assertTrue(urlRequest.isDone());
        assertEquals(expectResponseInfo, listener.mResponseInfo != null);
        assertEquals(expectError, listener.mError != null);
        assertEquals(expectError, listener.mOnErrorCalled);
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
        TestUrlRequestListener listener = new TestUrlRequestListener();
        listener.setFailure(FailureType.THROW_SYNC, ResponseStep.ON_SUCCEEDED);
        UrlRequest.Builder builder = new UrlRequest.Builder(NativeTestServer.getRedirectURL(),
                listener, listener.getExecutor(), mActivity.mCronetEngine);
        UrlRequest urlRequest = builder.build();
        urlRequest.start();
        listener.blockForDone();
        assertEquals(1, listener.mRedirectCount);
        assertEquals(listener.mResponseStep, ResponseStep.ON_SUCCEEDED);
        assertTrue(urlRequest.isDone());
        assertNotNull(listener.mResponseInfo);
        assertNull(listener.mError);
        assertFalse(listener.mOnErrorCalled);
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testExecutorShutdown() {
        TestUrlRequestListener listener = new TestUrlRequestListener();

        listener.setAutoAdvance(false);
        UrlRequest.Builder builder = new UrlRequest.Builder(NativeTestServer.getEchoBodyURL(),
                listener, listener.getExecutor(), mActivity.mCronetEngine);
        CronetUrlRequest urlRequest = (CronetUrlRequest) builder.build();
        urlRequest.start();
        listener.waitForNextStep();
        assertFalse(listener.isDone());
        assertFalse(urlRequest.isDone());

        final ConditionVariable requestDestroyed = new ConditionVariable(false);
        urlRequest.setOnDestroyedCallbackForTests(new Runnable() {
            @Override
            public void run() {
                requestDestroyed.open();
            }
        });

        // Shutdown the executor, so posting the task will throw an exception.
        listener.shutdownExecutor();
        ByteBuffer readBuffer = ByteBuffer.allocateDirect(5);
        urlRequest.readNew(readBuffer);
        // Listener will never be called again because executor is shutdown,
        // but request will be destroyed from network thread.
        requestDestroyed.block();

        assertFalse(listener.isDone());
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

        TestUrlRequestListener listener = new TestUrlRequestListener();
        UrlRequest.Builder builder = new UrlRequest.Builder(NativeTestServer.getEchoBodyURL(),
                listener, listener.getExecutor(), mActivity.mCronetEngine);

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
        // Listener.onFailed will be called on request executor even though upload
        // executor is shutdown.
        listener.blockForDone();
        assertTrue(listener.isDone());
        assertTrue(listener.mOnErrorCalled);
        assertEquals("Exception received from UploadDataProvider", listener.mError.getMessage());
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
