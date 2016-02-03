// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import android.os.SystemClock;
import android.support.annotation.Nullable;
import android.util.Log;

import org.chromium.base.VisibleForTesting;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNIAdditionalImport;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeClassQualifiedName;
import org.chromium.net.CronetEngine.UrlRequestInfo;
import org.chromium.net.CronetEngine.UrlRequestMetrics;

import java.nio.ByteBuffer;
import java.util.AbstractMap;
import java.util.ArrayList;
import java.util.Collection;
import java.util.List;
import java.util.Map;
import java.util.concurrent.Executor;
import java.util.concurrent.RejectedExecutionException;

import javax.annotation.concurrent.GuardedBy;

/**
 * UrlRequest using Chromium HTTP stack implementation. Could be accessed from
 * any thread on Executor. Cancel can be called from any thread.
 * All @CallByNative methods are called on native network thread
 * and post tasks with listener calls onto Executor. Upon return from listener
 * callback native request adapter is called on executive thread and posts
 * native tasks to native network thread. Because Cancel could be called from
 * any thread it is protected by mUrlRequestAdapterLock.
 */
@JNINamespace("cronet")
// Qualifies UrlRequest.StatusListener which is used in onStatus, a JNI method.
@JNIAdditionalImport(UrlRequest.class)
final class CronetUrlRequest implements UrlRequest {
    private static final UrlRequestMetrics EMPTY_METRICS =
            new UrlRequestMetrics(null, null, null, null);

    /* Native adapter object, owned by UrlRequest. */
    @GuardedBy("mUrlRequestAdapterLock")
    private long mUrlRequestAdapter;

    @GuardedBy("mUrlRequestAdapterLock")
    private boolean mStarted = false;
    private boolean mDisableCache = false;
    @GuardedBy("mUrlRequestAdapterLock")
    private boolean mWaitingOnRedirect = false;
    @GuardedBy("mUrlRequestAdapterLock")
    private boolean mWaitingOnRead = false;
    /*
     * When a read call completes, should the ByteBuffer limit() be updated
     * instead of the position(). This controls legacy read() behavior.
     * TODO(pauljensen): remove when all callers of read() are switched over
     * to calling readNew().
     */
    private boolean mLegacyReadByteBufferAdjustment = false;

    /*
     * Synchronize access to mUrlRequestAdapter, mStarted, mWaitingOnRedirect,
     * and mWaitingOnRead.
     */
    private final Object mUrlRequestAdapterLock = new Object();
    private final CronetUrlRequestContext mRequestContext;
    private final Executor mExecutor;

    /*
     * URL chain contains the URL currently being requested, and
     * all URLs previously requested. New URLs are added before
     * mCallback.onRedirectReceived is called.
     */
    private final List<String> mUrlChain = new ArrayList<String>();
    private long mReceivedBytesCountFromRedirects;

    private final UrlRequest.Callback mCallback;
    private final String mInitialUrl;
    private final int mPriority;
    private String mInitialMethod;
    private final HeadersList mRequestHeaders = new HeadersList();
    private final Collection<Object> mRequestAnnotations;
    @Nullable private final UrlRequestMetricsAccumulator mRequestMetricsAccumulator;

    private CronetUploadDataStream mUploadDataStream;

    private UrlResponseInfo mResponseInfo;

    /*
     * Listener callback is repeatedly invoked when each read is completed, so it
     * is cached as a member variable.
     */
    private OnReadCompletedRunnable mOnReadCompletedTask;

    private Runnable mOnDestroyedCallbackForTesting;

    private static final class HeadersList extends ArrayList<Map.Entry<String, String>> {}

    private final class OnReadCompletedRunnable implements Runnable {
        ByteBuffer mByteBuffer;

        @Override
        public void run() {
            if (isDone()) {
                return;
            }
            try {
                synchronized (mUrlRequestAdapterLock) {
                    if (mUrlRequestAdapter == 0) {
                        return;
                    }
                    mWaitingOnRead = true;
                }
                // Null out mByteBuffer, out of paranoia. Has to be done before
                // mCallback call, to avoid any race when there are multiple
                // executor threads.
                ByteBuffer buffer = mByteBuffer;
                mByteBuffer = null;
                mCallback.onReadCompleted(CronetUrlRequest.this, mResponseInfo, buffer);
            } catch (Exception e) {
                onListenerException(e);
            }
        }
    }

