// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import static org.chromium.base.CollectionUtil.newHashSet;

import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.test.util.Feature;
import org.chromium.net.MetricsTestUtil.TestExecutor;
import org.chromium.net.MetricsTestUtil.TestRequestFinishedListener;
import org.chromium.net.impl.CronetMetrics;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.ArrayList;
import java.util.Date;
import java.util.HashSet;
import java.util.List;
import java.util.concurrent.Executor;

/**
 * Test RequestFinishedInfo.Listener and the metrics information it provides.
 */
public class RequestFinishedInfoTest extends CronetTestBase {
    CronetTestFramework mTestFramework;
    private EmbeddedTestServer mTestServer;
    private String mUrl;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mTestServer = EmbeddedTestServer.createAndStartDefaultServer(getContext());
        mUrl = mTestServer.getURL("/echo?status=200");
    }

    @Override
    protected void tearDown() throws Exception {
        mTestServer.stopAndDestroyServer();
        super.tearDown();
    }

    static class DirectExecutor implements Executor {
        @Override
        public void execute(Runnable task) {
            task.run();
        }
    }

    static class ThreadExecutor implements Executor {
        private List<Thread> mThreads = new ArrayList<Thread>();

        @Override
        public void execute(Runnable task) {
            Thread newThread = new Thread(task);
            mThreads.add(newThread);
            newThread.start();
        }

        public void joinAll() throws InterruptedException {
            for (Thread thread : mThreads) {
                thread.join();
            }
        }
    }

    @SmallTest
    @Feature({"Cronet"})
    @SuppressWarnings("deprecation")
    public void testRequestFinishedListener() throws Exception {
        mTestFramework = startCronetTestFramework();
        TestExecutor testExecutor = new TestExecutor();
        TestRequestFinishedListener requestFinishedListener =
                new TestRequestFinishedListener(testExecutor);
        mTestFramework.mCronetEngine.addRequestFinishedListener(requestFinishedListener);
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder urlRequestBuilder = new UrlRequest.Builder(
                mUrl, callback, callback.getExecutor(), mTestFramework.mCronetEngine);
        urlRequestBuilder.addRequestAnnotation("request annotation")
                .addRequestAnnotation(this)
                .build()
                .start();
        callback.blockForDone();
        testExecutor.runAllTasks();

        RequestFinishedInfo requestInfo = requestFinishedListener.getRequestInfo();
        assertNotNull("RequestFinishedInfo.Listener must be called", requestInfo);
        assertEquals(mUrl, requestInfo.getUrl());
        assertNotNull(requestInfo.getResponseInfo());
        assertEquals(newHashSet("request annotation", this), // Use sets for unordered comparison.
                new HashSet<Object>(requestInfo.getAnnotations()));
        RequestFinishedInfo.Metrics metrics = requestInfo.getMetrics();
        assertNotNull("RequestFinishedInfo.getMetrics() must not be null", metrics);
        assertTrue(metrics.getTotalTimeMs() > 0);
        assertTrue(metrics.getTotalTimeMs() >= metrics.getTtfbMs());
        assertTrue(metrics.getReceivedBytesCount() > 0);
        mTestFramework.mCronetEngine.shutdown();
    }

    @SmallTest
    @Feature({"Cronet"})
    @SuppressWarnings("deprecation")
    public void testRequestFinishedListenerDirectExecutor() throws Exception {
        mTestFramework = startCronetTestFramework();
        Executor testExecutor = new DirectExecutor();
        TestRequestFinishedListener requestFinishedListener =
                new TestRequestFinishedListener(testExecutor);
        mTestFramework.mCronetEngine.addRequestFinishedListener(requestFinishedListener);
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder urlRequestBuilder = new UrlRequest.Builder(
                mUrl, callback, callback.getExecutor(), mTestFramework.mCronetEngine);
        urlRequestBuilder.addRequestAnnotation("request annotation")
                .addRequestAnnotation(this)
                .build()
                .start();
        callback.blockForDone();

        RequestFinishedInfo requestInfo = requestFinishedListener.getRequestInfo();
        assertNotNull("RequestFinishedInfo.Listener must be called", requestInfo);
        assertEquals(mUrl, requestInfo.getUrl());
        assertNotNull(requestInfo.getResponseInfo());
        assertEquals(newHashSet("request annotation", this), // Use sets for unordered comparison.
                new HashSet<Object>(requestInfo.getAnnotations()));
        RequestFinishedInfo.Metrics metrics = requestInfo.getMetrics();
        assertNotNull("RequestFinishedInfo.getMetrics() must not be null", metrics);
        assertTrue(metrics.getTotalTimeMs() > 0);
        assertTrue(metrics.getTotalTimeMs() >= metrics.getTtfbMs());
        assertTrue(metrics.getReceivedBytesCount() > 0);
        mTestFramework.mCronetEngine.shutdown();
    }

    @SmallTest
    @Feature({"Cronet"})
    @SuppressWarnings("deprecation")
    public void testRequestFinishedListenerDifferentThreads() throws Exception {
        mTestFramework = startCronetTestFramework();
        ThreadExecutor testExecutor = new ThreadExecutor();
        TestRequestFinishedListener firstListener = new TestRequestFinishedListener(testExecutor);
        TestRequestFinishedListener secondListener = new TestRequestFinishedListener(testExecutor);
        mTestFramework.mCronetEngine.addRequestFinishedListener(firstListener);
        mTestFramework.mCronetEngine.addRequestFinishedListener(secondListener);
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder urlRequestBuilder = new UrlRequest.Builder(
                mUrl, callback, callback.getExecutor(), mTestFramework.mCronetEngine);
        urlRequestBuilder.addRequestAnnotation("request annotation")
                .addRequestAnnotation(this)
                .build()
                .start();
        callback.blockForDone();
        testExecutor.joinAll();

        RequestFinishedInfo firstRequestInfo = firstListener.getRequestInfo();
        RequestFinishedInfo secondRequestInfo = secondListener.getRequestInfo();
        assertNotNull("First RequestFinishedInfo.Listener must be called", firstRequestInfo);
        assertNotNull("Second RequestFinishedInfo.Listener must be called", secondRequestInfo);
        assertEquals(mUrl, firstRequestInfo.getUrl());
        assertEquals(mUrl, secondRequestInfo.getUrl());
        assertNotNull(firstRequestInfo.getResponseInfo());
        assertNotNull(secondRequestInfo.getResponseInfo());
        assertEquals(newHashSet("request annotation", this), // Use sets for unordered comparison.
                new HashSet<Object>(firstRequestInfo.getAnnotations()));
        assertEquals(newHashSet("request annotation", this),
                new HashSet<Object>(secondRequestInfo.getAnnotations()));
        RequestFinishedInfo.Metrics firstMetrics = firstRequestInfo.getMetrics();
        assertNotNull("RequestFinishedInfo.getMetrics() must not be null", firstMetrics);
        assertTrue(firstMetrics.getTotalTimeMs() > 0);
        assertTrue(firstMetrics.getTotalTimeMs() >= firstMetrics.getTtfbMs());
        assertTrue(firstMetrics.getReceivedBytesCount() > 0);
        RequestFinishedInfo.Metrics secondMetrics = secondRequestInfo.getMetrics();
        assertNotNull("RequestFinishedInfo.getMetrics() must not be null", secondMetrics);
        assertTrue(secondMetrics.getTotalTimeMs() > 0);
        assertTrue(secondMetrics.getTotalTimeMs() >= secondMetrics.getTtfbMs());
        assertTrue(secondMetrics.getReceivedBytesCount() > 0);
        mTestFramework.mCronetEngine.shutdown();
    }

    @SmallTest
    @Feature({"Cronet"})
    @SuppressWarnings("deprecation")
    public void testRequestFinishedListenerFailedRequest() throws Exception {
        String connectionRefusedUrl = "http://127.0.0.1:3";
        mTestFramework = startCronetTestFramework();
        TestExecutor testExecutor = new TestExecutor();
        TestRequestFinishedListener requestFinishedListener =
                new TestRequestFinishedListener(testExecutor);
        mTestFramework.mCronetEngine.addRequestFinishedListener(requestFinishedListener);
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder urlRequestBuilder = new UrlRequest.Builder(connectionRefusedUrl,
                callback, callback.getExecutor(), mTestFramework.mCronetEngine);
        urlRequestBuilder.build().start();
        callback.blockForDone();
        assertTrue(callback.mOnErrorCalled);
        testExecutor.runAllTasks();

        RequestFinishedInfo requestInfo = requestFinishedListener.getRequestInfo();
        assertNotNull("RequestFinishedInfo.Listener must be called", requestInfo);
        assertEquals(connectionRefusedUrl, requestInfo.getUrl());
        assertTrue(requestInfo.getAnnotations().isEmpty());
        RequestFinishedInfo.Metrics metrics = requestInfo.getMetrics();
        assertNotNull("RequestFinishedInfo.getMetrics() must not be null", metrics);
        // The failure is occasionally fast enough that time reported is 0, so just check for null
        assertNotNull(metrics.getTotalTimeMs());
        assertNull(metrics.getTtfbMs());
        assertTrue(metrics.getReceivedBytesCount() == null || metrics.getReceivedBytesCount() == 0);
        mTestFramework.mCronetEngine.shutdown();
    }

    @SmallTest
    @Feature({"Cronet"})
    @SuppressWarnings("deprecation")
    public void testRequestFinishedListenerRemoved() throws Exception {
        mTestFramework = startCronetTestFramework();
        TestExecutor testExecutor = new TestExecutor();
        TestRequestFinishedListener requestFinishedListener =
                new TestRequestFinishedListener(testExecutor);
        mTestFramework.mCronetEngine.addRequestFinishedListener(requestFinishedListener);
        mTestFramework.mCronetEngine.removeRequestFinishedListener(requestFinishedListener);
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder urlRequestBuilder = new UrlRequest.Builder(
                mUrl, callback, callback.getExecutor(), mTestFramework.mCronetEngine);
        urlRequestBuilder.build().start();
        callback.blockForDone();
        testExecutor.runAllTasks();

        assertNull("RequestFinishedInfo.Listener must not be called",
                requestFinishedListener.getRequestInfo());
        mTestFramework.mCronetEngine.shutdown();
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testMetricsGetters() throws Exception {
        long requestStart = 1;
        long dnsStart = 2;
        long dnsEnd = -1;
        long connectStart = 4;
        long connectEnd = 5;
        long sslStart = 6;
        long sslEnd = 7;
        long sendingStart = 8;
        long sendingEnd = 9;
        long pushStart = 10;
        long pushEnd = 11;
        long responseStart = 12;
        long responseEnd = 13;
        boolean socketReused = true;
        long sentBytesCount = 14;
        long receivedBytesCount = 15;
        // Make sure nothing gets reordered inside the Metrics class
        RequestFinishedInfo.Metrics metrics = new CronetMetrics(requestStart, dnsStart, dnsEnd,
                connectStart, connectEnd, sslStart, sslEnd, sendingStart, sendingEnd, pushStart,
                pushEnd, responseStart, responseEnd, socketReused, sentBytesCount,
                receivedBytesCount);
        assertEquals(new Date(requestStart), metrics.getRequestStart());
        // -1 timestamp should translate to null
        assertNull(metrics.getDnsEnd());
        assertEquals(new Date(dnsStart), metrics.getDnsStart());
        assertEquals(new Date(connectStart), metrics.getConnectStart());
        assertEquals(new Date(connectEnd), metrics.getConnectEnd());
        assertEquals(new Date(sslStart), metrics.getSslStart());
        assertEquals(new Date(sslEnd), metrics.getSslEnd());
        assertEquals(new Date(pushStart), metrics.getPushStart());
        assertEquals(new Date(pushEnd), metrics.getPushEnd());
        assertEquals(new Date(responseStart), metrics.getResponseStart());
        assertEquals(new Date(responseEnd), metrics.getResponseEnd());
        assertEquals(socketReused, metrics.getSocketReused());
        assertEquals(sentBytesCount, (long) metrics.getSentBytesCount());
        assertEquals(receivedBytesCount, (long) metrics.getReceivedBytesCount());
    }
}
