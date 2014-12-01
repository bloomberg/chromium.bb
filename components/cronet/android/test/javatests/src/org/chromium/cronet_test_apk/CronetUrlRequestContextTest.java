// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.cronet_test_apk;

import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.PathUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.cronet_test_apk.TestUrlRequestListener.FailureType;
import org.chromium.cronet_test_apk.TestUrlRequestListener.ResponseStep;
import org.chromium.net.ExtendedResponseInfo;
import org.chromium.net.ResponseInfo;
import org.chromium.net.UrlRequest;
import org.chromium.net.UrlRequestContextConfig;
import org.chromium.net.UrlRequestException;

import java.io.File;
import java.nio.ByteBuffer;
import java.util.Arrays;

/**
 * Test CronetUrlRequestContext.
 */
public class CronetUrlRequestContextTest extends CronetTestBase {
    // URLs used for tests.
    private static final String TEST_URL = "http://127.0.0.1:8000";
    private static final String MOCK_CRONET_TEST_FAILED_URL =
            "http://mock.failed.request/-2";
    private static final String MOCK_CRONET_TEST_SUCCESS_URL =
            "http://mock.http/success.txt";

    CronetTestActivity mActivity;

    /**
     * Listener that shutdowns the request context when request has succeeded
     * or failed.
     */
    class ShutdownTestUrlRequestListener extends TestUrlRequestListener {
        @Override
        public void onDataReceived(UrlRequest request,
                ResponseInfo info,
                ByteBuffer byteBuffer) {
            assertTrue(byteBuffer.capacity() != 0);
            byte[] receivedDataBefore = new byte[byteBuffer.capacity()];
            byteBuffer.get(receivedDataBefore);
            // super will block if necessary.
            super.onDataReceived(request, info, byteBuffer);
            // |byteBuffer| is still accessible even if 'cancel' was called on
            // another thread.
            assertTrue(byteBuffer.capacity() != 0);
            byte[] receivedDataAfter = new byte[byteBuffer.capacity()];
            byteBuffer.get(receivedDataAfter);
            assertTrue(Arrays.equals(receivedDataBefore, receivedDataAfter));
        }

        @Override
        public void onSucceeded(UrlRequest request, ExtendedResponseInfo info) {
            super.onSucceeded(request, info);
            mActivity.mUrlRequestContext.shutdown();
        }

