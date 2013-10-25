// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.mojo_shell_apk;

import android.content.Context;

import org.chromium.base.JNINamespace;

@JNINamespace("mojo")
public class MojoMain {
    /**
     * Initialize application context in native side.
     **/
    public static void initApplicationContext(Context context) {
        nativeInitApplicationContext(context);
    }

    private static native void nativeInitApplicationContext(Context context);
};
