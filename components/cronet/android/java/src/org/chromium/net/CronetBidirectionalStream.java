// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import org.chromium.base.Log;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeClassQualifiedName;

import java.nio.ByteBuffer;
import java.util.AbstractMap;
import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.concurrent.Executor;
import java.util.concurrent.RejectedExecutionException;

import javax.annotation.concurrent.GuardedBy;

/**
 * {@link BidirectionalStream} implementation using Chromium network stack.
 * All @CalledByNative methods are called on the native network thread
 * and post tasks with callback calls onto Executor. Upon returning from callback, the native
 * stream is called on Executor thread and posts native tasks to the native network thread.
 */
@JNINamespace("cronet")
class CronetBidirectionalStream extends BidirectionalStream {
    /**
     * States of BidirectionalStream are tracked in mReadState and mWriteState.
     * The write state is separated out as it changes independently of the read state.
     * There is one initial state: State.NOT_STARTED. There is one normal final state:
     * State.SUCCESS, reached after State.READING_DONE and State.WRITING_DONE. There are two
     * exceptional final states: State.CANCELED and State.ERROR, which can be reached from
     * any other non-final state.
     */
    private enum State {
        /* Initial state, stream not started. */
        NOT_STARTED,
        /* Stream started, request headers are being sent. */
        STARTED,
        /* Waiting for {@code read()} to be called. */
        WAITING_FOR_READ,
        /* Reading from the remote, {@code onReadCompleted()} callback will be called when done. */
        READING,
        /* There is no more data to read and stream is half-closed by the remote side. */
        READING_DONE,
        /* Stream is canceled. */
        CANCELED,
        /* Error has occured, stream is closed. */
        ERROR,
        /* Reading and writing are done, and the stream is closed successfully. */
        SUCCESS,
        /* Waiting for {@code write()} to be called. */
        WAITING_FOR_WRITE,
        /* Writing to the remote, {@code onWriteCompleted()} callback will be called when done. */
        WRITING,
        /* There is no more data to write and stream is half-closed by the local side. */
        WRITING_DONE,
    }

    private final CronetUrlRequestContext mRequestContext;
    private final Executor mExecutor;
    private final Callback mCallback;
    private final String mInitialUrl;
    private final int mInitialPriority;
    private final String mInitialMethod;
    private final String mRequestHeaders[];

    /*
     * Synchronizes access to mNativeStream, mReadState and mWriteState.
     */
    private final Object mNativeStreamLock = new Object();

    /* Native BidirectionalStream object, owned by CronetBidirectionalStream. */
    @GuardedBy("mNativeStreamLock")
    private long mNativeStream;

    /**
     * Read state is tracking reading flow.
     *                         / <--- READING <--- \
     *                         |                   |
     *                         \                   /
     * NOT_STARTED -> STARTED --> WAITING_FOR_READ -> READING_DONE -> SUCCESS
     */
    @GuardedBy("mNativeStreamLock")
    private State mReadState = State.NOT_STARTED;

    /**
     * Write state is tracking writing flow.
     *                         / <--- WRITING <---  \
     *                         |                    |
     *                         \                    /
     * NOT_STARTED -> STARTED --> WAITING_FOR_WRITE -> WRITING_DONE -> SUCCESS
     */
    @GuardedBy("mNativeStreamLock")
    private State mWriteState = State.NOT_STARTED;

    private UrlResponseInfo mResponseInfo;

    /*
     * OnReadCompleted callback is repeatedly invoked when each read is completed, so it
     * is cached as a member variable.
     */
    private OnReadCompletedRunnable mOnReadCompletedTask;

    /*
     * OnWriteCompleted callback is repeatedly invoked when each write is completed, so it
     * is cached as a member variable.
     */
    private OnWriteCompletedRunnable mOnWriteCompletedTask;

    private Runnable mOnDestroyedCallbackForTesting;

    private final class OnReadCompletedRunnable implements Runnable {
        // Buffer passed back from current invocation of onReadCompleted.
        ByteBuffer mByteBuffer;
        // End of stream flag from current invocation of onReadCompleted.
        boolean mEndOfStream;

        @Override
        public void run() {
            try {
                // Null out mByteBuffer, to pass buffer ownership to callback or release if done.
                ByteBuffer buffer = mByteBuffer;
                mByteBuffer = null;
                synchronized (mNativeStreamLock) {
                    if (isDoneLocked()) {
                        return;
                    }
                    if (mEndOfStream) {
                        mReadState = State.READING_DONE;
                        if (maybeSucceedLocked()) {
                            return;
                        }
                    } else {
                        mReadState = State.WAITING_FOR_READ;
                    }
                }
                mCallback.onReadCompleted(CronetBidirectionalStream.this, mResponseInfo, buffer);
            } catch (Exception e) {
                onCallbackException(e);
            }
        }
    }

