// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;

import android.graphics.Canvas;
import android.os.Build;
import android.view.View;

import com.android.webview.chromium.WebViewDelegateFactory.WebViewDelegate;

import org.chromium.content.common.CleanupReference;

/**
 * Simple Java abstraction and wrapper for the native DrawGLFunctor flow.
 * An instance of this class can be constructed, bound to a single view context (i.e. AwContennts)
 * and then drawn and detached from the view tree any number of times (using requestDrawGL and
 * detach respectively).
 */
class DrawGLFunctor {
    private static final String TAG = DrawGLFunctor.class.getSimpleName();

    // Pointer to native side instance
    private CleanupReference mCleanupReference;
    private DestroyRunnable mDestroyRunnable;
    private final long mNativeDrawGLFunctor;
    private WebViewDelegate mWebViewDelegate;
    View mContainerView;

    public DrawGLFunctor(long viewContext, WebViewDelegate webViewDelegate) {
        mNativeDrawGLFunctor = nativeCreateGLFunctor(viewContext);
        mDestroyRunnable = new DestroyRunnable(mNativeDrawGLFunctor);
        mCleanupReference = new CleanupReference(this, mDestroyRunnable);
        mWebViewDelegate = webViewDelegate;
    }

    public void detach() {
        if (mWebViewDelegate != null && mContainerView != null) {
            mWebViewDelegate.detachDrawGlFunctor(mContainerView, mNativeDrawGLFunctor);
        }
    }

    private static final boolean sSupportFunctorReleasedCallback =
            (Build.VERSION.SDK_INT > Build.VERSION_CODES.M) || "N".equals(Build.VERSION.CODENAME);
    public boolean requestDrawGL(Canvas canvas, View containerView, boolean waitForCompletion,
            Runnable releasedCallback) {
        if (mDestroyRunnable.mNativeDrawGLFunctor == 0) {
            throw new RuntimeException("requested DrawGL on already destroyed DrawGLFunctor");
        }

        if (canvas != null && waitForCompletion) {
            throw new IllegalArgumentException(
                    "requested a blocking DrawGL with a not null canvas.");
        }

        if (!mWebViewDelegate.canInvokeDrawGlFunctor(containerView)) {
            return false;
        }

        mContainerView = containerView;

        if (canvas == null) {
            assert releasedCallback == null;
            mWebViewDelegate.invokeDrawGlFunctor(
                    containerView, mDestroyRunnable.mNativeDrawGLFunctor, waitForCompletion);
            return true;
        }

        if (sSupportFunctorReleasedCallback) {
            assert releasedCallback != null;
            mWebViewDelegate.callDrawGlFunction(
                    canvas, mDestroyRunnable.mNativeDrawGLFunctor, releasedCallback);
        } else {
            assert releasedCallback == null;
            mWebViewDelegate.callDrawGlFunction(canvas, mDestroyRunnable.mNativeDrawGLFunctor);
        }
        return true;
    }

    public static boolean supportsDrawGLFunctorReleasedCallback() {
        return sSupportFunctorReleasedCallback;
    }

    public static void setChromiumAwDrawGLFunction(long functionPointer) {
        nativeSetChromiumAwDrawGLFunction(functionPointer);
    }

    // Holds the core resources of the class, everything required to correctly cleanup.
    // IMPORTANT: this class must not hold any reference back to the outer DrawGLFunctor
    // instance, as that will defeat GC of that object.
    private static final class DestroyRunnable implements Runnable {
        private long mNativeDrawGLFunctor;
        DestroyRunnable(long nativeDrawGLFunctor) {
            mNativeDrawGLFunctor = nativeDrawGLFunctor;
            assert mNativeDrawGLFunctor != 0;
        }

        // Called when the outer DrawGLFunctor instance has been GC'ed, i.e this is its finalizer.
        @Override
        public void run() {
            assert mNativeDrawGLFunctor != 0;
            nativeDestroyGLFunctor(mNativeDrawGLFunctor);
            mNativeDrawGLFunctor = 0;
        }
    }

    private static native long nativeCreateGLFunctor(long viewContext);
    private static native void nativeDestroyGLFunctor(long functor);
    private static native void nativeSetChromiumAwDrawGLFunction(long functionPointer);
}
