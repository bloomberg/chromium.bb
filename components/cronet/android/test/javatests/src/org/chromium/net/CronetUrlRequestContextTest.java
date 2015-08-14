// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import android.content.Context;
import android.content.ContextWrapper;
import android.os.ConditionVariable;
import android.os.Handler;
import android.os.Looper;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.PathUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.net.TestUrlRequestListener.ResponseStep;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;

/**
 * Test CronetUrlRequestContext.
 */
public class CronetUrlRequestContextTest extends CronetTestBase {
    // URLs used for tests.
    private static final String TEST_URL = "http://127.0.0.1:8000";
    private static final String URL_404 = "http://127.0.0.1:8000/notfound404";
    private static final String MOCK_CRONET_TEST_FAILED_URL =
            "http://mock.failed.request/-2";
    private static final String MOCK_CRONET_TEST_SUCCESS_URL =
            "http://mock.http/success.txt";

    CronetTestActivity mActivity;

    static class RequestThread extends Thread {
        public TestUrlRequestListener mListener;

        final CronetTestActivity mActivity;
        final String mUrl;
        final ConditionVariable mRunBlocker;

        public RequestThread(CronetTestActivity activity, String url,
                ConditionVariable runBlocker) {
            mActivity = activity;
            mUrl = url;
            mRunBlocker = runBlocker;
        }

        @Override
        public void run() {
            mRunBlocker.block();
            UrlRequestContext requestContext = mActivity.initRequestContext();
            mListener = new TestUrlRequestListener();
            UrlRequest urlRequest =
                    requestContext.createRequest(mUrl, mListener, mListener.getExecutor());
            urlRequest.start();
            mListener.blockForDone();
        }
    }

