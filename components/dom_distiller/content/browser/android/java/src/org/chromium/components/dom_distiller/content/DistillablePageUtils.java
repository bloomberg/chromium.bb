// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.dom_distiller.content;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
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

    /**
     * Delegate to receive distillability updates.
     */
    public interface PageDistillableDelegate {
        /**
         * Called when the distillability status changes.
         * @param isDistillable Whether the page is distillable.
         * @param isLast Whether the update is the last one for this page.
         * @param isMobileOptimized Whether the page is optimized for mobile. Only valid when
         *                         the heuristics is ADABOOST_MODEL or ALL_ARTICLES.
         */
        void onIsPageDistillableResult(
                boolean isDistillable, boolean isLast, boolean isMobileOptimized);
    }

    public static void setDelegate(WebContents webContents,
            PageDistillableDelegate delegate) {
        nativeSetDelegate(webContents, delegate);
    }

    @CalledByNative
    private static void callOnIsPageDistillableUpdate(PageDistillableDelegate delegate,
            boolean isDistillable, boolean isLast, boolean isMobileOptimized) {
        if (delegate != null) {
            delegate.onIsPageDistillableResult(isDistillable, isLast, isMobileOptimized);
        }
    }

    private static native void nativeSetDelegate(
            WebContents webContents, PageDistillableDelegate delegate);
}
