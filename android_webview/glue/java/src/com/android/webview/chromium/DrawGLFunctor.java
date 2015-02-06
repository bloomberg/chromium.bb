// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;

import android.graphics.Canvas;
import android.view.View;

import com.android.webview.chromium.WebViewDelegateFactory.WebViewDelegate;

import org.chromium.content.common.CleanupReference;

/**
 * Simple Java abstraction and wrapper for the native DrawGLFunctor flow.
 * An instance of this class can be constructed, bound to a single view context (i.e. AwContennts)
 * and then drawn and detached from the view tree any number of times (using requestDrawGL and
 * detach respectively). Then when finished with, it can be explicitly released by calling
 * destroy() or will clean itself up as required via finalizer / CleanupReference.
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

    public void destroy() {
        detach();
        if (mCleanupReference != null) {
            mCleanupReference.cleanupNow();
            mCleanupReference = null;
            mDestroyRunnable = null;
            mWebViewDelegate = null;
            mContainerView = null;
        }
    }

    public void detach() {
        if (mWebViewDelegate != null && mContainerView != null) {
            mWebViewDelegate.detachDrawGlFunctor(mContainerView, mNativeDrawGLFunctor);
        }
    }

    public boolean requestDrawGL(Canvas canvas, View containerView, boolean waitForCompletion) {
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
            mWebViewDelegate.invokeDrawGlFunctor(
                    containerView, mDestroyRunnable.mNativeDrawGLFunctor, waitForCompletion);
            return true;
        }

        mWebViewDelegate.callDrawGlFunction(canvas, mDestroyRunnable.mNativeDrawGLFunctor);
        return true;
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