    CronetUrlRequest(CronetUrlRequestContext requestContext, long urlRequestContextAdapter,
            String url, int priority, UrlRequest.Callback callback, Executor executor,
            Collection<Object> requestAnnotations, boolean metricsCollectionEnabled) {
        if (url == null) {
            throw new NullPointerException("URL is required");
        }
        if (callback == null) {
            throw new NullPointerException("Listener is required");
        }
        if (executor == null) {
            throw new NullPointerException("Executor is required");
        }
        if (requestAnnotations == null) {
            throw new NullPointerException("requestAnnotations is required");
        }

        mRequestContext = requestContext;
        mInitialUrl = url;
        mUrlChain.add(url);
        mPriority = convertRequestPriority(priority);
        mCallback = callback;
        mExecutor = executor;
        mRequestAnnotations = requestAnnotations;
        mRequestMetricsAccumulator =
                metricsCollectionEnabled ? new UrlRequestMetricsAccumulator() : null;
    }

    @Override
    public void setHttpMethod(String method) {
        checkNotStarted();
        if (method == null) {
            throw new NullPointerException("Method is required.");
        }
        mInitialMethod = method;
    }

    @Override
    public void addHeader(String header, String value) {
        checkNotStarted();
        if (header == null) {
            throw new NullPointerException("Invalid header name.");
        }
        if (value == null) {
            throw new NullPointerException("Invalid header value.");
        }
        mRequestHeaders.add(new AbstractMap.SimpleImmutableEntry<String, String>(header, value));
    }

    @Override
    public void setUploadDataProvider(UploadDataProvider uploadDataProvider, Executor executor) {
        if (uploadDataProvider == null) {
            throw new NullPointerException("Invalid UploadDataProvider.");
        }
        if (mInitialMethod == null) {
            mInitialMethod = "POST";
        }
        mUploadDataStream = new CronetUploadDataStream(uploadDataProvider, executor);
    }

    @Override
    public void start() {
        synchronized (mUrlRequestAdapterLock) {
            checkNotStarted();

            try {
                mUrlRequestAdapter = nativeCreateRequestAdapter(
                        mRequestContext.getUrlRequestContextAdapter(), mInitialUrl, mPriority);
                mRequestContext.onRequestStarted();
                if (mInitialMethod != null) {
                    if (!nativeSetHttpMethod(mUrlRequestAdapter, mInitialMethod)) {
                        throw new IllegalArgumentException("Invalid http method " + mInitialMethod);
                    }
                }

                boolean hasContentType = false;
                for (Map.Entry<String, String> header : mRequestHeaders) {
                    if (header.getKey().equalsIgnoreCase("Content-Type")
                            && !header.getValue().isEmpty()) {
                        hasContentType = true;
                    }
                    if (!nativeAddRequestHeader(
                                mUrlRequestAdapter, header.getKey(), header.getValue())) {
                        throw new IllegalArgumentException(
                                "Invalid header " + header.getKey() + "=" + header.getValue());
                    }
                }
                if (mUploadDataStream != null) {
                    if (!hasContentType) {
                        throw new IllegalArgumentException(
                                "Requests with upload data must have a Content-Type.");
                    }
                    mUploadDataStream.attachToRequest(this, mUrlRequestAdapter);
                }
            } catch (RuntimeException e) {
                // If there's an exception, cleanup and then throw the
                // exception to the caller.
                destroyRequestAdapter(false);
                throw e;
            }
            if (mDisableCache) {
                nativeDisableCache(mUrlRequestAdapter);
            }
            mStarted = true;
            if (mRequestMetricsAccumulator != null) {
                mRequestMetricsAccumulator.onRequestStarted();
            }
            nativeStart(mUrlRequestAdapter);
        }
    }

    @Override
    public void followRedirect() {
        synchronized (mUrlRequestAdapterLock) {
            if (!mWaitingOnRedirect) {
                throw new IllegalStateException("No redirect to follow.");
            }
            mWaitingOnRedirect = false;

            if (isDone()) {
                return;
            }

            nativeFollowDeferredRedirect(mUrlRequestAdapter);
        }
    }

