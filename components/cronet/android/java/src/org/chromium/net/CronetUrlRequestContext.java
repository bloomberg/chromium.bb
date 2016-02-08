// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import android.os.Build;
import android.os.ConditionVariable;
import android.os.Handler;
import android.os.Looper;
import android.os.Process;
import android.util.Log;

import org.chromium.base.ObserverList;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeClassQualifiedName;
import org.chromium.base.annotations.UsedByReflection;
import org.chromium.net.urlconnection.CronetHttpURLConnection;
import org.chromium.net.urlconnection.CronetURLStreamHandlerFactory;

import java.net.Proxy;
import java.net.URL;
import java.net.URLConnection;
import java.net.URLStreamHandlerFactory;
import java.util.Collection;
import java.util.List;
import java.util.Map;
import java.util.concurrent.Executor;
import java.util.concurrent.RejectedExecutionException;
import java.util.concurrent.atomic.AtomicInteger;

import javax.annotation.concurrent.GuardedBy;

/**
 * CronetEngine using Chromium HTTP stack implementation.
 */
@JNINamespace("cronet")
@UsedByReflection("CronetEngine.java")
class CronetUrlRequestContext extends CronetEngine {
    private static final int LOG_NONE = 3;  // LOG(FATAL), no VLOG.
    private static final int LOG_DEBUG = -1;  // LOG(FATAL...INFO), VLOG(1)
    private static final int LOG_VERBOSE = -2;  // LOG(FATAL...INFO), VLOG(2)
    static final String LOG_TAG = "ChromiumNetwork";

    /**
     * Synchronize access to mUrlRequestContextAdapter and shutdown routine.
     */
    private final Object mLock = new Object();
    private final ConditionVariable mInitCompleted = new ConditionVariable(false);
    private final AtomicInteger mActiveRequestCount = new AtomicInteger(0);

    private long mUrlRequestContextAdapter = 0;
    private Thread mNetworkThread;

    private Executor mNetworkQualityExecutor;
    private boolean mNetworkQualityEstimatorEnabled;

    /** Locks operations on network quality listeners, because listener
     * addition and removal may occur on a different thread from notification.
     */
    private final Object mNetworkQualityLock = new Object();

    @GuardedBy("mNetworkQualityLock")
    private final ObserverList<NetworkQualityRttListener> mRttListenerList =
            new ObserverList<NetworkQualityRttListener>();

    @GuardedBy("mNetworkQualityLock")
    private final ObserverList<NetworkQualityThroughputListener> mThroughputListenerList =
            new ObserverList<NetworkQualityThroughputListener>();

    @GuardedBy("mNetworkQualityLock")
    private final ObserverList<RequestFinishedListener> mFinishedListenerList =
            new ObserverList<RequestFinishedListener>();

    @UsedByReflection("CronetEngine.java")
    public CronetUrlRequestContext(CronetEngine.Builder builder) {
        CronetLibraryLoader.ensureInitialized(builder.getContext(), builder);
        nativeSetMinLogLevel(getLoggingLevel());
        mUrlRequestContextAdapter =
                nativeCreateRequestContextAdapter(createNativeUrlRequestContextConfig(builder));
        if (mUrlRequestContextAdapter == 0) {
            throw new NullPointerException("Context Adapter creation failed.");
        }

        // Init native Chromium URLRequestContext on main UI thread.
        Runnable task = new Runnable() {
            @Override
            public void run() {
                synchronized (mLock) {
                    // mUrlRequestContextAdapter is guaranteed to exist until
                    // initialization on main and network threads completes and
                    // initNetworkThread is called back on network thread.
                    nativeInitRequestContextOnMainThread(mUrlRequestContextAdapter);
                }
            }
        };
        // Run task immediately or post it to the UI thread.
        if (Looper.getMainLooper() == Looper.myLooper()) {
            task.run();
        } else {
            new Handler(Looper.getMainLooper()).post(task);
        }
    }

    static long createNativeUrlRequestContextConfig(CronetEngine.Builder builder) {
        final long urlRequestContextConfig = nativeCreateRequestContextConfig(
                builder.getUserAgent(), builder.storagePath(), builder.quicEnabled(),
                builder.http2Enabled(), builder.sdchEnabled(), builder.dataReductionProxyKey(),
                builder.dataReductionProxyPrimaryProxy(), builder.dataReductionProxyFallbackProxy(),
                builder.dataReductionProxySecureProxyCheckUrl(), builder.cacheDisabled(),
                builder.httpCacheMode(), builder.httpCacheMaxSize(), builder.experimentalOptions(),
                builder.mockCertVerifier());
        for (Builder.QuicHint quicHint : builder.quicHints()) {
            nativeAddQuicHint(urlRequestContextConfig, quicHint.mHost, quicHint.mPort,
                    quicHint.mAlternatePort);
        }
        for (Builder.Pkp pkp : builder.publicKeyPins()) {
            nativeAddPkp(urlRequestContextConfig, pkp.mHost, pkp.mHashes, pkp.mIncludeSubdomains,
                    pkp.mExpirationDate.getTime());
        }
        return urlRequestContextConfig;
    }

