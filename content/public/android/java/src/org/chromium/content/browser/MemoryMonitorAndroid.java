// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.app.ActivityManager;
import android.content.ComponentCallbacks2;
import android.content.Context;
import android.content.res.Configuration;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

/**
 * Android implementation of MemoryMonitor.
 */
@JNINamespace("content")
class MemoryMonitorAndroid {
    private static final ActivityManager.MemoryInfo sMemoryInfo =
            new ActivityManager.MemoryInfo();
    private static ComponentCallbacks2 sCallbacks = null;

    private MemoryMonitorAndroid() {
    }

    /**
     * Get the current MemoryInfo from ActivityManager and invoke the native
     * callback to populate the MemoryInfo.
     *
     * @param context The context of the application.
     * @param outPtr A native output pointer to populate MemoryInfo. This is
     * passed back to the native callback.
     */
    @CalledByNative
    private static void getMemoryInfo(Context context, long outPtr) {
        ActivityManager am = (ActivityManager) context.getSystemService(
                Context.ACTIVITY_SERVICE);
        am.getMemoryInfo(sMemoryInfo);
        nativeGetMemoryInfoCallback(
                sMemoryInfo.availMem, sMemoryInfo.lowMemory,
                sMemoryInfo.threshold, sMemoryInfo.totalMem, outPtr);
    }

    /**
     * Register ComponentCallbacks2 to receive memory pressure signals.
     *
     * @param context The context of the application.
     */
    @CalledByNative
    private static void registerComponentCallbacks(Context context) {
        sCallbacks = new ComponentCallbacks2() {
                @Override
                public void onTrimMemory(int level) {
                    if (level != ComponentCallbacks2.TRIM_MEMORY_UI_HIDDEN) {
                        nativeOnTrimMemory(level);
                    }
                }
                @Override
                public void onLowMemory() {
                    // Don't support old onLowMemory().
                }
                @Override
                public void onConfigurationChanged(Configuration config) {
                }
            };
        context.registerComponentCallbacks(sCallbacks);
    }

    private static native void nativeGetMemoryInfoCallback(
            long availMem, boolean lowMemory,
            long threshold, long totalMem, long outPtr);

    private static native void nativeOnTrimMemory(int level);
}
