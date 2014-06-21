// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import android.content.Context;
import android.os.ConditionVariable;
import android.os.Process;
import android.util.Log;

import org.chromium.base.CalledByNative;
import org.chromium.base.JNINamespace;

/**
 * Provides context for the native HTTP operations.
 */
@JNINamespace("cronet")
public class UrlRequestContext {
    private static final int LOG_NONE = 0;
    private static final int LOG_DEBUG = 1;
    private static final int LOG_VERBOSE = 2;
    private static final String LOG_TAG = "ChromiumNetwork";

    /**
     * Native peer object, owned by UrlRequestContext.
     */
    private long mUrlRequestContextPeer;

    private final ConditionVariable mStarted = new ConditionVariable();

    /**
     * Constructor.
     *
     */
    protected UrlRequestContext(Context context, String userAgent,
            String config) {
        mUrlRequestContextPeer = nativeCreateRequestContextPeer(context,
                userAgent, getLoggingLevel(), config);
        if (mUrlRequestContextPeer == 0)
            throw new NullPointerException("Context Peer creation failed");

        // TODO(mef): Revisit the need of block here.
        mStarted.block(2000);
    }

    /**
     * Returns the version of this network stack formatted as N.N.N.N/X where
     * N.N.N.N is the version of Chromium and X is the revision number.
     */
    public static String getVersion() {
        return Version.getVersion();
    }

    /**
     * Initializes statistics recorder.
     */
    public void initializeStatistics() {
        nativeInitializeStatistics();
    }

    /**
     * Gets current statistics recorded since |initializeStatistics| with
     * |filter| as a substring as JSON text (an empty |filter| will include all
     * registered histograms).
     */
    public String getStatisticsJSON(String filter) {
        return nativeGetStatisticsJSON(filter);
    }

    /**
     * Starts NetLog logging to a file named |fileName| in the
     * application temporary directory. |fileName| must not be empty. Log level
     * is LOG_ALL_BUT_BYTES. If the file exists it is truncated before starting.
     * If actively logging the call is ignored.
     */
    public void startNetLogToFile(String fileName) {
        nativeStartNetLogToFile(mUrlRequestContextPeer, fileName);
    }

    /**
     * Stops NetLog logging and flushes file to disk. If a logging session is
     * not in progress this call is ignored.
     */
    public void stopNetLog() {
        nativeStopNetLog(mUrlRequestContextPeer);
    }

    @CalledByNative
    private void initNetworkThread() {
        Thread.currentThread().setName("ChromiumNet");
        Process.setThreadPriority(Process.THREAD_PRIORITY_BACKGROUND);
        mStarted.open();
    }

    @Override
    protected void finalize() throws Throwable {
        nativeReleaseRequestContextPeer(mUrlRequestContextPeer);
        super.finalize();
    }

    protected long getUrlRequestContextPeer() {
        return mUrlRequestContextPeer;
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

    // Returns an instance URLRequestContextPeer to be stored in
    // mUrlRequestContextPeer.
    private native long nativeCreateRequestContextPeer(Context context,
            String userAgent, int loggingLevel, String config);

    private native void nativeReleaseRequestContextPeer(
            long urlRequestContextPeer);

    private native void nativeInitializeStatistics();

    private native String nativeGetStatisticsJSON(String filter);

    private native void nativeStartNetLogToFile(long urlRequestContextPeer,
            String fileName);

    private native void nativeStopNetLog(long urlRequestContextPeer);
}
