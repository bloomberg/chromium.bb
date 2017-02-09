// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import android.content.Context;
import android.content.ContextWrapper;
import android.os.ConditionVariable;
import android.os.Handler;
import android.os.Looper;
import android.os.StrictMode;
import android.support.test.filters.SmallTest;

import org.json.JSONObject;

import static org.chromium.net.CronetEngine.Builder.HTTP_CACHE_IN_MEMORY;

import org.chromium.base.FileUtils;
import org.chromium.base.Log;
import org.chromium.base.PathUtils;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.SuppressFBWarnings;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.MetricsUtils.HistogramDelta;
import org.chromium.net.MetricsTestUtil.TestExecutor;
import org.chromium.net.TestUrlRequestCallback.ResponseStep;
import org.chromium.net.impl.CronetEngineBase;
import org.chromium.net.impl.CronetEngineBuilderImpl;
import org.chromium.net.impl.CronetLibraryLoader;
import org.chromium.net.impl.CronetUrlRequestContext;
import org.chromium.net.test.EmbeddedTestServer;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileReader;
import java.io.IOException;
import java.net.URL;
import java.util.Arrays;
import java.util.concurrent.Executor;
import java.util.concurrent.Executors;
import java.util.concurrent.ThreadFactory;
import java.util.concurrent.atomic.AtomicReference;

/**
 * Test CronetEngine.
 */
@JNINamespace("cronet")
public class CronetUrlRequestContextTest extends CronetTestBase {
    private static final String TAG = CronetUrlRequestContextTest.class.getSimpleName();

    // URLs used for tests.
    private static final String MOCK_CRONET_TEST_FAILED_URL =
            "http://mock.failed.request/-2";
    private static final String MOCK_CRONET_TEST_SUCCESS_URL =
            "http://mock.http/success.txt";
    private static final int MAX_FILE_SIZE = 1000000000;
    private static final int NUM_EVENT_FILES = 10;

    private EmbeddedTestServer mTestServer;
    private String mUrl;
    private String mUrl404;
    private String mUrl500;