    @Override
    public void read(ByteBuffer buffer) {
        synchronized (mUrlRequestAdapterLock) {
            if (buffer.position() >= buffer.capacity()) {
                throw new IllegalArgumentException(
                        "ByteBuffer is already full.");
            }
            Preconditions.checkDirect(buffer);

            if (!mWaitingOnRead) {
                throw new IllegalStateException("Unexpected read attempt.");
            }
            mWaitingOnRead = false;
            mLegacyReadByteBufferAdjustment = true;

            if (isDone()) {
                return;
            }

            // Indicate buffer has no new data. This is primarily to make it
            // clear the buffer has no data in the failure and completion cases.
            buffer.limit(buffer.position());

            if (!nativeReadData(mUrlRequestAdapter, buffer, buffer.position(),
                    buffer.capacity())) {
                // Still waiting on read. This is just to have consistent
                // behavior with the other error cases.
                mWaitingOnRead = true;
                throw new IllegalArgumentException("Unable to call native read");
            }
        }
    }

    @Override
    public void readNew(ByteBuffer buffer) {
        Preconditions.checkHasRemaining(buffer);
        Preconditions.checkDirect(buffer);
        synchronized (mUrlRequestAdapterLock) {
            if (!mWaitingOnRead) {
                throw new IllegalStateException("Unexpected read attempt.");
            }
            mWaitingOnRead = false;
            mLegacyReadByteBufferAdjustment = false;

            if (isDone()) {
                return;
            }

            if (!nativeReadData(mUrlRequestAdapter, buffer, buffer.position(), buffer.limit())) {
                // Still waiting on read. This is just to have consistent
                // behavior with the other error cases.
                mWaitingOnRead = true;
                throw new IllegalArgumentException("Unable to call native read");
            }
        }
    }

    @Override
    public void cancel() {
        synchronized (mUrlRequestAdapterLock) {
            if (isDone() || !mStarted) {
                return;
            }
            destroyRequestAdapter(true);
        }
    }

    @Override
    public boolean isDone() {
        synchronized (mUrlRequestAdapterLock) {
            return mStarted && mUrlRequestAdapter == 0;
        }
    }

    @Override
    public void disableCache() {
        checkNotStarted();
        mDisableCache = true;
    }

    @Override
    public void getStatus(final UrlRequest.StatusListener listener) {
        synchronized (mUrlRequestAdapterLock) {
            if (mUrlRequestAdapter != 0) {
                nativeGetStatus(mUrlRequestAdapter, listener);
                return;
            }
        }
        Runnable task = new Runnable() {
            @Override
            public void run() {
                listener.onStatus(UrlRequest.Status.INVALID);
            }
        };
        postTaskToExecutor(task);
    }

    @VisibleForTesting
    public void setOnDestroyedCallbackForTesting(Runnable onDestroyedCallbackForTesting) {
        mOnDestroyedCallbackForTesting = onDestroyedCallbackForTesting;
    }

    @VisibleForTesting
    CronetUploadDataStream getUploadDataStreamForTesting() {
        return mUploadDataStream;
    }

    /**
     * Posts task to application Executor. Used for Listener callbacks
     * and other tasks that should not be executed on network thread.
     */
    private void postTaskToExecutor(Runnable task) {
        try {
            mExecutor.execute(task);
        } catch (RejectedExecutionException failException) {
            Log.e(CronetUrlRequestContext.LOG_TAG,
                    "Exception posting task to executor", failException);
            // If posting a task throws an exception, then there is no choice
            // but to cancel the request.
            cancel();
        }
    }

    private static int convertRequestPriority(int priority) {
        switch (priority) {
            case Builder.REQUEST_PRIORITY_IDLE:
                return RequestPriority.IDLE;
            case Builder.REQUEST_PRIORITY_LOWEST:
                return RequestPriority.LOWEST;
            case Builder.REQUEST_PRIORITY_LOW:
                return RequestPriority.LOW;
            case Builder.REQUEST_PRIORITY_MEDIUM:
                return RequestPriority.MEDIUM;
            case Builder.REQUEST_PRIORITY_HIGHEST:
                return RequestPriority.HIGHEST;
            default:
                return RequestPriority.MEDIUM;
        }
    }

    private UrlResponseInfo prepareResponseInfoOnNetworkThread(
            int httpStatusCode, String[] headers) {
        long urlRequestAdapter;
        synchronized (mUrlRequestAdapterLock) {
            if (mUrlRequestAdapter == 0) {
                return null;
            }
            // This method is running on network thread, so even if
            // mUrlRequestAdapter is set to 0 from another thread the actual
            // deletion of the adapter is posted to network thread, so it is
            // safe to preserve and use urlRequestAdapter outside the lock.
            urlRequestAdapter = mUrlRequestAdapter;
        }

        HeadersList headersList = new HeadersList();
        for (int i = 0; i < headers.length; i += 2) {
            headersList.add(new AbstractMap.SimpleImmutableEntry<String, String>(
                    headers[i], headers[i + 1]));
        }

        UrlResponseInfo responseInfo = new UrlResponseInfo(new ArrayList<String>(mUrlChain),
                httpStatusCode, nativeGetHttpStatusText(urlRequestAdapter), headersList,
                nativeGetWasCached(urlRequestAdapter),
                nativeGetNegotiatedProtocol(urlRequestAdapter),
                nativeGetProxyServer(urlRequestAdapter));
        return responseInfo;
    }

