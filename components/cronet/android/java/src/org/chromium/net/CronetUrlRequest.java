// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import android.util.Log;
import android.util.Pair;

import org.chromium.base.VisibleForTesting;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeClassQualifiedName;

import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Map;
import java.util.TreeMap;
import java.util.concurrent.Executor;
import java.util.concurrent.RejectedExecutionException;

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
final class CronetUrlRequest implements UrlRequest {
    /* Native adapter object, owned by UrlRequest. */
    private long mUrlRequestAdapter;

    private boolean mStarted = false;
    private boolean mDisableCache = false;
    private boolean mWaitingOnRedirect = false;
    private boolean mWaitingOnRead = false;

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
     * mListener.onReceivedRedirect is called.
     */
    private final List<String> mUrlChain = new ArrayList<String>();

    private final UrlRequestListener mListener;
    private final String mInitialUrl;
    private final int mPriority;
    private String mInitialMethod;
    private final HeadersList mRequestHeaders = new HeadersList();

    private CronetUploadDataStream mUploadDataStream;

    private NativeResponseInfo mResponseInfo;

    /*
     * Listener callback is repeatedly called when each read is completed, so it
     * is cached as a member variable.
     */
    private OnReadCompletedRunnable mOnReadCompletedTask;

    private Runnable mOnDestroyedCallbackForTests;

