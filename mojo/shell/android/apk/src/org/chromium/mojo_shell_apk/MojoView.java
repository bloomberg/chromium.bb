// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.mojo_shell_apk;

import android.content.Context;
import android.util.AttributeSet;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;

import org.chromium.base.JNINamespace;

@JNINamespace("mojo")
public class MojoView extends SurfaceView {

    private int mNativeMojoView;
    private final SurfaceHolder.Callback mSurfaceCallback;

    public MojoView(Context context, AttributeSet attrs) {
        super(context, attrs);

        mNativeMojoView = nativeInit();
        assert mNativeMojoView != 0;

        mSurfaceCallback = new SurfaceHolder.Callback() {
            @Override
            public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
                assert mNativeMojoView != 0;
                nativeSurfaceSetSize(mNativeMojoView, width, height);
            }

            @Override
            public void surfaceCreated(SurfaceHolder holder) {
                assert mNativeMojoView != 0;
                nativeSurfaceCreated(mNativeMojoView, holder.getSurface());
            }

            @Override
            public void surfaceDestroyed(SurfaceHolder holder) {
                assert mNativeMojoView != 0;
                nativeSurfaceDestroyed(mNativeMojoView);
            }
        };
        getHolder().addCallback(mSurfaceCallback);

    }

    // TODO(abarth): Someone needs to call destroy at some point.
    public void destroy() {
        getHolder().removeCallback(mSurfaceCallback);
        nativeDestroy(mNativeMojoView);
        mNativeMojoView = 0;
    }

    private static native int nativeInit();
    private static native void nativeDestroy(int nativeMojoView);
    private static native void nativeSurfaceCreated(int nativeMojoView, Surface surface);
    private static native void nativeSurfaceDestroyed(int nativeMojoView);
    private static native void nativeSurfaceSetSize(int nativeMojoView, int width, int height);
};