    private void checkNotStarted() {
        synchronized (mUrlRequestAdapterLock) {
            if (mStarted || isDone()) {
                throw new IllegalStateException("Request is already started.");
            }
        }
    }

    private void destroyRequestAdapter(boolean sendOnCanceled) {
        synchronized (mUrlRequestAdapterLock) {
            if (mUrlRequestAdapter == 0) {
                return;
            }
            if (mRequestMetricsAccumulator != null) {
                mRequestMetricsAccumulator.onRequestFinished();
            }
            nativeDestroy(mUrlRequestAdapter, sendOnCanceled);
            mRequestContext.reportFinished(this);
            mRequestContext.onRequestDestroyed();
            mUrlRequestAdapter = 0;
            if (mOnDestroyedCallbackForTesting != null) {
                mOnDestroyedCallbackForTesting.run();
            }
        }
    }

    /**
     * If listener method throws an exception, request gets canceled
     * and exception is reported via onFailed listener callback.
     * Only called on the Executor.
     */
    private void onListenerException(Exception e) {
        UrlRequestException requestError =
                new UrlRequestException("Exception received from UrlRequest.Callback", e);
        Log.e(CronetUrlRequestContext.LOG_TAG,
                "Exception in CalledByNative method", e);
        // Do not call into listener if request is finished.
        synchronized (mUrlRequestAdapterLock) {
            if (isDone()) {
                return;
            }
            destroyRequestAdapter(false);
        }
        try {
            mCallback.onFailed(this, mResponseInfo, requestError);
        } catch (Exception failException) {
            Log.e(CronetUrlRequestContext.LOG_TAG,
                    "Exception notifying of failed request", failException);
        }
    }

    /**
     * Called when UploadDataProvider encounters an error.
     */
    void onUploadException(Exception e) {
        UrlRequestException uploadError =
                new UrlRequestException("Exception received from UploadDataProvider", e);
        Log.e(CronetUrlRequestContext.LOG_TAG, "Exception in upload method", e);
        failWithException(uploadError);
    }

    /**
     * Fails the request with an exception. Can be called on any thread.
     */
    private void failWithException(final UrlRequestException exception) {
        Runnable task = new Runnable() {
            @Override
            public void run() {
                synchronized (mUrlRequestAdapterLock) {
                    if (isDone()) {
                        return;
                    }
                    destroyRequestAdapter(false);
                }
                try {
                    mCallback.onFailed(CronetUrlRequest.this, mResponseInfo, exception);
                } catch (Exception e) {
                    Log.e(CronetUrlRequestContext.LOG_TAG,
                            "Exception in onError method", e);
                }
            }
        };
        postTaskToExecutor(task);
    }

    ////////////////////////////////////////////////
    // Private methods called by the native code.
    // Always called on network thread.
    ////////////////////////////////////////////////

    /**
     * Called before following redirects. The redirect will automatically be
     * followed, unless the request is paused or canceled during this
     * callback. If the redirect response has a body, it will be ignored.
     * This will only be called between start and onResponseStarted.
     *
     * @param newLocation Location where request is redirected.
     * @param httpStatusCode from redirect response
     * @param receivedBytesCount count of bytes received for redirect response
     * @param headers an array of response headers with keys at the even indices
     *         followed by the corresponding values at the odd indices.
     */
    @SuppressWarnings("unused")
    @CalledByNative
    private void onRedirectReceived(final String newLocation, int httpStatusCode, String[] headers,
            long receivedBytesCount) {
        final UrlResponseInfo responseInfo =
                prepareResponseInfoOnNetworkThread(httpStatusCode, headers);
        mReceivedBytesCountFromRedirects += receivedBytesCount;
        responseInfo.setReceivedBytesCount(mReceivedBytesCountFromRedirects);

        // Have to do this after creating responseInfo.
        mUrlChain.add(newLocation);

        Runnable task = new Runnable() {
            @Override
            public void run() {
                synchronized (mUrlRequestAdapterLock) {
                    if (isDone()) {
                        return;
                    }
                    mWaitingOnRedirect = true;
                }

                try {
                    mCallback.onRedirectReceived(CronetUrlRequest.this, responseInfo, newLocation);
                } catch (Exception e) {
                    onListenerException(e);
                }
            }
        };
        postTaskToExecutor(task);
    }