        @Override
        public void onFailed(UrlRequest request,
                ResponseInfo info,
                UrlRequestException error) {
            super.onFailed(request, info, error);
            mActivity.mUrlRequestContext.shutdown();
        }
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testConfigUserAgent() throws Exception {
        String userAgentName = "User-Agent";
        String userAgentValue = "User-Agent-Value";
        UrlRequestContextConfig config = new UrlRequestContextConfig();
        config.setUserAgent(userAgentValue);
        config.setLibraryName("cronet_tests");
        String[] commandLineArgs = {
                CronetTestActivity.CONFIG_KEY, config.toString()
        };
        mActivity = launchCronetTestAppWithUrlAndCommandLineArgs(TEST_URL,
                commandLineArgs);
        waitForActiveShellToBeDoneLoading();
        assertTrue(UploadTestServer.startUploadTestServer());
        TestUrlRequestListener listener = new TestUrlRequestListener();
        UrlRequest urlRequest = mActivity.mUrlRequestContext.createRequest(
                UploadTestServer.getEchoHeaderURL(userAgentName), listener,
                listener.getExecutor());
        urlRequest.start();
        listener.blockForDone();
        assertEquals(userAgentValue, listener.mResponseAsString);
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testShutdown() throws Exception {
        mActivity = launchCronetTestApp();
        TestUrlRequestListener listener = new ShutdownTestUrlRequestListener();
        // Block listener when response starts to verify that shutdown fails
        // if there are active requests.
        listener.setFailure(FailureType.BLOCK,
                ResponseStep.ON_RESPONSE_STARTED);
        UrlRequest urlRequest = mActivity.mUrlRequestContext.createRequest(
                TEST_URL, listener, listener.getExecutor());
        urlRequest.start();
        try {
            mActivity.mUrlRequestContext.shutdown();
            fail("Should throw an exception");
        } catch (Exception e) {
            assertEquals("Cannot shutdown with active requests.",
                         e.getMessage());
        }
        listener.openBlockedStep();
        listener.blockForDone();
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testShutdownAfterError() throws Exception {
        mActivity = launchCronetTestApp();
        TestUrlRequestListener listener = new ShutdownTestUrlRequestListener();
        UrlRequest urlRequest = mActivity.mUrlRequestContext.createRequest(
                MOCK_CRONET_TEST_FAILED_URL, listener, listener.getExecutor());
        urlRequest.start();
        listener.blockForDone();
        assertTrue(listener.mOnErrorCalled);
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testShutdownAfterCancel() throws Exception {
        mActivity = launchCronetTestApp();
        TestUrlRequestListener listener = new TestUrlRequestListener();
        // Block listener when response starts to verify that shutdown fails
        // if there are active requests.
        listener.setFailure(FailureType.BLOCK,
                ResponseStep.ON_RESPONSE_STARTED);
        UrlRequest urlRequest = mActivity.mUrlRequestContext.createRequest(
                TEST_URL, listener, listener.getExecutor());
        urlRequest.start();
        try {
            mActivity.mUrlRequestContext.shutdown();
            fail("Should throw an exception");
        } catch (Exception e) {
            assertEquals("Cannot shutdown with active requests.",
                         e.getMessage());
        }
        urlRequest.cancel();
        mActivity.mUrlRequestContext.shutdown();
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testShutdownAfterCancelDuringOnDataReceived() throws Exception {
        mActivity = launchCronetTestApp();
        TestUrlRequestListener listener = new TestUrlRequestListener();
        // Block listener when response starts to verify that shutdown fails
        // if there are active requests.
        listener.setFailure(FailureType.BLOCK, ResponseStep.ON_DATA_RECEIVED);
        UrlRequest urlRequest = mActivity.mUrlRequestContext.createRequest(
                MOCK_CRONET_TEST_SUCCESS_URL, listener, listener.getExecutor());
        urlRequest.start();
        try {
            mActivity.mUrlRequestContext.shutdown();
            fail("Should throw an exception");
        } catch (Exception e) {
            assertEquals("Cannot shutdown with active requests.",
                         e.getMessage());
        }
        // This cancel happens during 'onDataReceived' step, but cancel is
        // delayed until listener call returns as it is accessing direct
        // data buffer owned by request.
        urlRequest.cancel();
        assertTrue(urlRequest.isCanceled());
        Thread.sleep(1000);
        // Cancel happens when listener returns.
        listener.openBlockedStep();
        Thread.sleep(1000);

        mActivity.mUrlRequestContext.shutdown();
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testNetLog() throws Exception {
        mActivity = launchCronetTestApp();
        waitForActiveShellToBeDoneLoading();
        File directory = new File(PathUtils.getDataDirectory(
                getInstrumentation().getTargetContext()));
        File file = File.createTempFile("cronet", "json", directory);
        mActivity.mUrlRequestContext.startNetLogToFile(file.getPath());
        // Start a request.
        TestUrlRequestListener listener = new TestUrlRequestListener();
        UrlRequest urlRequest = mActivity.mUrlRequestContext.createRequest(
                TEST_URL, listener, listener.getExecutor());
        urlRequest.start();
        listener.blockForDone();
        mActivity.mUrlRequestContext.stopNetLog();
        assertTrue(file.exists());
        assertTrue(file.length() != 0);
        assertTrue(file.delete());
        assertTrue(!file.exists());
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testNetLogAfterShutdown() throws Exception {
        mActivity = launchCronetTestApp();
        waitForActiveShellToBeDoneLoading();
        TestUrlRequestListener listener = new TestUrlRequestListener();
        UrlRequest urlRequest = mActivity.mUrlRequestContext.createRequest(
                TEST_URL, listener, listener.getExecutor());
        urlRequest.start();
        listener.blockForDone();
        mActivity.mUrlRequestContext.shutdown();

        File directory = new File(PathUtils.getDataDirectory(
                getInstrumentation().getTargetContext()));
        File file = File.createTempFile("cronet", "json", directory);
        mActivity.mUrlRequestContext.startNetLogToFile(file.getPath());
        mActivity.mUrlRequestContext.stopNetLog();
        assertTrue(file.exists());
        assertTrue(file.length() == 0);
        assertTrue(file.delete());
        assertTrue(!file.exists());
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testNetLogStartMultipleTimes() throws Exception {
        mActivity = launchCronetTestApp();
        waitForActiveShellToBeDoneLoading();
        File directory = new File(PathUtils.getDataDirectory(
                getInstrumentation().getTargetContext()));
        File file = File.createTempFile("cronet", "json", directory);
        // Start NetLog multiple times.
        mActivity.mUrlRequestContext.startNetLogToFile(file.getPath());
        mActivity.mUrlRequestContext.startNetLogToFile(file.getPath());
        mActivity.mUrlRequestContext.startNetLogToFile(file.getPath());
        mActivity.mUrlRequestContext.startNetLogToFile(file.getPath());
        // Start a request.
        TestUrlRequestListener listener = new TestUrlRequestListener();
        UrlRequest urlRequest = mActivity.mUrlRequestContext.createRequest(
                TEST_URL, listener, listener.getExecutor());
        urlRequest.start();
        listener.blockForDone();
        mActivity.mUrlRequestContext.stopNetLog();
        assertTrue(file.exists());
        assertTrue(file.length() != 0);
        assertTrue(file.delete());
        assertTrue(!file.exists());
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testNetLogStopMultipleTimes() throws Exception {
        mActivity = launchCronetTestApp();
        waitForActiveShellToBeDoneLoading();
        File directory = new File(PathUtils.getDataDirectory(
                getInstrumentation().getTargetContext()));
        File file = File.createTempFile("cronet", "json", directory);
        mActivity.mUrlRequestContext.startNetLogToFile(file.getPath());
        // Start a request.
        TestUrlRequestListener listener = new TestUrlRequestListener();
        UrlRequest urlRequest = mActivity.mUrlRequestContext.createRequest(
                TEST_URL, listener, listener.getExecutor());
        urlRequest.start();
        listener.blockForDone();
        // Stop NetLog multiple times.
        mActivity.mUrlRequestContext.stopNetLog();
        mActivity.mUrlRequestContext.stopNetLog();
        mActivity.mUrlRequestContext.stopNetLog();
        mActivity.mUrlRequestContext.stopNetLog();
        mActivity.mUrlRequestContext.stopNetLog();
        assertTrue(file.exists());
        assertTrue(file.length() != 0);
        assertTrue(file.delete());
        assertTrue(!file.exists());
    }
}
