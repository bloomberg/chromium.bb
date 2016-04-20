// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.graphics.Canvas;
import android.view.ViewGroup;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.content.common.CleanupReference;

/**
 * Manages state associated with the Android render thread and the draw functor
 * that the WebView uses to render its contents. AwGLFunctor is responsible for
 * managing the lifetime of native RenderThreadManager and HardwareRenderer,
 * ensuring that they continue to exist while the functor remains attached to
 * the render node hierarchy.
 */
@JNINamespace("android_webview")
class AwGLFunctor {
    private static final class DestroyRunnable implements Runnable {
        private final long mNativeAwGLFunctor;

        private DestroyRunnable(long nativeAwGLFunctor) {
            mNativeAwGLFunctor = nativeAwGLFunctor;
        }
        @Override
        public void run() {
            nativeDestroy(mNativeAwGLFunctor);
        }
    }

    private final long mNativeAwGLFunctor;
    // Same gc-life time as this, but does not reference any members like |mContainerView|.
    private final Object mLifetimeObject;
    private final CleanupReference mCleanupReference;
    private final AwContents.NativeGLDelegate mNativeGLDelegate;
    private final ViewGroup mContainerView;
    private final Runnable mFunctorReleasedCallback;

    public AwGLFunctor(AwContents.NativeGLDelegate nativeGLDelegate, ViewGroup containerView) {
        mNativeAwGLFunctor = nativeCreate(this);
        mLifetimeObject = new Object();
        mCleanupReference =
                new CleanupReference(mLifetimeObject, new DestroyRunnable(mNativeAwGLFunctor));
        mNativeGLDelegate = nativeGLDelegate;
        mContainerView = containerView;
        if (mNativeGLDelegate.supportsDrawGLFunctorReleasedCallback()) {
            mFunctorReleasedCallback = new Runnable() {
                @Override
                public void run() {
                    // Deliberate no-op. This Runnable is holding a strong reference back to the
                    // AwGLFunctor, which serves its purpose for now.
                }
            };
        } else {
            mFunctorReleasedCallback = null;
        }
    }

    public static long getAwDrawGLFunction() {
        return nativeGetAwDrawGLFunction();
    }

    public long getNativeAwGLFunctor() {
        return mNativeAwGLFunctor;
    }

    public Object getNativeLifetimeObject() {
        return mLifetimeObject;
    }

    public boolean requestDrawGLForCanvas(Canvas canvas) {
        return mNativeGLDelegate.requestDrawGL(
                canvas, false, mContainerView, mFunctorReleasedCallback);
    }

    @CalledByNative
    private boolean requestDrawGL(boolean waitForCompletion) {
        return mNativeGLDelegate.requestDrawGL(null, waitForCompletion, mContainerView, null);
    }

    @CalledByNative
    private void detachFunctorFromView() {
        mNativeGLDelegate.detachGLFunctor();
        mContainerView.invalidate();
    }

    public void deleteHardwareRenderer() {
        nativeDeleteHardwareRenderer(mNativeAwGLFunctor);
    }

    public long getAwDrawGLViewContext() {
        return nativeGetAwDrawGLViewContext(mNativeAwGLFunctor);
    }

    private native void nativeDeleteHardwareRenderer(long nativeAwGLFunctor);
    private native long nativeGetAwDrawGLViewContext(long nativeAwGLFunctor);

    private static native long nativeGetAwDrawGLFunction();
    private static native void nativeDestroy(long nativeAwGLFunctor);
    private static native long nativeCreate(AwGLFunctor javaProxy);
}
