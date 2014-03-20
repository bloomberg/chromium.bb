// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.content.Context;
import android.view.accessibility.AccessibilityManager;

import org.chromium.base.CalledByNative;

/**
 * Exposes information about the current accessibility state
 */
public class AccessibilityUtil {
    private AccessibilityUtil() { }

    /**
     * Checks to see that this device has accessibility and touch exploration enabled.
     * @param context A {@link Context} instance.
     * @return        Whether or not accessibility and touch exploration are enabled.
     */
    @CalledByNative
    public static boolean isAccessibilityEnabled(Context context) {
        AccessibilityManager manager = (AccessibilityManager)
                context.getSystemService(Context.ACCESSIBILITY_SERVICE);
        return manager != null && manager.isEnabled() && manager.isTouchExplorationEnabled();
    }
}