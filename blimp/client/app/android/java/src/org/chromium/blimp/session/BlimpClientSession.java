// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.blimp.session;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

/**
 * The Java representation of a native BlimpClientSession.  This is primarily used to provide access
 * to the native session methods and to facilitate passing a BlimpClientSession object between Java
 * classes with native counterparts.
 */
@JNINamespace("blimp::client")
public class BlimpClientSession {
    private long mNativeBlimpClientSessionAndroidPtr;

    public BlimpClientSession() {
        mNativeBlimpClientSessionAndroidPtr = nativeInit();
    }

    /**
     * Retrieves an assignment and uses it to connect to the engine.
     */
    public void connect() {
        nativeConnect(mNativeBlimpClientSessionAndroidPtr);
    }

    /**
     * Destroys the native BlimpClientSession.  This class should not be used after this is called.
     */
    public void destroy() {
        if (mNativeBlimpClientSessionAndroidPtr == 0) return;

        nativeDestroy(mNativeBlimpClientSessionAndroidPtr);
        mNativeBlimpClientSessionAndroidPtr = 0;
    }

    // Methods that are called by native via JNI.
    @CalledByNative
    private long getNativePtr() {
        assert mNativeBlimpClientSessionAndroidPtr != 0;
        return mNativeBlimpClientSessionAndroidPtr;
    }

    private native long nativeInit();
    private native void nativeConnect(long nativeBlimpClientSessionAndroid);
    private native void nativeDestroy(long nativeBlimpClientSessionAndroid);
}