    /**
     * Listener that shutdowns the request context when request has succeeded
     * or failed.
     */
    class ShutdownTestUrlRequestListener extends TestUrlRequestListener {
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
        assertTrue(NativeTestServer.startNativeTestServer(
                getInstrumentation().getTargetContext()));
        TestUrlRequestListener listener = new TestUrlRequestListener();
        UrlRequest urlRequest = mActivity.mUrlRequestContext.createRequest(
                NativeTestServer.getEchoHeaderURL(userAgentName), listener,
                listener.getExecutor());
        urlRequest.start();
        listener.blockForDone();
        assertEquals(userAgentValue, listener.mResponseAsString);
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testDataReductionProxyEnabled() throws Exception {
        mActivity = launchCronetTestAppAndSkipFactoryInit();

        // Ensure native code is loaded before trying to start test server.
        UrlRequestContext.createContext(
                getInstrumentation().getTargetContext(),
                new UrlRequestContextConfig().setLibraryName("cronet_tests"))
                .shutdown();

        assertTrue(NativeTestServer.startNativeTestServer(
                getInstrumentation().getTargetContext()));
        if (!NativeTestServer.isDataReductionProxySupported()) {
            return;
        }
        String serverHostPort = NativeTestServer.getHostPort();

        // Enable the Data Reduction Proxy and configure it to use the test
        // server as its primary proxy, and to check successfully that this
        // proxy is OK to use.
        UrlRequestContextConfig config = new UrlRequestContextConfig();
        config.enableDataReductionProxy("test-key");
        config.setDataReductionProxyOptions(
                serverHostPort, "unused.net:9999",
                NativeTestServer.getFileURL("/secureproxychecksuccess.txt"));
        config.setLibraryName("cronet_tests");
        mActivity.mUrlRequestContext = UrlRequestContext.createContext(
                getInstrumentation().getTargetContext(), config);
        TestUrlRequestListener listener = new TestUrlRequestListener();

        // Construct and start a request that can only be returned by the test
        // server. This request will fail if the configuration logic for the
        // Data Reduction Proxy is not used.
        UrlRequest urlRequest = mActivity.mUrlRequestContext.createRequest(
                "http://DomainThatDoesnt.Resolve/datareductionproxysuccess.txt",
                listener, listener.getExecutor());
        urlRequest.start();
        listener.blockForDone();

        // Verify that the request is successful and that the Data Reduction
        // Proxy logic configured to use the test server as its proxy.
        assertEquals(200, listener.mResponseInfo.getHttpStatusCode());
        assertEquals(serverHostPort, listener.mResponseInfo.getProxyServer());
        assertEquals(
                "http://DomainThatDoesnt.Resolve/datareductionproxysuccess.txt",
                listener.mResponseInfo.getUrl());
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testShutdown() throws Exception {
        mActivity = launchCronetTestApp();
        TestUrlRequestListener listener = new ShutdownTestUrlRequestListener();
        // Block listener when response starts to verify that shutdown fails
        // if there are active requests.
        listener.setAutoAdvance(false);
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

        listener.waitForNextStep();
        assertEquals(ResponseStep.ON_RESPONSE_STARTED, listener.mResponseStep);
        try {
            mActivity.mUrlRequestContext.shutdown();
            fail("Should throw an exception");
        } catch (Exception e) {
            assertEquals("Cannot shutdown with active requests.",
                         e.getMessage());
        }
        listener.startNextRead(urlRequest);

        listener.waitForNextStep();
        assertEquals(ResponseStep.ON_READ_COMPLETED, listener.mResponseStep);
        try {
            mActivity.mUrlRequestContext.shutdown();
            fail("Should throw an exception");
        } catch (Exception e) {
            assertEquals("Cannot shutdown with active requests.",
                         e.getMessage());
        }

        // May not have read all the data, in theory. Just enable auto-advance
        // and finish the request.
        listener.setAutoAdvance(true);
        listener.startNextRead(urlRequest);
        listener.blockForDone();
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testShutdownDuringInit() throws Exception {
        final CronetTestActivity activity = launchCronetTestAppAndSkipFactoryInit();
        final ConditionVariable block = new ConditionVariable(false);

        // Post a task to main thread to block until shutdown is called to test
        // scenario when shutdown is called right after construction before
        // context is fully initialized on the main thread.
        Runnable blockingTask = new Runnable() {
            public void run() {
                try {
                    block.block();
                } catch (Exception e) {
                    fail("Caught " + e.getMessage());
                }
            }
        };
        // Ensure that test is not running on the main thread.
        assertTrue(Looper.getMainLooper() != Looper.myLooper());
        new Handler(Looper.getMainLooper()).post(blockingTask);

        // Create new request context, but its initialization on the main thread
        // will be stuck behind blockingTask.
        final UrlRequestContext requestContext = activity.initRequestContext();
        // Unblock the main thread, so context gets initialized and shutdown on
        // it.
        block.open();
        // Shutdown will wait for init to complete on main thread.
        requestContext.shutdown();
        // Verify that context is shutdown.
        try {
            requestContext.stopNetLog();
            fail("Should throw an exception.");
        } catch (Exception e) {
            assertEquals("Context is shut down.", e.getMessage());
        }
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testInitAndShutdownOnMainThread() throws Exception {
        final CronetTestActivity activity = launchCronetTestAppAndSkipFactoryInit();
        final ConditionVariable block = new ConditionVariable(false);

        // Post a task to main thread to init and shutdown on the main thread.
        Runnable blockingTask = new Runnable() {
            public void run() {
                // Create new request context, loading the library.
                final UrlRequestContext requestContext = activity.initRequestContext();
                // Shutdown right after init.
                requestContext.shutdown();
                // Verify that context is shutdown.
                try {
                    requestContext.stopNetLog();
                    fail("Should throw an exception.");
                } catch (Exception e) {
                    assertEquals("Context is shut down.", e.getMessage());
                }
                block.open();
            }
        };
        new Handler(Looper.getMainLooper()).post(blockingTask);
        // Wait for shutdown to complete on main thread.
        block.block();
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testMultipleShutdown() throws Exception {
        mActivity = launchCronetTestApp();
        try {
            mActivity.mUrlRequestContext.shutdown();
            mActivity.mUrlRequestContext.shutdown();
            fail("Should throw an exception");
        } catch (Exception e) {
            assertEquals("Context is shut down.",
                         e.getMessage());
        }
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
        listener.setAutoAdvance(false);
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
        listener.waitForNextStep();
        assertEquals(ResponseStep.ON_RESPONSE_STARTED, listener.mResponseStep);
        urlRequest.cancel();
        mActivity.mUrlRequestContext.shutdown();
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testNetLog() throws Exception {
        Context context = getInstrumentation().getTargetContext();
        File directory = new File(PathUtils.getDataDirectory(context));
        File file = File.createTempFile("cronet", "json", directory);
        CronetUrlRequestContext requestContext = new CronetUrlRequestContext(
                context,
                new UrlRequestContextConfig().setLibraryName("cronet_tests"));
        // Start NetLog immediately after the request context is created to make
        // sure that the call won't crash the app even when the native request
        // context is not fully initialized. See crbug.com/470196.
        requestContext.startNetLogToFile(file.getPath(), false);

        // Start a request.
        TestUrlRequestListener listener = new TestUrlRequestListener();
        UrlRequest request = requestContext.createRequest(
                TEST_URL, listener, listener.getExecutor());
        request.start();
        listener.blockForDone();
        requestContext.stopNetLog();
        assertTrue(file.exists());
        assertTrue(file.length() != 0);
        assertFalse(hasBytesInNetLog(file));
        assertTrue(file.delete());
        assertTrue(!file.exists());
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testNetLogAfterShutdown() throws Exception {
        mActivity = launchCronetTestApp();
        TestUrlRequestListener listener = new TestUrlRequestListener();
        UrlRequest urlRequest = mActivity.mUrlRequestContext.createRequest(
                TEST_URL, listener, listener.getExecutor());
        urlRequest.start();
        listener.blockForDone();
        mActivity.mUrlRequestContext.shutdown();

        File directory = new File(PathUtils.getDataDirectory(
                getInstrumentation().getTargetContext()));
        File file = File.createTempFile("cronet", "json", directory);
        try {
            mActivity.mUrlRequestContext.startNetLogToFile(file.getPath(),
                    false);
            fail("Should throw an exception.");
        } catch (Exception e) {
            assertEquals("Context is shut down.", e.getMessage());
        }
        assertFalse(hasBytesInNetLog(file));
        assertTrue(file.delete());
        assertTrue(!file.exists());
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testNetLogStartMultipleTimes() throws Exception {
        mActivity = launchCronetTestApp();
        File directory = new File(PathUtils.getDataDirectory(
                getInstrumentation().getTargetContext()));
        File file = File.createTempFile("cronet", "json", directory);
        // Start NetLog multiple times.
        mActivity.mUrlRequestContext.startNetLogToFile(file.getPath(), false);
        mActivity.mUrlRequestContext.startNetLogToFile(file.getPath(), false);
        mActivity.mUrlRequestContext.startNetLogToFile(file.getPath(), false);
        mActivity.mUrlRequestContext.startNetLogToFile(file.getPath(), false);
        // Start a request.
        TestUrlRequestListener listener = new TestUrlRequestListener();
        UrlRequest urlRequest = mActivity.mUrlRequestContext.createRequest(
                TEST_URL, listener, listener.getExecutor());
        urlRequest.start();
        listener.blockForDone();
        mActivity.mUrlRequestContext.stopNetLog();
        assertTrue(file.exists());
        assertTrue(file.length() != 0);
        assertFalse(hasBytesInNetLog(file));
        assertTrue(file.delete());
        assertTrue(!file.exists());
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testNetLogStopMultipleTimes() throws Exception {
        mActivity = launchCronetTestApp();
        File directory = new File(PathUtils.getDataDirectory(
                getInstrumentation().getTargetContext()));
        File file = File.createTempFile("cronet", "json", directory);
        mActivity.mUrlRequestContext.startNetLogToFile(file.getPath(), false);
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
        assertFalse(hasBytesInNetLog(file));
        assertTrue(file.delete());
        assertTrue(!file.exists());
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testNetLogWithBytes() throws Exception {
        Context context = getInstrumentation().getTargetContext();
        File directory = new File(PathUtils.getDataDirectory(context));
        File file = File.createTempFile("cronet", "json", directory);
        CronetUrlRequestContext requestContext = new CronetUrlRequestContext(
                context,
                new UrlRequestContextConfig().setLibraryName("cronet_tests"));
        // Start NetLog with logAll as true.
        requestContext.startNetLogToFile(file.getPath(), true);
        // Start a request.
        TestUrlRequestListener listener = new TestUrlRequestListener();
        UrlRequest request = requestContext.createRequest(
                TEST_URL, listener, listener.getExecutor());
        request.start();
        listener.blockForDone();
        requestContext.stopNetLog();
        assertTrue(file.exists());
        assertTrue(file.length() != 0);
        assertTrue(hasBytesInNetLog(file));
        assertTrue(file.delete());
        assertTrue(!file.exists());
    }

    private boolean hasBytesInNetLog(File logFile) throws Exception {
        BufferedReader logReader = new BufferedReader(new FileReader(logFile));
        try {
            String logLine;
            while ((logLine = logReader.readLine()) != null) {
                if (logLine.contains("\"hex_encoded_bytes\"")) {
                    return true;
                }
            }
            return false;
        } finally {
            logReader.close();
        }
    }

    private void enableCache(int cacheType) throws Exception {
        String cacheTypeString = "";
        if (cacheType == UrlRequestContextConfig.HTTP_CACHE_DISK) {
            cacheTypeString = CronetTestActivity.CACHE_DISK;
        } else if (cacheType == UrlRequestContextConfig.HTTP_CACHE_DISK_NO_HTTP) {
            cacheTypeString = CronetTestActivity.CACHE_DISK_NO_HTTP;
        } else if (cacheType == UrlRequestContextConfig.HTTP_CACHE_IN_MEMORY) {
            cacheTypeString = CronetTestActivity.CACHE_IN_MEMORY;
        }
        String[] commandLineArgs = {CronetTestActivity.CACHE_KEY, cacheTypeString};
        mActivity = launchCronetTestAppWithUrlAndCommandLineArgs(null,
                commandLineArgs);
        assertTrue(NativeTestServer.startNativeTestServer(
                getInstrumentation().getTargetContext()));
    }

    private void checkRequestCaching(String url, boolean expectCached) {
        checkRequestCaching(url, expectCached, false);
    }

    private void checkRequestCaching(String url, boolean expectCached,
            boolean disableCache) {
        TestUrlRequestListener listener = new TestUrlRequestListener();
        UrlRequest urlRequest = mActivity.mUrlRequestContext.createRequest(
                url, listener, listener.getExecutor());
        if (disableCache) {
            urlRequest.disableCache();
        }
        urlRequest.start();
        listener.blockForDone();
        assertEquals(expectCached, listener.mResponseInfo.wasCached());
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testEnableHttpCacheDisabled() throws Exception {
        enableCache(UrlRequestContextConfig.HTTP_CACHE_DISABLED);
        String url = NativeTestServer.getFileURL("/cacheable.txt");
        checkRequestCaching(url, false);
        checkRequestCaching(url, false);
        checkRequestCaching(url, false);
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testEnableHttpCacheInMemory() throws Exception {
        enableCache(UrlRequestContextConfig.HTTP_CACHE_IN_MEMORY);
        String url = NativeTestServer.getFileURL("/cacheable.txt");
        checkRequestCaching(url, false);
        checkRequestCaching(url, true);
        NativeTestServer.shutdownNativeTestServer();
        checkRequestCaching(url, true);
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testEnableHttpCacheDisk() throws Exception {
        enableCache(UrlRequestContextConfig.HTTP_CACHE_DISK);
        String url = NativeTestServer.getFileURL("/cacheable.txt");
        checkRequestCaching(url, false);
        checkRequestCaching(url, true);
        NativeTestServer.shutdownNativeTestServer();
        checkRequestCaching(url, true);
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testEnableHttpCacheDiskNoHttp() throws Exception {
        enableCache(UrlRequestContextConfig.HTTP_CACHE_DISABLED);
        String url = NativeTestServer.getFileURL("/cacheable.txt");
        checkRequestCaching(url, false);
        checkRequestCaching(url, false);
        checkRequestCaching(url, false);
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testDisableCache() throws Exception {
        enableCache(UrlRequestContextConfig.HTTP_CACHE_DISK);
        String url = NativeTestServer.getFileURL("/cacheable.txt");

        // When cache is disabled, making a request does not write to the cache.
        checkRequestCaching(url, false, true /** disable cache */);
        checkRequestCaching(url, false);

        // When cache is enabled, the second request is cached.
        checkRequestCaching(url, false, true /** disable cache */);
        checkRequestCaching(url, true);

        // Shut down the server, next request should have a cached response.
        NativeTestServer.shutdownNativeTestServer();
        checkRequestCaching(url, true);

        // Cache is disabled after server is shut down, request should fail.
        TestUrlRequestListener listener = new TestUrlRequestListener();
        UrlRequest urlRequest = mActivity.mUrlRequestContext.createRequest(
                url, listener, listener.getExecutor());
        urlRequest.disableCache();
        urlRequest.start();
        listener.blockForDone();
        assertNotNull(listener.mError);
        assertEquals("Exception in CronetUrlRequest: net::ERR_CONNECTION_REFUSED",
                listener.mError.getMessage());
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testEnableHttpCacheDiskNewContext() throws Exception {
        enableCache(UrlRequestContextConfig.HTTP_CACHE_DISK);
        String url = NativeTestServer.getFileURL("/cacheable.txt");
        checkRequestCaching(url, false);
        checkRequestCaching(url, true);
        NativeTestServer.shutdownNativeTestServer();
        checkRequestCaching(url, true);

        // Shutdown original context and create another that uses the same cache.
        mActivity.mUrlRequestContext.shutdown();
        mActivity.mUrlRequestContext = UrlRequestContext.createContext(
                getInstrumentation().getTargetContext(), mActivity.getContextConfig());
        checkRequestCaching(url, true);
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testInitContextAndStartRequest() {
        CronetTestActivity activity = launchCronetTestAppAndSkipFactoryInit();

        // Immediately make a request after initializing the context.
        UrlRequestContext requestContext = activity.initRequestContext();
        TestUrlRequestListener listener = new TestUrlRequestListener();
        UrlRequest urlRequest =
                requestContext.createRequest(TEST_URL, listener, listener.getExecutor());
        urlRequest.start();
        listener.blockForDone();
        assertEquals(200, listener.mResponseInfo.getHttpStatusCode());
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testInitContextStartTwoRequests() throws Exception {
        CronetTestActivity activity = launchCronetTestAppAndSkipFactoryInit();

        // Make two requests after initializing the context.
        UrlRequestContext requestContext = activity.initRequestContext();
        int[] statusCodes = {0, 0};
        String[] urls = {TEST_URL, URL_404};
        for (int i = 0; i < 2; i++) {
            TestUrlRequestListener listener = new TestUrlRequestListener();
            UrlRequest urlRequest =
                    requestContext.createRequest(urls[i], listener, listener.getExecutor());
            urlRequest.start();
            listener.blockForDone();
            statusCodes[i] = listener.mResponseInfo.getHttpStatusCode();
        }
        assertEquals(200, statusCodes[0]);
        assertEquals(404, statusCodes[1]);
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testInitTwoContextsSimultaneously() throws Exception {
        final CronetTestActivity activity = launchCronetTestAppAndSkipFactoryInit();

        // Threads will block on runBlocker to ensure simultaneous execution.
        ConditionVariable runBlocker = new ConditionVariable(false);
        RequestThread thread1 = new RequestThread(activity, TEST_URL, runBlocker);
        RequestThread thread2 = new RequestThread(activity, URL_404, runBlocker);

        thread1.start();
        thread2.start();
        runBlocker.open();
        thread1.join();
        thread2.join();
        assertEquals(200, thread1.mListener.mResponseInfo.getHttpStatusCode());
        assertEquals(404, thread2.mListener.mResponseInfo.getHttpStatusCode());
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testInitTwoContextsInSequence() throws Exception {
        final CronetTestActivity activity = launchCronetTestAppAndSkipFactoryInit();

        ConditionVariable runBlocker = new ConditionVariable(true);
        RequestThread thread1 = new RequestThread(activity, TEST_URL, runBlocker);
        RequestThread thread2 = new RequestThread(activity, URL_404, runBlocker);

        thread1.start();
        thread1.join();
        thread2.start();
        thread2.join();
        assertEquals(200, thread1.mListener.mResponseInfo.getHttpStatusCode());
        assertEquals(404, thread2.mListener.mResponseInfo.getHttpStatusCode());
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testInitDifferentContexts() throws Exception {
        // Test that concurrently instantiating Cronet context's upon various
        // different versions of the same Android Context does not cause crashes
        // like crbug.com/453845
        mActivity = launchCronetTestApp();
        CronetUrlRequestContext firstContext =
                new CronetUrlRequestContext(mActivity, mActivity.getContextConfig());
        CronetUrlRequestContext secondContext = new CronetUrlRequestContext(
                mActivity.getApplicationContext(), mActivity.getContextConfig());
        CronetUrlRequestContext thirdContext = new CronetUrlRequestContext(
                new ContextWrapper(mActivity), mActivity.getContextConfig());
        firstContext.shutdown();
        secondContext.shutdown();
        thirdContext.shutdown();
    }
}
