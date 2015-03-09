// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import android.content.Context;
import android.os.Build;
import android.os.ConditionVariable;
import android.os.Handler;
import android.os.Looper;
import android.os.Process;
import android.util.Log;

import org.chromium.base.CalledByNative;
import org.chromium.base.JNINamespace;
import org.chromium.base.NativeClassQualifiedName;

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

    /**
     * Synchronize access to mUrlRequestContextAdapter and shutdown routine.
     */
    private final Object mLock = new Object();
    private final ConditionVariable mInitCompleted = new ConditionVariable(false);
    private final AtomicInteger mActiveRequestCount = new AtomicInteger(0);

    private long mUrlRequestContextAdapter = 0;
    private Thread mNetworkThread;

    public CronetUrlRequestContext(Context context,
                                   UrlRequestContextConfig config) {
        CronetLibraryLoader.ensureInitialized(context, config);
        nativeSetMinLogLevel(getLoggingLevel());
        mUrlRequestContextAdapter = nativeCreateRequestContextAdapter(config.toString());
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

    @Override
    public UrlRequest createRequest(String url, UrlRequestListener listener,
                                    Executor executor) {
        synchronized (mLock) {
            checkHaveAdapter();
            return new CronetUrlRequest(this, mUrlRequestContextAdapter, url,
                    UrlRequest.REQUEST_PRIORITY_MEDIUM, listener, executor);
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
    public void startNetLogToFile(String fileName) {
        synchronized (mLock) {
            checkHaveAdapter();
            nativeStartNetLogToFile(mUrlRequestContextAdapter, fileName);
        }
    }

    @Override
    public void stopNetLog() {
        synchronized (mLock) {
            checkHaveAdapter();
            nativeStopNetLog(mUrlRequestContextAdapter);
        }
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
        synchronized (mLock) {
            checkHaveAdapter();
            return mUrlRequestContextAdapter;
        }
    }

    private void checkHaveAdapter() throws IllegalStateException {
        if (!haveRequestContextAdapter()) {
            throw new IllegalStateException("Context is shut down.");
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

    // Native methods are implemented in cronet_url_request_context.cc.
    private static native long nativeCreateRequestContextAdapter(String config);

    private static native int nativeSetMinLogLevel(int loggingLevel);

    @NativeClassQualifiedName("CronetURLRequestContextAdapter")
    private native void nativeDestroy(long nativePtr);

    @NativeClassQualifiedName("CronetURLRequestContextAdapter")
    private native void nativeStartNetLogToFile(long nativePtr,
            String fileName);

    @NativeClassQualifiedName("CronetURLRequestContextAdapter")
    private native void nativeStopNetLog(long nativePtr);

    @NativeClassQualifiedName("CronetURLRequestContextAdapter")
    private native void nativeInitRequestContextOnMainThread(long nativePtr);
}
