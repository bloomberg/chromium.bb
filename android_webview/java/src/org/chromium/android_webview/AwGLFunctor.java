// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.view.ViewGroup;

import org.chromium.base.Log;
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
    private static final String TAG = "AwGLFunctor";

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

    private long mNativeAwGLFunctor;
    private CleanupReference mCleanupReference;
    private AwContents.NativeGLDelegate mNativeGLDelegate;
    private ViewGroup mContainerView;

    public AwGLFunctor() {
        mNativeAwGLFunctor = nativeCreate(this);
        mCleanupReference = new CleanupReference(this, new DestroyRunnable(mNativeAwGLFunctor));
    }

    public void onAttachedToWindow(
            AwContents.NativeGLDelegate nativeGLDelegate, ViewGroup containerView) {
        mNativeGLDelegate = nativeGLDelegate;
        mContainerView = containerView;
    }

    public void onDetachedFromWindow() {
        deleteHardwareRenderer();
        mNativeGLDelegate = null;
        mContainerView = null;
    }

    public void destroy() {
        if (mCleanupReference != null) {
            mNativeGLDelegate = null;
            mContainerView = null;
            mCleanupReference.cleanupNow();
            mCleanupReference = null;
            mNativeAwGLFunctor = 0;
        }
    }

    public static long getAwDrawGLFunction() {
        return nativeGetAwDrawGLFunction();
    }

    public long getNativeAwGLFunctor() {
        assert mNativeAwGLFunctor != 0;
        return mNativeAwGLFunctor;
    }

    @CalledByNative
    private boolean requestDrawGL(boolean waitForCompletion) {
        if (mNativeGLDelegate == null) {
            return false;
        }
        return mNativeGLDelegate.requestDrawGL(null, waitForCompletion, mContainerView);
    }

    @CalledByNative
    private void detachFunctorFromView() {
        if (mNativeGLDelegate != null) {
            mNativeGLDelegate.detachGLFunctor();
            mContainerView.invalidate();
        } else {
            Log.w(TAG, "Unable to detach functor from view. Already detached.");
        }
    }

    public void deleteHardwareRenderer() {
        assert mNativeAwGLFunctor != 0;
        nativeDeleteHardwareRenderer(mNativeAwGLFunctor);
    }

    public long getAwDrawGLViewContext() {
        assert mNativeAwGLFunctor != 0;
        return nativeGetAwDrawGLViewContext(mNativeAwGLFunctor);
    }

    private native void nativeDeleteHardwareRenderer(long nativeAwGLFunctor);
    private native long nativeGetAwDrawGLViewContext(long nativeAwGLFunctor);

    private static native long nativeGetAwDrawGLFunction();
    private static native void nativeDestroy(long nativeAwGLFunctor);
    private static native long nativeCreate(AwGLFunctor javaProxy);
}
