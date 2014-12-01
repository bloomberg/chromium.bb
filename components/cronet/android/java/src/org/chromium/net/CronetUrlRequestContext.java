// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import android.content.Context;
import android.os.Build;
import android.os.Process;
import android.util.Log;

import org.chromium.base.CalledByNative;
import org.chromium.base.JNINamespace;

import java.util.concurrent.Executor;
import java.util.concurrent.atomic.AtomicInteger;

/**
 * UrlRequest context using Chromium HTTP stack implementation.
 */
@JNINamespace("cronet")
public class CronetUrlRequestContext extends UrlRequestContext  {
    private static final int LOG_NONE = 3;  // LOG(FATAL), no VLOG.
    private static final int LOG_DEBUG = -1;  // LOG(FATAL...INFO), VLOG(1)
    private static final int LOG_VERBOSE = -2;  // LOG(FATAL...INFO), VLOG(2)
    static final String LOG_TAG = "ChromiumNetwork";

    private long mUrlRequestContextAdapter = 0;
    private Thread mNetworkThread;
    private AtomicInteger mActiveRequestCount = new AtomicInteger(0);

    public CronetUrlRequestContext(Context context,
                                   UrlRequestContextConfig config) {
        nativeSetMinLogLevel(getLoggingLevel());
        mUrlRequestContextAdapter = nativeCreateRequestContextAdapter(
                context, config.toString());
        if (mUrlRequestContextAdapter == 0) {
            throw new NullPointerException("Context Adapter creation failed");
        }
    }

    @Override
    public UrlRequest createRequest(String url, UrlRequestListener listener,
                                    Executor executor) {
        if (mUrlRequestContextAdapter == 0) {
            throw new IllegalStateException(
                    "Cannot create requests on shutdown context.");
        }
        return new CronetUrlRequest(this, mUrlRequestContextAdapter, url,
                UrlRequest.REQUEST_PRIORITY_MEDIUM, listener, executor);
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
        if (mActiveRequestCount.get() != 0) {
            throw new IllegalStateException(
                    "Cannot shutdown with active requests.");
        }
        // Destroying adapter stops the network thread, so it cannot be called
        // on network thread.
        if (Thread.currentThread() == mNetworkThread) {
            throw new IllegalThreadStateException(
                    "Cannot shutdown from network thread.");
        }
        nativeDestroyRequestContextAdapter(mUrlRequestContextAdapter);
        mUrlRequestContextAdapter = 0;
    }

    @Override
    public void startNetLogToFile(String fileName) {
        nativeStartNetLogToFile(mUrlRequestContextAdapter, fileName);
    }

    @Override
    public void stopNetLog() {
        nativeStopNetLog(mUrlRequestContextAdapter);
    }

    /**
     * Mark request as started to prevent shutdown when there are active
     * requests.
     */
    void onRequestStarted(UrlRequest urlRequest) {
        mActiveRequestCount.incrementAndGet();
    }

    /**
     * Mark request as completed to allow shutdown when there are no active
     * requests.
     */
    void onRequestDestroyed(UrlRequest urlRequest) {
        mActiveRequestCount.decrementAndGet();
    }

    long getUrlRequestContextAdapter() {
        if (mUrlRequestContextAdapter == 0) {
            throw new IllegalStateException("Context Adapter is destroyed.");
        }
        return mUrlRequestContextAdapter;
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
        mNetworkThread = Thread.currentThread();
        Thread.currentThread().setName("ChromiumNet");
        Process.setThreadPriority(Process.THREAD_PRIORITY_BACKGROUND);
    }

    // Native methods are implemented in cronet_url_request_context.cc.
    private native long nativeCreateRequestContextAdapter(Context context,
            String config);

    private native void nativeDestroyRequestContextAdapter(
            long urlRequestContextAdapter);

    private native void nativeStartNetLogToFile(
            long urlRequestContextAdapter, String fileName);

    private native void nativeStopNetLog(long urlRequestContextAdapter);

    private native int nativeSetMinLogLevel(int loggingLevel);
}
