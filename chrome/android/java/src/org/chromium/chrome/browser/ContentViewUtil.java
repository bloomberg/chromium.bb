// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import org.chromium.content.browser.ContentViewCore;
import org.chromium.content_public.browser.WebContents;

/**
 * This class provides a way to create the native WebContents required for instantiating a
 * ContentView.
 */
public abstract class ContentViewUtil {
    // Don't instantiate me.
    private ContentViewUtil() {
    }

    /**
     * A factory method to build a {@link WebContents} object.
     * @param incognito       Whether or not the {@link WebContents} should be built with an
     *                        incognito profile or not.
     * @param initiallyHidden Whether or not the {@link WebContents} should be initially hidden.
     * @return                A newly created {@link WebContents} object.
     */
    public static WebContents createWebContents(boolean incognito, boolean initiallyHidden) {
        return nativeCreateWebContents(incognito, initiallyHidden);
    }

    /**
     * TODO(dtrainor): Remove when this is no longer used.
     * Helper method for getting a {@link WebContents} from a native WebContents pointer.
     * @param webContentsPtr A native WebContents pointer.
     * @return               A {@link WebContents} object that is linked to {@code webContentsPtr}.
     */
    public static WebContents fromNativeWebContents(long webContentsPtr) {
        return nativeGetWebContentsFromNative(webContentsPtr);
    }

    /**
     * TODO(dtrainor): Remove when this is no longer used.
     * Helper method for getting a native WebContents pointer from a {@link WebContents} object.
     * @param webContents A {@link WebContents} object.
     * @return            A native WebContents poniter that is linked to {@code webContents}.
     */
    public static long getNativeWebContentsFromWebContents(WebContents webContents) {
        return nativeGetNativeWebContentsPtr(webContents);
    }

    /**
     * @return pointer to native WebContents instance, suitable for using with a
     *         (java) ContentViewCore instance.
     */
    public static WebContents createWebContentsWithSharedSiteInstance(
            ContentViewCore contentViewCore) {
        return nativeCreateWebContentsWithSharedSiteInstance(contentViewCore);
    }

    private static native WebContents nativeCreateWebContents(boolean incognito,
            boolean initiallyHidden);
    private static native WebContents nativeCreateWebContentsWithSharedSiteInstance(
            ContentViewCore contentViewCore);
    private static native WebContents nativeGetWebContentsFromNative(long webContentsPtr);
    private static native long nativeGetNativeWebContentsPtr(WebContents webContents);
}
