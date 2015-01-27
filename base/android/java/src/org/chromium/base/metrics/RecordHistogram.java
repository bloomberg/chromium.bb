// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.metrics;

import org.chromium.base.JNINamespace;
import org.chromium.base.VisibleForTesting;

/**
 * Java API for recording UMA histograms. As opposed to macros used in native code, these calls are
 * not caching the histogram pointer; also, the JNI calls are relatively costly - avoid calling
 * these methods in performance-critical code.
 */
@JNINamespace("base::android")
public class RecordHistogram {
    /**
     * Records a sample in a boolean UMA histogram of the given name. Boolean histogram has two
     * buckets, corresponding to success (true) and failure (false).
     * @param name name of the histogram
     * @param sample sample to be recorded, either true or false
     */
    public static void recordBooleanHistogram(String name, boolean sample) {
        nativeRecordBooleanHistogram(name, System.identityHashCode(name), sample);
    }

    /**
     * Records a sample in an enumerated histogram of the given name and boundary. Note that
     * |boundary| identifies the histogram - it should be the same at every invocation.
     * @param name name of the histogram
     * @param sample sample to be recorded, at least 0 and at most |boundary| - 1
     * @param boundary upper bound for legal sample values - all sample values has to be strictly
     *        lower than |boundary|
     */
    public static void recordEnumeratedHistogram(String name, int sample, int boundary) {
        nativeRecordEnumeratedHistogram(name, System.identityHashCode(name), sample, boundary);
    }

    /**
     * Returns the number of samples recorded in the given bucket of the given histogram.
     * @param name name of the histogram to look up
     * @param sample the bucket containing this sample value will be looked up
     */
    @VisibleForTesting
    public static int getHistogramValueCountForTesting(String name, int sample) {
        return nativeGetHistogramValueCountForTesting(name, sample);
    }

    /**
     * Initializes the metrics system.
     */
    public static void initialize() {
        nativeInitialize();
    }

    private static native void nativeRecordBooleanHistogram(String name, int key, boolean sample);
    private static native void nativeRecordEnumeratedHistogram(
            String name, int key, int sample, int boundary);

    private static native int nativeGetHistogramValueCountForTesting(String name, int sample);
    private static native void nativeInitialize();
}
