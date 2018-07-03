// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.dynamicmodule;

import android.os.SystemClock;

import org.chromium.base.metrics.RecordHistogram;

import java.util.concurrent.TimeUnit;

/**
 * Records metrics related to custom tabs dynamic modules.
 */
public final class ModuleMetrics {
    private ModuleMetrics() {}

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
