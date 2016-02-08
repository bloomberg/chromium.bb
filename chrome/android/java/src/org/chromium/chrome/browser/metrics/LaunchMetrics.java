// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.metrics;

import android.util.Pair;

import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.content_public.browser.WebContents;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.TimeUnit;

/**
 * Used for recording metrics about Chrome launches that need to be recorded before the native
 * library may have been loaded.  Metrics are cached until the library is known to be loaded, then
 * committed to the MetricsService all at once.
 */
@JNINamespace("metrics")
public class LaunchMetrics {

    /**
     * Creating an instance of a subclass of this class automatically adds it to a list of objects
     * that are committed when the native library is available.
     */
    private abstract static class CachedHistogram {
        private static final List<CachedHistogram> sEvents = new ArrayList<CachedHistogram>();

        protected String mHistogramName;

        /**
         * @param histogramName Name of the histogram to record.
         */
        protected CachedHistogram(String histogramName) {
            mHistogramName = histogramName;
            sEvents.add(this);
        }

        /** Commits the histogram. Expects the native library to be loaded. */
        protected abstract void commitAndClear();
    }

    /** Caches whether an event happened. */
    public static class BooleanEvent extends CachedHistogram {
        private boolean mIsHit;

        public BooleanEvent(String histogramName) {
            super(histogramName);
        }

        /** Records that the histogram condition occurred. */
        public void recordHit() {
            mIsHit = true;
        }

        @Override
        protected void commitAndClear() {
            RecordHistogram.recordBooleanHistogram(mHistogramName, mIsHit);
            mIsHit = false;
        }
    }

    /** Caches a set of integer histogram samples. */
    public static class SparseHistogramSample extends CachedHistogram {
        private final List<Integer> mSamples = new ArrayList<Integer>();

        public SparseHistogramSample(String histogramName) {
            super(histogramName);
        }

        public void record(int sample) {
            mSamples.add(sample);
        }

        @Override
        protected void commitAndClear() {
            for (Integer sample : mSamples) {
                RecordHistogram.recordSparseSlowlyHistogram(mHistogramName, sample);
            }
            mSamples.clear();
        }
    }

    // Each list item is a pair of the url and where it was added from e.g. from the add to
    // homescreen menu item, an app banner, or unknown. The mapping of int source values to
    // their string names is found in the C++ ShortcutInfo struct.
    private static final List<Pair<String, Integer>> sActivityUrls =
            new ArrayList<Pair<String, Integer>>();
    private static final List<Pair<String, Integer>> sTabUrls =
            new ArrayList<Pair<String, Integer>>();

    private static final List<Long> sWebappHistogramTimes = new ArrayList<Long>();

    /**
     * Records the launch of a standalone Activity for a URL (i.e. a WebappActivity)
     * added from a specific source.
     * @param url URL that kicked off the Activity's creation.
     * @param source integer id of the source from where the URL was added.
     */
    public static void recordHomeScreenLaunchIntoStandaloneActivity(String url, int source) {
        sActivityUrls.add(new Pair<String, Integer>(url, source));
    }

    /**
     * Records the launch of a Tab for a URL (i.e. a Home screen shortcut).
     * @param url URL that kicked off the Tab's creation.
     * @param source integer id of the source from where the URL was added.
     */
    public static void recordHomeScreenLaunchIntoTab(String url, int source) {
        sTabUrls.add(new Pair<String, Integer>(url, source));
    }

    /**
     * Records the time it took to look up from disk whether a MAC is valid during webapp startup.
     * @param time the number of milliseconds it took to finish.
     */
    public static void recordWebappHistogramTimes(long time) {
        sWebappHistogramTimes.add(time);
    }

    /**
     * Calls out to native code to record URLs that have been launched via the Home screen.
     * This intermediate step is necessary because Activity.onCreate() may be called when
     * the native library has not yet been loaded.
     * @param webContents WebContents for the current Tab.
     */
    public static void commitLaunchMetrics(WebContents webContents) {
        for (Pair<String, Integer> item : sActivityUrls) {
            nativeRecordLaunch(true, item.first, item.second, webContents);
        }
        sActivityUrls.clear();

        for (Pair<String, Integer> item : sTabUrls) {
            nativeRecordLaunch(false, item.first, item.second, webContents);
        }
        sTabUrls.clear();

        for (Long time : sWebappHistogramTimes) {
            RecordHistogram.recordTimesHistogram("Android.StrictMode.WebappAuthenticatorMac", time,
                    TimeUnit.MILLISECONDS);
        }
        sWebappHistogramTimes.clear();

        // Record generic cached events.
        for (CachedHistogram event : CachedHistogram.sEvents) event.commitAndClear();
    }

    private static native void nativeRecordLaunch(
            boolean standalone, String url, int source, WebContents webContents);
}
