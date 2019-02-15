// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.CalledByNative;

/**
 * Platform-provided UI constants.
 */
public class UiConstants {
    private static final String TAG = "UiConstants";
    private static final String UI_CONSTANTS_INTERNAL =
            "org.chromium.content.browser.UiConstantsInternal";
    private static UiConstants sInstance;

    private static UiConstants getInstance() {
        ThreadUtils.assertOnUiThread();
        if (sInstance != null) return sInstance;
        try {
            sInstance = (UiConstants) Class.forName(UI_CONSTANTS_INTERNAL).newInstance();
        } catch (ClassNotFoundException | InstantiationException | IllegalAccessException
                | IllegalArgumentException e) {
            Log.w(TAG, "Could not summon UiConstantsInternal", e);
            sInstance = new UiConstants();
        }
        return sInstance;
    }

    @CalledByNative
    private static boolean isFocusRingOutset() {
        return getInstance().isFocusRingOutsetInternal();
    }

    @CalledByNative
    private static boolean hasCustomFocusRingColor() {
        return getInstance().hasCustomFocusRingColorInternal();
    }

    @CalledByNative
    private static int getFocusRingColor() {
        return getInstance().getFocusRingColorInternal();
    }

    @CalledByNative
    private static boolean hasCustomMinimumStrokeWidthForFocusRing() {
        return getInstance().hasCustomMinimumStrokeWidthForFocusRingInternal();
    }

    @CalledByNative
    private static float getMinimumStrokeWidthForFocusRing() {
        return getInstance().getMinimumStrokeWidthForFocusRingInternal();
    }

    protected UiConstants() {}

    protected boolean isFocusRingOutsetInternal() {
        return false;
    }

    protected boolean hasCustomFocusRingColorInternal() {
        return false;
    }

    protected int getFocusRingColorInternal() {
        assert false;
        return 0;
    }

    protected boolean hasCustomMinimumStrokeWidthForFocusRingInternal() {
        return false;
    }

    protected float getMinimumStrokeWidthForFocusRingInternal() {
        assert false;
        return 1.0f;
    }
}
