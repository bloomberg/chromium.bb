// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.accessibility;

import android.annotation.TargetApi;
import android.os.Build;
import android.provider.Settings;

import org.chromium.base.ContextUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.metrics.RecordHistogram;

/**
 * Provides utility methods relating to measuring accessibility state on the current platform (i.e.
 * Android in this case). See content::BrowserAccessibilityStateImpl.
 */
@JNINamespace("content")
public class BrowserAccessibilityState {
    @CalledByNative
    @TargetApi(Build.VERSION_CODES.JELLY_BEAN_MR1)
    private static void recordAccessibilityHistograms() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.JELLY_BEAN_MR1) return;

        float durationScale =
                Settings.Global.getFloat(ContextUtils.getApplicationContext().getContentResolver(),
                        Settings.Global.ANIMATOR_DURATION_SCALE, 0);
        RecordHistogram.recordBooleanHistogram(
                "Accessibility.Android.AnimationsEnabled", durationScale != 0.0);
    }
}
