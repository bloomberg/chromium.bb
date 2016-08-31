// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.blimp.core.contents;

import android.content.Context;
import android.view.MotionEvent;
import android.view.View;
import android.widget.FrameLayout;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.ui.base.ViewAndroidDelegate;
import org.chromium.ui.base.WindowAndroid;

/**
 * A {@link View} that will visually represent the Blimp rendered content.
 */
@JNINamespace("blimp::client")
public class BlimpView extends FrameLayout {
    @CalledByNative
    private static BlimpView create(long nativeBlimpView, WindowAndroid window) {
        Context context = window.getContext().get();
        if (context == null) throw new AssertionError("WindowAndroid must have a valid context.");
        return new BlimpView(nativeBlimpView, context);
    }

    /**
     * Pointer to the native JNI bridge.
     */
    private long mNativeBlimpViewPtr;

    /**
     * Builds a new {@link BlimpView}.
     * @param nativeBlimpView the pointer to the native BlimpView.
     * @param context A {@link Context} instance.
     */
    private BlimpView(long nativeBlimpView, Context context) {
        super(context);
        setFocusable(true);
        setFocusableInTouchMode(true);
        mNativeBlimpViewPtr = nativeBlimpView;
    }

    // View overrides.
    @Override
    protected void onSizeChanged(int w, int h, int oldw, int oldh) {
        if (mNativeBlimpViewPtr == 0) return;
        nativeOnSizeChanged(
                mNativeBlimpViewPtr, w, h, getContext().getResources().getDisplayMetrics().density);
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        if (mNativeBlimpViewPtr == 0) return false;

        if (!hasValidTouchEventActionForNative(event)) return false;

        int pointerCount = event.getPointerCount();

        float[] touchMajor = {event.getTouchMajor(), pointerCount > 1 ? event.getTouchMajor(1) : 0};
        float[] touchMinor = {event.getTouchMinor(), pointerCount > 1 ? event.getTouchMinor(1) : 0};

        for (int i = 0; i < 2; i++) {
            if (touchMajor[i] < touchMinor[i]) {
                float tmp = touchMajor[i];
                touchMajor[i] = touchMinor[i];
                touchMinor[i] = tmp;
            }
        }

        // Native returns whether the event was consumed.
        return nativeOnTouchEvent(mNativeBlimpViewPtr, event, event.getEventTime(),
                event.getActionMasked(), pointerCount, event.getHistorySize(),
                event.getActionIndex(), event.getX(), event.getY(),
                pointerCount > 1 ? event.getX(1) : 0, pointerCount > 1 ? event.getY(1) : 0,
                event.getPointerId(0), pointerCount > 1 ? event.getPointerId(1) : -1, touchMajor[0],
                touchMajor[1], touchMinor[0], touchMinor[1], event.getOrientation(),
                pointerCount > 1 ? event.getOrientation(1) : 0,
                event.getAxisValue(MotionEvent.AXIS_TILT),
                pointerCount > 1 ? event.getAxisValue(MotionEvent.AXIS_TILT, 1) : 0,
                event.getRawX(), event.getRawY(), event.getToolType(0),
                pointerCount > 1 ? event.getToolType(1) : MotionEvent.TOOL_TYPE_UNKNOWN,
                event.getButtonState(), event.getMetaState(),
                getContext().getResources().getDisplayMetrics().density);
    }

    @CalledByNative
    public void clearNativePtr() {
        mNativeBlimpViewPtr = 0;
    }

    @CalledByNative
    private ViewAndroidDelegate createViewAndroidDelegate() {
        return ViewAndroidDelegate.createBasicDelegate(this);
    }

    /**
     * This method inspects the event action of a TouchEvent and returns whether it should be
     * passed along to native or not.
     *
     * Only some actions have any effect on gesture detection, and other actions again have no
     * corresponding WebTouchEvent type and may confuse the touch pipeline, so they are ignored
     * completely.
     *
     * @return whether this |eventAction| is a valid touch event action for native code.
     */
    private static boolean hasValidTouchEventActionForNative(MotionEvent event) {
        int eventAction = event.getActionMasked();
        return eventAction == MotionEvent.ACTION_DOWN || eventAction == MotionEvent.ACTION_UP
                || eventAction == MotionEvent.ACTION_CANCEL
                || eventAction == MotionEvent.ACTION_MOVE
                || eventAction == MotionEvent.ACTION_POINTER_DOWN
                || eventAction == MotionEvent.ACTION_POINTER_UP;
    }

    private native void nativeOnSizeChanged(
            long nativeBlimpView, int width, int height, float deviceScaleFactorDpToPx);
    private native boolean nativeOnTouchEvent(long nativeBlimpView, MotionEvent event, long timeMs,
            int action, int pointerCount, int historySize, int actionIndex, float x0, float y0,
            float x1, float y1, int pointerId0, int pointerId1, float touchMajor0,
            float touchMajor1, float touchMinor0, float touchMinor1, float orientation0,
            float orientation1, float tilt0, float tilt1, float rawX, float rawY,
            int androidToolType0, int androidToolType1, int androidButtonState,
            int androidMetaState, float deviceScaleFactorDpToPx);
}
