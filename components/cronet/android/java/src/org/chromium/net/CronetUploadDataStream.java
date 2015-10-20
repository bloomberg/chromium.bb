// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeClassQualifiedName;

import java.nio.ByteBuffer;
import java.util.concurrent.Executor;
import java.util.concurrent.RejectedExecutionException;

/**
 * CronetUploadDataStream handles communication between an upload body
 * encapsulated in the embedder's {@link UploadDataSink} and a C++
 * UploadDataStreamAdapter, which it owns. It's attached to a {@link
 * CronetURLRequest}'s during the construction of request's native C++ objects
 * on the network thread, though it's created on one of the embedder's threads.
 * It is called by the UploadDataStreamAdapter on the network thread, but calls
 * into the UploadDataSink and the UploadDataStreamAdapter on the Executor
 * passed into its constructor.
 */
@JNINamespace("cronet")
final class CronetUploadDataStream implements UploadDataSink {
    // These are never changed, once a request starts.
    private final Executor mExecutor;
    private final UploadDataProvider mDataProvider;
    private final long mLength;
    private CronetUrlRequest mRequest;

    // Reusable read task, to reduce redundant memory allocation.
    private final Runnable mReadTask = new Runnable() {
        @Override
        public void run() {
            synchronized (mLock) {
                if (mUploadDataStreamAdapter == 0) {
                    return;
                }
                if (mReading) {
                    throw new IllegalStateException(
                            "Unexpected readData call. Already reading.");
                }
                if (mRewinding) {
                    throw new IllegalStateException(
                            "Unexpected readData call. Already rewinding.");
                }
                if (mByteBuffer == null) {
                    throw new IllegalStateException(
                            "Unexpected readData call. Buffer is null");
                }
                mReading = true;
            }
            try {
                mDataProvider.read(CronetUploadDataStream.this, mByteBuffer);
            } catch (Exception exception) {
                onError(exception);
            }
        }
    };

    // ByteBuffer created in the native code and passed to
    // UploadDataProvider for reading. It is only valid from the
    // call to mDataProvider.read until onError or onReadSucceeded.
    private ByteBuffer mByteBuffer = null;

    // Lock that protects all subsequent variables. The adapter has to be
    // protected to ensure safe shutdown, mReading and mRewinding are protected
    // to robustly detect getting read/rewind results more often than expected.
    private final Object mLock = new Object();

    // Native adapter object, owned by the CronetUploadDataStream. It's only
    // deleted after the native UploadDataStream object is destroyed. All access
    // to the adapter is synchronized, for safe usage and cleanup.
    private long mUploadDataStreamAdapter = 0;

    private boolean mReading = false;
    private boolean mRewinding = false;
    private boolean mDestroyAdapterPostponed = false;

    /**
     * Constructs a CronetUploadDataStream.
     * @param dataProvider the UploadDataProvider to read data from.
     * @param executor the Executor to execute UploadDataProvider tasks.
     */
    public CronetUploadDataStream(UploadDataProvider dataProvider,
            Executor executor) {
        mExecutor = executor;
        mDataProvider = dataProvider;
        mLength = mDataProvider.getLength();
    }

    /**
     * Called by native code to make the UploadDataProvider read data into
     * {@code byteBuffer}.
     */
    @SuppressWarnings("unused")
    @CalledByNative
    void readData(ByteBuffer byteBuffer) {
        mByteBuffer = byteBuffer;
        postTaskToExecutor(mReadTask);
    }

    // TODO(mmenke): Consider implementing a cancel method.
    // currently wait for any pending read to complete.

    /**
     * Called by native code to make the UploadDataProvider rewind upload data.
     */
    @SuppressWarnings("unused")
    @CalledByNative
    void rewind() {
        Runnable task = new Runnable() {
            @Override
            public void run() {
                synchronized (mLock) {
                    if (mUploadDataStreamAdapter == 0) {
                        return;
                    }
                    if (mReading) {
                        throw new IllegalStateException(
                                "Unexpected rewind call. Already reading");
                    }
                    if (mRewinding) {
                        throw new IllegalStateException(
                                "Unexpected rewind call. Already rewinding");
                    }
                    mRewinding = true;
                }
                try {
                    mDataProvider.rewind(CronetUploadDataStream.this);
                } catch (Exception exception) {
                    onError(exception);
                }
            }
        };
        postTaskToExecutor(task);
    }

    /**
     * Called when the native UploadDataStream is destroyed.  At this point,
     * the native adapter needs to be destroyed, but only after any pending
     * read operation completes, as the adapter owns the read buffer.
     */
    @SuppressWarnings("unused")
    @CalledByNative
    void onUploadDataStreamDestroyed() {
        Runnable task = new Runnable() {
            @Override
            public void run() {
                destroyAdapter();
            }
        };

        postTaskToExecutor(task);
    }