    /**
     * Called when the final set of headers, after all redirects,
     * is received. Can only be called once for each request.
     */
    @SuppressWarnings("unused")
    @CalledByNative
    private void onResponseStarted(int httpStatusCode, String[] headers) {
        if (mRequestMetricsAccumulator != null) {
            mRequestMetricsAccumulator.onResponseStarted();
        }
        mResponseInfo = prepareResponseInfoOnNetworkThread(httpStatusCode, headers);
        Runnable task = new Runnable() {
            @Override
            public void run() {
                synchronized (mUrlRequestAdapterLock) {
                    if (isDone()) {
                        return;
                    }
                    mWaitingOnRead = true;
                }

                try {
                    mCallback.onResponseStarted(CronetUrlRequest.this, mResponseInfo);
                } catch (Exception e) {
                    onListenerException(e);
                }
            }
        };
        postTaskToExecutor(task);
    }

    /**
     * Called whenever data is received. The ByteBuffer remains
     * valid only until listener callback. Or if the callback
     * pauses the request, it remains valid until the request is resumed.
     * Cancelling the request also invalidates the buffer.
     *
     * @param byteBuffer ByteBuffer containing received data, starting at
     *        initialPosition. Guaranteed to have at least one read byte. Its
     *        limit has not yet been updated to reflect the bytes read.
     * @param bytesRead Number of bytes read.
     * @param initialPosition Original position of byteBuffer when passed to
     *        read(). Used as a minimal check that the buffer hasn't been
     *        modified while reading from the network.
     * @param receivedBytesCount number of bytes received.
     */
    @SuppressWarnings("unused")
    @CalledByNative
    private void onReadCompleted(final ByteBuffer byteBuffer, int bytesRead, int initialPosition,
            long receivedBytesCount) {
        mResponseInfo.setReceivedBytesCount(mReceivedBytesCountFromRedirects + receivedBytesCount);
        if (byteBuffer.position() != initialPosition) {
            failWithException(new UrlRequestException(
                    "ByteBuffer modified externally during read", null));
            return;
        }
        if (mOnReadCompletedTask == null) {
            mOnReadCompletedTask = new OnReadCompletedRunnable();
        }
        if (mLegacyReadByteBufferAdjustment) {
            byteBuffer.limit(initialPosition + bytesRead);
        } else {
            byteBuffer.position(initialPosition + bytesRead);
        }
        mOnReadCompletedTask.mByteBuffer = byteBuffer;
        postTaskToExecutor(mOnReadCompletedTask);
    }

    /**
     * Called when request is completed successfully, no callbacks will be
     * called afterwards.
     *
     * @param receivedBytesCount number of bytes received.
     */
    @SuppressWarnings("unused")
    @CalledByNative
    private void onSucceeded(long receivedBytesCount) {
        mResponseInfo.setReceivedBytesCount(mReceivedBytesCountFromRedirects + receivedBytesCount);
        Runnable task = new Runnable() {
            @Override
            public void run() {
                synchronized (mUrlRequestAdapterLock) {
                    if (isDone()) {
                        return;
                    }
                    // Destroy adapter first, so request context could be shut
                    // down from the listener.
                    destroyRequestAdapter(false);
                }
                try {
                    mCallback.onSucceeded(CronetUrlRequest.this, mResponseInfo);
                } catch (Exception e) {
                    Log.e(CronetUrlRequestContext.LOG_TAG,
                            "Exception in onComplete method", e);
                }
            }
        };
        postTaskToExecutor(task);
    }

    /**
     * Called when error has occured, no callbacks will be called afterwards.
     *
     * @param errorCode error code from {@link UrlRequestException.ERROR_LISTENER_EXCEPTION_THROWN
     *         UrlRequestException.ERROR_*}.
     * @param nativeError native net error code.
     * @param errorString textual representation of the error code.
     * @param receivedBytesCount number of bytes received.
     */
    @SuppressWarnings("unused")
    @CalledByNative
    private void onError(
            int errorCode, int nativeError, String errorString, long receivedBytesCount) {
        if (mResponseInfo != null) {
            mResponseInfo.setReceivedBytesCount(
                    mReceivedBytesCountFromRedirects + receivedBytesCount);
        }
        UrlRequestException requestError = new UrlRequestException(
                "Exception in CronetUrlRequest: " + errorString, errorCode, nativeError);
        failWithException(requestError);
    }

