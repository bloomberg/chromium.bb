// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.blimp.session;

import android.view.View;

import org.chromium.base.annotations.JNINamespace;

/**
 * A Java representation of the native ControlFeature class.  Provides easy access for Java control
 * UI to interact with the native content-lite feature proxy and talk to the engine.
 */
@JNINamespace("blimp::client")
public class TabControlFeature implements View.OnLayoutChangeListener {
    private final float mDpToPx;

    private View mContentAreaView;
    private long mNativeTabControlFeatureAndroidPtr;

    /**
     * Creates an instance of a {@link TabControlFeature}.  This will register with
     * {@code contentAreaView} as a {@link android.view.View.OnLayoutChangeListener} and will
     * unregister when {@link #destroy()} is called.
     * @param blimpClientSession The {@link BlimpClientSession} that contains the content-lite
     *                           features required by the native components of the
     *                           {@link TabControlFeature}.
     * @param contentAreaView    A {@link View} that represents the content area of the screen.
     *                           This is used to notify the engine of the correct size of the web
     *                           content area.
     */
    public TabControlFeature(BlimpClientSession blimpClientSession, View contentAreaView) {
        mContentAreaView = contentAreaView;
        mContentAreaView.addOnLayoutChangeListener(this);
        mDpToPx = mContentAreaView.getContext().getResources().getDisplayMetrics().density;
        mNativeTabControlFeatureAndroidPtr = nativeInit(blimpClientSession);

        // Push down the current size of the content area view.
        nativeOnContentAreaSizeChanged(mNativeTabControlFeatureAndroidPtr,
                mContentAreaView.getWidth(), mContentAreaView.getHeight(), mDpToPx);
    }

    /**
     * Tears down the native counterpart to this class and unregisters any {@link View} listeners.
     * This class should not be used after this.
     */
    public void destroy() {
        if (mContentAreaView != null) {
            mContentAreaView.removeOnLayoutChangeListener(this);
            mContentAreaView = null;
        }

        if (mNativeTabControlFeatureAndroidPtr != 0) {
            nativeDestroy(mNativeTabControlFeatureAndroidPtr);
            mNativeTabControlFeatureAndroidPtr = 0;
        }
    }

    // View.OnLayoutChangeListener implementation.
    @Override
    public void onLayoutChange(View v, int left, int top, int right, int bottom,
            int oldLeft, int oldTop, int oldRight, int oldBottom) {
        if (mNativeTabControlFeatureAndroidPtr == 0) return;
        nativeOnContentAreaSizeChanged(mNativeTabControlFeatureAndroidPtr, right - left,
                bottom - top, mDpToPx);
    }

    private native long nativeInit(BlimpClientSession blimpClientSession);
    private native void nativeDestroy(long nativeTabControlFeatureAndroid);
    private native void nativeOnContentAreaSizeChanged(
            long nativeTabControlFeatureAndroid, int width, int height, float dpToPx);
}
