// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.blimp.core;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.blimp.core_public.BlimpNavigationController;

/**
 * BlimpNavigationControllerImpl is a Java wrapper to allow communicating with the native
 * BlimpNavigationControllerImpl object.
 */
@JNINamespace("blimp::client")
public class BlimpNavigationControllerImpl implements BlimpNavigationController {
    @CalledByNative
    private static BlimpNavigationControllerImpl create(
            long nativeBlimpNavigationControllerImplAndroid) {
        return new BlimpNavigationControllerImpl(nativeBlimpNavigationControllerImplAndroid);
    }

    private long mNativeBlimpNavigationControllerImplAndroid;

    private BlimpNavigationControllerImpl(long blimpNavigationControllerImplAndroid) {
        mNativeBlimpNavigationControllerImplAndroid = blimpNavigationControllerImplAndroid;
    }

    @CalledByNative
    private void clearNativePtr() {
        mNativeBlimpNavigationControllerImplAndroid = 0;
    }

    @CalledByNative
    private long getNativePtr() {
        assert mNativeBlimpNavigationControllerImplAndroid != 0;
        return mNativeBlimpNavigationControllerImplAndroid;
    }

    @Override
    public void loadUrl(String url) {
        if (mNativeBlimpNavigationControllerImplAndroid == 0) return;
        nativeLoadURL(mNativeBlimpNavigationControllerImplAndroid, url);
    }

    @Override
    public String getUrl() {
        if (mNativeBlimpNavigationControllerImplAndroid == 0) return "";
        return nativeGetURL(mNativeBlimpNavigationControllerImplAndroid);
    }

    private native void nativeLoadURL(long nativeBlimpNavigationControllerImplAndroid, String url);
    private native String nativeGetURL(long nativeBlimpNavigationControllerImplAndroid);
}