    /**
     * Called when request is canceled, no callbacks will be called afterwards.
     */
    @SuppressWarnings("unused")
    @CalledByNative
    private void onCanceled() {
        Runnable task = new Runnable() {
            @Override
            public void run() {
                try {
                    mCallback.onCanceled(CronetUrlRequest.this, mResponseInfo);
                } catch (Exception e) {
                    Log.e(CronetUrlRequestContext.LOG_TAG, "Exception in onCanceled method", e);
                }
            }
        };
        postTaskToExecutor(task);
    }

    /**
     * Called by the native code when request status is fetched from the
     * native stack.
     */
    @SuppressWarnings("unused")
    @CalledByNative
    private void onStatus(final UrlRequest.StatusListener listener, final int loadState) {
        Runnable task = new Runnable() {
            @Override
            public void run() {
                listener.onStatus(UrlRequest.Status.convertLoadState(loadState));
            }
        };
        postTaskToExecutor(task);
    }

    UrlRequestInfo getRequestInfo() {
        return new UrlRequestInfo(mInitialUrl, mRequestAnnotations,
                (mRequestMetricsAccumulator != null ? mRequestMetricsAccumulator.getRequestMetrics()
                                                    : EMPTY_METRICS),
                mResponseInfo);
    }

    private final class UrlRequestMetricsAccumulator {
        @Nullable private Long mRequestStartTime;
        @Nullable private Long mTtfbMs;
        @Nullable private Long mTotalTimeMs;

        private UrlRequestMetrics getRequestMetrics() {
            return new UrlRequestMetrics(mTtfbMs, mTotalTimeMs,
                    null, // TODO(klm): Compute sentBytesCount.
                    (mResponseInfo != null ? mResponseInfo.getReceivedBytesCount() : 0));
        }

        private void onRequestStarted() {
            if (mRequestStartTime != null) {
                throw new IllegalStateException("onRequestStarted called repeatedly");
            }
            mRequestStartTime = SystemClock.elapsedRealtime();
        }

        private void onRequestFinished() {
            if (mRequestStartTime != null && mTotalTimeMs == null) {
                mTotalTimeMs = SystemClock.elapsedRealtime() - mRequestStartTime;
            }
        }

        private void onResponseStarted() {
            if (mRequestStartTime != null && mTtfbMs == null) {
                mTtfbMs = SystemClock.elapsedRealtime() - mRequestStartTime;
            }
        }
    }

    // Native methods are implemented in cronet_url_request_adapter.cc.

    private native long nativeCreateRequestAdapter(
            long urlRequestContextAdapter, String url, int priority);

    @NativeClassQualifiedName("CronetURLRequestAdapter")
    private native boolean nativeSetHttpMethod(long nativePtr, String method);

    @NativeClassQualifiedName("CronetURLRequestAdapter")
    private native boolean nativeAddRequestHeader(long nativePtr, String name, String value);

    @NativeClassQualifiedName("CronetURLRequestAdapter")
    private native void nativeDisableCache(long nativePtr);

    @NativeClassQualifiedName("CronetURLRequestAdapter")
    private native void nativeStart(long nativePtr);

    @NativeClassQualifiedName("CronetURLRequestAdapter")
    private native void nativeFollowDeferredRedirect(long nativePtr);

    @NativeClassQualifiedName("CronetURLRequestAdapter")
    private native boolean nativeReadData(long nativePtr, ByteBuffer byteBuffer,
            int position, int capacity);

    @NativeClassQualifiedName("CronetURLRequestAdapter")
    private native void nativeDestroy(long nativePtr, boolean sendOnCanceled);

    @NativeClassQualifiedName("CronetURLRequestAdapter")
    private native String nativeGetHttpStatusText(long nativePtr);

    @NativeClassQualifiedName("CronetURLRequestAdapter")
    private native String nativeGetNegotiatedProtocol(long nativePtr);

    @NativeClassQualifiedName("CronetURLRequestAdapter")
    private native String nativeGetProxyServer(long nativePtr);

    @NativeClassQualifiedName("CronetURLRequestAdapter")
    private native void nativeGetStatus(long nativePtr, UrlRequest.StatusListener listener);

    @NativeClassQualifiedName("CronetURLRequestAdapter")
    private native boolean nativeGetWasCached(long nativePtr);
}