    // Thread on which network quality listeners should be notified.
    private Thread mNetworkQualityThread;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mTestServer = EmbeddedTestServer.createAndStartServer(getContext());
        mUrl = mTestServer.getURL("/echo?status=200");
        mUrl404 = mTestServer.getURL("/echo?status=404");
        mUrl500 = mTestServer.getURL("/echo?status=500");
    }

    @Override
    protected void tearDown() throws Exception {
        mTestServer.stopAndDestroyServer();
        super.tearDown();
    }

    static class RequestThread extends Thread {
        public TestUrlRequestCallback mCallback;

        final CronetTestFramework mTestFramework;
        final String mUrl;
        final ConditionVariable mRunBlocker;

        public RequestThread(
                CronetTestFramework testFramework, String url, ConditionVariable runBlocker) {
            mTestFramework = testFramework;
            mUrl = url;
            mRunBlocker = runBlocker;
        }

        @Override
        public void run() {
            mRunBlocker.block();
            CronetEngine cronetEngine = mTestFramework.initCronetEngine();
            mCallback = new TestUrlRequestCallback();
            UrlRequest.Builder urlRequestBuilder =
                    cronetEngine.newUrlRequestBuilder(mUrl, mCallback, mCallback.getExecutor());
            urlRequestBuilder.build().start();
            mCallback.blockForDone();
        }
    }

    /**
     * Callback that shutdowns the request context when request has succeeded
     * or failed.
     */
    static class ShutdownTestUrlRequestCallback extends TestUrlRequestCallback {
        private final CronetEngine mCronetEngine;
        private final ConditionVariable mCallbackCompletionBlock = new ConditionVariable();

        ShutdownTestUrlRequestCallback(CronetEngine cronetEngine) {
            mCronetEngine = cronetEngine;
        }

        @Override
        public void onSucceeded(UrlRequest request, UrlResponseInfo info) {
            super.onSucceeded(request, info);
            mCronetEngine.shutdown();
            mCallbackCompletionBlock.open();
        }

        @Override
        public void onFailed(UrlRequest request, UrlResponseInfo info, CronetException error) {
            super.onFailed(request, info, error);
            mCronetEngine.shutdown();
            mCallbackCompletionBlock.open();
        }

        // Wait for request completion callback.
        void blockForCallbackToComplete() {
            mCallbackCompletionBlock.block();
        }
    }

    private class ExecutorThreadFactory implements ThreadFactory {
        public Thread newThread(final Runnable r) {
            mNetworkQualityThread = new Thread(new Runnable() {
                @Override
                public void run() {
                    StrictMode.ThreadPolicy threadPolicy = StrictMode.getThreadPolicy();
                    try {
                        StrictMode.setThreadPolicy(new StrictMode.ThreadPolicy.Builder()
                                                           .detectNetwork()
                                                           .penaltyLog()
                                                           .penaltyDeath()
                                                           .build());
                        r.run();
                    } finally {
                        StrictMode.setThreadPolicy(threadPolicy);
                    }
                }
            });
            return mNetworkQualityThread;
        }
    }

    @SmallTest
    @Feature({"Cronet"})
    @SuppressWarnings("deprecation")
    public void testConfigUserAgent() throws Exception {
        String userAgentName = "User-Agent";
        String userAgentValue = "User-Agent-Value";
        ExperimentalCronetEngine.Builder cronetEngineBuilder =
                new ExperimentalCronetEngine.Builder(getContext());
        if (testingJavaImpl()) {
            cronetEngineBuilder = createJavaEngineBuilder();
        }
        cronetEngineBuilder.setUserAgent(userAgentValue);
        final CronetTestFramework testFramework =
                startCronetTestFrameworkWithUrlAndCronetEngineBuilder(mUrl, cronetEngineBuilder);
        NativeTestServer.shutdownNativeTestServer(); // startNativeTestServer returns false if it's
        // already running
        assertTrue(NativeTestServer.startNativeTestServer(getContext()));
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder urlRequestBuilder = testFramework.mCronetEngine.newUrlRequestBuilder(
                NativeTestServer.getEchoHeaderURL(userAgentName), callback, callback.getExecutor());
        urlRequestBuilder.build().start();
        callback.blockForDone();
        assertEquals(userAgentValue, callback.mResponseAsString);
    }

    @SmallTest
    @Feature({"Cronet"})
    @OnlyRunNativeCronet
    public void testDataReductionProxyEnabled() throws Exception {
        final CronetTestFramework testFramework = startCronetTestFrameworkAndSkipLibraryInit();

        // Ensure native code is loaded before trying to start test server.
        new ExperimentalCronetEngine.Builder(getContext()).build().shutdown();

        assertTrue(NativeTestServer.startNativeTestServer(getContext()));
        if (!NativeTestServer.isDataReductionProxySupported()) {
            return;
        }
        String serverHostPort = NativeTestServer.getHostPort();

        // Enable the Data Reduction Proxy and configure it to use the test
        // server as its primary proxy, and to check successfully that this
        // proxy is OK to use.
        ExperimentalCronetEngine.Builder cronetEngineBuilder =
                new ExperimentalCronetEngine.Builder(getContext());
        cronetEngineBuilder.enableDataReductionProxy("test-key");
        cronetEngineBuilder.setDataReductionProxyOptions(serverHostPort, "unused.net:9999",
                NativeTestServer.getFileURL("/secureproxychecksuccess.txt"));
        testFramework.mCronetEngine = (CronetEngineBase) cronetEngineBuilder.build();
        TestUrlRequestCallback callback = new TestUrlRequestCallback();

        // Construct and start a request that can only be returned by the test
        // server. This request will fail if the configuration logic for the
        // Data Reduction Proxy is not used.
        UrlRequest.Builder urlRequestBuilder = testFramework.mCronetEngine.newUrlRequestBuilder(
                "http://DomainThatDoesnt.Resolve/datareductionproxysuccess.txt", callback,
                callback.getExecutor());
        urlRequestBuilder.build().start();
        callback.blockForDone();

        // Verify that the request is successful and that the Data Reduction
        // Proxy logic configured to use the test server as its proxy.
        assertEquals(200, callback.mResponseInfo.getHttpStatusCode());
        assertEquals(serverHostPort, callback.mResponseInfo.getProxyServer());
        assertEquals("http://DomainThatDoesnt.Resolve/datareductionproxysuccess.txt",
                callback.mResponseInfo.getUrl());
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testRealTimeNetworkQualityObservationsNotEnabled() throws Exception {
        ExperimentalCronetEngine.Builder mCronetEngineBuilder =
                new ExperimentalCronetEngine.Builder(getContext());
        final CronetTestFramework testFramework =
                startCronetTestFrameworkWithUrlAndCronetEngineBuilder(null, mCronetEngineBuilder);
        Executor networkQualityExecutor = Executors.newSingleThreadExecutor();
        TestNetworkQualityRttListener rttListener =
                new TestNetworkQualityRttListener(networkQualityExecutor);
        TestNetworkQualityThroughputListener throughputListener =
                new TestNetworkQualityThroughputListener(networkQualityExecutor, null);
        try {
            testFramework.mCronetEngine.addRttListener(rttListener);
            fail("Should throw an exception.");
        } catch (IllegalStateException e) {
        }
        try {
            testFramework.mCronetEngine.addThroughputListener(throughputListener);
            fail("Should throw an exception.");
        } catch (IllegalStateException e) {
        }
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder builder = testFramework.mCronetEngine.newUrlRequestBuilder(
                mUrl, callback, callback.getExecutor());
        UrlRequest urlRequest = builder.build();

        urlRequest.start();
        callback.blockForDone();
        assertEquals(0, rttListener.rttObservationCount());
        assertEquals(0, throughputListener.throughputObservationCount());
        testFramework.mCronetEngine.shutdown();
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testRealTimeNetworkQualityObservationsListenerRemoved() throws Exception {
        ExperimentalCronetEngine.Builder mCronetEngineBuilder =
                new ExperimentalCronetEngine.Builder(getContext());
        TestExecutor networkQualityExecutor = new TestExecutor();
        TestNetworkQualityRttListener rttListener =
                new TestNetworkQualityRttListener(networkQualityExecutor);
        mCronetEngineBuilder.enableNetworkQualityEstimator(true);
        final CronetTestFramework testFramework =
                startCronetTestFrameworkWithUrlAndCronetEngineBuilder(null, mCronetEngineBuilder);
        testFramework.mCronetEngine.configureNetworkQualityEstimatorForTesting(true, true, false);

        testFramework.mCronetEngine.addRttListener(rttListener);
        testFramework.mCronetEngine.removeRttListener(rttListener);
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder builder = testFramework.mCronetEngine.newUrlRequestBuilder(
                mUrl, callback, callback.getExecutor());
        UrlRequest urlRequest = builder.build();
        urlRequest.start();
        callback.blockForDone();
        networkQualityExecutor.runAllTasks();
        assertEquals(0, rttListener.rttObservationCount());
        testFramework.mCronetEngine.shutdown();
    }

    // Returns whether a file contains a particular string.
    @SuppressFBWarnings("OBL_UNSATISFIED_OBLIGATION_EXCEPTION_EDGE")
    private boolean fileContainsString(String filename, String content) throws IOException {
        File file =
                new File(CronetTestFramework.getTestStorage(getContext()) + "/prefs/" + filename);
        FileInputStream fileInputStream = new FileInputStream(file);
        byte[] data = new byte[(int) file.length()];
        fileInputStream.read(data);
        fileInputStream.close();
        return new String(data, "UTF-8").contains(content);
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testRealTimeNetworkQualityObservationsQuicDisabled() throws Exception {
        ExperimentalCronetEngine.Builder mCronetEngineBuilder =
                new ExperimentalCronetEngine.Builder(getContext());
        assert RttThroughputValues.INVALID_RTT_THROUGHPUT < 0;
        Executor listenersExecutor = Executors.newSingleThreadExecutor(new ExecutorThreadFactory());
        ConditionVariable waitForThroughput = new ConditionVariable();
        TestNetworkQualityRttListener rttListener =
                new TestNetworkQualityRttListener(listenersExecutor);
        TestNetworkQualityThroughputListener throughputListener =
                new TestNetworkQualityThroughputListener(listenersExecutor, waitForThroughput);
        mCronetEngineBuilder.enableNetworkQualityEstimator(true).enableHttp2(true).enableQuic(
                false);
        mCronetEngineBuilder.setStoragePath(CronetTestFramework.getTestStorage(getContext()));
        final CronetTestFramework testFramework =
                startCronetTestFrameworkWithUrlAndCronetEngineBuilder(null, mCronetEngineBuilder);
        testFramework.mCronetEngine.configureNetworkQualityEstimatorForTesting(true, true, true);

        testFramework.mCronetEngine.addRttListener(rttListener);
        testFramework.mCronetEngine.addThroughputListener(throughputListener);

        HistogramDelta writeCountHistogram = new HistogramDelta("NQE.Prefs.WriteCount", 1);
        assertEquals(0, writeCountHistogram.getDelta()); // Sanity check.

        HistogramDelta readCountHistogram = new HistogramDelta("NQE.Prefs.ReadCount", 1);
        assertEquals(0, readCountHistogram.getDelta()); // Sanity check.

        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder builder = testFramework.mCronetEngine.newUrlRequestBuilder(
                mUrl, callback, callback.getExecutor());
        UrlRequest urlRequest = builder.build();
        urlRequest.start();
        callback.blockForDone();

        // Throughput observation is posted to the network quality estimator on the network thread
        // after the UrlRequest is completed. The observations are then eventually posted to
        // throughput listeners on the executor provided to network quality.
        waitForThroughput.block();
        assertTrue(throughputListener.throughputObservationCount() > 0);

        // Prefs must be read at startup.
        assertTrue(readCountHistogram.getDelta() > 0);

        // Check RTT observation count after throughput observation has been received. This ensures
        // that executor has finished posting the RTT observation to the RTT listeners.
        assertTrue(rttListener.rttObservationCount() > 0);

        // NETWORK_QUALITY_OBSERVATION_SOURCE_URL_REQUEST
        assertTrue(rttListener.rttObservationCount(0) > 0);

        // NETWORK_QUALITY_OBSERVATION_SOURCE_TCP
        assertTrue(rttListener.rttObservationCount(1) > 0);

        // NETWORK_QUALITY_OBSERVATION_SOURCE_QUIC
        assertEquals(0, rttListener.rttObservationCount(2));

        // Verify that the listeners were notified on the expected thread.
        assertEquals(mNetworkQualityThread, rttListener.getThread());
        assertEquals(mNetworkQualityThread, throughputListener.getThread());

        // Verify that effective connection type callback is received and
        // effective connection type is correctly set.
        assertTrue(testFramework.mCronetEngine.getEffectiveConnectionType()
                != EffectiveConnectionType.TYPE_UNKNOWN);

        // Verify that the HTTP RTT, transport RTT and downstream throughput
        // estimates are available.
        assertTrue(testFramework.mCronetEngine.getHttpRttMs() >= 0);
        assertTrue(testFramework.mCronetEngine.getTransportRttMs() >= 0);
        assertTrue(testFramework.mCronetEngine.getDownstreamThroughputKbps() >= 0);

        // Verify that the cached estimates were written to the prefs.
        while (true) {
            Log.i(TAG, "Still waiting for pref file update.....");
            Thread.sleep(12000);
            try {
                if (fileContainsString("local_prefs.json", "network_qualities")) {
                    break;
                }
            } catch (FileNotFoundException e) {
                // Ignored this exception since the file will only be created when updates are
                // flushed to the disk.
            }
        }
        assertTrue(fileContainsString("local_prefs.json", "network_qualities"));

        testFramework.mCronetEngine.shutdown();
        assertTrue(writeCountHistogram.getDelta() > 0);
    }

    @SmallTest
    @Feature({"Cronet"})
    // TODO: Remove the annotation after fixing http://crbug.com/637979 & http://crbug.com/637972
    @OnlyRunNativeCronet
    public void testShutdown() throws Exception {
        final CronetTestFramework testFramework = startCronetTestFramework();
        ShutdownTestUrlRequestCallback callback =
                new ShutdownTestUrlRequestCallback(testFramework.mCronetEngine);
        // Block callback when response starts to verify that shutdown fails
        // if there are active requests.
        callback.setAutoAdvance(false);
        UrlRequest.Builder urlRequestBuilder = testFramework.mCronetEngine.newUrlRequestBuilder(
                mUrl, callback, callback.getExecutor());
        UrlRequest urlRequest = urlRequestBuilder.build();
        urlRequest.start();
        try {
            testFramework.mCronetEngine.shutdown();
            fail("Should throw an exception");
        } catch (Exception e) {
            assertEquals("Cannot shutdown with active requests.",
                         e.getMessage());
        }

        callback.waitForNextStep();
        assertEquals(ResponseStep.ON_RESPONSE_STARTED, callback.mResponseStep);
        try {
            testFramework.mCronetEngine.shutdown();
            fail("Should throw an exception");
        } catch (Exception e) {
            assertEquals("Cannot shutdown with active requests.",
                         e.getMessage());
        }
        callback.startNextRead(urlRequest);

        callback.waitForNextStep();
        assertEquals(ResponseStep.ON_READ_COMPLETED, callback.mResponseStep);
        try {
            testFramework.mCronetEngine.shutdown();
            fail("Should throw an exception");
        } catch (Exception e) {
            assertEquals("Cannot shutdown with active requests.",
                         e.getMessage());
        }

        // May not have read all the data, in theory. Just enable auto-advance
        // and finish the request.
        callback.setAutoAdvance(true);
        callback.startNextRead(urlRequest);
        callback.blockForDone();
        callback.blockForCallbackToComplete();
        callback.shutdownExecutor();
    }

    @SmallTest
    @Feature({"Cronet"})
    @OnlyRunNativeCronet
    public void testShutdownDuringInit() throws Exception {
        final CronetTestFramework testFramework = startCronetTestFrameworkAndSkipLibraryInit();
        final ConditionVariable block = new ConditionVariable(false);

        // Post a task to main thread to block until shutdown is called to test
        // scenario when shutdown is called right after construction before
        // context is fully initialized on the main thread.
        Runnable blockingTask = new Runnable() {
            @Override
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
        final CronetUrlRequestContext cronetEngine =
                (CronetUrlRequestContext) testFramework.initCronetEngine();
        // Unblock the main thread, so context gets initialized and shutdown on
        // it.
        block.open();
        // Shutdown will wait for init to complete on main thread.
        cronetEngine.shutdown();
        // Verify that context is shutdown.
        try {
            cronetEngine.getUrlRequestContextAdapter();
            fail("Should throw an exception.");
        } catch (Exception e) {
            assertEquals("Engine is shut down.", e.getMessage());
        }
    }

    @SmallTest
    @Feature({"Cronet"})
    @OnlyRunNativeCronet
    public void testInitAndShutdownOnMainThread() throws Exception {
        final CronetTestFramework testFramework = startCronetTestFrameworkAndSkipLibraryInit();
        final ConditionVariable block = new ConditionVariable(false);

        // Post a task to main thread to init and shutdown on the main thread.
        Runnable blockingTask = new Runnable() {
            @Override
            public void run() {
                // Create new request context, loading the library.
                final CronetUrlRequestContext cronetEngine =
                        (CronetUrlRequestContext) testFramework.initCronetEngine();
                // Shutdown right after init.
                cronetEngine.shutdown();
                // Verify that context is shutdown.
                try {
                    cronetEngine.getUrlRequestContextAdapter();
                    fail("Should throw an exception.");
                } catch (Exception e) {
                    assertEquals("Engine is shut down.", e.getMessage());
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
        final CronetTestFramework testFramework = startCronetTestFramework();
        try {
            testFramework.mCronetEngine.shutdown();
            testFramework.mCronetEngine.shutdown();
            fail("Should throw an exception");
        } catch (Exception e) {
            assertEquals("Engine is shut down.", e.getMessage());
        }
    }

    @SmallTest
    @Feature({"Cronet"})
    // TODO: Remove the annotation after fixing http://crbug.com/637972
    @OnlyRunNativeCronet
    public void testShutdownAfterError() throws Exception {
        final CronetTestFramework testFramework = startCronetTestFramework();
        ShutdownTestUrlRequestCallback callback =
                new ShutdownTestUrlRequestCallback(testFramework.mCronetEngine);
        UrlRequest.Builder urlRequestBuilder = testFramework.mCronetEngine.newUrlRequestBuilder(
                MOCK_CRONET_TEST_FAILED_URL, callback, callback.getExecutor());
        urlRequestBuilder.build().start();
        callback.blockForDone();
        assertTrue(callback.mOnErrorCalled);
        callback.blockForCallbackToComplete();
        callback.shutdownExecutor();
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testShutdownAfterCancel() throws Exception {
        final CronetTestFramework testFramework = startCronetTestFramework();
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        // Block callback when response starts to verify that shutdown fails
        // if there are active requests.
        callback.setAutoAdvance(false);
        UrlRequest.Builder urlRequestBuilder = testFramework.mCronetEngine.newUrlRequestBuilder(
                mUrl, callback, callback.getExecutor());
        UrlRequest urlRequest = urlRequestBuilder.build();
        urlRequest.start();
        try {
            testFramework.mCronetEngine.shutdown();
            fail("Should throw an exception");
        } catch (Exception e) {
            assertEquals("Cannot shutdown with active requests.",
                         e.getMessage());
        }
        callback.waitForNextStep();
        assertEquals(ResponseStep.ON_RESPONSE_STARTED, callback.mResponseStep);
        urlRequest.cancel();
        testFramework.mCronetEngine.shutdown();
    }

    @SmallTest
    @Feature({"Cronet"})
    @OnlyRunNativeCronet // No netlogs for pure java impl
    public void testNetLog() throws Exception {
        Context context = getContext();
        File directory = new File(PathUtils.getDataDirectory());
        File file = File.createTempFile("cronet", "json", directory);
        CronetEngine cronetEngine = new CronetEngine.Builder(context).build();
        // Start NetLog immediately after the request context is created to make
        // sure that the call won't crash the app even when the native request
        // context is not fully initialized. See crbug.com/470196.
        cronetEngine.startNetLogToFile(file.getPath(), false);

        // Start a request.
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder urlRequestBuilder =
                cronetEngine.newUrlRequestBuilder(mUrl, callback, callback.getExecutor());
        urlRequestBuilder.build().start();
        callback.blockForDone();
        cronetEngine.stopNetLog();
        assertTrue(file.exists());
        assertTrue(file.length() != 0);
        assertFalse(hasBytesInNetLog(file));
        assertTrue(file.delete());
        assertTrue(!file.exists());
    }

    @SmallTest
    @Feature({"Cronet"})
    @OnlyRunNativeCronet // No netlogs for pure java impl
    public void testBoundedFileNetLog() throws Exception {
        Context context = getContext();
        File directory = new File(PathUtils.getDataDirectory());
        File netLogDir = new File(directory, "NetLog");
        assertFalse(netLogDir.exists());
        assertTrue(netLogDir.mkdir());
        File eventFile = new File(netLogDir, "event_file_0.json");
        ExperimentalCronetEngine cronetEngine =
                new ExperimentalCronetEngine.Builder(context).build();
        // Start NetLog immediately after the request context is created to make
        // sure that the call won't crash the app even when the native request
        // context is not fully initialized. See crbug.com/470196.
        cronetEngine.startNetLogToDisk(netLogDir.getPath(), false, MAX_FILE_SIZE);

        // Start a request.
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder urlRequestBuilder =
                cronetEngine.newUrlRequestBuilder(mUrl, callback, callback.getExecutor());
        urlRequestBuilder.build().start();
        callback.blockForDone();
        cronetEngine.stopNetLog();
        assertTrue(eventFile.exists());
        assertTrue(eventFile.length() != 0);
        assertFalse(hasBytesInNetLog(eventFile));
        FileUtils.recursivelyDeleteFile(netLogDir);
        assertFalse(netLogDir.exists());
    }

    @SmallTest
    @Feature({"Cronet"})
    @OnlyRunNativeCronet // No netlogs for pure java impl
    // Tests that if stopNetLog is not explicity called, CronetEngine.shutdown()
    // will take care of it. crbug.com/623701.
    public void testNoStopNetLog() throws Exception {
        Context context = getContext();
        File directory = new File(PathUtils.getDataDirectory());
        File file = File.createTempFile("cronet", "json", directory);
        CronetEngine cronetEngine = new CronetEngine.Builder(context).build();
        cronetEngine.startNetLogToFile(file.getPath(), false);

        // Start a request.
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder urlRequestBuilder =
                cronetEngine.newUrlRequestBuilder(mUrl, callback, callback.getExecutor());
        urlRequestBuilder.build().start();
        callback.blockForDone();
        // Shut down the engine without calling stopNetLog.
        cronetEngine.shutdown();
        assertTrue(file.exists());
        assertTrue(file.length() != 0);
        assertFalse(hasBytesInNetLog(file));
        assertTrue(file.delete());
        assertTrue(!file.exists());
    }

    @SmallTest
    @Feature({"Cronet"})
    @OnlyRunNativeCronet // No netlogs for pure java impl
    // Tests that if stopNetLog is not explicity called, CronetEngine.shutdown()
    // will take care of it. crbug.com/623701.
    public void testNoStopBoundedFileNetLog() throws Exception {
        Context context = getContext();
        File directory = new File(PathUtils.getDataDirectory());
        File netLogDir = new File(directory, "NetLog");
        assertFalse(netLogDir.exists());
        assertTrue(netLogDir.mkdir());
        File eventFile = new File(netLogDir, "event_file_0.json");
        ExperimentalCronetEngine cronetEngine =
                new ExperimentalCronetEngine.Builder(context).build();
        cronetEngine.startNetLogToDisk(netLogDir.getPath(), false, MAX_FILE_SIZE);

        // Start a request.
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder urlRequestBuilder =
                cronetEngine.newUrlRequestBuilder(mUrl, callback, callback.getExecutor());
        urlRequestBuilder.build().start();
        callback.blockForDone();
        // Shut down the engine without calling stopNetLog.
        cronetEngine.shutdown();
        assertTrue(eventFile.exists());
        assertTrue(eventFile.length() != 0);

        FileUtils.recursivelyDeleteFile(netLogDir);
        assertFalse(netLogDir.exists());
    }

    @SmallTest
    @Feature({"Cronet"})
    @OnlyRunNativeCronet
    // Tests that NetLog contains events emitted by all live CronetEngines.
    public void testNetLogContainEventsFromAllLiveEngines() throws Exception {
        Context context = getContext();
        File directory = new File(PathUtils.getDataDirectory());
        File file1 = File.createTempFile("cronet1", "json", directory);
        File file2 = File.createTempFile("cronet2", "json", directory);
        CronetEngine cronetEngine1 = new CronetEngine.Builder(context).build();
        CronetEngine cronetEngine2 = new CronetEngine.Builder(context).build();

        cronetEngine1.startNetLogToFile(file1.getPath(), false);
        cronetEngine2.startNetLogToFile(file2.getPath(), false);

        // Warm CronetEngine and make sure both CronetUrlRequestContexts are
        // initialized before testing the logs.
        makeRequestAndCheckStatus(cronetEngine1, mUrl, 200);
        makeRequestAndCheckStatus(cronetEngine2, mUrl, 200);

        // Use cronetEngine1 to make a request to mUrl404.
        makeRequestAndCheckStatus(cronetEngine1, mUrl404, 404);

        // Use cronetEngine2 to make a request to mUrl500.
        makeRequestAndCheckStatus(cronetEngine2, mUrl500, 500);

        cronetEngine1.stopNetLog();
        cronetEngine2.stopNetLog();
        assertTrue(file1.exists());
        assertTrue(file2.exists());
        // Make sure both files contain the two requests made separately using
        // different engines.
        assertTrue(containsStringInNetLog(file1, mUrl404));
        assertTrue(containsStringInNetLog(file1, mUrl500));
        assertTrue(containsStringInNetLog(file2, mUrl404));
        assertTrue(containsStringInNetLog(file2, mUrl500));
        assertTrue(file1.delete());
        assertTrue(file2.delete());
    }

    @SmallTest
    @Feature({"Cronet"})
    @OnlyRunNativeCronet
    // Tests that NetLog contains events emitted by all live CronetEngines.
    public void testBoundedFileNetLogContainEventsFromAllLiveEngines() throws Exception {
        Context context = getContext();
        File directory = new File(PathUtils.getDataDirectory());
        File netLogDir1 = new File(directory, "NetLog1");
        assertFalse(netLogDir1.exists());
        assertTrue(netLogDir1.mkdir());
        File netLogDir2 = new File(directory, "NetLog2");
        assertFalse(netLogDir2.exists());
        assertTrue(netLogDir2.mkdir());
        File eventFile1 = new File(netLogDir1, "event_file_0.json");
        File eventFile2 = new File(netLogDir2, "event_file_0.json");

        ExperimentalCronetEngine cronetEngine1 =
                new ExperimentalCronetEngine.Builder(context).build();
        ExperimentalCronetEngine cronetEngine2 =
                new ExperimentalCronetEngine.Builder(context).build();

        cronetEngine1.startNetLogToDisk(netLogDir1.getPath(), false, MAX_FILE_SIZE);
        cronetEngine2.startNetLogToDisk(netLogDir2.getPath(), false, MAX_FILE_SIZE);

        // Warm CronetEngine and make sure both CronetUrlRequestContexts are
        // initialized before testing the logs.
        makeRequestAndCheckStatus(cronetEngine1, mUrl, 200);
        makeRequestAndCheckStatus(cronetEngine2, mUrl, 200);

        // Use cronetEngine1 to make a request to mUrl404.
        makeRequestAndCheckStatus(cronetEngine1, mUrl404, 404);

        // Use cronetEngine2 to make a request to mUrl500.
        makeRequestAndCheckStatus(cronetEngine2, mUrl500, 500);

        cronetEngine1.stopNetLog();
        cronetEngine2.stopNetLog();

        assertTrue(eventFile1.exists());
        assertTrue(eventFile2.exists());
        assertTrue(eventFile1.length() != 0);
        assertTrue(eventFile2.length() != 0);

        // Make sure both files contain the two requests made separately using
        // different engines.
        assertTrue(containsStringInNetLog(eventFile1, mUrl404));
        assertTrue(containsStringInNetLog(eventFile1, mUrl500));
        assertTrue(containsStringInNetLog(eventFile2, mUrl404));
        assertTrue(containsStringInNetLog(eventFile2, mUrl500));

        FileUtils.recursivelyDeleteFile(netLogDir1);
        assertFalse(netLogDir1.exists());
        FileUtils.recursivelyDeleteFile(netLogDir2);
        assertFalse(netLogDir2.exists());
    }
    @SmallTest
    @Feature({"Cronet"})
    @OnlyRunNativeCronet
    // Tests that if CronetEngine is shut down on the network thread, an appropriate exception
    // is thrown.
    public void testShutDownEngineOnNetworkThread() throws Exception {
        final CronetTestFramework testFramework =
                startCronetTestFrameworkWithCacheEnabled(CronetEngine.Builder.HTTP_CACHE_DISK);
        String url = NativeTestServer.getFileURL("/cacheable.txt");
        // Make a request to a cacheable resource.
        checkRequestCaching(testFramework.mCronetEngine, url, false);

        final AtomicReference<Throwable> thrown = new AtomicReference<>();
        // Shut down the server.
        NativeTestServer.shutdownNativeTestServer();
        class CancelUrlRequestCallback extends TestUrlRequestCallback {
            @Override
            public void onResponseStarted(UrlRequest request, UrlResponseInfo info) {
                super.onResponseStarted(request, info);
                request.cancel();
                // Shut down CronetEngine immediately after request is destroyed.
                try {
                    testFramework.mCronetEngine.shutdown();
                } catch (Exception e) {
                    thrown.set(e);
                }
            }

            @Override
            public void onSucceeded(UrlRequest request, UrlResponseInfo info) {
                // onSucceeded will not happen, because the request is canceled
                // after sending first read and the executor is single threaded.
                throw new RuntimeException("Unexpected");
            }

            @Override
            public void onFailed(UrlRequest request, UrlResponseInfo info, CronetException error) {
                throw new RuntimeException("Unexpected");
            }
        }
        Executor directExecutor = new Executor() {
            @Override
            public void execute(Runnable command) {
                command.run();
            }
        };
        CancelUrlRequestCallback callback = new CancelUrlRequestCallback();
        callback.setAllowDirectExecutor(true);
        UrlRequest.Builder urlRequestBuilder =
                testFramework.mCronetEngine.newUrlRequestBuilder(url, callback, directExecutor);
        urlRequestBuilder.allowDirectExecutor();
        urlRequestBuilder.build().start();
        callback.blockForDone();
        assertTrue(thrown.get() instanceof RuntimeException);
    }

    @SmallTest
    @Feature({"Cronet"})
    @OnlyRunNativeCronet
    // Tests that if CronetEngine is shut down when reading from disk cache,
    // there isn't a crash. See crbug.com/486120.
    public void testShutDownEngineWhenReadingFromDiskCache() throws Exception {
        final CronetTestFramework testFramework =
                startCronetTestFrameworkWithCacheEnabled(CronetEngine.Builder.HTTP_CACHE_DISK);
        String url = NativeTestServer.getFileURL("/cacheable.txt");
        // Make a request to a cacheable resource.
        checkRequestCaching(testFramework.mCronetEngine, url, false);

        // Shut down the server.
        NativeTestServer.shutdownNativeTestServer();
        class CancelUrlRequestCallback extends TestUrlRequestCallback {
            @Override
            public void onResponseStarted(UrlRequest request, UrlResponseInfo info) {
                super.onResponseStarted(request, info);
                request.cancel();
                // Shut down CronetEngine immediately after request is destroyed.
                testFramework.mCronetEngine.shutdown();
            }

            @Override
            public void onSucceeded(UrlRequest request, UrlResponseInfo info) {
                // onSucceeded will not happen, because the request is canceled
                // after sending first read and the executor is single threaded.
                throw new RuntimeException("Unexpected");
            }

            @Override
            public void onFailed(UrlRequest request, UrlResponseInfo info, CronetException error) {
                throw new RuntimeException("Unexpected");
            }
        }
        CancelUrlRequestCallback callback = new CancelUrlRequestCallback();
        UrlRequest.Builder urlRequestBuilder = testFramework.mCronetEngine.newUrlRequestBuilder(
                url, callback, callback.getExecutor());
        urlRequestBuilder.build().start();
        callback.blockForDone();
        assertEquals(200, callback.mResponseInfo.getHttpStatusCode());
        assertTrue(callback.mResponseInfo.wasCached());
        assertTrue(callback.mOnCanceledCalled);
    }

    @SmallTest
    @Feature({"Cronet"})
    @OnlyRunNativeCronet
    public void testNetLogAfterShutdown() throws Exception {
        final CronetTestFramework testFramework = startCronetTestFramework();
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder urlRequestBuilder = testFramework.mCronetEngine.newUrlRequestBuilder(
                mUrl, callback, callback.getExecutor());
        urlRequestBuilder.build().start();
        callback.blockForDone();
        testFramework.mCronetEngine.shutdown();

        File directory = new File(PathUtils.getDataDirectory());
        File file = File.createTempFile("cronet", "json", directory);
        try {
            testFramework.mCronetEngine.startNetLogToFile(file.getPath(), false);
            fail("Should throw an exception.");
        } catch (Exception e) {
            assertEquals("Engine is shut down.", e.getMessage());
        }
        assertFalse(hasBytesInNetLog(file));
        assertTrue(file.delete());
        assertTrue(!file.exists());
    }

    @SmallTest
    @Feature({"Cronet"})
    @OnlyRunNativeCronet
    public void testBoundedFileNetLogAfterShutdown() throws Exception {
        final CronetTestFramework testFramework = startCronetTestFramework();
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder urlRequestBuilder = testFramework.mCronetEngine.newUrlRequestBuilder(
                mUrl, callback, callback.getExecutor());
        urlRequestBuilder.build().start();
        callback.blockForDone();
        testFramework.mCronetEngine.shutdown();

        File directory = new File(PathUtils.getDataDirectory());
        File netLogDir = new File(directory, "NetLog");
        assertFalse(netLogDir.exists());
        assertTrue(netLogDir.mkdir());
        File constantsFile = new File(netLogDir, "constants.json");
        try {
            testFramework.mCronetEngine.startNetLogToDisk(
                    netLogDir.getPath(), false, MAX_FILE_SIZE);
            fail("Should throw an exception.");
        } catch (Exception e) {
            assertEquals("Engine is shut down.", e.getMessage());
        }
        assertFalse(constantsFile.exists());
        FileUtils.recursivelyDeleteFile(netLogDir);
        assertFalse(netLogDir.exists());
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testNetLogStartMultipleTimes() throws Exception {
        final CronetTestFramework testFramework = startCronetTestFramework();
        File directory = new File(PathUtils.getDataDirectory());
        File file = File.createTempFile("cronet", "json", directory);
        // Start NetLog multiple times.
        testFramework.mCronetEngine.startNetLogToFile(file.getPath(), false);
        testFramework.mCronetEngine.startNetLogToFile(file.getPath(), false);
        testFramework.mCronetEngine.startNetLogToFile(file.getPath(), false);
        testFramework.mCronetEngine.startNetLogToFile(file.getPath(), false);
        // Start a request.
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder urlRequestBuilder = testFramework.mCronetEngine.newUrlRequestBuilder(
                mUrl, callback, callback.getExecutor());
        urlRequestBuilder.build().start();
        callback.blockForDone();
        testFramework.mCronetEngine.stopNetLog();
        assertTrue(file.exists());
        assertTrue(file.length() != 0);
        assertFalse(hasBytesInNetLog(file));
        assertTrue(file.delete());
        assertTrue(!file.exists());
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testBoundedFileNetLogStartMultipleTimes() throws Exception {
        final CronetTestFramework testFramework = startCronetTestFramework();
        File directory = new File(PathUtils.getDataDirectory());
        File netLogDir = new File(directory, "NetLog");
        assertFalse(netLogDir.exists());
        assertTrue(netLogDir.mkdir());
        File eventFile = new File(netLogDir, "event_file_0.json");
        // Start NetLog multiple times. This should be equivalent to starting NetLog
        // once. Each subsequent start (without calling stopNetLog) should be a no-op.
        testFramework.mCronetEngine.startNetLogToDisk(netLogDir.getPath(), false, MAX_FILE_SIZE);
        testFramework.mCronetEngine.startNetLogToDisk(netLogDir.getPath(), false, MAX_FILE_SIZE);
        testFramework.mCronetEngine.startNetLogToDisk(netLogDir.getPath(), false, MAX_FILE_SIZE);
        testFramework.mCronetEngine.startNetLogToDisk(netLogDir.getPath(), false, MAX_FILE_SIZE);
        // Start a request.
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder urlRequestBuilder = testFramework.mCronetEngine.newUrlRequestBuilder(
                mUrl, callback, callback.getExecutor());
        urlRequestBuilder.build().start();
        callback.blockForDone();
        testFramework.mCronetEngine.stopNetLog();
        assertTrue(eventFile.exists());
        assertTrue(eventFile.length() != 0);
        assertFalse(hasBytesInNetLog(eventFile));
        FileUtils.recursivelyDeleteFile(netLogDir);
        assertFalse(netLogDir.exists());
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testNetLogStopMultipleTimes() throws Exception {
        final CronetTestFramework testFramework = startCronetTestFramework();
        File directory = new File(PathUtils.getDataDirectory());
        File file = File.createTempFile("cronet", "json", directory);
        testFramework.mCronetEngine.startNetLogToFile(file.getPath(), false);
        // Start a request.
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder urlRequestBuilder = testFramework.mCronetEngine.newUrlRequestBuilder(
                mUrl, callback, callback.getExecutor());
        urlRequestBuilder.build().start();
        callback.blockForDone();
        // Stop NetLog multiple times.
        testFramework.mCronetEngine.stopNetLog();
        testFramework.mCronetEngine.stopNetLog();
        testFramework.mCronetEngine.stopNetLog();
        testFramework.mCronetEngine.stopNetLog();
        testFramework.mCronetEngine.stopNetLog();
        assertTrue(file.exists());
        assertTrue(file.length() != 0);
        assertFalse(hasBytesInNetLog(file));
        assertTrue(file.delete());
        assertTrue(!file.exists());
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testBoundedFileNetLogStopMultipleTimes() throws Exception {
        final CronetTestFramework testFramework = startCronetTestFramework();
        File directory = new File(PathUtils.getDataDirectory());
        File netLogDir = new File(directory, "NetLog");
        assertFalse(netLogDir.exists());
        assertTrue(netLogDir.mkdir());
        File eventFile = new File(netLogDir, "event_file_0.json");
        testFramework.mCronetEngine.startNetLogToDisk(netLogDir.getPath(), false, MAX_FILE_SIZE);
        // Start a request.
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder urlRequestBuilder = testFramework.mCronetEngine.newUrlRequestBuilder(
                mUrl, callback, callback.getExecutor());
        urlRequestBuilder.build().start();
        callback.blockForDone();
        // Stop NetLog multiple times. This should be equivalent to stopping NetLog once.
        // Each subsequent stop (without calling startNetLogToDisk first) should be a no-op.
        testFramework.mCronetEngine.stopNetLog();
        testFramework.mCronetEngine.stopNetLog();
        testFramework.mCronetEngine.stopNetLog();
        testFramework.mCronetEngine.stopNetLog();
        testFramework.mCronetEngine.stopNetLog();
        assertTrue(eventFile.exists());
        assertTrue(eventFile.length() != 0);
        assertFalse(hasBytesInNetLog(eventFile));
        FileUtils.recursivelyDeleteFile(netLogDir);
        assertFalse(netLogDir.exists());
    }

    @SmallTest
    @Feature({"Cronet"})
    @OnlyRunNativeCronet
    public void testNetLogWithBytes() throws Exception {
        Context context = getContext();
        File directory = new File(PathUtils.getDataDirectory());
        File file = File.createTempFile("cronet", "json", directory);
        CronetEngine cronetEngine = new CronetEngine.Builder(context).build();
        // Start NetLog with logAll as true.
        cronetEngine.startNetLogToFile(file.getPath(), true);
        // Start a request.
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder urlRequestBuilder =
                cronetEngine.newUrlRequestBuilder(mUrl, callback, callback.getExecutor());
        urlRequestBuilder.build().start();
        callback.blockForDone();
        cronetEngine.stopNetLog();
        assertTrue(file.exists());
        assertTrue(file.length() != 0);
        assertTrue(hasBytesInNetLog(file));
        assertTrue(file.delete());
        assertTrue(!file.exists());
    }

    @SmallTest
    @Feature({"Cronet"})
    @OnlyRunNativeCronet
    public void testBoundedFileNetLogWithBytes() throws Exception {
        Context context = getContext();
        File directory = new File(PathUtils.getDataDirectory());
        File netLogDir = new File(directory, "NetLog");
        assertFalse(netLogDir.exists());
        assertTrue(netLogDir.mkdir());
        File eventFile = new File(netLogDir, "event_file_0.json");
        ExperimentalCronetEngine cronetEngine =
                new ExperimentalCronetEngine.Builder(context).build();
        // Start NetLog with logAll as true.
        cronetEngine.startNetLogToDisk(netLogDir.getPath(), true, MAX_FILE_SIZE);
        // Start a request.
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder urlRequestBuilder =
                cronetEngine.newUrlRequestBuilder(mUrl, callback, callback.getExecutor());
        urlRequestBuilder.build().start();
        callback.blockForDone();
        cronetEngine.stopNetLog();

        assertTrue(eventFile.exists());
        assertTrue(eventFile.length() != 0);
        assertTrue(hasBytesInNetLog(eventFile));
        FileUtils.recursivelyDeleteFile(netLogDir);
        assertFalse(netLogDir.exists());
    }

    private boolean hasBytesInNetLog(File logFile) throws Exception {
        return containsStringInNetLog(logFile, "\"hex_encoded_bytes\"");
    }

    private boolean containsStringInNetLog(File logFile, String content) throws Exception {
        BufferedReader logReader = new BufferedReader(new FileReader(logFile));
        try {
            String logLine;
            while ((logLine = logReader.readLine()) != null) {
                if (logLine.contains(content)) {
                    return true;
                }
            }
            return false;
        } finally {
            logReader.close();
        }
    }

    /**
     * Helper method to make a request to {@code url}, wait for it to
     * complete, and check that the status code is the same as {@code expectedStatusCode}.
     */
    private void makeRequestAndCheckStatus(
            CronetEngine engine, String url, int expectedStatusCode) {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest request =
                engine.newUrlRequestBuilder(url, callback, callback.getExecutor()).build();
        request.start();
        callback.blockForDone();
        assertEquals(expectedStatusCode, callback.mResponseInfo.getHttpStatusCode());
    }

    private CronetTestFramework startCronetTestFrameworkWithCacheEnabled(int cacheType)
            throws Exception {
        String cacheTypeString = "";
        if (cacheType == CronetEngine.Builder.HTTP_CACHE_DISK) {
            cacheTypeString = CronetTestFramework.CACHE_DISK;
        } else if (cacheType == CronetEngine.Builder.HTTP_CACHE_DISK_NO_HTTP) {
            cacheTypeString = CronetTestFramework.CACHE_DISK_NO_HTTP;
        } else if (cacheType == HTTP_CACHE_IN_MEMORY) {
            cacheTypeString = CronetTestFramework.CACHE_IN_MEMORY;
        }
        String[] commandLineArgs = {CronetTestFramework.CACHE_KEY, cacheTypeString};
        CronetTestFramework testFramework =
                startCronetTestFrameworkWithUrlAndCommandLineArgs(null, commandLineArgs);
        assertTrue(NativeTestServer.startNativeTestServer(getContext()));
        return testFramework;
    }

    private void checkRequestCaching(CronetEngine engine, String url, boolean expectCached) {
        checkRequestCaching(engine, url, expectCached, false);
    }

    private void checkRequestCaching(
            CronetEngine engine, String url, boolean expectCached, boolean disableCache) {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder urlRequestBuilder =
                engine.newUrlRequestBuilder(url, callback, callback.getExecutor());
        if (disableCache) {
            urlRequestBuilder.disableCache();
        }
        urlRequestBuilder.build().start();
        callback.blockForDone();
        assertEquals(expectCached, callback.mResponseInfo.wasCached());
        assertEquals("this is a cacheable file\n", callback.mResponseAsString);
    }

    @SmallTest
    @Feature({"Cronet"})
    @OnlyRunNativeCronet
    public void testEnableHttpCacheDisabled() throws Exception {
        final CronetTestFramework testFramework =
                startCronetTestFrameworkWithCacheEnabled(CronetEngine.Builder.HTTP_CACHE_DISABLED);
        String url = NativeTestServer.getFileURL("/cacheable.txt");
        checkRequestCaching(testFramework.mCronetEngine, url, false);
        checkRequestCaching(testFramework.mCronetEngine, url, false);
        checkRequestCaching(testFramework.mCronetEngine, url, false);
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testEnableHttpCacheInMemory() throws Exception {
        final CronetTestFramework testFramework =
                startCronetTestFrameworkWithCacheEnabled(HTTP_CACHE_IN_MEMORY);
        String url = NativeTestServer.getFileURL("/cacheable.txt");
        checkRequestCaching(testFramework.mCronetEngine, url, false);
        checkRequestCaching(testFramework.mCronetEngine, url, true);
        NativeTestServer.shutdownNativeTestServer();
        checkRequestCaching(testFramework.mCronetEngine, url, true);
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testEnableHttpCacheDisk() throws Exception {
        final CronetTestFramework testFramework =
                startCronetTestFrameworkWithCacheEnabled(CronetEngine.Builder.HTTP_CACHE_DISK);
        String url = NativeTestServer.getFileURL("/cacheable.txt");
        checkRequestCaching(testFramework.mCronetEngine, url, false);
        checkRequestCaching(testFramework.mCronetEngine, url, true);
        NativeTestServer.shutdownNativeTestServer();
        checkRequestCaching(testFramework.mCronetEngine, url, true);
    }

    @SmallTest
    @Feature({"Cronet"})
    @OnlyRunNativeCronet
    public void testEnableHttpCacheDiskNoHttp() throws Exception {
        final CronetTestFramework testFramework =
                startCronetTestFrameworkWithCacheEnabled(CronetEngine.Builder.HTTP_CACHE_DISABLED);
        String url = NativeTestServer.getFileURL("/cacheable.txt");
        checkRequestCaching(testFramework.mCronetEngine, url, false);
        checkRequestCaching(testFramework.mCronetEngine, url, false);
        checkRequestCaching(testFramework.mCronetEngine, url, false);
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testDisableCache() throws Exception {
        final CronetTestFramework testFramework =
                startCronetTestFrameworkWithCacheEnabled(CronetEngine.Builder.HTTP_CACHE_DISK);
        String url = NativeTestServer.getFileURL("/cacheable.txt");

        // When cache is disabled, making a request does not write to the cache.
        checkRequestCaching(testFramework.mCronetEngine, url, false, true /** disable cache */);
        checkRequestCaching(testFramework.mCronetEngine, url, false);

        // When cache is enabled, the second request is cached.
        checkRequestCaching(testFramework.mCronetEngine, url, false, true /** disable cache */);
        checkRequestCaching(testFramework.mCronetEngine, url, true);

        // Shut down the server, next request should have a cached response.
        NativeTestServer.shutdownNativeTestServer();
        checkRequestCaching(testFramework.mCronetEngine, url, true);

        // Cache is disabled after server is shut down, request should fail.
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder urlRequestBuilder = testFramework.mCronetEngine.newUrlRequestBuilder(
                url, callback, callback.getExecutor());
        urlRequestBuilder.disableCache();
        urlRequestBuilder.build().start();
        callback.blockForDone();
        assertNotNull(callback.mError);
        assertContains("Exception in CronetUrlRequest: net::ERR_CONNECTION_REFUSED",
                callback.mError.getMessage());
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testEnableHttpCacheDiskNewEngine() throws Exception {
        final CronetTestFramework testFramework =
                startCronetTestFrameworkWithCacheEnabled(CronetEngine.Builder.HTTP_CACHE_DISK);
        String url = NativeTestServer.getFileURL("/cacheable.txt");
        checkRequestCaching(testFramework.mCronetEngine, url, false);
        checkRequestCaching(testFramework.mCronetEngine, url, true);
        NativeTestServer.shutdownNativeTestServer();
        checkRequestCaching(testFramework.mCronetEngine, url, true);

        // Shutdown original context and create another that uses the same cache.
        testFramework.mCronetEngine.shutdown();
        testFramework.mCronetEngine =
                (CronetEngineBase) testFramework.getCronetEngineBuilder().build();
        checkRequestCaching(testFramework.mCronetEngine, url, true);
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testInitEngineAndStartRequest() {
        CronetTestFramework testFramework = startCronetTestFrameworkAndSkipLibraryInit();

        // Immediately make a request after initializing the engine.
        CronetEngine cronetEngine = testFramework.initCronetEngine();
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder urlRequestBuilder =
                cronetEngine.newUrlRequestBuilder(mUrl, callback, callback.getExecutor());
        urlRequestBuilder.build().start();
        callback.blockForDone();
        assertEquals(200, callback.mResponseInfo.getHttpStatusCode());
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testEmptyGetCertVerifierData() {
        CronetTestFramework testFramework = startCronetTestFrameworkAndSkipLibraryInit();

        // Immediately make a request after initializing the engine.
        ExperimentalCronetEngine cronetEngine = testFramework.initCronetEngine();
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder urlRequestBuilder =
                cronetEngine.newUrlRequestBuilder(mUrl, callback, callback.getExecutor());
        urlRequestBuilder.build().start();
        callback.blockForDone();
        assertEquals(200, callback.mResponseInfo.getHttpStatusCode());

        try {
            cronetEngine.getCertVerifierData(-1);
            fail("Should throw an exception");
        } catch (Exception e) {
            assertEquals("timeout must be a positive value", e.getMessage());
        }
        // Because mUrl is http, getCertVerifierData() will return empty data.
        String data = cronetEngine.getCertVerifierData(100);
        assertTrue(data.isEmpty());
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testInitEngineStartTwoRequests() throws Exception {
        CronetTestFramework testFramework = startCronetTestFrameworkAndSkipLibraryInit();

        // Make two requests after initializing the context.
        CronetEngine cronetEngine = testFramework.initCronetEngine();
        int[] statusCodes = {0, 0};
        String[] urls = {mUrl, mUrl404};
        for (int i = 0; i < 2; i++) {
            TestUrlRequestCallback callback = new TestUrlRequestCallback();
            UrlRequest.Builder urlRequestBuilder =
                    cronetEngine.newUrlRequestBuilder(urls[i], callback, callback.getExecutor());
            urlRequestBuilder.build().start();
            callback.blockForDone();
            statusCodes[i] = callback.mResponseInfo.getHttpStatusCode();
        }
        assertEquals(200, statusCodes[0]);
        assertEquals(404, statusCodes[1]);
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testInitTwoEnginesSimultaneously() throws Exception {
        final CronetTestFramework testFramework = startCronetTestFrameworkAndSkipLibraryInit();

        // Threads will block on runBlocker to ensure simultaneous execution.
        ConditionVariable runBlocker = new ConditionVariable(false);
        RequestThread thread1 = new RequestThread(testFramework, mUrl, runBlocker);
        RequestThread thread2 = new RequestThread(testFramework, mUrl404, runBlocker);

        thread1.start();
        thread2.start();
        runBlocker.open();
        thread1.join();
        thread2.join();
        assertEquals(200, thread1.mCallback.mResponseInfo.getHttpStatusCode());
        assertEquals(404, thread2.mCallback.mResponseInfo.getHttpStatusCode());
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testInitTwoEnginesInSequence() throws Exception {
        final CronetTestFramework testFramework = startCronetTestFrameworkAndSkipLibraryInit();

        ConditionVariable runBlocker = new ConditionVariable(true);
        RequestThread thread1 = new RequestThread(testFramework, mUrl, runBlocker);
        RequestThread thread2 = new RequestThread(testFramework, mUrl404, runBlocker);

        thread1.start();
        thread1.join();
        thread2.start();
        thread2.join();
        assertEquals(200, thread1.mCallback.mResponseInfo.getHttpStatusCode());
        assertEquals(404, thread2.mCallback.mResponseInfo.getHttpStatusCode());
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testInitDifferentEngines() throws Exception {
        // Test that concurrently instantiating Cronet context's upon various
        // different versions of the same Android Context does not cause crashes
        // like crbug.com/453845
        CronetEngine firstEngine = new CronetEngine.Builder(getContext()).build();
        CronetEngine secondEngine =
                new CronetEngine.Builder(getContext().getApplicationContext()).build();
        CronetEngine thirdEngine =
                new CronetEngine.Builder(new ContextWrapper(getContext())).build();
        firstEngine.shutdown();
        secondEngine.shutdown();
        thirdEngine.shutdown();
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testGetGlobalMetricsDeltas() throws Exception {
        final CronetTestFramework testFramework = startCronetTestFramework();

        byte delta1[] = testFramework.mCronetEngine.getGlobalMetricsDeltas();

        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder builder = testFramework.mCronetEngine.newUrlRequestBuilder(
                mUrl, callback, callback.getExecutor());
        builder.build().start();
        callback.blockForDone();
        byte delta2[] = testFramework.mCronetEngine.getGlobalMetricsDeltas();
        assertTrue(delta2.length != 0);
        assertFalse(Arrays.equals(delta1, delta2));
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testCronetEngineBuilderConfig() throws Exception {
        // This is to prompt load of native library.
        startCronetTestFramework();
        // Verify CronetEngine.Builder config is passed down accurately to native code.
        ExperimentalCronetEngine.Builder builder =
                new ExperimentalCronetEngine.Builder(getContext());
        builder.enableHttp2(false);
        builder.enableQuic(true);
        builder.enableSdch(true);
        builder.addQuicHint("example.com", 12, 34);
        builder.setCertVerifierData("test_cert_verifier_data");
        builder.enableHttpCache(HTTP_CACHE_IN_MEMORY, 54321);
        builder.enableDataReductionProxy("abcd");
        builder.setUserAgent("efgh");
        builder.setExperimentalOptions("ijkl");
        builder.setDataReductionProxyOptions("mnop", "qrst", "uvwx");
        builder.setStoragePath(CronetTestFramework.getTestStorage(getContext()));
        builder.enablePublicKeyPinningBypassForLocalTrustAnchors(false);
        nativeVerifyUrlRequestContextConfig(
                CronetUrlRequestContext.createNativeUrlRequestContextConfig(
                        (CronetEngineBuilderImpl) builder.mBuilderDelegate),
                CronetTestFramework.getTestStorage(getContext()));
    }

    // Verifies that CronetEngine.Builder config from testCronetEngineBuilderConfig() is properly
    // translated to a native UrlRequestContextConfig.
    private static native void nativeVerifyUrlRequestContextConfig(long config, String storagePath);

    private static class TestBadLibraryLoader extends CronetEngine.Builder.LibraryLoader {
        private boolean mWasCalled = false;

        public void loadLibrary(String libName) {
            // Report that this method was called, but don't load the library
            mWasCalled = true;
        }

        boolean wasCalled() {
            return mWasCalled;
        }
    }

    @SmallTest
    @Feature({"Cronet"})
    @OnlyRunNativeCronet
    public void testSkipLibraryLoading() throws Exception {
        CronetEngine.Builder builder = new CronetEngine.Builder(getContext());
        TestBadLibraryLoader loader = new TestBadLibraryLoader();
        builder.setLibraryLoader(loader);
        try {
            // ensureInitialized() calls native code to check the version right after library load
            // and will error with the message below if library loading was skipped
            CronetLibraryLoader.ensureInitialized(getContext().getApplicationContext(),
                    (CronetEngineBuilderImpl) builder.mBuilderDelegate);
            fail("Native library should not be loaded");
        } catch (UnsatisfiedLinkError e) {
            assertTrue(loader.wasCalled());
        }
    }

    // Creates a CronetEngine on another thread and then one on the main thread.  This shouldn't
    // crash.
    @SmallTest
    @Feature({"Cronet"})
    public void testThreadedStartup() throws Exception {
        final ConditionVariable otherThreadDone = new ConditionVariable();
        final ConditionVariable uiThreadDone = new ConditionVariable();
        new Handler(Looper.getMainLooper()).post(new Runnable() {
            public void run() {
                final ExperimentalCronetEngine.Builder builder =
                        new ExperimentalCronetEngine.Builder(getContext());
                new Thread() {
                    public void run() {
                        CronetEngine cronetEngine = builder.build();
                        otherThreadDone.open();
                        cronetEngine.shutdown();
                    }
                }.start();
                otherThreadDone.block();
                builder.build().shutdown();
                uiThreadDone.open();
            }
        });
        assertTrue(uiThreadDone.block(1000));
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testHostResolverRules() throws Exception {
        String resolverTestHostname = "some-weird-hostname";
        URL testUrl = new URL(mUrl);
        ExperimentalCronetEngine.Builder cronetEngineBuilder =
                new ExperimentalCronetEngine.Builder(getContext());
        JSONObject hostResolverRules = new JSONObject().put(
                "host_resolver_rules", "MAP " + resolverTestHostname + " " + testUrl.getHost());
        JSONObject experimentalOptions =
                new JSONObject().put("HostResolverRules", hostResolverRules);
        cronetEngineBuilder.setExperimentalOptions(experimentalOptions.toString());

        final CronetTestFramework testFramework =
                startCronetTestFrameworkWithUrlAndCronetEngineBuilder(null, cronetEngineBuilder);
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        URL requestUrl =
                new URL("http", resolverTestHostname, testUrl.getPort(), testUrl.getFile());
        UrlRequest.Builder urlRequestBuilder = testFramework.mCronetEngine.newUrlRequestBuilder(
                requestUrl.toString(), callback, callback.getExecutor());
        urlRequestBuilder.build().start();
        callback.blockForDone();
        assertEquals(200, callback.mResponseInfo.getHttpStatusCode());
    }
}
