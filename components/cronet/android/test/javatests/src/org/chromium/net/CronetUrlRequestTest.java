// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import android.test.suitebuilder.annotation.SmallTest;
import android.util.Pair;

import org.chromium.base.test.util.Feature;
import org.chromium.net.TestUrlRequestListener.FailureType;
import org.chromium.net.TestUrlRequestListener.ResponseStep;

import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * Test functionality of CronetUrlRequest.
 */
public class CronetUrlRequestTest extends CronetTestBase {
    // URL used for base tests.
    private static final String TEST_URL = "http://127.0.0.1:8000";
    private static final String MOCK_SUCCESS_PATH = "success.txt";

    private CronetTestActivity mActivity;
    private MockUrlRequestJobFactory mMockUrlRequestJobFactory;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mActivity = launchCronetTestApp();
        assertTrue(NativeTestServer.startNativeTestServer(
                getInstrumentation().getTargetContext()));
        // Add url interceptors after native application context is initialized.
        mMockUrlRequestJobFactory = new MockUrlRequestJobFactory(
                getInstrumentation().getTargetContext());
    }

    @Override
    protected void tearDown() throws Exception {
        NativeTestServer.shutdownNativeTestServer();
        mActivity.mUrlRequestContext.shutdown();
        super.tearDown();
    }

    private TestUrlRequestListener startAndWaitForComplete(String url)
            throws Exception {
        TestUrlRequestListener listener = new TestUrlRequestListener();
        // Create request.
        UrlRequest urlRequest = mActivity.mUrlRequestContext.createRequest(
                url, listener, listener.getExecutor());
        urlRequest.start();
        listener.blockForDone();
        return listener;
    }

    private void checkResponseInfo(ResponseInfo responseInfo,
            String expectedUrl, int expectedHttpStatusCode,
            String expectedHttpStatusText) {
        assertEquals(expectedUrl, responseInfo.getUrl());
        assertEquals(expectedUrl, responseInfo.getUrlChain()[
                responseInfo.getUrlChain().length - 1]);
        assertEquals(expectedHttpStatusCode, responseInfo.getHttpStatusCode());
        assertEquals(expectedHttpStatusText, responseInfo.getHttpStatusText());
        assertFalse(responseInfo.wasCached());
    }

    private void checkResponseInfoHeader(ResponseInfo responseInfo,
            String headerName, String headerValue) {
        Map<String, List<String>> responseHeaders =
                responseInfo.getAllHeaders();
        List<String> header = responseHeaders.get(headerName);
        assertNotNull(header);
        assertTrue(header.contains(headerValue));
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testSimpleGet() throws Exception {
        TestUrlRequestListener listener = startAndWaitForComplete(
                NativeTestServer.getEchoMethodURL());
        assertEquals(200, listener.mResponseInfo.getHttpStatusCode());
        // Default method is 'GET'.
        assertEquals("GET", listener.mResponseAsString);
        assertFalse(listener.mOnRedirectCalled);
        assertEquals(listener.mResponseStep, ResponseStep.ON_SUCCEEDED);
        checkResponseInfo(listener.mResponseInfo,
                NativeTestServer.getEchoMethodURL(), 200, "OK");
        checkResponseInfo(listener.mExtendedResponseInfo.getResponseInfo(),
                NativeTestServer.getEchoMethodURL(), 200, "OK");
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testNotFound() throws Exception {
        String url = NativeTestServer.getFileURL("/notfound.html");
        TestUrlRequestListener listener = startAndWaitForComplete(url);
        checkResponseInfo(listener.mResponseInfo, url, 404, "Not Found");
        checkResponseInfo(listener.mExtendedResponseInfo.getResponseInfo(),
                url, 404, "Not Found");
        assertEquals(
                "<!DOCTYPE html>\n<html>\n<head>\n<title>Not found</title>\n"
                + "<p>Test page loaded.</p>\n</head>\n</html>\n",
                listener.mResponseAsString);
        assertFalse(listener.mOnRedirectCalled);
        assertEquals(listener.mResponseStep, ResponseStep.ON_SUCCEEDED);
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testSetHttpMethod() throws Exception {
        TestUrlRequestListener listener = new TestUrlRequestListener();
        String methodName = "HEAD";
        UrlRequest urlRequest = mActivity.mUrlRequestContext.createRequest(
                NativeTestServer.getEchoMethodURL(), listener,
                listener.getExecutor());
        // Try to set 'null' method.
        try {
            urlRequest.setHttpMethod(null);
            fail("Exception not thrown");
        } catch (NullPointerException e) {
            assertEquals("Method is required.", e.getMessage());
        }

        urlRequest.setHttpMethod(methodName);
        urlRequest.start();
        try {
            urlRequest.setHttpMethod("toolate");
            fail("Exception not thrown");
        } catch (IllegalStateException e) {
            assertEquals("Request is already started.", e.getMessage());
        }
        listener.blockForDone();
        assertEquals(200, listener.mResponseInfo.getHttpStatusCode());
        assertEquals(0, listener.mHttpResponseDataLength);
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testBadMethod() throws Exception {
        TestUrlRequestListener listener = new TestUrlRequestListener();
        UrlRequest urlRequest = mActivity.mUrlRequestContext.createRequest(
                TEST_URL, listener, listener.getExecutor());
        try {
            urlRequest.setHttpMethod("bad:method!");
            urlRequest.start();
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
        UrlRequest urlRequest = mActivity.mUrlRequestContext.createRequest(
                TEST_URL, listener, listener.getExecutor());
        try {
            urlRequest.addHeader("header:name", "headervalue");
            urlRequest.start();
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
        UrlRequest urlRequest = mActivity.mUrlRequestContext.createRequest(
                TEST_URL, listener, listener.getExecutor());
        try {
            urlRequest.addHeader("headername", "bad header\r\nvalue");
            urlRequest.start();
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
        UrlRequest urlRequest = mActivity.mUrlRequestContext.createRequest(
                NativeTestServer.getEchoHeaderURL(headerName), listener,
                listener.getExecutor());

        urlRequest.addHeader(headerName, headerValue);
        urlRequest.start();
        try {
            urlRequest.addHeader("header2", "value");
            fail("Exception not thrown");
        } catch (IllegalStateException e) {
            assertEquals("Request is already started.", e.getMessage());
        }
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
        UrlRequest urlRequest = mActivity.mUrlRequestContext.createRequest(
                NativeTestServer.getEchoAllHeadersURL(), listener,
                listener.getExecutor());
        urlRequest.addHeader(headerName, headerValue1);
        urlRequest.addHeader(headerName, headerValue2);
        urlRequest.start();
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
        UrlRequest urlRequest = mActivity.mUrlRequestContext.createRequest(
                NativeTestServer.getEchoHeaderURL(userAgentName), listener,
                listener.getExecutor());
        urlRequest.addHeader(userAgentName, userAgentValue);
        urlRequest.start();
        listener.blockForDone();
        assertEquals(200, listener.mResponseInfo.getHttpStatusCode());
        assertEquals(userAgentValue, listener.mResponseAsString);
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testDefaultUserAgent() throws Exception {
        TestUrlRequestListener listener = new TestUrlRequestListener();
        String headerName = "User-Agent";
        UrlRequest urlRequest = mActivity.mUrlRequestContext.createRequest(
                NativeTestServer.getEchoHeaderURL(headerName), listener,
                listener.getExecutor());
        urlRequest.start();
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
        TestUrlRequestListener listener = startAndWaitForComplete(
                MockUrlRequestJobFactory.SUCCESS_URL);
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
        TestUrlRequestListener listener = startAndWaitForComplete(
                MockUrlRequestJobFactory.SUCCESS_URL);
        assertEquals(200, listener.mResponseInfo.getHttpStatusCode());
        List<Pair<String, String>> responseHeaders =
                listener.mResponseInfo.getAllHeadersAsList();
        assertEquals(5, responseHeaders.size());
        assertEquals("Content-Type", responseHeaders.get(0).first);
        assertEquals("text/plain", responseHeaders.get(0).second);
        assertEquals("Access-Control-Allow-Origin",
                responseHeaders.get(1).first);
        assertEquals("*", responseHeaders.get(1).second);
        assertEquals("header-name", responseHeaders.get(2).first);
        assertEquals("header-value", responseHeaders.get(2).second);
        assertEquals("multi-header-name", responseHeaders.get(3).first);
        assertEquals("header-value1", responseHeaders.get(3).second);
        assertEquals("multi-header-name", responseHeaders.get(4).first);
        assertEquals("header-value2", responseHeaders.get(4).second);
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testMockRedirect() throws Exception {
        TestUrlRequestListener listener = startAndWaitForComplete(
                MockUrlRequestJobFactory.REDIRECT_URL);
        ResponseInfo mResponseInfo = listener.mResponseInfo;
        assertTrue(listener.mOnRedirectCalled);
        assertEquals(200, mResponseInfo.getHttpStatusCode());
        assertEquals(1, listener.mRedirectResponseInfoList.size());
        assertEquals(MockUrlRequestJobFactory.SUCCESS_URL,
                mResponseInfo.getUrl());
        assertEquals(2, mResponseInfo.getUrlChain().length);
        assertEquals(MockUrlRequestJobFactory.REDIRECT_URL,
                mResponseInfo.getUrlChain()[0]);
        assertEquals(MockUrlRequestJobFactory.SUCCESS_URL,
                mResponseInfo.getUrlChain()[1]);
        checkResponseInfo(listener.mRedirectResponseInfoList.get(0),
                MockUrlRequestJobFactory.REDIRECT_URL,
                302, "Found");
        checkResponseInfoHeader(listener.mRedirectResponseInfoList.get(0),
                "redirect-header", "header-value");
        assertTrue(listener.mHttpResponseDataLength != 0);
        assertTrue(listener.mOnRedirectCalled);
        assertEquals(listener.mResponseStep, ResponseStep.ON_SUCCEEDED);
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testMockMultiRedirect() throws Exception {
        TestUrlRequestListener listener = startAndWaitForComplete(
                MockUrlRequestJobFactory.MULTI_REDIRECT_URL);
        ResponseInfo mResponseInfo = listener.mResponseInfo;
        assertTrue(listener.mOnRedirectCalled);
        assertEquals(200, mResponseInfo.getHttpStatusCode());
        assertEquals(2, listener.mRedirectResponseInfoList.size());

        // Check first redirect (multiredirect.html -> redirect.html)
        ResponseInfo firstRedirectResponseInfo =
                listener.mRedirectResponseInfoList.get(0);
        assertEquals(1, firstRedirectResponseInfo.getUrlChain().length);
        assertEquals(MockUrlRequestJobFactory.MULTI_REDIRECT_URL,
                firstRedirectResponseInfo.getUrlChain()[0]);
        checkResponseInfo(firstRedirectResponseInfo,
                MockUrlRequestJobFactory.MULTI_REDIRECT_URL,
                302, "Found");
        checkResponseInfoHeader(firstRedirectResponseInfo,
                "redirect-header0", "header-value");

        // Check second redirect (redirect.html -> success.txt)
        ResponseInfo secondRedirectResponseInfo =
                listener.mRedirectResponseInfoList.get(1);
        assertEquals(2, secondRedirectResponseInfo.getUrlChain().length);
        assertEquals(MockUrlRequestJobFactory.MULTI_REDIRECT_URL,
                secondRedirectResponseInfo.getUrlChain()[0]);
        assertEquals(MockUrlRequestJobFactory.REDIRECT_URL,
                secondRedirectResponseInfo.getUrlChain()[1]);
        checkResponseInfo(secondRedirectResponseInfo,
                MockUrlRequestJobFactory.REDIRECT_URL, 302, "Found");
        checkResponseInfoHeader(secondRedirectResponseInfo,
                "redirect-header", "header-value");

        // Check final response (success.txt).
        assertEquals(MockUrlRequestJobFactory.SUCCESS_URL,
                mResponseInfo.getUrl());
        assertEquals(3, mResponseInfo.getUrlChain().length);
        assertEquals(MockUrlRequestJobFactory.MULTI_REDIRECT_URL,
                mResponseInfo.getUrlChain()[0]);
        assertEquals(MockUrlRequestJobFactory.REDIRECT_URL,
                mResponseInfo.getUrlChain()[1]);
        assertEquals(MockUrlRequestJobFactory.SUCCESS_URL,
                mResponseInfo.getUrlChain()[2]);
        assertTrue(listener.mHttpResponseDataLength != 0);
        assertTrue(listener.mOnRedirectCalled);
        assertEquals(listener.mResponseStep, ResponseStep.ON_SUCCEEDED);
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testMockNotFound() throws Exception {
        TestUrlRequestListener listener = startAndWaitForComplete(
                MockUrlRequestJobFactory.NOTFOUND_URL);
        assertEquals(404, listener.mResponseInfo.getHttpStatusCode());
        assertTrue(listener.mHttpResponseDataLength != 0);
        assertFalse(listener.mOnRedirectCalled);
        assertFalse(listener.mOnErrorCalled);
        assertEquals(listener.mResponseStep, ResponseStep.ON_SUCCEEDED);
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testMockStartAsyncError() throws Exception {
        final int arbitraryNetError = -3;
        TestUrlRequestListener listener = startAndWaitForComplete(
                mMockUrlRequestJobFactory.getMockUrlWithFailure(
                        MOCK_SUCCESS_PATH,
                        MockUrlRequestJobFactory.FailurePhase.START,
                        arbitraryNetError));
        assertNull(listener.mResponseInfo);
        assertNotNull(listener.mError);
        assertEquals(arbitraryNetError, listener.mError.netError());
        assertFalse(listener.mOnRedirectCalled);
        assertTrue(listener.mOnErrorCalled);
        assertEquals(listener.mResponseStep, ResponseStep.NOTHING);
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testMockReadDataSyncError() throws Exception {
        final int arbitraryNetError = -4;
        TestUrlRequestListener listener = startAndWaitForComplete(
                mMockUrlRequestJobFactory.getMockUrlWithFailure(
                        MOCK_SUCCESS_PATH,
                        MockUrlRequestJobFactory.FailurePhase.READ_SYNC,
                        arbitraryNetError));
        assertEquals(200, listener.mResponseInfo.getHttpStatusCode());
        assertNotNull(listener.mError);
        assertEquals(arbitraryNetError, listener.mError.netError());
        assertFalse(listener.mOnRedirectCalled);
        assertTrue(listener.mOnErrorCalled);
        assertEquals(listener.mResponseStep, ResponseStep.ON_RESPONSE_STARTED);
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testMockReadDataAsyncError() throws Exception {
        final int arbitraryNetError = -5;
        TestUrlRequestListener listener = startAndWaitForComplete(
                mMockUrlRequestJobFactory.getMockUrlWithFailure(
                        MOCK_SUCCESS_PATH,
                        MockUrlRequestJobFactory.FailurePhase.READ_ASYNC,
                        arbitraryNetError));
        assertEquals(200, listener.mResponseInfo.getHttpStatusCode());
        assertNotNull(listener.mError);
        assertEquals(arbitraryNetError, listener.mError.netError());
        assertFalse(listener.mOnRedirectCalled);
        assertTrue(listener.mOnErrorCalled);
        assertEquals(listener.mResponseStep, ResponseStep.ON_RESPONSE_STARTED);
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testUploadSetDataProvider() throws Exception {
        TestUrlRequestListener listener = new TestUrlRequestListener();
        UrlRequest urlRequest = mActivity.mUrlRequestContext.createRequest(
                NativeTestServer.getEchoBodyURL(), listener, listener.getExecutor());

        try {
            urlRequest.setUploadDataProvider(null, listener.getExecutor());
            fail("Exception not thrown");
        } catch (NullPointerException e) {
            assertEquals("Invalid UploadDataProvider.", e.getMessage());
        }

        TestUploadDataProvider dataProvider = new TestUploadDataProvider(
                TestUploadDataProvider.SuccessCallbackMode.SYNC, listener.getExecutor());
        urlRequest.setUploadDataProvider(dataProvider, listener.getExecutor());
        try {
            urlRequest.start();
            fail("Exception not thrown");
        } catch (IllegalArgumentException e) {
            assertEquals("Requests with upload data must have a Content-Type.", e.getMessage());
        }
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testUploadEmptyBodySync() throws Exception {
        TestUrlRequestListener listener = new TestUrlRequestListener();
        UrlRequest urlRequest = mActivity.mUrlRequestContext.createRequest(
                NativeTestServer.getEchoBodyURL(), listener, listener.getExecutor());

        TestUploadDataProvider dataProvider = new TestUploadDataProvider(
                TestUploadDataProvider.SuccessCallbackMode.SYNC, listener.getExecutor());
        urlRequest.setUploadDataProvider(dataProvider, listener.getExecutor());
        urlRequest.addHeader("Content-Type", "useless/string");
        urlRequest.start();
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
        UrlRequest urlRequest = mActivity.mUrlRequestContext.createRequest(
                NativeTestServer.getEchoBodyURL(), listener, listener.getExecutor());

        TestUploadDataProvider dataProvider = new TestUploadDataProvider(
                TestUploadDataProvider.SuccessCallbackMode.SYNC, listener.getExecutor());
        dataProvider.addRead("test".getBytes());
        urlRequest.setUploadDataProvider(dataProvider, listener.getExecutor());
        urlRequest.addHeader("Content-Type", "useless/string");
        urlRequest.start();
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
        UrlRequest urlRequest = mActivity.mUrlRequestContext.createRequest(
                NativeTestServer.getEchoBodyURL(), listener, listener.getExecutor());

        TestUploadDataProvider dataProvider = new TestUploadDataProvider(
                TestUploadDataProvider.SuccessCallbackMode.SYNC, listener.getExecutor());
        dataProvider.addRead("Y".getBytes());
        dataProvider.addRead("et ".getBytes());
        dataProvider.addRead("another ".getBytes());
        dataProvider.addRead("test".getBytes());

        urlRequest.setUploadDataProvider(dataProvider, listener.getExecutor());
        urlRequest.addHeader("Content-Type", "useless/string");
        urlRequest.start();
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
        UrlRequest urlRequest = mActivity.mUrlRequestContext.createRequest(
                NativeTestServer.getEchoBodyURL(), listener, listener.getExecutor());

        TestUploadDataProvider dataProvider = new TestUploadDataProvider(
                TestUploadDataProvider.SuccessCallbackMode.ASYNC, listener.getExecutor());
        dataProvider.addRead("Y".getBytes());
        dataProvider.addRead("et ".getBytes());
        dataProvider.addRead("another ".getBytes());
        dataProvider.addRead("test".getBytes());

        urlRequest.setUploadDataProvider(dataProvider, listener.getExecutor());
        urlRequest.addHeader("Content-Type", "useless/string");
        urlRequest.start();
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
        UrlRequest urlRequest = mActivity.mUrlRequestContext.createRequest(
                NativeTestServer.getEchoMethodURL(), listener, listener.getExecutor());

        TestUploadDataProvider dataProvider = new TestUploadDataProvider(
                TestUploadDataProvider.SuccessCallbackMode.SYNC, listener.getExecutor());
        dataProvider.addRead("test".getBytes());
        urlRequest.setUploadDataProvider(dataProvider, listener.getExecutor());
        urlRequest.addHeader("Content-Type", "useless/string");
        urlRequest.start();
        listener.blockForDone();

        assertEquals(200, listener.mResponseInfo.getHttpStatusCode());
        assertEquals("POST", listener.mResponseAsString);
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testUploadWithSetMethod() throws Exception {
        TestUrlRequestListener listener = new TestUrlRequestListener();
        UrlRequest urlRequest = mActivity.mUrlRequestContext.createRequest(
                NativeTestServer.getEchoMethodURL(), listener, listener.getExecutor());

        final String method = "PUT";
        urlRequest.setHttpMethod(method);

        TestUploadDataProvider dataProvider = new TestUploadDataProvider(
                TestUploadDataProvider.SuccessCallbackMode.SYNC, listener.getExecutor());
        dataProvider.addRead("test".getBytes());
        urlRequest.setUploadDataProvider(dataProvider, listener.getExecutor());
        urlRequest.addHeader("Content-Type", "useless/string");
        urlRequest.start();
        listener.blockForDone();

        assertEquals(200, listener.mResponseInfo.getHttpStatusCode());
        assertEquals("PUT", listener.mResponseAsString);
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testUploadRedirectSync() throws Exception {
        TestUrlRequestListener listener = new TestUrlRequestListener();
        UrlRequest urlRequest = mActivity.mUrlRequestContext.createRequest(
                NativeTestServer.getRedirectToEchoBody(), listener, listener.getExecutor());

        TestUploadDataProvider dataProvider = new TestUploadDataProvider(
                TestUploadDataProvider.SuccessCallbackMode.SYNC, listener.getExecutor());
        dataProvider.addRead("test".getBytes());
        urlRequest.setUploadDataProvider(dataProvider, listener.getExecutor());
        urlRequest.addHeader("Content-Type", "useless/string");
        urlRequest.start();
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
        UrlRequest urlRequest = mActivity.mUrlRequestContext.createRequest(
                NativeTestServer.getRedirectToEchoBody(), listener, listener.getExecutor());

        TestUploadDataProvider dataProvider = new TestUploadDataProvider(
                TestUploadDataProvider.SuccessCallbackMode.ASYNC, listener.getExecutor());
        dataProvider.addRead("test".getBytes());
        urlRequest.setUploadDataProvider(dataProvider, listener.getExecutor());
        urlRequest.addHeader("Content-Type", "useless/string");
        urlRequest.start();
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
        UrlRequest urlRequest = mActivity.mUrlRequestContext.createRequest(
                NativeTestServer.getEchoBodyURL(), listener, listener.getExecutor());

        TestUploadDataProvider dataProvider = new TestUploadDataProvider(
                TestUploadDataProvider.SuccessCallbackMode.SYNC, listener.getExecutor());
        dataProvider.setReadFailure(0, TestUploadDataProvider.FailMode.CALLBACK_SYNC);
        // This will never be read, but if the length is 0, read may never be
        // called.
        dataProvider.addRead("test".getBytes());
        urlRequest.setUploadDataProvider(dataProvider, listener.getExecutor());
        urlRequest.addHeader("Content-Type", "useless/string");
        urlRequest.start();
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
        UrlRequest urlRequest = mActivity.mUrlRequestContext.createRequest(
                NativeTestServer.getEchoBodyURL(), listener, listener.getExecutor());

        TestUploadDataProvider dataProvider = new TestUploadDataProvider(
                TestUploadDataProvider.SuccessCallbackMode.SYNC, listener.getExecutor());
        dataProvider.setReadFailure(0, TestUploadDataProvider.FailMode.CALLBACK_ASYNC);
        // This will never be read, but if the length is 0, read may never be
        // called.
        dataProvider.addRead("test".getBytes());
        urlRequest.setUploadDataProvider(dataProvider, listener.getExecutor());
        urlRequest.addHeader("Content-Type", "useless/string");
        urlRequest.start();
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
        UrlRequest urlRequest = mActivity.mUrlRequestContext.createRequest(
                NativeTestServer.getEchoBodyURL(), listener, listener.getExecutor());

        TestUploadDataProvider dataProvider = new TestUploadDataProvider(
                TestUploadDataProvider.SuccessCallbackMode.SYNC, listener.getExecutor());
        dataProvider.setReadFailure(0, TestUploadDataProvider.FailMode.THROWN);
        // This will never be read, but if the length is 0, read may never be
        // called.
        dataProvider.addRead("test".getBytes());
        urlRequest.setUploadDataProvider(dataProvider, listener.getExecutor());
        urlRequest.addHeader("Content-Type", "useless/string");
        urlRequest.start();
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
        UrlRequest urlRequest = mActivity.mUrlRequestContext.createRequest(
                NativeTestServer.getRedirectToEchoBody(), listener, listener.getExecutor());

        TestUploadDataProvider dataProvider = new TestUploadDataProvider(
                TestUploadDataProvider.SuccessCallbackMode.SYNC, listener.getExecutor());
        dataProvider.setRewindFailure(TestUploadDataProvider.FailMode.CALLBACK_SYNC);
        dataProvider.addRead("test".getBytes());
        urlRequest.setUploadDataProvider(dataProvider, listener.getExecutor());
        urlRequest.addHeader("Content-Type", "useless/string");
        urlRequest.start();
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
        UrlRequest urlRequest = mActivity.mUrlRequestContext.createRequest(
                NativeTestServer.getRedirectToEchoBody(), listener, listener.getExecutor());

        TestUploadDataProvider dataProvider = new TestUploadDataProvider(
                TestUploadDataProvider.SuccessCallbackMode.ASYNC, listener.getExecutor());
        dataProvider.setRewindFailure(TestUploadDataProvider.FailMode.CALLBACK_ASYNC);
        dataProvider.addRead("test".getBytes());
        urlRequest.setUploadDataProvider(dataProvider, listener.getExecutor());
        urlRequest.addHeader("Content-Type", "useless/string");
        urlRequest.start();
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
        UrlRequest urlRequest = mActivity.mUrlRequestContext.createRequest(
                NativeTestServer.getRedirectToEchoBody(), listener, listener.getExecutor());

        TestUploadDataProvider dataProvider = new TestUploadDataProvider(
                TestUploadDataProvider.SuccessCallbackMode.SYNC, listener.getExecutor());
        dataProvider.setRewindFailure(TestUploadDataProvider.FailMode.THROWN);
        dataProvider.addRead("test".getBytes());
        urlRequest.setUploadDataProvider(dataProvider, listener.getExecutor());
        urlRequest.addHeader("Content-Type", "useless/string");
        urlRequest.start();
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
        UrlRequest urlRequest = mActivity.mUrlRequestContext.createRequest(
                NativeTestServer.getEchoBodyURL(), listener, listener.getExecutor());

        TestUploadDataProvider dataProvider = new TestUploadDataProvider(
                TestUploadDataProvider.SuccessCallbackMode.SYNC, listener.getExecutor());
        dataProvider.addRead("test hello".getBytes());
        dataProvider.setChunked(true);
        urlRequest.setUploadDataProvider(dataProvider, listener.getExecutor());
        urlRequest.addHeader("Content-Type", "useless/string");

        assertEquals(-1, dataProvider.getLength());

        urlRequest.start();
        listener.blockForDone();

        // 1 read call for one data chunk.
        assertEquals(1, dataProvider.getNumReadCalls());
        assertEquals("test hello", listener.mResponseAsString);
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testUploadChunkedLastReadZeroLengthBody() throws Exception {
        TestUrlRequestListener listener = new TestUrlRequestListener();
        UrlRequest urlRequest = mActivity.mUrlRequestContext.createRequest(
                NativeTestServer.getEchoBodyURL(), listener, listener.getExecutor());

        TestUploadDataProvider dataProvider = new TestUploadDataProvider(
                TestUploadDataProvider.SuccessCallbackMode.SYNC, listener.getExecutor());
        // Add 3 reads. The last read has a 0-length body.
        dataProvider.addRead("hello there".getBytes());
        dataProvider.addRead("!".getBytes());
        dataProvider.addRead("".getBytes());
        dataProvider.setChunked(true);
        urlRequest.setUploadDataProvider(dataProvider, listener.getExecutor());
        urlRequest.addHeader("Content-Type", "useless/string");

        assertEquals(-1, dataProvider.getLength());

        urlRequest.start();
        listener.blockForDone();

        // 2 read call for the first two data chunks, and 1 for final chunk.
        assertEquals(3, dataProvider.getNumReadCalls());
        assertEquals("hello there!", listener.mResponseAsString);
    }

    private void throwOrCancel(FailureType failureType, ResponseStep failureStep,
            boolean expectResponseInfo, boolean expectError) {
        TestUrlRequestListener listener = new TestUrlRequestListener();
        listener.setFailure(failureType, failureStep);
        UrlRequest urlRequest = mActivity.mUrlRequestContext.createRequest(
                MockUrlRequestJobFactory.REDIRECT_URL,
                listener, listener.getExecutor());
        urlRequest.start();
        listener.blockForDone();
        assertTrue(listener.mOnRedirectCalled);
        assertEquals(listener.mResponseStep, failureStep);
        assertTrue(urlRequest.isCanceled());
        assertEquals(expectResponseInfo, listener.mResponseInfo != null);
        assertEquals(expectError, listener.mError != null);
        assertEquals(expectError, listener.mOnErrorCalled);
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testFailures() throws Exception {
        throwOrCancel(FailureType.CANCEL_SYNC, ResponseStep.ON_REDIRECT,
                false, false);
        throwOrCancel(FailureType.CANCEL_ASYNC, ResponseStep.ON_REDIRECT,
                false, false);
        throwOrCancel(FailureType.THROW_SYNC, ResponseStep.ON_REDIRECT,
                false, true);

        throwOrCancel(FailureType.CANCEL_SYNC, ResponseStep.ON_RESPONSE_STARTED,
                true, false);
        throwOrCancel(FailureType.CANCEL_ASYNC, ResponseStep.ON_RESPONSE_STARTED,
                true, false);
        throwOrCancel(FailureType.THROW_SYNC, ResponseStep.ON_RESPONSE_STARTED,
                true, true);
        throwOrCancel(FailureType.CANCEL_SYNC, ResponseStep.ON_DATA_RECEIVED,
                true, false);
        throwOrCancel(FailureType.CANCEL_ASYNC, ResponseStep.ON_DATA_RECEIVED,
                true, false);
        throwOrCancel(FailureType.THROW_SYNC, ResponseStep.ON_DATA_RECEIVED,
                true, true);
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testThrowON_SUCCEEDED() {
        TestUrlRequestListener listener = new TestUrlRequestListener();
        listener.setFailure(FailureType.THROW_SYNC, ResponseStep.ON_SUCCEEDED);
        UrlRequest urlRequest = mActivity.mUrlRequestContext.createRequest(
                MockUrlRequestJobFactory.REDIRECT_URL,
                listener, listener.getExecutor());
        urlRequest.start();
        listener.blockForDone();
        assertTrue(listener.mOnRedirectCalled);
        assertEquals(listener.mResponseStep, ResponseStep.ON_SUCCEEDED);
        assertFalse(urlRequest.isCanceled());
        assertNotNull(listener.mResponseInfo);
        assertNull(listener.mError);
        assertFalse(listener.mOnErrorCalled);
    }
}