    private final class OnWriteCompletedRunnable implements Runnable {
        // Buffer passed back from current invocation of onWriteCompleted.
        ByteBuffer mByteBuffer;
        // End of stream flag from current call to write.
        boolean mEndOfStream;

        @Override
        public void run() {
            try {
                // Null out mByteBuffer, to pass buffer ownership to callback or release if done.
                ByteBuffer buffer = mByteBuffer;
                mByteBuffer = null;
                synchronized (mNativeStreamLock) {
                    if (isDoneLocked()) {
                        return;
                    }
                    if (mEndOfStream) {
                        mWriteState = State.WRITING_DONE;
                        if (maybeSucceedLocked()) {
                            return;
                        }
                    } else {
                        mWriteState = State.WAITING_FOR_WRITE;
                    }
                }
                mCallback.onWriteCompleted(CronetBidirectionalStream.this, mResponseInfo, buffer);
            } catch (Exception e) {
                onCallbackException(e);
            }
        }
    }

    CronetBidirectionalStream(CronetUrlRequestContext requestContext, String url,
            @BidirectionalStream.Builder.StreamPriority int priority, Callback callback,
            Executor executor, String httpMethod, List<Map.Entry<String, String>> requestHeaders) {
        mRequestContext = requestContext;
        mInitialUrl = url;
        mInitialPriority = convertStreamPriority(priority);
        mCallback = callback;
        mExecutor = executor;
        mInitialMethod = httpMethod;
        mRequestHeaders = stringsFromHeaderList(requestHeaders);
    }

    @Override
    public void start() {
        synchronized (mNativeStreamLock) {
            if (mReadState != State.NOT_STARTED) {
                throw new IllegalStateException("Stream is already started.");
            }
            try {
                mNativeStream = nativeCreateBidirectionalStream(
                        mRequestContext.getUrlRequestContextAdapter());
                mRequestContext.onRequestStarted();
                // Non-zero startResult means an argument error.
                int startResult = nativeStart(mNativeStream, mInitialUrl, mInitialPriority,
                        mInitialMethod, mRequestHeaders, !doesMethodAllowWriteData(mInitialMethod));
                if (startResult == -1) {
                    throw new IllegalArgumentException("Invalid http method " + mInitialMethod);
                }
                if (startResult > 0) {
                    int headerPos = startResult - 1;
                    throw new IllegalArgumentException("Invalid header "
                            + mRequestHeaders[headerPos] + "=" + mRequestHeaders[headerPos + 1]);
                }
                mReadState = mWriteState = State.STARTED;
            } catch (RuntimeException e) {
                // If there's an exception, clean up and then throw the
                // exception to the caller.
                destroyNativeStreamLocked(false);
                throw e;
            }
        }
    }

    @Override
    public void read(ByteBuffer buffer) {
        synchronized (mNativeStreamLock) {
            Preconditions.checkHasRemaining(buffer);
            Preconditions.checkDirect(buffer);
            if (mReadState != State.WAITING_FOR_READ) {
                throw new IllegalStateException("Unexpected read attempt.");
            }
            if (isDoneLocked()) {
                return;
            }
            if (mOnReadCompletedTask == null) {
                mOnReadCompletedTask = new OnReadCompletedRunnable();
            }
            mReadState = State.READING;
            if (!nativeReadData(mNativeStream, buffer, buffer.position(), buffer.limit())) {
                // Still waiting on read. This is just to have consistent
                // behavior with the other error cases.
                mReadState = State.WAITING_FOR_READ;
                throw new IllegalArgumentException("Unable to call native read");
            }
        }
    }

    @Override
    public void write(ByteBuffer buffer, boolean endOfStream) {
        synchronized (mNativeStreamLock) {
            Preconditions.checkDirect(buffer);
            if (!buffer.hasRemaining() && !endOfStream) {
                throw new IllegalArgumentException("Empty buffer before end of stream.");
            }
            if (mWriteState != State.WAITING_FOR_WRITE) {
                throw new IllegalStateException("Unexpected write attempt.");
            }
            if (isDoneLocked()) {
                return;
            }
            if (mOnWriteCompletedTask == null) {
                mOnWriteCompletedTask = new OnWriteCompletedRunnable();
            }
            mOnWriteCompletedTask.mEndOfStream = endOfStream;
            mWriteState = State.WRITING;
            if (!nativeWriteData(
                        mNativeStream, buffer, buffer.position(), buffer.limit(), endOfStream)) {
                // Still waiting on write. This is just to have consistent
                // behavior with the other error cases.
                mWriteState = State.WAITING_FOR_WRITE;
                throw new IllegalArgumentException("Unable to call native write");
            }
        }
    }

