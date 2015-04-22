// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.urlconnection;

import java.io.IOException;
import java.util.concurrent.BlockingQueue;
import java.util.concurrent.Executor;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.RejectedExecutionException;

/**
 * A MessageLoop class for use in {@link CronetHttpURLConnection}.
 */
class MessageLoop implements Executor {
    private final BlockingQueue<Runnable> mQueue;

    // Indicates whether this message loop is currently running.
    private boolean mLoopRunning = false;

    // Indicates whether an InterruptedException or a RuntimeException has
    // occurred in loop(). If true, the loop cannot be safely started because
    // this might cause the loop to terminate immediately if there is a quit
    // task enqueued.
    private boolean mLoopFailed = false;

    // Used when assertions are enabled to enforce single-threaded use.
    private static final long INVALID_THREAD_ID = -1;
    private long mThreadId = INVALID_THREAD_ID;

    MessageLoop() {
        mQueue = new LinkedBlockingQueue<Runnable>();
    }

    private boolean calledOnValidThread() {
        if (mThreadId == INVALID_THREAD_ID) {
            mThreadId = Thread.currentThread().getId();
            return true;
        }
        return mThreadId == Thread.currentThread().getId();
    }

    /**
     * Runs the message loop. Be sure to call {@link MessageLoop#quit()}
     * to end the loop. If an interruptedException occurs, the loop cannot be
     * started again (see {@link #mLoopFailed}).
     * @throws IOException
     */
    public void loop() throws IOException {
        assert calledOnValidThread();
        if (mLoopFailed) {
            throw new IllegalStateException(
                    "Cannot run loop as an exception has occurred previously.");
        }
        if (mLoopRunning) {
            throw new IllegalStateException(
                    "Cannot run loop when it is already running.");
        }
        mLoopRunning = true;
        while (mLoopRunning) {
            try {
                Runnable task = mQueue.take(); // Blocks if the queue is empty.
                task.run();
            } catch (InterruptedException | RuntimeException e) {
                mLoopRunning = false;
                mLoopFailed = true;
                if (e instanceof InterruptedException) {
                    throw new IOException(e);
                } else if (e instanceof RuntimeException) {
                    throw (RuntimeException) e;
                }
            }
        }
    }

    /**
     * This causes {@link #loop()} to stop executing messages after the current
     * message being executed.  Should only be called from the currently
     * executing message.
     */
    public void quit() {
        assert calledOnValidThread();
        mLoopRunning = false;
    }

    /**
     * Posts a task to the message loop.
     */
    @Override
    public void execute(Runnable task) throws RejectedExecutionException {
        if (task == null) {
            throw new IllegalArgumentException();
        }
        try {
            mQueue.put(task);
        } catch (InterruptedException e) {
            // In theory this exception won't happen, since we have an blocking
            // queue with Integer.MAX_Value capacity, put() call will not block.
            throw new RejectedExecutionException(e);
        }
    }

    /**
     * Returns whether the loop is currently running. Used in testing.
     */
    public boolean isRunning() {
        return mLoopRunning;
    }

    /**
     * Returns whether an exception occurred in {#loop()}. Used in testing.
     */
    public boolean hasLoopFailed() {
        return mLoopFailed;
    }
}
