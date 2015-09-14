// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import android.net.TrafficStats;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

/**
 * This class interacts with TrafficStats API provided by Android.
 */
@JNINamespace("net::android::traffic_stats")
public class AndroidTrafficStats {
    private AndroidTrafficStats() {}

    /**
     * @return Number of bytes transmitted since device boot. Counts packets across all network
     *         interfaces, and always increases monotonically since device boot. Statistics are
     *         measured at the network layer, so they include both TCP and UDP usage.
     */
    @CalledByNative
    private static long getTotalTxBytes() {
        long bytes = TrafficStats.getTotalTxBytes();
        return bytes != TrafficStats.UNSUPPORTED ? bytes : TrafficStatsError.ERROR_NOT_SUPPORTED;
    }
}
