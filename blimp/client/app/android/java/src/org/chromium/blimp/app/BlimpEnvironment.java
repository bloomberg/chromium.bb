// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.blimp.app;

import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.blimp_public.BlimpClientContext;

/**
 * BlimpEnvironment is the core environment required to run Blimp for Android.
 */
@JNINamespace("blimp::client")
public class BlimpEnvironment {
    private static BlimpEnvironment sInstance;

    private long mBlimpEnvironmentNativePtr;

    public static BlimpEnvironment getInstance() {
        ThreadUtils.assertOnUiThread();
        if (sInstance == null) {
            sInstance = new BlimpEnvironment();
        }
        return sInstance;
    }

    private static void clearInstance() {
        ThreadUtils.assertOnUiThread();
        sInstance = null;
    }

    private BlimpEnvironment() {
        mBlimpEnvironmentNativePtr = nativeInit();
    }

    public void destroy() {
        ThreadUtils.assertOnUiThread();
        assert mBlimpEnvironmentNativePtr != 0;
        nativeDestroy(mBlimpEnvironmentNativePtr);
        mBlimpEnvironmentNativePtr = 0;
        clearInstance();
    }

    public BlimpClientContext getBlimpClientContext() {
        return nativeGetBlimpClientContext(mBlimpEnvironmentNativePtr);
    }

    @CalledByNative
    private long getNativePtr() {
        return mBlimpEnvironmentNativePtr;
    }

    private native long nativeInit();
    private native void nativeDestroy(long nativeBlimpEnvironment);
    private native BlimpClientContext nativeGetBlimpClientContext(long nativeBlimpEnvironment);
}
