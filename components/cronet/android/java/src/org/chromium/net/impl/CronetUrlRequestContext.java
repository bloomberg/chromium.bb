// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.impl;

import android.os.ConditionVariable;
import android.os.Process;

import org.chromium.base.Log;
import org.chromium.base.ObserverList;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeClassQualifiedName;
import org.chromium.base.annotations.UsedByReflection;
import org.chromium.net.BidirectionalStream;
import org.chromium.net.EffectiveConnectionType;
import org.chromium.net.ExperimentalBidirectionalStream;
import org.chromium.net.NetworkQualityRttListener;
import org.chromium.net.NetworkQualityThroughputListener;
import org.chromium.net.RequestFinishedInfo;
import org.chromium.net.RttThroughputValues;
import org.chromium.net.UrlRequest;
import org.chromium.net.urlconnection.CronetHttpURLConnection;
import org.chromium.net.urlconnection.CronetURLStreamHandlerFactory;

import java.net.Proxy;
import java.net.URL;
import java.net.URLConnection;
import java.net.URLStreamHandlerFactory;
import java.util.ArrayList;
import java.util.Collection;
import java.util.HashMap;
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
@VisibleForTesting
public class CronetUrlRequestContext extends CronetEngineBase {
    private static final int LOG_NONE = 3; // LOG(FATAL), no VLOG.
    private static final int LOG_DEBUG = -1; // LOG(FATAL...INFO), VLOG(1)
    private static final int LOG_VERBOSE = -2; // LOG(FATAL...INFO), VLOG(2)
    static final String LOG_TAG = CronetUrlRequestContext.class.getSimpleName();

    /**
     * Synchronize access to mUrlRequestContextAdapter and shutdown routine.
     */
    private final Object mLock = new Object();
    private final ConditionVariable mInitCompleted = new ConditionVariable(false);
    private final AtomicInteger mActiveRequestCount = new AtomicInteger(0);

    @GuardedBy("mLock")
    private long mUrlRequestContextAdapter = 0;
    /**
     * This field is accessed without synchronization, but only for the purposes of reference
     * equality comparison with other threads. If such a comparison is performed on the network
     * thread, then there is a happens-before edge between the write of this field and the
     * subsequent read; if it's performed on another thread, then observing a value of null won't
     * change the result of the comparison.
     */
    private Thread mNetworkThread;

    private final boolean mNetworkQualityEstimatorEnabled;
    private final int mNetworkThreadPriority;

    /**
     * Locks operations on network quality listeners, because listener
     * addition and removal may occur on a different thread from notification.
     */
    private final Object mNetworkQualityLock = new Object();

    /**
     * Locks operations on the list of RequestFinishedInfo.Listeners, because operations can happen
     * on any thread. This should be used for fine-grained locking only. In particular, don't call
     * any UrlRequest methods that acquire mUrlRequestAdapterLock while holding this lock.
     */
    private final Object mFinishedListenerLock = new Object();

    /**
     * Current effective connection type as computed by the network quality
     * estimator.
     */
    @GuardedBy("mNetworkQualityLock")
    private int mEffectiveConnectionType = EffectiveConnectionType.TYPE_UNKNOWN;

    /**
     * Current estimate of the HTTP RTT (in milliseconds) computed by the
     * network quality estimator.
     */
    @GuardedBy("mNetworkQualityLock")
    private int mHttpRttMs = RttThroughputValues.INVALID_RTT_THROUGHPUT;

    /**
     * Current estimate of the transport RTT (in milliseconds) computed by the
     * network quality estimator.
     */
    @GuardedBy("mNetworkQualityLock")
    private int mTransportRttMs = RttThroughputValues.INVALID_RTT_THROUGHPUT;

    /**
     * Current estimate of the downstream throughput (in kilobits per second)
     * computed by the network quality estimator.
     */
    @GuardedBy("mNetworkQualityLock")
    private int mDownstreamThroughputKbps = RttThroughputValues.INVALID_RTT_THROUGHPUT;

    @GuardedBy("mNetworkQualityLock")
    private final ObserverList<VersionSafeCallbacks.NetworkQualityRttListenerWrapper>
            mRttListenerList =
                    new ObserverList<VersionSafeCallbacks.NetworkQualityRttListenerWrapper>();

    @GuardedBy("mNetworkQualityLock")
    private final ObserverList<VersionSafeCallbacks.NetworkQualityThroughputListenerWrapper>
            mThroughputListenerList =
                    new ObserverList<VersionSafeCallbacks
                                             .NetworkQualityThroughputListenerWrapper>();

