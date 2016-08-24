// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.blimp.core.contents;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.blimp_public.contents.BlimpNavigationController;

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

    @Override
    public String getTitle() {
        if (mNativeBlimpNavigationControllerImplAndroid == 0) return "";
        return nativeGetTitle(mNativeBlimpNavigationControllerImplAndroid);
    }

    @Override
    public boolean canGoBack() {
        if (mNativeBlimpNavigationControllerImplAndroid == 0) return false;
        return nativeCanGoBack(mNativeBlimpNavigationControllerImplAndroid);
    }

    @Override
    public boolean canGoForward() {
        if (mNativeBlimpNavigationControllerImplAndroid == 0) return false;
        return nativeCanGoForward(mNativeBlimpNavigationControllerImplAndroid);
    }

    @Override
    public void goBack() {
        if (mNativeBlimpNavigationControllerImplAndroid == 0) return;
        nativeGoBack(mNativeBlimpNavigationControllerImplAndroid);
    }

    @Override
    public void goForward() {
        if (mNativeBlimpNavigationControllerImplAndroid == 0) return;
        nativeGoForward(mNativeBlimpNavigationControllerImplAndroid);
    }

    @Override
    public void reload() {
        if (mNativeBlimpNavigationControllerImplAndroid == 0) return;
        nativeReload(mNativeBlimpNavigationControllerImplAndroid);
    }

    private native void nativeGoBack(long nativeBlimpNavigationControllerImplAndroid);
    private native void nativeGoForward(long nativeBlimpNavigationControllerImplAndroid);
    private native void nativeReload(long nativeBlimpNavigationControllerImplAndroid);
    private native boolean nativeCanGoBack(long nativeBlimpNavigationControllerImplAndroid);
    private native boolean nativeCanGoForward(long nativeBlimpNavigationControllerImplAndroid);
    private native void nativeLoadURL(long nativeBlimpNavigationControllerImplAndroid, String url);
    private native String nativeGetURL(long nativeBlimpNavigationControllerImplAndroid);
    private native String nativeGetTitle(long nativeBlimpNavigationControllerImplAndroid);
}
