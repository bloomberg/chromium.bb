// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.mojo;

import android.app.Activity;
import android.content.Context;
import android.view.MotionEvent;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;

import org.chromium.base.CalledByNative;
import org.chromium.base.JNINamespace;

/**
 * Exposes SurfaceView to native code.
 */
@JNINamespace("mojo")
public class PlatformViewportAndroid extends SurfaceView {

    private long mNativeMojoViewport;
    private final SurfaceHolder.Callback mSurfaceCallback;

    @SuppressWarnings("unused")
    @CalledByNative
    public static void createForActivity(Activity activity, long nativeViewport) {
        activity.setContentView(new PlatformViewportAndroid(activity, nativeViewport));
    }

    public PlatformViewportAndroid(Context context, long nativeViewport) {
        super(context);

        mNativeMojoViewport = nativeViewport;
        assert mNativeMojoViewport != 0;

        mSurfaceCallback = new SurfaceHolder.Callback() {
            @Override
            public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
                assert mNativeMojoViewport != 0;
                nativeSurfaceSetSize(mNativeMojoViewport, width, height);
            }

            @Override
            public void surfaceCreated(SurfaceHolder holder) {
                assert mNativeMojoViewport != 0;
                nativeSurfaceCreated(mNativeMojoViewport, holder.getSurface());
            }

            @Override
            public void surfaceDestroyed(SurfaceHolder holder) {
                assert mNativeMojoViewport != 0;
                nativeSurfaceDestroyed(mNativeMojoViewport);
            }
        };
        getHolder().addCallback(mSurfaceCallback);

    }

    // TODO(abarth): Someone needs to call destroy at some point.
    public void destroy() {
        getHolder().removeCallback(mSurfaceCallback);
        nativeDestroy(mNativeMojoViewport);
        mNativeMojoViewport = 0;
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        return nativeTouchEvent(mNativeMojoViewport,
                                event.getPointerId(0),
                                event.getAction(),
                                event.getX(), event.getY(),
                                event.getEventTime());
    }

    private static native void nativeDestroy(long nativePlatformViewportAndroid);
    private static native void nativeSurfaceCreated(
        long nativePlatformViewportAndroid, Surface surface);
    private static native void nativeSurfaceDestroyed(
        long nativePlatformViewportAndroid);
    private static native void nativeSurfaceSetSize(
        long nativePlatformViewportAndroid,
        int width, int height);
    private static native boolean nativeTouchEvent(
        long nativePlatformViewportAndroid,
        int pointerId,
        int action,
        float x, float y,
        long timeMs);
};