    @GuardedBy("mFinishedListenerLock")
    private final Map<RequestFinishedInfo.Listener,
            VersionSafeCallbacks.RequestFinishedInfoListener> mFinishedListenerMap =
            new HashMap<RequestFinishedInfo.Listener,
                    VersionSafeCallbacks.RequestFinishedInfoListener>();

    /**
     * Synchronize access to mCertVerifierData.
     */
    private ConditionVariable mWaitGetCertVerifierDataComplete = new ConditionVariable();

    /** Holds CertVerifier data. */
    private String mCertVerifierData;

    private volatile ConditionVariable mStopNetLogCompleted;

    /**
     * True if a NetLog observer is active.
     */
    @GuardedBy("mLock")
    private boolean mIsLogging;

    @UsedByReflection("CronetEngine.java")
    public CronetUrlRequestContext(final CronetEngineBuilderImpl builder) {
        mNetworkQualityEstimatorEnabled = builder.networkQualityEstimatorEnabled();
        mNetworkThreadPriority = builder.threadPriority(Process.THREAD_PRIORITY_BACKGROUND);
        CronetLibraryLoader.ensureInitialized(builder.getContext(), builder);
        nativeSetMinLogLevel(getLoggingLevel());
        synchronized (mLock) {
            mUrlRequestContextAdapter =
                    nativeCreateRequestContextAdapter(createNativeUrlRequestContextConfig(builder));
            if (mUrlRequestContextAdapter == 0) {
                throw new NullPointerException("Context Adapter creation failed.");
            }
        }

        // Init native Chromium URLRequestContext on init thread.
        CronetLibraryLoader.postToInitThread(new Runnable() {
            @Override
            public void run() {
                CronetLibraryLoader.ensureInitializedOnInitThread();
                synchronized (mLock) {
                    // mUrlRequestContextAdapter is guaranteed to exist until
                    // initialization on init and network threads completes and
                    // initNetworkThread is called back on network thread.
                    nativeInitRequestContextOnInitThread(mUrlRequestContextAdapter);
                }
            }
        });
    }

    @VisibleForTesting
    public static long createNativeUrlRequestContextConfig(CronetEngineBuilderImpl builder) {
        final long urlRequestContextConfig = nativeCreateRequestContextConfig(
                builder.getUserAgent(), builder.storagePath(), builder.quicEnabled(),
                builder.getDefaultQuicUserAgentId(), builder.http2Enabled(), builder.sdchEnabled(),
                builder.brotliEnabled(), builder.cacheDisabled(), builder.httpCacheMode(),
                builder.httpCacheMaxSize(), builder.experimentalOptions(),
                builder.mockCertVerifier(), builder.networkQualityEstimatorEnabled(),
                builder.publicKeyPinningBypassForLocalTrustAnchorsEnabled(),
                builder.certVerifierData());
        for (CronetEngineBuilderImpl.QuicHint quicHint : builder.quicHints()) {
            nativeAddQuicHint(urlRequestContextConfig, quicHint.mHost, quicHint.mPort,
                    quicHint.mAlternatePort);
        }
        for (CronetEngineBuilderImpl.Pkp pkp : builder.publicKeyPins()) {
            nativeAddPkp(urlRequestContextConfig, pkp.mHost, pkp.mHashes, pkp.mIncludeSubdomains,
                    pkp.mExpirationDate.getTime());
        }
        return urlRequestContextConfig;
    }

    @Override
    public ExperimentalBidirectionalStream.Builder newBidirectionalStreamBuilder(
            String url, BidirectionalStream.Callback callback, Executor executor) {
        return new BidirectionalStreamBuilderImpl(url, callback, executor, this);
    }

    @Override
    public UrlRequestBase createRequest(String url, UrlRequest.Callback callback, Executor executor,
            int priority, Collection<Object> requestAnnotations, boolean disableCache,
            boolean disableConnectionMigration, boolean allowDirectExecutor) {
        synchronized (mLock) {
            checkHaveAdapter();
            return new CronetUrlRequest(this, url, priority, callback, executor, requestAnnotations,
                    disableCache, disableConnectionMigration, allowDirectExecutor);
        }
    }

    @Override
    protected ExperimentalBidirectionalStream createBidirectionalStream(String url,
            BidirectionalStream.Callback callback, Executor executor, String httpMethod,
            List<Map.Entry<String, String>> requestHeaders, @StreamPriority int priority,
            boolean delayRequestHeadersUntilFirstFlush, Collection<Object> requestAnnotations) {
        synchronized (mLock) {
            checkHaveAdapter();
            return new CronetBidirectionalStream(this, url, priority, callback, executor,
                    httpMethod, requestHeaders, delayRequestHeadersUntilFirstFlush,
                    requestAnnotations);
        }
    }

