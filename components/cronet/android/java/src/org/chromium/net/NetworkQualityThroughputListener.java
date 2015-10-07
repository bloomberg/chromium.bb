// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

/**
 * Interface to watch for observations of throughput.
 */
public interface NetworkQualityThroughputListener {
    /**
     * Reports a new throughput observation.
     * @param throughputKbps the downstream throughput in kilobits per second.
     * @param whenMs milliseconds since the Epoch (January 1st 1970, 00:00:00.000).
     * @param source the observation source from {@link NetworkQualityObservationSource}.
     */
    public void onThroughputObservation(int throughputKbps, long whenMs, int source);
}