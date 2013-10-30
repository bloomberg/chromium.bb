// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.mojo_shell_apk;

import android.content.Context;

import org.chromium.base.JNINamespace;

@JNINamespace("mojo")
public class MojoMain {
    /**
     * Initializes the native system.
     **/
    public static void init(Context context) {
        nativeInit(context);
    }

    /**
     * Starts the specified application in the specified context.
     **/
    public static void start(Context context, String appUrl) {
        nativeStart(context, appUrl);
    }

    private static native void nativeInit(Context context);
    private static native void nativeStart(Context context, String appUrl);
};
