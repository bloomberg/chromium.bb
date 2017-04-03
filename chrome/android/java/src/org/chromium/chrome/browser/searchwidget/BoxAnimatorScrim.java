// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.searchwidget;

import android.animation.ValueAnimator;
import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.view.View;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;

/** Draws a scrim and animates a search box moving into place. */
class BoxAnimatorScrim extends View implements ValueAnimator.AnimatorUpdateListener {
    /** Transparency used for the scrim. */
    private static final int BACKGROUND_TRANSPARENCY = 154;

    private final Drawable mCardDrawable;
    private final Rect mAnimationRect = new Rect();
    private final int mBackgroundColor;

    private Rect mSourceRect;
    private Rect mTargetRect;
    private float mInterpolatedValue;

    public BoxAnimatorScrim(Context context, AttributeSet attrs) {
        super(context, attrs);

        mBackgroundColor =
                ApiCompatibilityUtils.getColor(getResources(), R.color.light_normal_color)
                & 0x00ffffff;

        // Mutate to prevent the animation from affecting other places the Drawable is used.
        mCardDrawable = ApiCompatibilityUtils.getDrawable(getResources(), R.drawable.card_single);
        mCardDrawable.mutate();
    }

    /** Sets where the box will animate from and where it will go. */
    public void setAnimationRects(Rect source, Rect target) {
        mSourceRect = source;
        mTargetRect = target;
    }

    /** Returns the last known value of the animation. */
    public float getInterpolatedValue() {
        return mInterpolatedValue;
    }

    /** Set the current value of the animation. */
    public void setInterpolatedValue(float value) {
        mInterpolatedValue = Math.max(0.0f, Math.min(1.0f, value));
        postInvalidateOnAnimation();
    }

    @Override
    public void onDraw(Canvas canvas) {
        // Draw the background.
        int currentAlpha = (int) (BACKGROUND_TRANSPARENCY * mInterpolatedValue);
        int color = (currentAlpha << 24) + mBackgroundColor;
        canvas.drawColor(color);

        // Draw the search box moving into place, if needed.
        if (mSourceRect == null || mTargetRect == null) return;
        if (mInterpolatedValue >= 1.0f) return;

        mAnimationRect.left = (int) interpolate(mSourceRect.left, mTargetRect.left);
        mAnimationRect.right = (int) interpolate(mSourceRect.right, mTargetRect.right);
        mAnimationRect.top = (int) interpolate(mSourceRect.top, mTargetRect.top);
        mAnimationRect.bottom = (int) interpolate(mSourceRect.bottom, mTargetRect.bottom);
        mCardDrawable.setBounds(mAnimationRect);
        mCardDrawable.draw(canvas);
    }

    @Override
    public void onAnimationUpdate(ValueAnimator animation) {
        setInterpolatedValue((Float) animation.getAnimatedValue());
    }

    private float interpolate(float start, float end) {
        return ((1.0f - mInterpolatedValue) * start) + (mInterpolatedValue * end);
    }
}
