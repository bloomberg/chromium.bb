// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices;

import android.os.SystemClock;
import android.support.annotation.IntDef;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.concurrent.TimeUnit;

/**
 * Class to contain metrics recording constants and behaviour for Browser Services.
 */
public class BrowserServicesMetrics {
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({VerificationResult.ONLINE_SUCCESS, VerificationResult.ONLINE_FAILURE,
            VerificationResult.OFFLINE_SUCCESS, VerificationResult.OFFLINE_FAILURE,
            VerificationResult.HTTPS_FAILURE, VerificationResult.REQUEST_FAILURE,
            VerificationResult.CACHED_SUCCESS})
    public @interface VerificationResult {
        // Don't reuse values or reorder values. If you add something new, change NUM_ENTRIES as
        // well.
        int ONLINE_SUCCESS = 0;
        int ONLINE_FAILURE = 1;
        int OFFLINE_SUCCESS = 2;
        int OFFLINE_FAILURE = 3;
        int HTTPS_FAILURE = 4;
        int REQUEST_FAILURE = 5;
        int CACHED_SUCCESS = 6;
        int NUM_ENTRIES = 7;
    }

    /**
     * Records the verification result for Trusted Web Activity verification.
     */
    public static void recordVerificationResult(@VerificationResult int result) {
        RecordHistogram.recordEnumeratedHistogram(
                "BrowserServices.VerificationResult", result, VerificationResult.NUM_ENTRIES);
    }

    /**
     * Records that a Trusted Web Activity has been opened.
     */
    public static void recordTwaOpened() {
        RecordUserAction.record("BrowserServices.TwaOpened");
    }

    /**
     * Records the time that a Trusted Web Activity has been open for.
     */
    public static void recordTwaOpenTime(long duration, TimeUnit unit) {
        RecordHistogram.recordTimesHistogram("BrowserServices.TwaOpenTime", duration, unit);
    }

    /**
     * Returns a {@link TimingMetric} that records the amount of time spent querying the Android
     * system for ResolveInfos that will deal with a given URL when launching from a background
     * service.
     */
    public static TimingMetric getServiceTabResolveInfoTimingContext() {
        return new TimingMetric("BrowserServices.ServiceTabResolveInfoQuery");
    }

    /**
     * A class to be used with a try-with-resources to record the elapsed time within the try block.
     */
    public static class TimingMetric implements AutoCloseable {
        private final String mMetric;
        private final long mStart;

        private static long now() {
            return SystemClock.uptimeMillis();
        }

        public TimingMetric(String metric) {
            mMetric = metric;
            mStart = now();
        }

        @Override
        public void close() {
            RecordHistogram.recordMediumTimesHistogram(
                    mMetric, now() - mStart, TimeUnit.MILLISECONDS);
        }
    }

    // Don't let anyone instantiate.
    private BrowserServicesMetrics() {}
}
