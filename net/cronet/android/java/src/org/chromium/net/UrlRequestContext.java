// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import android.content.Context;
import android.os.ConditionVariable;
import android.os.Process;

import org.chromium.base.AccessedByNative;
import org.chromium.base.CalledByNative;

/**
 * Provides context for the native HTTP operations.
 */
public class UrlRequestContext {
    protected static final int LOG_NONE = 0;

    protected static final int LOG_DEBUG = 1;

    protected static final int LOG_VERBOSE = 2;

    /**
     * This field is accessed exclusively from the native layer.
     */
    @AccessedByNative
    private long mRequestContext;

    private final ConditionVariable mStarted = new ConditionVariable();

    /**
     * Constructor.
     *
     * @param loggingLevel see {@link #LOG_NONE}, {@link #LOG_DEBUG} and
     *            {@link #LOG_VERBOSE}.
     */
    protected UrlRequestContext(Context context, String userAgent,
            int loggingLevel) {
        nativeInitialize(context, userAgent, loggingLevel);
        mStarted.block(2000);
    }

    /**
     * Returns the version of this network stack formatted as N.N.N.N/X where
     * N.N.N.N is the version of Chromium and X is the version of the JNI layer.
     */
    public static native String getVersion();

    @CalledByNative
    private void initNetworkThread() {
        Thread.currentThread().setName("ChromiumNet");
        Process.setThreadPriority(Process.THREAD_PRIORITY_BACKGROUND);
        mStarted.open();
    }

    @Override
    protected void finalize() throws Throwable {
        nativeFinalize();
        super.finalize();
    }

    private native void nativeInitialize(Context context, String userAgent,
            int loggingLevel);

    private native void nativeFinalize();
}