    @Override
    public void ping(PingCallback callback, Executor executor) {
        // TODO(mef): May be last thing to be implemented on Android.
        throw new UnsupportedOperationException("ping is not supported yet.");
    }

    @Override
    public void windowUpdate(int windowSizeIncrement) {
        // TODO(mef): Understand the needs and semantics of this method.
        throw new UnsupportedOperationException("windowUpdate is not supported yet.");
    }

    @Override
    public void cancel() {
        synchronized (mNativeStreamLock) {
            if (isDoneLocked() || mReadState == State.NOT_STARTED) {
                return;
            }
            mReadState = mWriteState = State.CANCELED;
            destroyNativeStreamLocked(true);
        }
    }

    @Override
    public boolean isDone() {
        synchronized (mNativeStreamLock) {
            return isDoneLocked();
        }
    }

    @GuardedBy("mNativeStreamLock")
    private boolean isDoneLocked() {
        return mReadState != State.NOT_STARTED && mNativeStream == 0;
    }

    @SuppressWarnings("unused")
    @CalledByNative
    private void onRequestHeadersSent() {
        postTaskToExecutor(new Runnable() {
            public void run() {
                synchronized (mNativeStreamLock) {
                    if (isDoneLocked()) {
                        return;
                    }
                    if (doesMethodAllowWriteData(mInitialMethod)) {
                        mWriteState = State.WAITING_FOR_WRITE;
                    } else {
                        mWriteState = State.WRITING_DONE;
                    }
                }

                try {
                    mCallback.onRequestHeadersSent(CronetBidirectionalStream.this);
                } catch (Exception e) {
                    onCallbackException(e);
                }
            }
        });
    }

    /**
     * Called when the final set of headers, after all redirects,
     * is received. Can only be called once for each stream.
     */
    @SuppressWarnings("unused")
    @CalledByNative
    private void onResponseHeadersReceived(int httpStatusCode, String negotiatedProtocol,
            String[] headers, long receivedBytesCount) {
        try {
            mResponseInfo = prepareResponseInfoOnNetworkThread(
                    httpStatusCode, negotiatedProtocol, headers, receivedBytesCount);
        } catch (Exception e) {
            failWithException(new CronetException("Cannot prepare ResponseInfo", null));
            return;
        }
        postTaskToExecutor(new Runnable() {
            public void run() {
                synchronized (mNativeStreamLock) {
                    if (isDoneLocked()) {
                        return;
                    }
                    mReadState = State.WAITING_FOR_READ;
                }

                try {
                    mCallback.onResponseHeadersReceived(
                            CronetBidirectionalStream.this, mResponseInfo);
                } catch (Exception e) {
                    onCallbackException(e);
                }
            }
        });
    }

    @SuppressWarnings("unused")
    @CalledByNative
    private void onReadCompleted(final ByteBuffer byteBuffer, int bytesRead, int initialPosition,
            int initialLimit, long receivedBytesCount) {
        mResponseInfo.setReceivedBytesCount(receivedBytesCount);
        if (byteBuffer.position() != initialPosition || byteBuffer.limit() != initialLimit) {
            failWithException(
                    new CronetException("ByteBuffer modified externally during read", null));
            return;
        }
        if (bytesRead < 0 || initialPosition + bytesRead > initialLimit) {
            failWithException(new CronetException("Invalid number of bytes read", null));
            return;
        }
        byteBuffer.position(initialPosition + bytesRead);
        assert mOnReadCompletedTask.mByteBuffer == null;
        mOnReadCompletedTask.mByteBuffer = byteBuffer;
        mOnReadCompletedTask.mEndOfStream = (bytesRead == 0);
        postTaskToExecutor(mOnReadCompletedTask);
    }

