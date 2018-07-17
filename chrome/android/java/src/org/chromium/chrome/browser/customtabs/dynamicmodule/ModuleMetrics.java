// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.dynamicmodule;

import android.os.SystemClock;
import android.support.annotation.IntDef;

import org.chromium.base.Log;
import org.chromium.base.metrics.RecordHistogram;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.concurrent.TimeUnit;

/**
 * Records metrics related to custom tabs dynamic modules.
 */
public final class ModuleMetrics {
    private ModuleMetrics() {}

    private static final String TAG = "ModuleMetrics";

    /**
     * Possible results when loading a dynamic module. Keep in sync with the
     * CustomTabs.DynamicModule.LoadResult enum in histograms.xml. Do not remove
     * or change existing values other than NUM_ENTRIES.
     */
    @IntDef({LoadResult.SUCCESS_NEW, LoadResult.SUCCESS_CACHED, LoadResult.FEATURE_DISABLED,
            LoadResult.NOT_GOOGLE_SIGNED, LoadResult.PACKAGE_NAME_NOT_FOUND_EXCEPTION,
            LoadResult.CLASS_NOT_FOUND_EXCEPTION, LoadResult.INSTANTIATION_EXCEPTION,
            LoadResult.INCOMPATIBLE_VERSION})
    @Retention(RetentionPolicy.SOURCE)
    public @interface LoadResult {
        /** A new instance of the module was loaded successfully. */
        int SUCCESS_NEW = 0;
        /** A cached instance of the module was used. */
        int SUCCESS_CACHED = 1;
        /** The module could not be loaded because the feature is disabled. */
        int FEATURE_DISABLED = 2;
        /** The module could not be loaded because the package is not Google-signed. */
        int NOT_GOOGLE_SIGNED = 3;
        /** The module could not be loaded because the package name could not be found. */
        int PACKAGE_NAME_NOT_FOUND_EXCEPTION = 4;
        /** The module could not be loaded because the entry point class could not be found. */
        int CLASS_NOT_FOUND_EXCEPTION = 5;
        /** The module could not be loaded because the entry point class could not be instantiated.
         */
        int INSTANTIATION_EXCEPTION = 6;
        /** The module was loaded but the host and module versions are incompatible. */
        int INCOMPATIBLE_VERSION = 7;
        /** Upper bound for legal sample values - all sample values have to be strictly lower. */
        int NUM_ENTRIES = 8;
    }

    /**
     * Records the result of attempting to load a dynamic module.
     * @param result result key, one of {@link LoadResult}'s values.
     */
    public static void recordLoadResult(@LoadResult int result) {
        RecordHistogram.recordEnumeratedHistogram(
                "CustomTabs.DynamicModule.LoadResult", result, LoadResult.NUM_ENTRIES);
        if (result != LoadResult.SUCCESS_NEW && result != LoadResult.SUCCESS_CACHED) {
            Log.w(TAG, "Did not load module, result: %s", result);
        }
    }

    /**
     * SystemClock.uptimeMillis() is used here as it uses the same system call as all the native
     * side of Chrome, and this is the same clock used for page load metrics.
     *
     * @return Milliseconds since boot, not counting time spent in deep sleep.
     */
    public static long now() {
        return SystemClock.uptimeMillis();
    }

    public static void recordCreateActivityDelegateTime(long startTime) {
        RecordHistogram.recordMediumTimesHistogram(
                "CustomTabs.DynamicModule.CreateActivityDelegateTime", now() - startTime,
                TimeUnit.MILLISECONDS);
    }

    public static void recordCreatePackageContextTime(long startTime) {
        RecordHistogram.recordMediumTimesHistogram(
                "CustomTabs.DynamicModule.CreatePackageContextTime", now() - startTime,
                TimeUnit.MILLISECONDS);
    }

    public static void recordLoadClassTime(long startTime) {
        RecordHistogram.recordMediumTimesHistogram(
                "CustomTabs.DynamicModule.EntryPointLoadClassTime", now() - startTime,
                TimeUnit.MILLISECONDS);
    }

    public static void recordEntryPointNewInstanceTime(long startTime) {
        RecordHistogram.recordMediumTimesHistogram(
                "CustomTabs.DynamicModule.EntryPointNewInstanceTime", now() - startTime,
                TimeUnit.MILLISECONDS);
    }
}
