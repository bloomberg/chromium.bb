// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.graphics.Canvas;
import android.view.ViewGroup;

import org.chromium.base.VisibleForTesting;
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
public class AwGLFunctor {
    private static final class DestroyRunnable implements Runnable {
        private final long mNativeAwGLFunctor;
        private final Runnable mNativeDrawGLFunctorDestroyRunnable;

        private DestroyRunnable(
                long nativeAwGLFunctor, Runnable nativeDrawGLFunctorDestroyRunnable) {
            mNativeAwGLFunctor = nativeAwGLFunctor;
            mNativeDrawGLFunctorDestroyRunnable = nativeDrawGLFunctorDestroyRunnable;
        }
        @Override
        public void run() {
            mNativeDrawGLFunctorDestroyRunnable.run();
            nativeDestroy(mNativeAwGLFunctor);
        }
    }

    private final long mNativeAwGLFunctor;
    // Same gc-life time as this, but does not reference any members like |mContainerView|.
    private final Object mLifetimeObject;
    private final CleanupReference mCleanupReference;
    private final AwContents.NativeDrawGLFunctor mNativeDrawGLFunctor;
    private final ViewGroup mContainerView;
    private final Runnable mFunctorReleasedCallback;

    public AwGLFunctor(AwContents.NativeDrawGLFunctorFactory nativeDrawGLFunctorFactory,
            ViewGroup containerView) {
        mNativeAwGLFunctor = nativeCreate(this);
        mNativeDrawGLFunctor = nativeDrawGLFunctorFactory.createFunctor(getAwDrawGLViewContext());
        mLifetimeObject = new Object();
        mCleanupReference = new CleanupReference(mLifetimeObject,
                new DestroyRunnable(mNativeAwGLFunctor, mNativeDrawGLFunctor.getDestroyRunnable()));
        mContainerView = containerView;
        if (mNativeDrawGLFunctor.supportsDrawGLFunctorReleasedCallback()) {
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
        return mNativeDrawGLFunctor.requestDrawGL(canvas, mFunctorReleasedCallback);
    }

    @CalledByNative
    private boolean requestInvokeGL(boolean waitForCompletion) {
        return mNativeDrawGLFunctor.requestInvokeGL(mContainerView, waitForCompletion);
    }

    @CalledByNative
    private void detachFunctorFromView() {
        mNativeDrawGLFunctor.detach(mContainerView);
        mContainerView.invalidate();
    }

    public void deleteHardwareRenderer() {
        nativeDeleteHardwareRenderer(mNativeAwGLFunctor);
    }

    public long getAwDrawGLViewContext() {
        return nativeGetAwDrawGLViewContext(mNativeAwGLFunctor);
    }

    /**
     * Intended for test code.
     * @return the number of native instances of this class.
     */
    @VisibleForTesting
    public static int getNativeInstanceCount() {
        return nativeGetNativeInstanceCount();
    }

    private native void nativeDeleteHardwareRenderer(long nativeAwGLFunctor);
    private native long nativeGetAwDrawGLViewContext(long nativeAwGLFunctor);

    private static native long nativeGetAwDrawGLFunction();
    private static native void nativeDestroy(long nativeAwGLFunctor);
    private static native long nativeCreate(AwGLFunctor javaProxy);
    private static native int nativeGetNativeInstanceCount();
}