    @SuppressWarnings("unused")
    @CalledByNative
    private void onWriteCompleted(
            final ByteBuffer byteBuffer, int initialPosition, int initialLimit) {
        if (byteBuffer.position() != initialPosition || byteBuffer.limit() != initialLimit) {
            failWithException(
                    new CronetException("ByteBuffer modified externally during write", null));
            return;
        }
        // Current implementation always writes the complete buffer.
        byteBuffer.position(byteBuffer.limit());
        assert mOnWriteCompletedTask.mByteBuffer == null;
        mOnWriteCompletedTask.mByteBuffer = byteBuffer;
        postTaskToExecutor(mOnWriteCompletedTask);
    }

    @SuppressWarnings("unused")
    @CalledByNative
    private void onResponseTrailersReceived(String[] trailers) {
        final UrlResponseInfo.HeaderBlock trailersBlock =
                new UrlResponseInfo.HeaderBlock(headersListFromStrings(trailers));
        postTaskToExecutor(new Runnable() {
            public void run() {
                synchronized (mNativeStreamLock) {
                    if (isDoneLocked()) {
                        return;
                    }
                }
                try {
                    mCallback.onResponseTrailersReceived(
                            CronetBidirectionalStream.this, mResponseInfo, trailersBlock);
                } catch (Exception e) {
                    onCallbackException(e);
                }
            }
        });
    }

    @SuppressWarnings("unused")
    @CalledByNative
    private void onError(final int nativeError, final String errorString, long receivedBytesCount) {
        if (mResponseInfo != null) {
            mResponseInfo.setReceivedBytesCount(receivedBytesCount);
        }
        failWithException(new CronetException(
                "Exception in BidirectionalStream: " + errorString, nativeError));
    }

    /**
     * Called when request is canceled, no callbacks will be called afterwards.
     */
    @SuppressWarnings("unused")
    @CalledByNative
    private void onCanceled() {
        postTaskToExecutor(new Runnable() {
            public void run() {
                try {
                    mCallback.onCanceled(CronetBidirectionalStream.this, mResponseInfo);
                } catch (Exception e) {
                    Log.e(CronetUrlRequestContext.LOG_TAG, "Exception in onCanceled method", e);
                }
            }
        });
    }

    @VisibleForTesting
    public void setOnDestroyedCallbackForTesting(Runnable onDestroyedCallbackForTesting) {
        mOnDestroyedCallbackForTesting = onDestroyedCallbackForTesting;
    }

    private static boolean doesMethodAllowWriteData(String methodName) {
        return !methodName.equals("GET") && !methodName.equals("HEAD");
    }

    private static ArrayList<Map.Entry<String, String>> headersListFromStrings(String[] headers) {
        ArrayList<Map.Entry<String, String>> headersList = new ArrayList<>(headers.length / 2);
        for (int i = 0; i < headers.length; i += 2) {
            headersList.add(new AbstractMap.SimpleImmutableEntry<>(headers[i], headers[i + 1]));
        }
        return headersList;
    }

    private static String[] stringsFromHeaderList(List<Map.Entry<String, String>> headersList) {
        String headersArray[] = new String[headersList.size() * 2];
        int i = 0;
        for (Map.Entry<String, String> requestHeader : headersList) {
            headersArray[i++] = requestHeader.getKey();
            headersArray[i++] = requestHeader.getValue();
        }
        return headersArray;
    }

    private static int convertStreamPriority(
            @BidirectionalStream.Builder.StreamPriority int priority) {
        switch (priority) {
            case Builder.STREAM_PRIORITY_IDLE:
                return RequestPriority.IDLE;
            case Builder.STREAM_PRIORITY_LOWEST:
                return RequestPriority.LOWEST;
            case Builder.STREAM_PRIORITY_LOW:
                return RequestPriority.LOW;
            case Builder.STREAM_PRIORITY_MEDIUM:
                return RequestPriority.MEDIUM;
            case Builder.STREAM_PRIORITY_HIGHEST:
                return RequestPriority.HIGHEST;
            default:
                throw new IllegalArgumentException("Invalid stream priority.");
        }
    }

    /**
     * Checks whether reading and writing are done.
     * @return false if either reading or writing is not done. If both reading and writing
     * are done, then posts cleanup task and returns true.
     */
    @GuardedBy("mNativeStreamLock")
    private boolean maybeSucceedLocked() {
        if (mReadState != State.READING_DONE || mWriteState != State.WRITING_DONE) {
            return false;
        }

        mReadState = mWriteState = State.SUCCESS;
        postTaskToExecutor(new Runnable() {
            public void run() {
                synchronized (mNativeStreamLock) {
                    if (isDoneLocked()) {
                        return;
                    }
                    // Destroy native stream first, so UrlRequestContext could be shut
                    // down from the listener.
                    destroyNativeStreamLocked(false);
                }
                try {
                    mCallback.onSucceeded(CronetBidirectionalStream.this, mResponseInfo);
                } catch (Exception e) {
                    Log.e(CronetUrlRequestContext.LOG_TAG, "Exception in onSucceeded method", e);
                }
            }
        });
        return true;
    }

