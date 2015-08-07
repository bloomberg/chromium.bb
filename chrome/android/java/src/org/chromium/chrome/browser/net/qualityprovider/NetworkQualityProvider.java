// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.net.qualityprovider;

import android.content.Context;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.browser.ChromeApplication;
import org.chromium.chrome.browser.util.NonThreadSafe;

/**
 * This class provides a base class implementation and may be overridden on operating systems that
 * provide more useful APIs. This class is not thread safe.
 */
public class NetworkQualityProvider {
    /**
     * Value to return if a valid value is unavailable.
     */
    protected static final int NO_VALUE = -1;
    private static final Object LOCK = new Object();
    private static NonThreadSafe sThreadCheck = null;
    private static NetworkQualityProvider sNetworkQualityProvider;

    @CalledByNative
    private static NetworkQualityProvider create(Context context) {
        synchronized (LOCK) {
            if (sNetworkQualityProvider == null) {
                assert sThreadCheck == null;
                assert sNetworkQualityProvider == null;
                sThreadCheck = new NonThreadSafe();
                sNetworkQualityProvider =
                        ((ChromeApplication) context).createNetworkQualityProvider();
            }
        }
        return sNetworkQualityProvider;
    }

    /**
     * Creates an instance of |@link #NetworkQualityProvider}.
     */
    public NetworkQualityProvider() {
        assert sThreadCheck.calledOnValidThread();
    }

    /**
     * @return True if the network quality estimate is available.
     */
    @CalledByNative
    protected boolean isEstimateAvailable() {
        assert sThreadCheck.calledOnValidThread();
        return false;
    }

    /**
     * Requests the provider to update the network quality.
     */
    @CalledByNative
    protected void requestUpdate() {
        assert sThreadCheck.calledOnValidThread();
    }

    /**
     * @return Expected RTT duration in milliseconds or {@link #NO_VALUE} if the estimate is
     *         unavailable.
     */
    @CalledByNative
    protected int getRTTMilliseconds() {
        assert sThreadCheck.calledOnValidThread();
        return NO_VALUE;
    }

    /**
     * @return The expected downstream throughput in Kbps (Kilobits per second) or
     *         {@link #NO_VALUE} if the estimate is unavailable.
     */
    @CalledByNative
    protected long getDownstreamThroughputKbps() {
        assert sThreadCheck.calledOnValidThread();
        return NO_VALUE;
    }

    /**
     * @return The expected upstream throughput in Kbps (Kilobits per second) or
     *         {@link #NO_VALUE} if the estimate is unavailable.
     */
    @CalledByNative
    protected long getUpstreamThroughputKbps() {
        assert sThreadCheck.calledOnValidThread();
        return NO_VALUE;
    }

    /**
     * @return Time (in seconds) since the network quality was last updated.
     */
    @CalledByNative
    protected long getTimeSinceLastUpdateSeconds() {
        assert sThreadCheck.calledOnValidThread();
        return NO_VALUE;
    }

    @CalledByNative
    private static int getNoValue() {
        assert sThreadCheck.calledOnValidThread();
        return NO_VALUE;
    }
}