    @Override
    public UrlRequest createRequest(String url, UrlRequest.Callback callback, Executor executor,
            int priority, Collection<Object> requestAnnotations) {
        synchronized (mLock) {
            checkHaveAdapter();
            boolean metricsCollectionEnabled = mNetworkQualityEstimatorEnabled;
            if (metricsCollectionEnabled) { // Collect metrics only if someone is listening.
                synchronized (mNetworkQualityLock) {
                    metricsCollectionEnabled = !mFinishedListenerList.isEmpty();
                }
            }
            return new CronetUrlRequest(this, url, priority, callback, executor, requestAnnotations,
                    metricsCollectionEnabled);
        }
    }

    @Override
    BidirectionalStream createBidirectionalStream(String url, BidirectionalStream.Callback callback,
            Executor executor, String httpMethod, List<Map.Entry<String, String>> requestHeaders,
            @BidirectionalStream.Builder.StreamPriority int priority) {
        synchronized (mLock) {
            checkHaveAdapter();
            return new CronetBidirectionalStream(
                    this, url, priority, callback, executor, httpMethod, requestHeaders);
        }
    }

    @Override
    public boolean isEnabled() {
        return Build.VERSION.SDK_INT >= 14;
    }

    @Override
    public String getVersionString() {
        return "Cronet/" + Version.getVersion();
    }

    @Override
    public void shutdown() {
        synchronized (mLock) {
            checkHaveAdapter();
            if (mActiveRequestCount.get() != 0) {
                throw new IllegalStateException(
                        "Cannot shutdown with active requests.");
            }
            // Destroying adapter stops the network thread, so it cannot be
            // called on network thread.
            if (Thread.currentThread() == mNetworkThread) {
                throw new IllegalThreadStateException(
                        "Cannot shutdown from network thread.");
            }
        }
        // Wait for init to complete on main and network thread (without lock,
        // so other thread could access it).
        mInitCompleted.block();

        synchronized (mLock) {
            // It is possible that adapter is already destroyed on another thread.
            if (!haveRequestContextAdapter()) {
                return;
            }
            nativeDestroy(mUrlRequestContextAdapter);
            mUrlRequestContextAdapter = 0;
        }
    }

    @Override
    public void startNetLogToFile(String fileName, boolean logAll) {
        synchronized (mLock) {
            checkHaveAdapter();
            nativeStartNetLogToFile(mUrlRequestContextAdapter, fileName,
                    logAll);
        }
    }

    @Override
    public void stopNetLog() {
        synchronized (mLock) {
            checkHaveAdapter();
            nativeStopNetLog(mUrlRequestContextAdapter);
        }
    }

    // This method is intentionally non-static to ensure Cronet native library
    // is loaded by class constructor.
    @Override
    public byte[] getGlobalMetricsDeltas() {
        return nativeGetHistogramDeltas();
    }

    @Override
    public void enableNetworkQualityEstimator(Executor executor) {
        enableNetworkQualityEstimatorForTesting(false, false, executor);
    }

    @VisibleForTesting
    @Override
    void enableNetworkQualityEstimatorForTesting(
            boolean useLocalHostRequests, boolean useSmallerResponses, Executor executor) {
        if (mNetworkQualityEstimatorEnabled) {
            throw new IllegalStateException("Network quality estimator already enabled");
        }
        mNetworkQualityEstimatorEnabled = true;
        if (executor == null) {
            throw new NullPointerException("Network quality estimator requires an executor");
        }
        mNetworkQualityExecutor = executor;
        synchronized (mLock) {
            checkHaveAdapter();
            nativeEnableNetworkQualityEstimator(
                    mUrlRequestContextAdapter, useLocalHostRequests, useSmallerResponses);
        }
    }

