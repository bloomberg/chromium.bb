// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.devtools_bridge;

import org.chromium.base.JNINamespace;

/**
 * Native implementation of session dependency factory on top of C++
 * libjingle API.
 */
@JNINamespace("devtools_bridge::android")
public class SessionDependencyFactoryNative {
    private final long mInstance;

    public SessionDependencyFactoryNative() {
        mInstance = nativeCreateFactory();
        assert mInstance != 0;
    }

    public void dispose() {
        nativeDestroyFactory(mInstance);
    }

    private static native long nativeCreateFactory();
    private static native void nativeDestroyFactory(long ptr);
}
