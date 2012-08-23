// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

/**
 * This class provides a way to create the native WebContents.
 */
public abstract class AndroidWebViewUtil {
    // TODO(mkosiba): rename to ContentViewUtil when we stop linking against chrome/ (or share the
    // chrome/ ContentViewUtil).
    private AndroidWebViewUtil() {
    }

    /**
     * @return pointer to native WebContents instance, suitable for using with a
     *         (java) ContentViewCore instance.
     */
    public static int createNativeWebContents(boolean incognito) {
        return nativeCreateNativeWebContents(incognito);
    }

    private static native int nativeCreateNativeWebContents(boolean incognito);
}
