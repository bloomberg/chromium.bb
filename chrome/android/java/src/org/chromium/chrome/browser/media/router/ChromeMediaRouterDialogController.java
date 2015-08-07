// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.router;

import android.content.Context;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

/**
 * Implements the JNI interface called from the C++ Media Router dialog controller implementation
 * on Android.
 */
@JNINamespace("media_router")
public class ChromeMediaRouterDialogController {
    private static final String TAG = "cr.MediaRouter";

    private final long mNativeDialogController;

    /**
     * Initializes the media router and its providers.
     */
    @CalledByNative
    public static ChromeMediaRouterDialogController create(
            long nativeDialogController, Context context) {
        return new ChromeMediaRouterDialogController(nativeDialogController, context);
    }

    private ChromeMediaRouterDialogController(long nativeDialogController, Context context) {
        mNativeDialogController = nativeDialogController;
    }
}
