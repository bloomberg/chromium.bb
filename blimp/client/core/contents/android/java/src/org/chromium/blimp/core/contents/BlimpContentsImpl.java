// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.blimp.core.contents;

import android.view.ViewGroup;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.blimp_public.contents.BlimpContents;
import org.chromium.blimp_public.contents.BlimpContentsObserver;
import org.chromium.blimp_public.contents.BlimpNavigationController;

/**
 * BlimpContentsImpl is a Java wrapper to allow communicating with the native BlimpContentsImpl
 * object.
 */
@JNINamespace("blimp::client")
public class BlimpContentsImpl implements BlimpContents {
    @CalledByNative
    private static BlimpContentsImpl create(long nativeBlimpContentsImplAndroid,
            BlimpNavigationController navigationController, BlimpView blimpView) {
        return new BlimpContentsImpl(
                nativeBlimpContentsImplAndroid, navigationController, blimpView);
    }

    // Light blue theme color on Blimp tab.
    private static final int BLIMP_THEME_COLOR = 0xffb3e5fc;

    private long mNativeBlimpContentsImplAndroid;

    // Given the importance of the navigation controller, this member is kept directly in Java to
    // ensure that calls to getNavigationController() can be completed with no JNI-hop.
    private BlimpNavigationController mBlimpNavigationController;

    // The BlimpContentsObserverProxy is lazily created when the first observer is added. It is
    // used instead of directly having an ObserverList in this class to ensure there is only a
    // single JNI hop for each call to observers.
    private BlimpContentsObserverProxy mObserverProxy;

    // The Android View for this BlimpContents.
    private BlimpView mBlimpView;

    private BlimpContentsImpl(long nativeBlimpContentsImplAndroid,
            BlimpNavigationController navigationController, BlimpView blimpView) {
        mNativeBlimpContentsImplAndroid = nativeBlimpContentsImplAndroid;
        mBlimpNavigationController = navigationController;
        mBlimpView = blimpView;
    }

    @CalledByNative
    private void clearNativePtr() {
        mNativeBlimpContentsImplAndroid = 0;
        mBlimpNavigationController = null;
        if (mObserverProxy != null) {
            mObserverProxy.destroy();
            mObserverProxy = null;
        }
    }

    @CalledByNative
    private long getNativePtr() {
        assert mNativeBlimpContentsImplAndroid != 0;
        return mNativeBlimpContentsImplAndroid;
    }

    @Override
    public ViewGroup getView() {
        return mBlimpView;
    }

    @Override
    public BlimpNavigationController getNavigationController() {
        return mBlimpNavigationController;
    }

    // TODO(xingliu): Use the correct theme color for Blimp.
    // crbug.com/644774
    @Override
    public int getThemeColor() {
        return BLIMP_THEME_COLOR;
    }

    @Override
    public void addObserver(BlimpContentsObserver observer) {
        assert mNativeBlimpContentsImplAndroid != 0;
        if (mObserverProxy == null) mObserverProxy = new BlimpContentsObserverProxy(this);
        mObserverProxy.addObserver(observer);
    }

    @Override
    public void removeObserver(BlimpContentsObserver observer) {
        if (mObserverProxy == null) return;
        mObserverProxy.removeObserver(observer);
    }

    @Override
    public void destroy() {
        assert mNativeBlimpContentsImplAndroid != 0;
        nativeDestroy(mNativeBlimpContentsImplAndroid);
    }

    @Override
    public void show() {
        if (mNativeBlimpContentsImplAndroid == 0) return;
        nativeShow(mNativeBlimpContentsImplAndroid);
    }

    @Override
    public void hide() {
        if (mNativeBlimpContentsImplAndroid == 0) return;
        nativeHide(mNativeBlimpContentsImplAndroid);
    }

    private native void nativeDestroy(long nativeBlimpContentsImplAndroid);
    private native void nativeShow(long nativeBlimpContentsImplAndroid);
    private native void nativeHide(long nativeBlimpContentsImplAndroid);
}