    /**
     * Posts task to application Executor. Used for callbacks
     * and other tasks that should not be executed on network thread.
     */
    private void postTaskToExecutor(Runnable task) {
        try {
            mExecutor.execute(task);
        } catch (RejectedExecutionException failException) {
            Log.e(CronetUrlRequestContext.LOG_TAG, "Exception posting task to executor",
                    failException);
            // If posting a task throws an exception, then there is no choice
            // but to destroy the stream without invoking the callback.
            synchronized (mNativeStreamLock) {
                mReadState = mWriteState = State.ERROR;
                destroyNativeStreamLocked(false);
            }
        }
    }

    private UrlResponseInfo prepareResponseInfoOnNetworkThread(int httpStatusCode,
            String negotiatedProtocol, String[] headers, long receivedBytesCount) {
        synchronized (mNativeStreamLock) {
            if (mNativeStream == 0) {
                return null;
            }
        }

        ArrayList<String> urlChain = new ArrayList<>();
        urlChain.add(mInitialUrl);

        UrlResponseInfo responseInfo = new UrlResponseInfo(urlChain, httpStatusCode, "",
                headersListFromStrings(headers), false, negotiatedProtocol, null);

        responseInfo.setReceivedBytesCount(receivedBytesCount);
        return responseInfo;
    }

    @GuardedBy("mNativeStreamLock")
    private void destroyNativeStreamLocked(boolean sendOnCanceled) {
        Log.i(CronetUrlRequestContext.LOG_TAG, "destroyNativeStreamLocked " + this.toString());
        if (mNativeStream == 0) {
            return;
        }
        nativeDestroy(mNativeStream, sendOnCanceled);
        mNativeStream = 0;
        mRequestContext.onRequestDestroyed();
        if (mOnDestroyedCallbackForTesting != null) {
            mOnDestroyedCallbackForTesting.run();
        }
    }

    /**
     * Fails the stream with an exception. Only called on the Executor.
     */
    private void failWithExceptionOnExecutor(CronetException e) {
        // Do not call into listener if request is complete.
        synchronized (mNativeStreamLock) {
            if (isDoneLocked()) {
                return;
            }
            mReadState = mWriteState = State.ERROR;
            destroyNativeStreamLocked(false);
        }
        try {
            mCallback.onFailed(this, mResponseInfo, e);
        } catch (Exception failException) {
            Log.e(CronetUrlRequestContext.LOG_TAG, "Exception notifying of failed request",
                    failException);
        }
    }

    /**
     * If callback method throws an exception, stream gets canceled
     * and exception is reported via onFailed callback.
     * Only called on the Executor.
     */
    private void onCallbackException(Exception e) {
        CronetException streamError =
                new CronetException("CalledByNative method has thrown an exception", e);
        Log.e(CronetUrlRequestContext.LOG_TAG, "Exception in CalledByNative method", e);
        failWithExceptionOnExecutor(streamError);
    }

    /**
     * Fails the stream with an exception. Can be called on any thread.
     */
    private void failWithException(final CronetException exception) {
        postTaskToExecutor(new Runnable() {
            public void run() {
                failWithExceptionOnExecutor(exception);
            }
        });
    }

    // Native methods are implemented in cronet_bidirectional_stream_adapter.cc.
    private native long nativeCreateBidirectionalStream(long urlRequestContextAdapter);

    @NativeClassQualifiedName("CronetBidirectionalStreamAdapter")
    private native int nativeStart(long nativePtr, String url, int priority, String method,
            String[] headers, boolean endOfStream);

    @NativeClassQualifiedName("CronetBidirectionalStreamAdapter")
    private native boolean nativeReadData(
            long nativePtr, ByteBuffer byteBuffer, int position, int limit);

    @NativeClassQualifiedName("CronetBidirectionalStreamAdapter")
    private native boolean nativeWriteData(
            long nativePtr, ByteBuffer byteBuffer, int position, int limit, boolean endOfStream);

    @NativeClassQualifiedName("CronetBidirectionalStreamAdapter")
    private native void nativeDestroy(long nativePtr, boolean sendOnCanceled);
}
