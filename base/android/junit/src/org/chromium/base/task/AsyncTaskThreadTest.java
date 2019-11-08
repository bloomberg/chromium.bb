// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.task;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import android.support.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Ignore;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.ExpectedException;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.android.util.concurrent.RoboExecutorService;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLog;
import org.robolectric.util.Scheduler;

import org.chromium.base.Log;
import org.chromium.base.test.BaseRobolectricTestRunner;

import java.util.concurrent.CancellationException;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.TimeUnit;

/**
 * Tests for {@link AsyncTask}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class AsyncTaskThreadTest {
    private static final String TAG = "AsyncTaskThreadTest";
    private static final boolean DEBUG = false;

    private static class BlockAndGetFeedDataTask extends AsyncTask<Boolean> {
        private LinkedBlockingQueue<Boolean> mIncomingQueue = new LinkedBlockingQueue<Boolean>();
        private LinkedBlockingQueue<Boolean> mOutgoingQueue = new LinkedBlockingQueue<Boolean>();
        private LinkedBlockingQueue<Boolean> mInterruptedExceptionQueue =
                new LinkedBlockingQueue<Boolean>();
        private Boolean mPostExecuteResult;

        @Override
        protected Boolean doInBackground() {
            if (DEBUG) Log.i(TAG, "doInBackground");
            mOutgoingQueue.add(true);
            return blockAndGetFeedData();
        }

        @Override
        protected void onPostExecute(Boolean result) {
            if (DEBUG) Log.i(TAG, "onPostExecute: " + result);
            mPostExecuteResult = result;
        }

        public void feedData(Boolean data) {
            mIncomingQueue.add(data);
        }

        private Boolean blockAndGetFeedData() {
            try {
                return mIncomingQueue.poll(3, TimeUnit.SECONDS);
            } catch (InterruptedException e) {
                mInterruptedExceptionQueue.add(true);
                return false;
            }
        }

        public void blockUntilDoInBackgroundStarts() throws Exception {
            mOutgoingQueue.poll(3, TimeUnit.SECONDS);
        }

        public Boolean getPostExecuteResult() {
            return mPostExecuteResult;
        }

        public LinkedBlockingQueue<Boolean> getInterruptedExceptionQueue() {
            return mInterruptedExceptionQueue;
        }
    }

    private final BlockAndGetFeedDataTask mTask = new BlockAndGetFeedDataTask();
    private final RoboExecutorService mRoboExecutorService = new RoboExecutorService();
    private final Scheduler mBackgroundScheduler = Robolectric.getBackgroundThreadScheduler();

    @Rule
    public ExpectedException thrown = ExpectedException.none();

    public AsyncTaskThreadTest() {
        if (DEBUG) ShadowLog.stream = System.out;
    }

    @Before
    public void setUp() {
        mBackgroundScheduler.pause();
    }

    @After
    public void tearDown() {
        // No unexpected interrupted exception.
        assertNull(mTask.getInterruptedExceptionQueue().poll());
        Assert.assertTrue(mRoboExecutorService.shutdownNow().isEmpty());
    }

    @Test
    @SmallTest
    public void testCancel_ReturnsFalseOnceTaskFinishes() throws Exception {
        // This test requires robo executor service such that we can run
        // one background task.
        mTask.executeOnExecutor(mRoboExecutorService);

        // Ensure that the background thread is not blocked.
        mTask.feedData(true);

        mBackgroundScheduler.runOneTask();

        // Cannot cancel. The task is already run.
        assertFalse(mTask.cancel(false /* mayInterruptIfRunning */));
        assertTrue(mTask.get());
        assertEquals(Boolean.valueOf(true), mTask.getPostExecuteResult());
    }

    @Test
    @SmallTest
    public void testCancel_CanReturnTrueEvenAfterTaskStarts() throws Exception {
        mTask.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);

        // Wait until the task is started. Note that data is not yet fed.
        mTask.blockUntilDoInBackgroundStarts();
        // This reflects FutureTask#cancel() behavior. Note that the task is
        // started but cancel can still return true.
        assertTrue(mTask.cancel(false /* mayInterruptIfRunning */));

        // Continue the task.
        mTask.feedData(true);

        // get() will raise an exception although the task is started.
        try {
            mTask.get();
            Assert.fail();
        } catch (CancellationException e) {
            // expected
        }
        assertNull(mTask.getPostExecuteResult()); // onPostExecute did not run.
    }

    @Test
    @SmallTest
    public void testCancel_MayInterrupt_ReturnsFalseOnceTaskFinishes() throws Exception {
        // This test requires robo executor service such that we can run
        // one background task.
        mTask.executeOnExecutor(mRoboExecutorService);

        // Ensure that the background thread is not blocked.
        mTask.feedData(true);

        mBackgroundScheduler.runOneTask();

        // Cannot cancel. The task is already run.
        assertFalse(mTask.cancel(true /* mayInterruptIfRunning */));
        assertTrue(mTask.get());
        assertEquals(Boolean.valueOf(true), mTask.getPostExecuteResult());
    }

    @Test
    @SmallTest
    @Ignore("crbug.com/1022954")
    public void testCancel_MayInterrupt_TaskIsInterrupted() throws Exception {
        mTask.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);

        // Wait until the task is started. Note that data is not yet fed.
        mTask.blockUntilDoInBackgroundStarts();

        // Cancel and interrupt the current task.
        assertTrue(mTask.cancel(true /* mayInterruptIfRunning */));

        // Ensure that the background thread is not blocked.
        mTask.feedData(true);

        // get() will raise an exception although the task is started.
        try {
            mTask.get();
            Assert.fail();
        } catch (CancellationException e) {
            // expected
        }
        assertNull(mTask.getPostExecuteResult()); // onPostExecute did not run.
        // Task was interrupted.
        assertEquals(Boolean.valueOf(true), mTask.getInterruptedExceptionQueue().poll());
    }
}
