// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.blimp.app;

import android.content.Context;
import android.graphics.Color;
import android.graphics.Point;
import android.os.Build;
import android.util.AttributeSet;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.view.WindowManager;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.blimp_public.contents.BlimpContents;

/**
 * A {@link View} that will visually represent the Blimp rendered content.  This {@link View} starts
 * a native compositor.
 */
@JNINamespace("blimp::client::app")
public class BlimpContentsDisplay
        extends SurfaceView implements SurfaceHolder.Callback, View.OnLayoutChangeListener {
    private long mNativeBlimpContentsDisplayPtr;

    /**
     * Builds a new {@link BlimpContentsDisplay}.
     * @param context A {@link Context} instance.
     * @param attrs   An {@link AttributeSet} instance.
     */
    public BlimpContentsDisplay(Context context, AttributeSet attrs) {
        super(context, attrs);
        setFocusable(true);
        setFocusableInTouchMode(true);
        addOnLayoutChangeListener(this);
    }

    /**
     * Starts up rendering for this {@link View}.  This will start up the native compositor and will
     * display it's contents.
     * @param blimpContents The {@link BlimpContents} that represents the web contents.
     */
    public void initializeRenderer(BlimpEnvironment blimpEnvironment, BlimpContents blimpContents) {
        assert mNativeBlimpContentsDisplayPtr == 0;

        WindowManager windowManager =
                (WindowManager) getContext().getSystemService(Context.WINDOW_SERVICE);
        Point displaySize = new Point();
        windowManager.getDefaultDisplay().getSize(displaySize);
        Point physicalSize = new Point();
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN_MR1) {
            windowManager.getDefaultDisplay().getRealSize(physicalSize);
        }
        mNativeBlimpContentsDisplayPtr = nativeInit(blimpEnvironment, blimpContents, physicalSize.x,
                physicalSize.y, displaySize.x, displaySize.y);
        getHolder().addCallback(this);
        setBackgroundColor(Color.WHITE);
        setVisibility(VISIBLE);
    }

    /**
     * Stops rendering for this {@link View} and destroys all internal state.  This {@link View}
     * should not be used after this.
     */
    public void destroyRenderer() {
        getHolder().removeCallback(this);
        if (mNativeBlimpContentsDisplayPtr != 0) {
            nativeDestroy(mNativeBlimpContentsDisplayPtr);
            mNativeBlimpContentsDisplayPtr = 0;
        }
    }

    // View.OnLayoutChangeListener implementation.
    @Override
    public void onLayoutChange(View v, int left, int top, int right, int bottom, int oldLeft,
            int oldTop, int oldRight, int oldBottom) {
        if (mNativeBlimpContentsDisplayPtr == 0) return;
        nativeOnContentAreaSizeChanged(mNativeBlimpContentsDisplayPtr, right - left, bottom - top,
                getContext().getResources().getDisplayMetrics().density);
    }

    // SurfaceView overrides.
    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        setZOrderMediaOverlay(true);
        setVisibility(GONE);
    }

    // SurfaceHolder.Callback2 interface.
    @Override
    public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
        if (mNativeBlimpContentsDisplayPtr == 0) return;
        nativeOnSurfaceChanged(
                mNativeBlimpContentsDisplayPtr, format, width, height, holder.getSurface());
    }

    @Override
    public void surfaceCreated(SurfaceHolder holder) {
        if (mNativeBlimpContentsDisplayPtr == 0) return;
        nativeOnSurfaceCreated(mNativeBlimpContentsDisplayPtr);
    }

    @Override
    public void surfaceDestroyed(SurfaceHolder holder) {
        if (mNativeBlimpContentsDisplayPtr == 0) return;
        nativeOnSurfaceDestroyed(mNativeBlimpContentsDisplayPtr);
    }

    @CalledByNative
    public void onSwapBuffersCompleted() {
        if (getBackground() == null) return;

        setBackgroundResource(0);
    }

    // Native Methods
    private native long nativeInit(BlimpEnvironment blimpEnvironment, BlimpContents blimpContents,
            int physicalWidth, int physicalHeight, int displayWidth, int displayHeight);
    private native void nativeDestroy(long nativeBlimpContentsDisplay);
    private native void nativeOnContentAreaSizeChanged(
            long nativeBlimpContentsDisplay, int width, int height, float dpToPx);
    private native void nativeOnSurfaceChanged(
            long nativeBlimpContentsDisplay, int format, int width, int height, Surface surface);
    private native void nativeOnSurfaceCreated(long nativeBlimpContentsDisplay);
    private native void nativeOnSurfaceDestroyed(long nativeBlimpContentsDisplay);
}
