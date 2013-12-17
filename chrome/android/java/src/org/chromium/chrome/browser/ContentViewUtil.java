// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

/**
 * This class provides a way to create the native WebContents required for instantiating a
 * ContentView.
 */
public abstract class ContentViewUtil {
    // Don't instantiate me.
    private ContentViewUtil() {
    }

    /**
     * @return pointer to native WebContents instance, suitable for using with a
     *         (java) ContentViewCore instance.
     */
    public static long createNativeWebContents(boolean incognito) {
        return nativeCreateNativeWebContents(incognito, false);
    }

    /**
     * @return pointer to native WebContents instance, suitable for using with a
     *         (java) ContentViewCore instance.
     */
    public static long createNativeWebContents(boolean incognito, boolean initiallyHidden) {
        return nativeCreateNativeWebContents(incognito, initiallyHidden);
    }

    /**
     * @param webContentsPtr The WebContents reference to be deleted.
     */
    public static void destroyNativeWebContents(long webContentsPtr) {
        nativeDestroyNativeWebContents(webContentsPtr);
    }

    private static native long nativeCreateNativeWebContents(boolean incognito,
            boolean initiallyHidden);
    private static native void nativeDestroyNativeWebContents(long webContentsPtr);
}
