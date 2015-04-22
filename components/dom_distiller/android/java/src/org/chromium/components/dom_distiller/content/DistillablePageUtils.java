// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.dom_distiller.content;

import org.chromium.base.CalledByNative;
import org.chromium.base.JNINamespace;
import org.chromium.content_public.browser.WebContents;

/**
 * Provides access to the native dom_distiller::IsPageDistillable function.
 */
@JNINamespace("dom_distiller::android")
public final class DistillablePageUtils {
    /**
     * Callback for handling the result of isPageDistillable.
     */
    public static interface PageDistillableCallback {
        public void onIsPageDistillableResult(boolean isDistillable);
    }

    public static void isPageDistillable(WebContents webContents, boolean isMobileOptimized,
            PageDistillableCallback callback) {
        nativeIsPageDistillable(webContents, isMobileOptimized, callback);
    }

    @CalledByNative
    private static void callOnIsPageDistillableResult(
            PageDistillableCallback callback, boolean isDistillable) {
        callback.onIsPageDistillableResult(isDistillable);
    }

    private static native void nativeIsPageDistillable(
            WebContents webContents, boolean isMobileOptimized, PageDistillableCallback callback);
}

