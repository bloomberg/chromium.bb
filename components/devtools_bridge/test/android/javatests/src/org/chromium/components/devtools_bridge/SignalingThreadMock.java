// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.devtools_bridge;

import java.util.concurrent.BlockingQueue;
import java.util.concurrent.Callable;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.LinkedBlockingDeque;
import java.util.concurrent.ScheduledExecutorService;
import java.util.concurrent.ScheduledFuture;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicInteger;

/**
 * Convinience class for tests. Like WebRTC threads supports posts
 * and synchromous invokes.
 */
class SignalingThreadMock {
    // TODO: use scaleTimeout when natives for org.chromium.base get available.
    private static final int EXECUTION_TIME_LIMIT_MS = 5000;

    private final AtomicInteger mInvokationCounter = new AtomicInteger(0);
    private final ExecutorService mExecutor = Executors.newSingleThreadExecutor();
    private final ScheduledExecutorService mWatchDogExecutor =
            Executors.newSingleThreadScheduledExecutor();
    private ScheduledFuture<?> mWatchDogFuture;
    private final Thread mThread;
    private final BlockingQueue<Runnable> mExecutionQueue = new LinkedBlockingDeque<Runnable>();

    public SignalingThreadMock() {
        mThread = new Thread() {
            @Override
            public void run() {
                try {
                    runExecutionLoop();
                } catch (InterruptedException e) {
                    // Normal finish.
                }
            }
        };
        mThread.start();
    }

    private void runExecutionLoop() throws InterruptedException {
        while (true) {
            mExecutionQueue.take().run();
        }
    }

    public void invoke(final Runnable runnable) {
        try {
            invoke(new TestUtils.RunnableAdapter(runnable));
        } catch (Exception e) {
            throw new RuntimeException(e);
        }
    }

    public <T> T invoke(final Callable<T> callable) throws Exception {
        if (isOnThread()) return callable.call();

        try {
            return new InvokeWrapper<T>(callable).invoke();
        } catch (InterruptedException e) {
            throw new RuntimeException(e);
        } catch (ExecutionException e) {
            throw (Exception) e.getCause();
        }
    }

    public void post(Runnable runnable) {
        boolean success = mExecutionQueue.offer(new PostWrapper(runnable));
        assert success;
    }

    public void dispose() {
        mWatchDogExecutor.shutdown();
        mThread.interrupt();
        try {
            mThread.join();
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
        }
    }

    public boolean isOnThread() {
        return Thread.currentThread() == mThread;
    }

    private void onStartedExecution(final int index, final Exception timeoutException) {
        mWatchDogFuture = mWatchDogExecutor.schedule(new Runnable() {
            @Override
            public void run() {
                throw new RuntimeException(
                        "Time limit on " + Integer.toString(index) + " invocation",
                        timeoutException);
            }
        }, EXECUTION_TIME_LIMIT_MS, TimeUnit.MILLISECONDS);
    }

    private void onFinishedExecution() {
        mWatchDogFuture.cancel(false);
    }

    private abstract class WrapperBase implements Runnable {
        private final int mIndex;
        private final Exception mTimeoutException;

        protected WrapperBase() {
            mIndex = mInvokationCounter.incrementAndGet();
            mTimeoutException = new Exception("Timeout exception");
        }

        @Override
        public final void run() {
            onStartedExecution(mIndex, mTimeoutException);
            try {
                runWrapped();
            } finally {
                onFinishedExecution();
            }
        }

        protected abstract void runWrapped();
    }

    private class InvokeWrapper<T> extends WrapperBase {
        private final Callable<T> mWrapped;
        private final TestUtils.InvokeHelper<T> mHelper = new TestUtils.InvokeHelper<T>();

        public InvokeWrapper(Callable<T> wrapped) {
            mWrapped = wrapped;
        }

        @Override
        protected void runWrapped() {
            mHelper.runOnTargetThread(mWrapped);
        }

        public T invoke() throws Exception {
            boolean success = mExecutionQueue.offer(this);
            assert success;
            return mHelper.takeResult();
        }
    }

    private class PostWrapper extends WrapperBase {
        private final Runnable mWrapped;

        public PostWrapper(Runnable wrapped) {
            mWrapped = wrapped;
        }

        @Override
        protected void runWrapped() {
            mWrapped.run();
        }
    }
}
