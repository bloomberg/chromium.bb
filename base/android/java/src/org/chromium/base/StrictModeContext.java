// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import android.os.StrictMode;

import java.io.Closeable;

/**
 * Enables try-with-resources compatible StrictMode violation whitelisting.
 *
 * Example:
 * <pre>
 *     try (StrictModeContext unused = StrictModeContext.allowDiskWrites()) {
 *         return Example.doThingThatRequiresDiskWrites();
 *     }
 * </pre>
 *
 */
public final class StrictModeContext implements Closeable {
    private final StrictMode.ThreadPolicy mThreadPolicy;

    private StrictModeContext(StrictMode.ThreadPolicy threadPolicy) {
        mThreadPolicy = threadPolicy;
    }

    /**
     * Convenience method for disabling StrictMode for disk-writes with try-with-resources.
     */
    public static StrictModeContext allowDiskWrites() {
        StrictMode.ThreadPolicy oldPolicy = StrictMode.allowThreadDiskWrites();
        return new StrictModeContext(oldPolicy);
    }

    /**
     * Convenience method for disabling StrictMode for disk-reads with try-with-resources.
     */
    public static StrictModeContext allowDiskReads() {
        StrictMode.ThreadPolicy oldPolicy = StrictMode.allowThreadDiskReads();
        return new StrictModeContext(oldPolicy);
    }

    @Override
    public void close() {
        StrictMode.setThreadPolicy(mThreadPolicy);
    }
}