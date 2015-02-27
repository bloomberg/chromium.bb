// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import org.chromium.base.CalledByNative;
import org.chromium.base.JNINamespace;
import org.chromium.base.NativeClassQualifiedName;

import java.nio.ByteBuffer;
import java.util.concurrent.Executor;

/**
 * Pass an upload body to a UrlRequest using an UploadDataProvider.
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
                if (mReading || mRewinding || mByteBuffer == null
                        || mUploadDataStreamDelegate == 0) {
                    throw new IllegalStateException(
                            "Unexpected readData call.");
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

    // Lock that protects all subsequent variables. The delegate has to be
    // protected to ensure safe shutdown, mReading and mRewinding are protected
    // to robustly detect getting read/rewind results more often than expected.
    private final Object mLock = new Object();

    // Native adapter delegate object, owned by the CronetUploadDataStream.
    // It's only deleted after the native UploadDataStream object is destroyed.
    // All access to the delegate is synchronized, for safe usage and cleanup.
    private long mUploadDataStreamDelegate = 0;

    private boolean mReading = false;
    private boolean mRewinding = false;
    private boolean mDestroyDelegatePostponed = false;

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
        mExecutor.execute(mReadTask);
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
                    if (mReading || mRewinding
                            || mUploadDataStreamDelegate == 0) {
                        throw new IllegalStateException(
                                "Unexpected rewind call.");
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
        mExecutor.execute(task);
    }

    /**
     * Called by native code to destroy the native adapter delegate, when the
     * adapter is destroyed.
     */
    @SuppressWarnings("unused")
    @CalledByNative
    void onAdapterDestroyed() {
        Runnable task = new Runnable() {
            @Override
            public void run() {
                destroyDelegate();
            }
        };

        mExecutor.execute(task);
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
            destroyDelegateIfPostponed();
        }

        // Just fail the request - simpler to fail directly, and
        // UploadDataStream only supports failing during initialization, not
        // while reading. This should be safe, even if we deleted the adapter,
        // because in that case, the request has already been cancelled.
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

            destroyDelegateIfPostponed();
            // Request may been canceled already.
            if (mUploadDataStreamDelegate == 0) {
                return;
            }
            nativeOnReadSucceeded(mUploadDataStreamDelegate, bytesRead,
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
            if (mUploadDataStreamDelegate == 0) {
                return;
            }
            nativeOnRewindSucceeded(mUploadDataStreamDelegate);
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
     * The delegate is owned by the CronetUploadDataStream, so it can be
     * destroyed safely when there is no pending read; however, destruction is
     * initiated by the destruction of the native UploadDataStream.
     */
    private void destroyDelegate() {
        synchronized (mLock) {
            if (mReading) {
                // Wait for the read to complete before destroy the delegate.
                mDestroyDelegatePostponed = true;
                return;
            }
            if (mUploadDataStreamDelegate == 0) {
                return;
            }
            nativeDestroyDelegate(mUploadDataStreamDelegate);
            mUploadDataStreamDelegate = 0;
        }
    }

    /**
     * Destroy the native delegate if the destruction is postponed due to a
     * pending read, which has since completed. Caller needs to be on executor
     * thread.
     */
    private void destroyDelegateIfPostponed() {
        synchronized (mLock) {
            if (mReading) {
                throw new IllegalStateException(
                        "Method should not be called when read has not completed.");
            }
            if (mDestroyDelegatePostponed) {
                destroyDelegate();
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
        mUploadDataStreamDelegate =
                nativeAttachUploadDataToRequest(requestAdapter, mLength);
    }

    /**
     * Creates a native UploadDataStreamDelegate and UploadDataStreamAdapter
     * for testing.
     * @return the address of the native CronetUploadDataStreamAdapter object.
     */
    public long createAdapterForTesting() {
        mUploadDataStreamDelegate = nativeCreateDelegateForTesting();
        return nativeCreateAdapterForTesting(mLength, mUploadDataStreamDelegate);
    }

    // Native methods are implemented in upload_data_stream_adapter.cc.

    private native long nativeAttachUploadDataToRequest(long urlRequestAdapter,
            long length);

    private native long nativeCreateDelegateForTesting();

    private native long nativeCreateAdapterForTesting(long length,
            long delegate);

    @NativeClassQualifiedName("CronetUploadDataStreamDelegate")
    private native void nativeOnReadSucceeded(long nativePtr,
            int bytesRead, boolean finalChunk);

    @NativeClassQualifiedName("CronetUploadDataStreamDelegate")
    private native void nativeOnRewindSucceeded(long nativePtr);

    private static native void nativeDestroyDelegate(
            long uploadDataStreamDelegate);
}