    @Override
    public String getVersionString() {
        return "Cronet/" + ImplVersion.getCronetVersionWithLastChange();
    }

    @Override
    public void shutdown() {
        synchronized (mLock) {
            checkHaveAdapter();
            if (mActiveRequestCount.get() != 0) {
                throw new IllegalStateException("Cannot shutdown with active requests.");
            }
            // Destroying adapter stops the network thread, so it cannot be
            // called on network thread.
            if (Thread.currentThread() == mNetworkThread) {
                throw new IllegalThreadStateException("Cannot shutdown from network thread.");
            }
        }
        // Wait for init to complete on init and network thread (without lock,
        // so other thread could access it).
        mInitCompleted.block();

        // If not logging, this is a no-op.
        stopNetLog();

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
            if (!nativeStartNetLogToFile(mUrlRequestContextAdapter, fileName, logAll)) {
                throw new RuntimeException("Unable to start NetLog");
            }
            mIsLogging = true;
        }
    }

    @Override
    public void startNetLogToDisk(String dirPath, boolean logAll, int maxSize) {
        synchronized (mLock) {
            checkHaveAdapter();
            nativeStartNetLogToDisk(mUrlRequestContextAdapter, dirPath, logAll, maxSize);
            mIsLogging = true;
        }
    }

    @Override
    public void stopNetLog() {
        synchronized (mLock) {
            if (!mIsLogging) {
                return;
            }
            checkHaveAdapter();
            mStopNetLogCompleted = new ConditionVariable();
            nativeStopNetLog(mUrlRequestContextAdapter);
            mIsLogging = false;
        }
        mStopNetLogCompleted.block();
    }

    @CalledByNative
    public void stopNetLogCompleted() {
        mStopNetLogCompleted.open();
    }

    @Override
    public String getCertVerifierData(long timeout) {
        if (timeout < 0) {
            throw new IllegalArgumentException("timeout must be a positive value");
        } else if (timeout == 0) {
            timeout = 100;
        }
        mWaitGetCertVerifierDataComplete.close();
        synchronized (mLock) {
            checkHaveAdapter();
            nativeGetCertVerifierData(mUrlRequestContextAdapter);
        }
        mWaitGetCertVerifierDataComplete.block(timeout);
        return mCertVerifierData;
    }

    // This method is intentionally non-static to ensure Cronet native library
    // is loaded by class constructor.
    @Override
    public byte[] getGlobalMetricsDeltas() {
        return nativeGetHistogramDeltas();
    }

    @Override
    public int getEffectiveConnectionType() {
        if (!mNetworkQualityEstimatorEnabled) {
            throw new IllegalStateException("Network quality estimator must be enabled");
        }
        synchronized (mNetworkQualityLock) {
            return convertConnectionTypeToApiValue(mEffectiveConnectionType);
        }
    }

    @Override
    public int getHttpRttMs() {
        if (!mNetworkQualityEstimatorEnabled) {
            throw new IllegalStateException("Network quality estimator must be enabled");
        }
        synchronized (mNetworkQualityLock) {
            return mHttpRttMs != RttThroughputValues.INVALID_RTT_THROUGHPUT
                    ? mHttpRttMs
                    : CONNECTION_METRIC_UNKNOWN;
        }
    }

    @Override
    public int getTransportRttMs() {
        if (!mNetworkQualityEstimatorEnabled) {
            throw new IllegalStateException("Network quality estimator must be enabled");
        }
        synchronized (mNetworkQualityLock) {
            return mTransportRttMs != RttThroughputValues.INVALID_RTT_THROUGHPUT
                    ? mTransportRttMs
                    : CONNECTION_METRIC_UNKNOWN;
        }
    }

    @Override
    public int getDownstreamThroughputKbps() {
        if (!mNetworkQualityEstimatorEnabled) {
            throw new IllegalStateException("Network quality estimator must be enabled");
        }
        synchronized (mNetworkQualityLock) {
            return mDownstreamThroughputKbps != RttThroughputValues.INVALID_RTT_THROUGHPUT
                    ? mDownstreamThroughputKbps
                    : CONNECTION_METRIC_UNKNOWN;
        }
    }

    @VisibleForTesting
    @Override
    public void configureNetworkQualityEstimatorForTesting(boolean useLocalHostRequests,
            boolean useSmallerResponses, boolean disableOfflineCheck) {
        if (!mNetworkQualityEstimatorEnabled) {
            throw new IllegalStateException("Network quality estimator must be enabled");
        }
        synchronized (mLock) {
            checkHaveAdapter();
            nativeConfigureNetworkQualityEstimatorForTesting(mUrlRequestContextAdapter,
                    useLocalHostRequests, useSmallerResponses, disableOfflineCheck);
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
            mRttListenerList.addObserver(
                    new VersionSafeCallbacks.NetworkQualityRttListenerWrapper(listener));
        }
    }

    @Override
    public void removeRttListener(NetworkQualityRttListener listener) {
        if (!mNetworkQualityEstimatorEnabled) {
            throw new IllegalStateException("Network quality estimator must be enabled");
        }
        synchronized (mNetworkQualityLock) {
            if (mRttListenerList.removeObserver(
                        new VersionSafeCallbacks.NetworkQualityRttListenerWrapper(listener))) {
                if (mRttListenerList.isEmpty()) {
                    synchronized (mLock) {
                        checkHaveAdapter();
                        nativeProvideRTTObservations(mUrlRequestContextAdapter, false);
                    }
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
            mThroughputListenerList.addObserver(
                    new VersionSafeCallbacks.NetworkQualityThroughputListenerWrapper(listener));
        }
    }

    @Override
    public void removeThroughputListener(NetworkQualityThroughputListener listener) {
        if (!mNetworkQualityEstimatorEnabled) {
            throw new IllegalStateException("Network quality estimator must be enabled");
        }
        synchronized (mNetworkQualityLock) {
            if (mThroughputListenerList.removeObserver(
                        new VersionSafeCallbacks.NetworkQualityThroughputListenerWrapper(
                                listener))) {
                if (mThroughputListenerList.isEmpty()) {
                    synchronized (mLock) {
                        checkHaveAdapter();
                        nativeProvideThroughputObservations(mUrlRequestContextAdapter, false);
                    }
                }
            }
        }
    }

    @Override
    public void addRequestFinishedListener(RequestFinishedInfo.Listener listener) {
        synchronized (mFinishedListenerLock) {
            mFinishedListenerMap.put(
                    listener, new VersionSafeCallbacks.RequestFinishedInfoListener(listener));
        }
    }

    @Override
    public void removeRequestFinishedListener(RequestFinishedInfo.Listener listener) {
        synchronized (mFinishedListenerLock) {
            mFinishedListenerMap.remove(listener);
        }
    }

    boolean hasRequestFinishedListener() {
        synchronized (mFinishedListenerLock) {
            return !mFinishedListenerMap.isEmpty();
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
    public long getUrlRequestContextAdapter() {
        synchronized (mLock) {
            checkHaveAdapter();
            return mUrlRequestContextAdapter;
        }
    }

    @GuardedBy("mLock")
    private void checkHaveAdapter() throws IllegalStateException {
        if (!haveRequestContextAdapter()) {
            throw new IllegalStateException("Engine is shut down.");
        }
    }

    @GuardedBy("mLock")
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

    private static int convertConnectionTypeToApiValue(@EffectiveConnectionType int type) {
        switch (type) {
            case EffectiveConnectionType.TYPE_OFFLINE:
                return EFFECTIVE_CONNECTION_TYPE_OFFLINE;
            case EffectiveConnectionType.TYPE_SLOW_2G:
                return EFFECTIVE_CONNECTION_TYPE_SLOW_2G;
            case EffectiveConnectionType.TYPE_2G:
                return EFFECTIVE_CONNECTION_TYPE_2G;
            case EffectiveConnectionType.TYPE_3G:
                return EFFECTIVE_CONNECTION_TYPE_3G;
            case EffectiveConnectionType.TYPE_4G:
                return EFFECTIVE_CONNECTION_TYPE_4G;
            case EffectiveConnectionType.TYPE_UNKNOWN:
                return EFFECTIVE_CONNECTION_TYPE_UNKNOWN;
            default:
                throw new RuntimeException(
                        "Internal Error: Illegal EffectiveConnectionType value " + type);
        }
    }

    @SuppressWarnings("unused")
    @CalledByNative
    private void initNetworkThread() {
        mNetworkThread = Thread.currentThread();
        mInitCompleted.open();
        Thread.currentThread().setName("ChromiumNet");
        Process.setThreadPriority(mNetworkThreadPriority);
    }

    @SuppressWarnings("unused")
    @CalledByNative
    private void onEffectiveConnectionTypeChanged(int effectiveConnectionType) {
        synchronized (mNetworkQualityLock) {
            // Convert the enum returned by the network quality estimator to an enum of type
            // EffectiveConnectionType.
            mEffectiveConnectionType = effectiveConnectionType;
        }
    }

    @SuppressWarnings("unused")
    @CalledByNative
    private void onRTTOrThroughputEstimatesComputed(
            final int httpRttMs, final int transportRttMs, final int downstreamThroughputKbps) {
        synchronized (mNetworkQualityLock) {
            mHttpRttMs = httpRttMs;
            mTransportRttMs = transportRttMs;
            mDownstreamThroughputKbps = downstreamThroughputKbps;
        }
    }

    @SuppressWarnings("unused")
    @CalledByNative
    private void onRttObservation(final int rttMs, final long whenMs, final int source) {
        synchronized (mNetworkQualityLock) {
            for (final VersionSafeCallbacks.NetworkQualityRttListenerWrapper listener :
                    mRttListenerList) {
                Runnable task = new Runnable() {
                    @Override
                    public void run() {
                        listener.onRttObservation(rttMs, whenMs, source);
                    }
                };
                postObservationTaskToExecutor(listener.getExecutor(), task);
            }
        }
    }

    @SuppressWarnings("unused")
    @CalledByNative
    private void onThroughputObservation(
            final int throughputKbps, final long whenMs, final int source) {
        synchronized (mNetworkQualityLock) {
            for (final VersionSafeCallbacks.NetworkQualityThroughputListenerWrapper listener :
                    mThroughputListenerList) {
                Runnable task = new Runnable() {
                    @Override
                    public void run() {
                        listener.onThroughputObservation(throughputKbps, whenMs, source);
                    }
                };
                postObservationTaskToExecutor(listener.getExecutor(), task);
            }
        }
    }

    @SuppressWarnings("unused")
    @CalledByNative
    private void onGetCertVerifierData(String certVerifierData) {
        mCertVerifierData = certVerifierData;
        mWaitGetCertVerifierDataComplete.open();
    }

    void reportFinished(final RequestFinishedInfo requestInfo) {
        ArrayList<VersionSafeCallbacks.RequestFinishedInfoListener> currentListeners;
        synchronized (mFinishedListenerLock) {
            currentListeners = new ArrayList<VersionSafeCallbacks.RequestFinishedInfoListener>(
                    mFinishedListenerMap.values());
        }
        for (final VersionSafeCallbacks.RequestFinishedInfoListener listener : currentListeners) {
            Runnable task = new Runnable() {
                @Override
                public void run() {
                    listener.onRequestFinished(requestInfo);
                }
            };
            postObservationTaskToExecutor(listener.getExecutor(), task);
        }
    }

    private static void postObservationTaskToExecutor(Executor executor, Runnable task) {
        try {
            executor.execute(task);
        } catch (RejectedExecutionException failException) {
            Log.e(CronetUrlRequestContext.LOG_TAG, "Exception posting task to executor",
                    failException);
        }
    }

    // Native methods are implemented in cronet_url_request_context_adapter.cc.
    private static native long nativeCreateRequestContextConfig(String userAgent,
            String storagePath, boolean quicEnabled, String quicUserAgentId, boolean http2Enabled,
            boolean sdchEnabled, boolean brotliEnabled, boolean disableCache, int httpCacheMode,
            long httpCacheMaxSize, String experimentalOptions, long mockCertVerifier,
            boolean enableNetworkQualityEstimator,
            boolean bypassPublicKeyPinningForLocalTrustAnchors, String certVerifierData);

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
    private native boolean nativeStartNetLogToFile(long nativePtr, String fileName, boolean logAll);

    @NativeClassQualifiedName("CronetURLRequestContextAdapter")
    private native void nativeStartNetLogToDisk(
            long nativePtr, String dirPath, boolean logAll, int maxSize);

    @NativeClassQualifiedName("CronetURLRequestContextAdapter")
    private native void nativeStopNetLog(long nativePtr);

    @NativeClassQualifiedName("CronetURLRequestContextAdapter")
    private native void nativeGetCertVerifierData(long nativePtr);

    @NativeClassQualifiedName("CronetURLRequestContextAdapter")
    private native void nativeInitRequestContextOnInitThread(long nativePtr);

    @NativeClassQualifiedName("CronetURLRequestContextAdapter")
    private native void nativeConfigureNetworkQualityEstimatorForTesting(long nativePtr,
            boolean useLocalHostRequests, boolean useSmallerResponses, boolean disableOfflineCheck);

    @NativeClassQualifiedName("CronetURLRequestContextAdapter")
    private native void nativeProvideRTTObservations(long nativePtr, boolean should);

    @NativeClassQualifiedName("CronetURLRequestContextAdapter")
    private native void nativeProvideThroughputObservations(long nativePtr, boolean should);

    public boolean isNetworkThread(Thread thread) {
        return thread == mNetworkThread;
    }
}
