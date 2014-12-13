// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import org.chromium.base.JNINamespace;

/**
 * Controls UMA histograms.
 */
@JNINamespace("cronet")
public final class HistogramManager {
    public HistogramManager() {
    }

    /**
     * Get histogram deltas serialized as protobuf.
     */
    public byte[] getHistogramDeltas() {
        return nativeGetHistogramDeltas();
    }

    private native byte[] nativeGetHistogramDeltas();
}
