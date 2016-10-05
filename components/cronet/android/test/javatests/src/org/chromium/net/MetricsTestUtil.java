// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import static junit.framework.Assert.assertNotNull;
import static junit.framework.Assert.assertNull;
import static junit.framework.Assert.assertTrue;

import android.os.ConditionVariable;

import java.util.Date;
import java.util.LinkedList;
import java.util.NoSuchElementException;
import java.util.concurrent.Executor;
import java.util.concurrent.Executors;

/**
 * Classes which are useful for testing Cronet's metrics implementation and are needed in more than
 * one test file.
 */
public class MetricsTestUtil {
    /**
     * Executor which runs tasks only when told to with runAllTasks().
     */
    public static class TestExecutor implements Executor {
        private final LinkedList<Runnable> mTaskQueue = new LinkedList<Runnable>();

        @Override
        public void execute(Runnable task) {
            mTaskQueue.add(task);
        }

        public void runAllTasks() {
            try {
                while (mTaskQueue.size() > 0) {
                    mTaskQueue.remove().run();
                }
            } catch (NoSuchElementException e) {
                throw new RuntimeException("Task was removed during iteration", e);
            }
        }
    }

    /**
     * RequestFinishedInfo.Listener for testing, which saves the RequestFinishedInfo
     */
    public static class TestRequestFinishedListener extends RequestFinishedInfo.Listener {
        private RequestFinishedInfo mRequestInfo;
        private ConditionVariable mBlock;
        private int mNumExpectedRequests = -1;

        // TODO(mgersh): it's weird that you can use either this constructor or blockUntilDone() but
        // not both. Either clean it up or document why it has to work this way.
        public TestRequestFinishedListener(Executor executor) {
            super(executor);
        }

        public TestRequestFinishedListener(int numExpectedRequests) {
            super(Executors.newSingleThreadExecutor());
            mNumExpectedRequests = numExpectedRequests;
            mBlock = new ConditionVariable();
        }

        public TestRequestFinishedListener() {
            super(Executors.newSingleThreadExecutor());
            mNumExpectedRequests = 1;
            mBlock = new ConditionVariable();
        }

        public RequestFinishedInfo getRequestInfo() {
            return mRequestInfo;
        }

        @Override
        public void onRequestFinished(RequestFinishedInfo requestInfo) {
            assertNull("onRequestFinished called repeatedly", mRequestInfo);
            assertNotNull(requestInfo);
            mRequestInfo = requestInfo;
            mNumExpectedRequests--;
            if (mNumExpectedRequests == 0) {
                mBlock.open();
            }
        }

        public void blockUntilDone() {
            mBlock.block();
        }

        public void reset() {
            mBlock.close();
            mNumExpectedRequests = 1;
            mRequestInfo = null;
        }
    }

    /**
     * Check existence of all the timing metrics that apply to most test requests,
     * except those that come from net::LoadTimingInfo::ConnectTiming.
     * Also check some timing differences, focusing on things we can't check with asserts in the
     * CronetMetrics constructor.
     * Don't check push times here.
     */
    public static void checkTimingMetrics(
            RequestFinishedInfo.Metrics metrics, Date startTime, Date endTime) {
        assertNotNull(metrics.getRequestStart());
        assertTrue(metrics.getRequestStart().after(startTime)
                || metrics.getRequestStart().equals(startTime));
        assertNotNull(metrics.getSendingStart());
        assertTrue(metrics.getSendingStart().after(startTime)
                || metrics.getSendingStart().equals(startTime));
        assertNotNull(metrics.getSendingEnd());
        assertTrue(metrics.getSendingEnd().before(endTime));
        assertNotNull(metrics.getResponseStart());
        assertTrue(metrics.getResponseStart().after(startTime));
        assertNotNull(metrics.getResponseEnd());
        assertTrue(metrics.getResponseEnd().before(endTime)
                || metrics.getResponseEnd().equals(endTime));
        // Entire request should take more than 0 ms
        assertTrue(metrics.getResponseEnd().getTime() - metrics.getRequestStart().getTime() > 0);
    }

    /**
     * Check that the timing metrics which come from net::LoadTimingInfo::ConnectTiming exist,
     * except SSL times in the case of non-https requests.
     */
    public static void checkHasConnectTiming(
            RequestFinishedInfo.Metrics metrics, Date startTime, Date endTime, boolean isSsl) {
        assertNotNull(metrics.getDnsStart());
        assertTrue(
                metrics.getDnsStart().after(startTime) || metrics.getDnsStart().equals(startTime));
        assertNotNull(metrics.getDnsEnd());
        assertTrue(metrics.getDnsEnd().before(endTime));
        assertNotNull(metrics.getConnectStart());
        assertTrue(metrics.getConnectStart().after(startTime)
                || metrics.getConnectStart().equals(startTime));
        assertNotNull(metrics.getConnectEnd());
        assertTrue(metrics.getConnectEnd().before(endTime));
        if (isSsl) {
            assertNotNull(metrics.getSslStart());
            assertTrue(metrics.getSslStart().after(startTime)
                    || metrics.getSslStart().equals(startTime));
            assertNotNull(metrics.getSslEnd());
            assertTrue(metrics.getSslEnd().before(endTime));
        } else {
            assertNull(metrics.getSslStart());
            assertNull(metrics.getSslEnd());
        }
    }

    /**
     * Check that the timing metrics from net::LoadTimingInfo::ConnectTiming don't exist.
     */
    public static void checkNoConnectTiming(RequestFinishedInfo.Metrics metrics) {
        assertNull(metrics.getDnsStart());
        assertNull(metrics.getDnsEnd());
        assertNull(metrics.getSslStart());
        assertNull(metrics.getSslEnd());
        assertNull(metrics.getConnectStart());
        assertNull(metrics.getConnectEnd());
    }
}
