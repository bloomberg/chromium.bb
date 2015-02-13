// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import android.os.ConditionVariable;

import static junit.framework.Assert.assertEquals;
import static junit.framework.Assert.assertTrue;

import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.concurrent.Executor;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.ThreadFactory;

/**
 * Listener that tracks information from different callbacks and and has a
 * method to block thread until the request completes on another thread.
 * Allows to cancel, block request or throw an exception from an arbitrary step.
 */
class TestUrlRequestListener implements UrlRequestListener {
    public ArrayList<ResponseInfo> mRedirectResponseInfoList =
            new ArrayList<ResponseInfo>();
    public ResponseInfo mResponseInfo;
    public ExtendedResponseInfo mExtendedResponseInfo;
    public UrlRequestException mError;

    public ResponseStep mResponseStep = ResponseStep.NOTHING;

    public boolean mOnRedirectCalled = false;
    public boolean mOnErrorCalled = false;

    public int mHttpResponseDataLength = 0;
    public byte[] mLastDataReceivedAsBytes;
    public String mResponseAsString = "";

    // Conditionally fail on certain steps.
    private FailureType mFailureType = FailureType.NONE;
    private ResponseStep mFailureStep = ResponseStep.NOTHING;

    // Signals when request is done either successfully or not.
    private ConditionVariable mDone = new ConditionVariable();
    private ConditionVariable mStepBlock = new ConditionVariable(true);

    // Executor for Cronet callbacks.
    ExecutorService mExecutor = Executors.newSingleThreadExecutor(
            new ExecutorThreadFactory());
    Thread mExecutorThread;

    private class ExecutorThreadFactory implements ThreadFactory {
        public Thread newThread(Runnable r) {
            mExecutorThread = new Thread(r);
            return mExecutorThread;
        }
    }

    public enum ResponseStep {
        NOTHING,
        ON_REDIRECT,
        ON_RESPONSE_STARTED,
        ON_DATA_RECEIVED,
        ON_SUCCEEDED
    };

    public enum FailureType {
        NONE,
        BLOCK,
        CANCEL_SYNC,
        CANCEL_ASYNC,
        THROW_SYNC
    };

    public TestUrlRequestListener() {
    }

    public void setFailure(FailureType failureType, ResponseStep failureStep) {
        mFailureStep = failureStep;
        mFailureType = failureType;
        if (failureType == FailureType.BLOCK) {
            mStepBlock.close();
        }
    }

    public void blockForDone() {
        mDone.block();
    }

    public void openBlockedStep() {
        mStepBlock.open();
    }

    public Executor getExecutor() {
        return mExecutor;
    }

    @Override
    public void onRedirect(UrlRequest request,
            ResponseInfo info,
            String newLocationUrl) {
        assertEquals(mExecutorThread, Thread.currentThread());
        assertTrue(mResponseStep == ResponseStep.NOTHING
                   || mResponseStep == ResponseStep.ON_REDIRECT);
        mRedirectResponseInfoList.add(info);
        mResponseStep = ResponseStep.ON_REDIRECT;
        mOnRedirectCalled = true;
        maybeThrowOrCancel(request);
    }

    @Override
    public void onResponseStarted(UrlRequest request, ResponseInfo info) {
        assertEquals(mExecutorThread, Thread.currentThread());
        assertTrue(mResponseStep == ResponseStep.NOTHING
                   || mResponseStep == ResponseStep.ON_REDIRECT);
        mResponseStep = ResponseStep.ON_RESPONSE_STARTED;
        mResponseInfo = info;
        maybeThrowOrCancel(request);
    }

    @Override
    public void onDataReceived(UrlRequest request,
            ResponseInfo info,
            ByteBuffer byteBuffer) {
        assertEquals(mExecutorThread, Thread.currentThread());
        assertTrue(mResponseStep == ResponseStep.ON_RESPONSE_STARTED
                   || mResponseStep == ResponseStep.ON_DATA_RECEIVED);
        mResponseStep = ResponseStep.ON_DATA_RECEIVED;

        mHttpResponseDataLength += byteBuffer.capacity();
        mLastDataReceivedAsBytes = new byte[byteBuffer.capacity()];
        byteBuffer.get(mLastDataReceivedAsBytes);
        mResponseAsString += new String(mLastDataReceivedAsBytes);
        maybeThrowOrCancel(request);
    }

    @Override
    public void onSucceeded(UrlRequest request, ExtendedResponseInfo info) {
        assertEquals(mExecutorThread, Thread.currentThread());
        assertTrue(mResponseStep == ResponseStep.ON_RESPONSE_STARTED
                   || mResponseStep == ResponseStep.ON_DATA_RECEIVED);
        mResponseStep = ResponseStep.ON_SUCCEEDED;
        mExtendedResponseInfo = info;
        openDone();
        maybeThrowOrCancel(request);
    }

    @Override
    public void onFailed(UrlRequest request,
            ResponseInfo info,
            UrlRequestException error) {
        assertEquals(mExecutorThread, Thread.currentThread());
        mOnErrorCalled = true;
        mError = error;
        openDone();
        maybeThrowOrCancel(request);
    }

    protected void openDone() {
        mDone.open();
    }

    private void maybeThrowOrCancel(final UrlRequest request) {
        if (mResponseStep != mFailureStep) {
            return;
        }
        if (mFailureType == FailureType.NONE) {
            return;
        }
        if (mFailureType == FailureType.THROW_SYNC) {
            throw new IllegalStateException("Listener Exception.");
        }
        Runnable task = new Runnable() {
            public void run() {
                request.cancel();
                openDone();
            }
        };
        if (mFailureType == FailureType.CANCEL_ASYNC) {
            mExecutor.execute(task);
        } else {
            task.run();
        }
    }
}

