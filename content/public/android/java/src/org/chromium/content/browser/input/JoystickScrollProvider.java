// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.input;

import android.util.TypedValue;
import android.view.InputDevice;
import android.view.MotionEvent;
import android.view.animation.AnimationUtils;

import org.chromium.base.Log;
import org.chromium.content.browser.ContentViewCore;

/**
 * This class implements auto scrolling and panning for gamepad left joystick motion event.
 */
public class JoystickScrollProvider {
    private static final String TAG = "JoystickScrollProvider";

    private static final float JOYSTICK_SCROLL_FACTOR_MULTIPLIER = 25f;
    // Joystick produces "noise", 0.20f has proven a safe value to
    // remove noise and still allow reasonable input range.
    private static final float JOYSTICK_SCROLL_DEADZONE = 0.2f;
    private static final float SCROLL_FACTOR_FALLBACK = 128f;

    private final ContentViewCore mView;

    private float mScrollVelocityX;
    private float mScrollVelocityY;
    private float mScrollFactor;

    private long mLastAnimateTimeMillis;

    private boolean mAutoScrollActive;

    private boolean mEnabled;

    private Runnable mScrollRunnable;

    /**
     * Constructs a new JoystickScrollProvider.
     *
     * @param contentview The ContentViewCore used to create this.
     */
    public JoystickScrollProvider(ContentViewCore contentView) {
        mView = contentView;
        mAutoScrollActive = false;
        mEnabled = true;
    }

    /**
     * This function enables or disables scrolling through joystick.
     * @param enabled Decides whether joystick scrolling should be
     *                enabled or not.
     */
    public void setEnabled(boolean enabled) {
        mEnabled = enabled;
        if (!enabled) stop();
    }

    /**
     * This function processes motion event and computes new
     * scroll offest in pixels which is propertional to left joystick
     * axes movement.
     * It also starts runnable to scroll content view core equal to
     * scroll offset pixels.
     *
     * @param event Motion event to be processed for scrolling.
     * @return Whether scrolling using left joystick is performed or not.
     */
    public boolean onMotion(MotionEvent event) {
        if (!mEnabled) return false;
        if ((event.getSource() & InputDevice.SOURCE_CLASS_JOYSTICK) == 0) return false;

        computeNewScrollVelocity(event);
        if (mScrollVelocityX == 0 && mScrollVelocityY == 0) {
            stop();
            return false;
        }
        if (mScrollRunnable == null) {
            mScrollRunnable = new Runnable() {
                @Override
                public void run() {
                    animateScroll();
                }
            };
        }
        if (!mAutoScrollActive) {
            mView.getContainerView().postOnAnimation(mScrollRunnable);
            mAutoScrollActive = true;
        }
        return true;
    }

    private void animateScroll() {
        if (!mEnabled || !mView.getContainerView().hasFocus()) {
            stop();
            return;
        }

        final long timeMillis = AnimationUtils.currentAnimationTimeMillis();
        if (mLastAnimateTimeMillis != 0 && timeMillis > mLastAnimateTimeMillis) {
            final long dt = timeMillis - mLastAnimateTimeMillis;
            final float dx = (mScrollVelocityX * dt / 1000.f);
            final float dy = (mScrollVelocityY * dt / 1000.f);
            mView.scrollBy(dx, dy, true);
        }
        assert mAutoScrollActive;
        mLastAnimateTimeMillis = timeMillis;
        mView.getContainerView().postOnAnimation(mScrollRunnable);
    }

    private void stop() {
        mLastAnimateTimeMillis = 0;
        if (mAutoScrollActive) {
            mAutoScrollActive = false;
            mView.getContainerView().removeCallbacks(mScrollRunnable);
        }
    }

    /**
     * Translates joystick axes movement to a scroll velocity.
     */
    private void computeNewScrollVelocity(MotionEvent event) {
        if (mScrollFactor == 0) {
            TypedValue outValue = new TypedValue();
            if (!mView.getContext().getTheme().resolveAttribute(
                        android.R.attr.listPreferredItemHeight, outValue, true)) {
                mScrollFactor = outValue.getDimension(
                        mView.getContext().getResources().getDisplayMetrics());
            } else {
                Log.w(TAG, "Theme attribute listPreferredItemHeight not defined"
                                + "switching to fallback scroll factor ");
                mScrollFactor = SCROLL_FACTOR_FALLBACK
                        * mView.getRenderCoordinates().getDeviceScaleFactor();
            }
        }
        mScrollVelocityX = getFilteredAxisValue(event, MotionEvent.AXIS_X) * mScrollFactor
                * JOYSTICK_SCROLL_FACTOR_MULTIPLIER;
        mScrollVelocityY = getFilteredAxisValue(event, MotionEvent.AXIS_Y) * mScrollFactor
                * JOYSTICK_SCROLL_FACTOR_MULTIPLIER;
    }

    /**
     * Removes noise from joystick motion events.
     */
    private float getFilteredAxisValue(MotionEvent event, int axis) {
        float axisValWithNoise = event.getAxisValue(axis);
        if (axisValWithNoise > JOYSTICK_SCROLL_DEADZONE) {
            return axisValWithNoise - JOYSTICK_SCROLL_DEADZONE;
        } else if (axisValWithNoise < -JOYSTICK_SCROLL_DEADZONE) {
            return axisValWithNoise + JOYSTICK_SCROLL_DEADZONE;
        }
        return 0f;
    }
}