    /**
     * Helper method called when an exception occurred. This method resets
     * states and propagates the error to the request.
     */
    private void onError(Exception exception) {
        synchronized (mLock) {
            if (!mReading && !mRewinding) {
                throw new IllegalStateException(
                        "There is no read or rewind in progress.");
            }
            mReading = false;
            mRewinding = false;
            mByteBuffer = null;
            destroyAdapterIfPostponed();
        }

        // Just fail the request - simpler to fail directly, and
        // UploadDataStream only supports failing during initialization, not
        // while reading. The request is smart enough to handle the case where
        // it was already canceled by the embedder.
        mRequest.onUploadException(exception);
    }

    @Override
    public void onReadSucceeded(boolean lastChunk) {
        synchronized (mLock) {
            if (!mReading) {
                throw new IllegalStateException("Non-existent read succeeded.");
            }
            if (lastChunk && mLength >= 0) {
                throw new IllegalArgumentException(
                        "Non-chunked upload can't have last chunk");
            }
            int bytesRead = mByteBuffer.position();

            mByteBuffer = null;
            mReading = false;

            destroyAdapterIfPostponed();
            // Request may been canceled already.
            if (mUploadDataStreamAdapter == 0) {
                return;
            }
            nativeOnReadSucceeded(mUploadDataStreamAdapter, bytesRead,
                    lastChunk);
        }
    }

    @Override
    public void onReadError(Exception exception) {
        synchronized (mLock) {
            if (!mReading) {
                throw new IllegalStateException("Non-existent read failed.");
            }
            onError(exception);
        }
    }

    @Override
    public void onRewindSucceeded() {
        synchronized (mLock) {
            if (!mRewinding) {
                throw new IllegalStateException(
                        "Non-existent rewind succeeded.");
            }
            mRewinding = false;
            // Request may been canceled already.
            if (mUploadDataStreamAdapter == 0) {
                return;
            }
            nativeOnRewindSucceeded(mUploadDataStreamAdapter);
        }
    }

    @Override
    public void onRewindError(Exception exception) {
        synchronized (mLock) {
            if (!mRewinding) {
                throw new IllegalStateException("Non-existent rewind failed.");
            }
            onError(exception);
        }
    }

    /**
     * Posts task to application Executor.
     */
    private void postTaskToExecutor(Runnable task) {
        try {
            mExecutor.execute(task);
        } catch (RejectedExecutionException e) {
            // Just fail the request. The request is smart enough to handle the
            // case where it was already canceled by the embedder.
            mRequest.onUploadException(e);
        }
    }

    /**
     * The adapter is owned by the CronetUploadDataStream, so it can be
     * destroyed safely when there is no pending read; however, destruction is
     * initiated by the destruction of the native UploadDataStream.
     */
    private void destroyAdapter() {
        synchronized (mLock) {
            if (mReading) {
                // Wait for the read to complete before destroy the adapter.
                mDestroyAdapterPostponed = true;
                return;
            }
            if (mUploadDataStreamAdapter == 0) {
                return;
            }
            nativeDestroyAdapter(mUploadDataStreamAdapter);
            mUploadDataStreamAdapter = 0;
        }
    }

    /**
     * Destroys the native adapter if the destruction is postponed due to a
     * pending read, which has since completed. Caller needs to be on executor
     * thread.
     */
    private void destroyAdapterIfPostponed() {
        synchronized (mLock) {
            if (mReading) {
                throw new IllegalStateException(
                        "Method should not be called when read has not completed.");
            }
            if (mDestroyAdapterPostponed) {
                destroyAdapter();
            }
        }
    }

    /**
     * Creates native objects and attaches them to the underlying request
     * adapter object.
     * TODO(mmenke):  If more types of native upload streams are needed, create
     * an interface with just this method, to minimize CronetURLRequest's
     * dependencies on each upload stream type.
     */
    void attachToRequest(CronetUrlRequest request, long requestAdapter) {
        mRequest = request;
        mUploadDataStreamAdapter =
                nativeAttachUploadDataToRequest(requestAdapter, mLength);
    }

    /**
     * Creates a native CronetUploadDataStreamAdapter and
     * CronetUploadDataStream for testing.
     * @return the address of the native CronetUploadDataStream object.
     */
    public long createUploadDataStreamForTesting() {
        mUploadDataStreamAdapter = nativeCreateAdapterForTesting();
        return nativeCreateUploadDataStreamForTesting(mLength,
                mUploadDataStreamAdapter);
    }

    // Native methods are implemented in upload_data_stream_adapter.cc.

    private native long nativeAttachUploadDataToRequest(long urlRequestAdapter,
            long length);

    private native long nativeCreateAdapterForTesting();

    private native long nativeCreateUploadDataStreamForTesting(long length,
            long adapter);

    @NativeClassQualifiedName("CronetUploadDataStreamAdapter")
    private native void nativeOnReadSucceeded(long nativePtr,
            int bytesRead, boolean finalChunk);

    @NativeClassQualifiedName("CronetUploadDataStreamAdapter")
    private native void nativeOnRewindSucceeded(long nativePtr);

    private static native void nativeDestroyAdapter(
            long uploadDataStreamAdapter);
}