    @Override
    public void addRttListener(NetworkQualityRttListener listener) {
        if (!mNetworkQualityEstimatorEnabled) {
            throw new IllegalStateException("Network quality estimator must be enabled");
        }
        synchronized (mNetworkQualityLock) {
            if (mRttListenerList.isEmpty()) {
                synchronized (mLock) {
                    checkHaveAdapter();
                    nativeProvideRTTObservations(mUrlRequestContextAdapter, true);
                }
            }
            mRttListenerList.addObserver(listener);
        }
    }

    @Override
    public void removeRttListener(NetworkQualityRttListener listener) {
        if (!mNetworkQualityEstimatorEnabled) {
            throw new IllegalStateException("Network quality estimator must be enabled");
        }
        synchronized (mNetworkQualityLock) {
            mRttListenerList.removeObserver(listener);
            if (mRttListenerList.isEmpty()) {
                synchronized (mLock) {
                    checkHaveAdapter();
                    nativeProvideRTTObservations(mUrlRequestContextAdapter, false);
                }
            }
        }
    }

    @Override
    public void addThroughputListener(NetworkQualityThroughputListener listener) {
        if (!mNetworkQualityEstimatorEnabled) {
            throw new IllegalStateException("Network quality estimator must be enabled");
        }
        synchronized (mNetworkQualityLock) {
            if (mThroughputListenerList.isEmpty()) {
                synchronized (mLock) {
                    checkHaveAdapter();
                    nativeProvideThroughputObservations(mUrlRequestContextAdapter, true);
                }
            }
            mThroughputListenerList.addObserver(listener);
        }
    }

    @Override
    public void removeThroughputListener(NetworkQualityThroughputListener listener) {
        if (!mNetworkQualityEstimatorEnabled) {
            throw new IllegalStateException("Network quality estimator must be enabled");
        }
        synchronized (mNetworkQualityLock) {
            mThroughputListenerList.removeObserver(listener);
            if (mThroughputListenerList.isEmpty()) {
                synchronized (mLock) {
                    checkHaveAdapter();
                    nativeProvideThroughputObservations(mUrlRequestContextAdapter, false);
                }
            }
        }
    }

    @Override
    public void addRequestFinishedListener(RequestFinishedListener listener) {
        if (!mNetworkQualityEstimatorEnabled) {
            throw new IllegalStateException("Network quality estimator must be enabled");
        }
        synchronized (mNetworkQualityLock) {
            mFinishedListenerList.addObserver(listener);
        }
    }

    @Override
    public void removeRequestFinishedListener(RequestFinishedListener listener) {
        if (!mNetworkQualityEstimatorEnabled) {
            throw new IllegalStateException("Network quality estimator must be enabled");
        }
        synchronized (mNetworkQualityLock) {
            mFinishedListenerList.removeObserver(listener);
        }
    }

    @Override
    public URLConnection openConnection(URL url) {
        return openConnection(url, Proxy.NO_PROXY);
    }

    @Override
    public URLConnection openConnection(URL url, Proxy proxy) {
        if (proxy.type() != Proxy.Type.DIRECT) {
            throw new UnsupportedOperationException();
        }
        String protocol = url.getProtocol();
        if ("http".equals(protocol) || "https".equals(protocol)) {
            return new CronetHttpURLConnection(url, this);
        }
        throw new UnsupportedOperationException("Unexpected protocol:" + protocol);
    }

    @Override
    public URLStreamHandlerFactory createURLStreamHandlerFactory() {
        return new CronetURLStreamHandlerFactory(this);
    }

    /**
     * Mark request as started to prevent shutdown when there are active
     * requests.
     */
    void onRequestStarted() {
        mActiveRequestCount.incrementAndGet();
    }

    /**
     * Mark request as finished to allow shutdown when there are no active
     * requests.
     */
    void onRequestDestroyed() {
        mActiveRequestCount.decrementAndGet();
    }

    @VisibleForTesting
    long getUrlRequestContextAdapter() {
        synchronized (mLock) {
            checkHaveAdapter();
            return mUrlRequestContextAdapter;
        }
    }

    private void checkHaveAdapter() throws IllegalStateException {
        if (!haveRequestContextAdapter()) {
            throw new IllegalStateException("Engine is shut down.");
        }
    }

    private boolean haveRequestContextAdapter() {
        return mUrlRequestContextAdapter != 0;
    }

    /**
     * @return loggingLevel see {@link #LOG_NONE}, {@link #LOG_DEBUG} and
     *         {@link #LOG_VERBOSE}.
     */
    private int getLoggingLevel() {
        int loggingLevel;
        if (Log.isLoggable(LOG_TAG, Log.VERBOSE)) {
            loggingLevel = LOG_VERBOSE;
        } else if (Log.isLoggable(LOG_TAG, Log.DEBUG)) {
            loggingLevel = LOG_DEBUG;
        } else {
            loggingLevel = LOG_NONE;
        }
        return loggingLevel;
    }

