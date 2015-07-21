// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.widget;

import android.content.Context;
import android.util.AttributeSet;

import org.chromium.base.VisibleForTesting;
import org.chromium.ui.interpolators.BakedBezierInterpolator;

/**
 * Progress bar for use in the Toolbar view.
 */
public class ToolbarProgressBar extends ClipDrawableProgressBar {
    private long mAlphaAnimationDurationMs = 130;
    private long mHidingDelayMs = 100;

    private boolean mIsStarted;
    private float mTargetAlpha = 0.0f;
    private final Runnable mHideRunnable = new Runnable() {
        @Override
        public void run() {
            animateAlphaTo(0.0f);
        }
    };

    /**
     * Creates a toolbar progress bar.
     *
     * @param context the application environment.
     * @param attrs the xml attributes that should be used to initialize this view.
     */
    public ToolbarProgressBar(Context context, AttributeSet attrs) {
        super(context, attrs);
        setAlpha(mTargetAlpha);
    }

    /**
     * Start showing progress bar animation.
     */
    public void start() {
        mIsStarted = true;
        removeCallbacks(mHideRunnable);
        animateAlphaTo(1.0f);
    }

    /**
     * Start hiding progress bar animation.
     * @param delayed Whether a delayed fading out animation should be posted.
     */
    public void finish(boolean delayed) {
        mIsStarted = false;

        removeCallbacks(mHideRunnable);

        if (delayed) {
            postDelayed(mHideRunnable, mHidingDelayMs);
        } else {
            mTargetAlpha = 0.0f;
            setAlpha(0.0f);
        }
    }

    /**
     * Set alpha show&hide animation duration. This is for faster testing.
     * @param alphaAnimationDurationMs Alpha animation duration in milliseconds.
     */
    @VisibleForTesting
    public void setAlphaAnimationDuration(long alphaAnimationDurationMs) {
        mAlphaAnimationDurationMs = alphaAnimationDurationMs;
    }

    /**
     * Set hiding delay duration. This is for faster testing.
     * @param hidngDelayMs Hiding delay duration in milliseconds.
     */
    @VisibleForTesting
    public void setHidingDelay(long hidngDelayMs) {
        mHidingDelayMs = hidngDelayMs;
    }

    private void animateAlphaTo(float targetAlpha) {
        mTargetAlpha = targetAlpha;
        float alphaDiff = targetAlpha - getAlpha();
        if (alphaDiff != 0.0f) {
            animate().alpha(targetAlpha)
                    .setDuration((long) Math.abs(alphaDiff * mAlphaAnimationDurationMs))
                    .setInterpolator(alphaDiff > 0
                            ? BakedBezierInterpolator.FADE_IN_CURVE
                            : BakedBezierInterpolator.FADE_OUT_CURVE);
        }
    }

    // ClipDrawableProgressBar implementation.

    @Override
    public void setProgress(float progress) {
        assert mIsStarted;
        super.setProgress(progress);
    }
}
