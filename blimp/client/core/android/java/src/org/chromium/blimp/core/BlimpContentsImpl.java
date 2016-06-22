// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.blimp.core;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.blimp.core_public.BlimpContents;
import org.chromium.blimp.core_public.BlimpContentsObserver;
import org.chromium.blimp.core_public.BlimpNavigationController;

/**
 * BlimpContentsImpl is a Java wrapper to allow communicating with the native BlimpContentsImpl
 * object.
 */
@JNINamespace("blimp::client")
public class BlimpContentsImpl implements BlimpContents {
    @CalledByNative
    private static BlimpContentsImpl create(
            long nativeBlimpContentsImplAndroid, BlimpNavigationController navigationController) {
        return new BlimpContentsImpl(nativeBlimpContentsImplAndroid, navigationController);
    }

    private long mNativeBlimpContentsImplAndroid;

    // Given the importance of the navigation controller, this member is kept directly in Java to
    // ensure that calls to getNavigationController() can be completed with no JNI-hop.
    private BlimpNavigationController mBlimpNavigationController;

    // The BlimpContentsObserverProxy is lazily created when the first observer is added. It is
    // used instead of directly having an ObserverList in this class to ensure there is only a
    // single JNI hop for each call to observers.
    private BlimpContentsObserverProxy mObserverProxy;

    private BlimpContentsImpl(
            long nativeBlimpContentsImplAndroid, BlimpNavigationController navigationController) {
        mNativeBlimpContentsImplAndroid = nativeBlimpContentsImplAndroid;
        mBlimpNavigationController = navigationController;
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
    public BlimpNavigationController getNavigationController() {
        return mBlimpNavigationController;
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

    private native void nativeDestroy(long nativeBlimpContentsImplAndroid);
}