    private static final class HeadersList extends ArrayList<Pair<String, String>> {}

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
                // mListener call, to avoid any race when there are multiple
                // executor threads.
                ByteBuffer buffer = mByteBuffer;
                mByteBuffer = null;
                mListener.onReadCompleted(CronetUrlRequest.this,
                        mResponseInfo, buffer);
            } catch (Exception e) {
                onListenerException(e);
            }
        }
    }

    private static final class NativeResponseInfo implements ResponseInfo {
        private final String[] mResponseInfoUrlChain;
        private final int mHttpStatusCode;
        private final String mHttpStatusText;
        private final boolean mWasCached;
        private final String mNegotiatedProtocol;
        private final String mProxyServer;
        private final HeadersList mAllHeaders = new HeadersList();
        private Map<String, List<String>> mResponseHeaders;
        private List<Pair<String, String>> mUnmodifiableAllHeaders;

        NativeResponseInfo(String[] urlChain, int httpStatusCode,
                String httpStatusText, boolean wasCached,
                String negotiatedProtocol, String proxyServer) {
            mResponseInfoUrlChain = urlChain;
            mHttpStatusCode = httpStatusCode;
            mHttpStatusText = httpStatusText;
            mWasCached = wasCached;
            mNegotiatedProtocol = negotiatedProtocol;
            mProxyServer = proxyServer;
        }

        @Override
        public String getUrl() {
            return mResponseInfoUrlChain[mResponseInfoUrlChain.length - 1];
        }

        @Override
        public String[] getUrlChain() {
            return mResponseInfoUrlChain;
        }

        @Override
        public int getHttpStatusCode() {
            return mHttpStatusCode;
        }

        @Override
        public String getHttpStatusText() {
            return mHttpStatusText;
        }

        @Override
        public List<Pair<String, String>> getAllHeadersAsList() {
            if (mUnmodifiableAllHeaders == null) {
                mUnmodifiableAllHeaders =
                        Collections.unmodifiableList(mAllHeaders);
            }
            return mUnmodifiableAllHeaders;
        }

        @Override
        public Map<String, List<String>> getAllHeaders() {
            if (mResponseHeaders != null) {
                return mResponseHeaders;
            }
            Map<String, List<String>> map = new TreeMap<String, List<String>>(
                    String.CASE_INSENSITIVE_ORDER);
            for (Pair<String, String> entry : mAllHeaders) {
                List<String> values = new ArrayList<String>();
                if (map.containsKey(entry.first)) {
                    values.addAll(map.get(entry.first));
                }
                values.add(entry.second);
                map.put(entry.first, Collections.unmodifiableList(values));
            }
            mResponseHeaders = Collections.unmodifiableMap(map);
            return mResponseHeaders;
        }

        @Override
        public boolean wasCached() {
            return mWasCached;
        }

        @Override
        public String getNegotiatedProtocol() {
            return mNegotiatedProtocol;
        }

        @Override
        public String getProxyServer() {
            return mProxyServer;
        }
    };

    private static final class NativeExtendedResponseInfo implements ExtendedResponseInfo {
        private final ResponseInfo mResponseInfo;
        private final long mTotalReceivedBytes;

        NativeExtendedResponseInfo(ResponseInfo responseInfo,
                                   long totalReceivedBytes) {
            mResponseInfo = responseInfo;
            mTotalReceivedBytes = totalReceivedBytes;
        }

        @Override
        public ResponseInfo getResponseInfo() {
            return mResponseInfo;
        }

        @Override
        public long getTotalReceivedBytes() {
            return mTotalReceivedBytes;
        }
    };

    CronetUrlRequest(CronetUrlRequestContext requestContext,
            long urlRequestContextAdapter,
            String url,
            int priority,
            UrlRequestListener listener,
            Executor executor) {
        if (url == null) {
            throw new NullPointerException("URL is required");
        }
        if (listener == null) {
            throw new NullPointerException("Listener is required");
        }
        if (executor == null) {
            throw new NullPointerException("Executor is required");
        }

        mRequestContext = requestContext;
        mInitialUrl = url;
        mUrlChain.add(url);
        mPriority = convertRequestPriority(priority);
        mListener = listener;
        mExecutor = executor;
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
        mRequestHeaders.add(Pair.create(header, value));
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
                mRequestContext.onRequestStarted(this);
                if (mInitialMethod != null) {
                    if (!nativeSetHttpMethod(mUrlRequestAdapter, mInitialMethod)) {
                        throw new IllegalArgumentException("Invalid http method " + mInitialMethod);
                    }
                }

                boolean hasContentType = false;
                for (Pair<String, String> header : mRequestHeaders) {
                    if (header.first.equalsIgnoreCase("Content-Type")
                            && !header.second.isEmpty()) {
                        hasContentType = true;
                    }
                    if (!nativeAddRequestHeader(mUrlRequestAdapter, header.first, header.second)) {
                        destroyRequestAdapter();
                        throw new IllegalArgumentException(
                                "Invalid header " + header.first + "=" + header.second);
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
                destroyRequestAdapter();
                throw e;
            }
            if (mDisableCache) {
                nativeDisableCache(mUrlRequestAdapter);
            }
            mStarted = true;
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

            if (!mWaitingOnRead) {
                throw new IllegalStateException("Unexpected read attempt.");
            }
            mWaitingOnRead = false;

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
                // Since accessing byteBuffer's memory failed, it's presumably
                // not a direct ByteBuffer.
                throw new IllegalArgumentException(
                        "byteBuffer must be a direct ByteBuffer.");
            }
        }
    }

    @Override
    public void cancel() {
        synchronized (mUrlRequestAdapterLock) {
            if (isDone() || !mStarted) {
                return;
            }
            destroyRequestAdapter();
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
    public void getStatus(final StatusListener listener) {
        synchronized (mUrlRequestAdapterLock) {
            if (mUrlRequestAdapter != 0) {
                nativeGetStatus(mUrlRequestAdapter, listener);
                return;
            }
        }
        Runnable task = new Runnable() {
            @Override
            public void run() {
                listener.onStatus(RequestStatus.INVALID);
            }
        };
        postTaskToExecutor(task);
    }

    @VisibleForTesting
    public void setOnDestroyedCallbackForTests(Runnable onDestroyedCallbackForTests) {
        mOnDestroyedCallbackForTests = onDestroyedCallbackForTests;
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
            case REQUEST_PRIORITY_IDLE:
                return RequestPriority.IDLE;
            case REQUEST_PRIORITY_LOWEST:
                return RequestPriority.LOWEST;
            case REQUEST_PRIORITY_LOW:
                return RequestPriority.LOW;
            case REQUEST_PRIORITY_MEDIUM:
                return RequestPriority.MEDIUM;
            case REQUEST_PRIORITY_HIGHEST:
                return RequestPriority.HIGHEST;
            default:
                return RequestPriority.MEDIUM;
        }
    }

    private NativeResponseInfo prepareResponseInfoOnNetworkThread(
            int httpStatusCode) {
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
        NativeResponseInfo responseInfo = new NativeResponseInfo(
                mUrlChain.toArray(new String[mUrlChain.size()]),
                httpStatusCode,
                nativeGetHttpStatusText(urlRequestAdapter),
                nativeGetWasCached(urlRequestAdapter),
                nativeGetNegotiatedProtocol(urlRequestAdapter),
                nativeGetProxyServer(urlRequestAdapter));
        nativePopulateResponseHeaders(urlRequestAdapter,
                                      responseInfo.mAllHeaders);
        return responseInfo;
    }

    private void checkNotStarted() {
        synchronized (mUrlRequestAdapterLock) {
            if (mStarted || isDone()) {
                throw new IllegalStateException("Request is already started.");
            }
        }
    }

    private void destroyRequestAdapter() {
        synchronized (mUrlRequestAdapterLock) {
            if (mUrlRequestAdapter == 0) {
                return;
            }
            nativeDestroy(mUrlRequestAdapter);
            mRequestContext.onRequestDestroyed(this);
            mUrlRequestAdapter = 0;
            if (mOnDestroyedCallbackForTests != null) {
                mOnDestroyedCallbackForTests.run();
            }
        }
    }

    /**
     * If listener method throws an exception, request gets canceled
     * and exception is reported via onFailed listener callback.
     * Only called on the Executor.
     */
    private void onListenerException(Exception e) {
        UrlRequestException requestError = new UrlRequestException(
                "CalledByNative method has thrown an exception", e);
        Log.e(CronetUrlRequestContext.LOG_TAG,
                "Exception in CalledByNative method", e);
        // Do not call into listener if request is complete.
        synchronized (mUrlRequestAdapterLock) {
            if (isDone()) {
                return;
            }
            destroyRequestAdapter();
        }
        try {
            mListener.onFailed(this, mResponseInfo, requestError);
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
            public void run() {
                synchronized (mUrlRequestAdapterLock) {
                    if (isDone()) {
                        return;
                    }
                    destroyRequestAdapter();
                }
                try {
                    mListener.onFailed(CronetUrlRequest.this,
                                       mResponseInfo,
                                       exception);
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
     */
    @SuppressWarnings("unused")
    @CalledByNative
    private void onReceivedRedirect(final String newLocation,
            int httpStatusCode) {
        final NativeResponseInfo responseInfo =
                prepareResponseInfoOnNetworkThread(httpStatusCode);
        // Have to do this after creating responseInfo.
        mUrlChain.add(newLocation);

        Runnable task = new Runnable() {
            public void run() {
                synchronized (mUrlRequestAdapterLock) {
                    if (isDone()) {
                        return;
                    }
                    mWaitingOnRedirect = true;
                }

                try {
                    mListener.onReceivedRedirect(CronetUrlRequest.this,
                            responseInfo, newLocation);
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
    private void onResponseStarted(int httpStatusCode) {
        mResponseInfo = prepareResponseInfoOnNetworkThread(httpStatusCode);
        Runnable task = new Runnable() {
            public void run() {
                synchronized (mUrlRequestAdapterLock) {
                    if (isDone()) {
                        return;
                    }
                    mWaitingOnRead = true;
                }

                try {
                    mListener.onResponseStarted(CronetUrlRequest.this,
                                                mResponseInfo);
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
     */
    @SuppressWarnings("unused")
    @CalledByNative
    private void onReadCompleted(final ByteBuffer byteBuffer, int bytesRead,
            int initialPosition) {
        if (byteBuffer.position() != initialPosition) {
            failWithException(new UrlRequestException(
                    "ByteBuffer modified externally during read", null));
            return;
        }
        if (mOnReadCompletedTask == null) {
            mOnReadCompletedTask = new OnReadCompletedRunnable();
        }
        byteBuffer.limit(initialPosition + bytesRead);
        mOnReadCompletedTask.mByteBuffer = byteBuffer;
        postTaskToExecutor(mOnReadCompletedTask);
    }

    /**
     * Called when request is completed successfully, no callbacks will be
     * called afterwards.
     */
    @SuppressWarnings("unused")
    @CalledByNative
    private void onSucceeded(long totalReceivedBytes) {
        final NativeExtendedResponseInfo extendedResponseInfo =
                new NativeExtendedResponseInfo(mResponseInfo,
                        totalReceivedBytes);
        Runnable task = new Runnable() {
            public void run() {
                synchronized (mUrlRequestAdapterLock) {
                    if (isDone()) {
                        return;
                    }
                    // Destroy adapter first, so request context could be shut
                    // down from the listener.
                    destroyRequestAdapter();
                }
                try {
                    mListener.onSucceeded(CronetUrlRequest.this,
                                          extendedResponseInfo);
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
     * @param nativeError native net error code.
     * @param errorString textual representation of the error code.
     */
    @SuppressWarnings("unused")
    @CalledByNative
    private void onError(final int nativeError, final String errorString) {
        UrlRequestException requestError = new UrlRequestException(
                "Exception in CronetUrlRequest: " + errorString,
                nativeError);
        failWithException(requestError);
    }

    /**
     * Appends header |name| with value |value| to |headersList|.
     */
    @SuppressWarnings("unused")
    @CalledByNative
    private void onAppendResponseHeader(HeadersList headersList,
            String name, String value) {
        headersList.add(Pair.create(name, value));
    }

    /**
     * Called by the native code when request status is fetched from the
     * native stack.
     */
    @SuppressWarnings("unused")
    @CalledByNative
    private void onStatus(final StatusListener listener, final int loadState) {
        Runnable task = new Runnable() {
            @Override
            public void run() {
                listener.onStatus(RequestStatus.convertLoadState(loadState));
            }
        };
        postTaskToExecutor(task);
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
    private native void nativeDestroy(long nativePtr);

    @NativeClassQualifiedName("CronetURLRequestAdapter")
    private native void nativePopulateResponseHeaders(long nativePtr, HeadersList headers);

    @NativeClassQualifiedName("CronetURLRequestAdapter")
    private native String nativeGetHttpStatusText(long nativePtr);

    @NativeClassQualifiedName("CronetURLRequestAdapter")
    private native String nativeGetNegotiatedProtocol(long nativePtr);

    @NativeClassQualifiedName("CronetURLRequestAdapter")
    private native String nativeGetProxyServer(long nativePtr);

    @NativeClassQualifiedName("CronetURLRequestAdapter")
    private native void nativeGetStatus(long nativePtr, StatusListener listener);

    @NativeClassQualifiedName("CronetURLRequestAdapter")
    private native boolean nativeGetWasCached(long nativePtr);
}
