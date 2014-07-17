// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.mojo_shell_apk;

import android.content.Context;

import org.chromium.base.JNINamespace;

/**
 * A placeholder class to call native functions.
 **/
@JNINamespace("mojo")
public class MojoMain {
    /**
     * A guard flag for calling nativeInit() only once.
     **/
    private static boolean sInitialized = false;

    /**
     * Initializes the native system.
     **/
    public static void ensureInitialized(Context context) {
        if (sInitialized)
            return;
        nativeInit(context);
        sInitialized = true;
    }

    /**
     * Starts the specified application in the specified context.
     **/
    public static void start(final String appUrl) {
        nativeStart(appUrl);
    }

    /**
     * Initializes the native system. This API should be called only once per process.
     **/
    private static native void nativeInit(Context context);
    private static native void nativeStart(String appUrl);
};
