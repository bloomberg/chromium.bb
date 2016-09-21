// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import static junit.framework.Assert.assertNotNull;
import static junit.framework.Assert.assertNull;

import java.util.LinkedList;
import java.util.NoSuchElementException;
import java.util.concurrent.Executor;

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

        public TestRequestFinishedListener(Executor executor) {
            super(executor);
        }

        public RequestFinishedInfo getRequestInfo() {
            return mRequestInfo;
        }

        @Override
        public void onRequestFinished(RequestFinishedInfo requestInfo) {
            assertNull("onRequestFinished called repeatedly", mRequestInfo);
            assertNotNull(requestInfo);
            mRequestInfo = requestInfo;
        }
    }
}
