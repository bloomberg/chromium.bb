// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.router;

import android.content.Context;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

/**
 * Implements the JNI interface called from the C++ Media Router implementation on Android.
 */
@JNINamespace("media_router")
public class ChromeMediaRouter {
    private static final String TAG = "cr.MediaRouter";

    private final long mNativeMediaRouter;

    /**
     * Initializes the media router and its providers.
     */
    @CalledByNative
    public static ChromeMediaRouter create(long nativeMediaRouter, Context context) {
        return new ChromeMediaRouter(nativeMediaRouter, context);
    }

    private ChromeMediaRouter(long nativeMediaRouter, Context context) {
        mNativeMediaRouter = nativeMediaRouter;
    }
}