    @SuppressWarnings("unused")
    @CalledByNative
    private void initNetworkThread() {
        synchronized (mLock) {
            mNetworkThread = Thread.currentThread();
            mInitCompleted.open();
        }
        Thread.currentThread().setName("ChromiumNet");
        Process.setThreadPriority(Process.THREAD_PRIORITY_BACKGROUND);
    }

    @SuppressWarnings("unused")
    @CalledByNative
    private void onRttObservation(final int rttMs, final long whenMs, final int source) {
        Runnable task = new Runnable() {
            @Override
            public void run() {
                synchronized (mNetworkQualityLock) {
                    for (NetworkQualityRttListener listener : mRttListenerList) {
                        listener.onRttObservation(rttMs, whenMs, source);
                    }
                }
            }
        };
        postObservationTaskToNetworkQualityExecutor(task);
    }

    @SuppressWarnings("unused")
    @CalledByNative
    private void onThroughputObservation(
            final int throughputKbps, final long whenMs, final int source) {
        Runnable task = new Runnable() {
            @Override
            public void run() {
                synchronized (mNetworkQualityLock) {
                    for (NetworkQualityThroughputListener listener : mThroughputListenerList) {
                        listener.onThroughputObservation(throughputKbps, whenMs, source);
                    }
                }
            }
        };
        postObservationTaskToNetworkQualityExecutor(task);
    }

    void reportFinished(final CronetUrlRequest request) {
        if (mNetworkQualityEstimatorEnabled) {
            Runnable task = new Runnable() {
                @Override
                public void run() {
                    synchronized (mNetworkQualityLock) {
                        UrlRequestInfo requestInfo = request.getRequestInfo();
                        for (RequestFinishedListener listener : mFinishedListenerList) {
                            listener.onRequestFinished(requestInfo);
                        }
                    }
                }
            };
            postObservationTaskToNetworkQualityExecutor(task);
        }
    }

    void postObservationTaskToNetworkQualityExecutor(Runnable task) {
        try {
            mNetworkQualityExecutor.execute(task);
        } catch (RejectedExecutionException failException) {
            Log.e(CronetUrlRequestContext.LOG_TAG, "Exception posting task to executor",
                    failException);
        }
    }

    // Native methods are implemented in cronet_url_request_context_adapter.cc.
    private static native long nativeCreateRequestContextConfig(String userAgent,
            String storagePath, boolean quicEnabled, boolean http2Enabled, boolean sdchEnabled,
            String dataReductionProxyKey, String dataReductionProxyPrimaryProxy,
            String dataReductionProxyFallbackProxy, String dataReductionProxySecureProxyCheckUrl,
            boolean disableCache, int httpCacheMode, long httpCacheMaxSize,
            String experimentalOptions, long mockCertVerifier);

    private static native void nativeAddQuicHint(
            long urlRequestContextConfig, String host, int port, int alternatePort);

    private static native void nativeAddPkp(long urlRequestContextConfig, String host,
            byte[][] hashes, boolean includeSubdomains, long expirationTime);

    private static native long nativeCreateRequestContextAdapter(long urlRequestContextConfig);

    private static native int nativeSetMinLogLevel(int loggingLevel);

    private static native byte[] nativeGetHistogramDeltas();

    @NativeClassQualifiedName("CronetURLRequestContextAdapter")
    private native void nativeDestroy(long nativePtr);

    @NativeClassQualifiedName("CronetURLRequestContextAdapter")
    private native void nativeStartNetLogToFile(long nativePtr,
            String fileName, boolean logAll);

    @NativeClassQualifiedName("CronetURLRequestContextAdapter")
    private native void nativeStopNetLog(long nativePtr);

    @NativeClassQualifiedName("CronetURLRequestContextAdapter")
    private native void nativeInitRequestContextOnMainThread(long nativePtr);

    @NativeClassQualifiedName("CronetURLRequestContextAdapter")
    private native void nativeEnableNetworkQualityEstimator(
            long nativePtr, boolean useLocalHostRequests, boolean useSmallerResponses);

    @NativeClassQualifiedName("CronetURLRequestContextAdapter")
    private native void nativeProvideRTTObservations(long nativePtr, boolean should);

    @NativeClassQualifiedName("CronetURLRequestContextAdapter")
    private native void nativeProvideThroughputObservations(long nativePtr, boolean should);
}
