// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.widget;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.AnimatorSet;
import android.animation.ValueAnimator;
import android.animation.ValueAnimator.AnimatorUpdateListener;
import android.content.Context;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.widget.FrameLayout.LayoutParams;
import android.widget.ImageView;

import org.chromium.ui.base.LocalizationUtils;
import org.chromium.ui.interpolators.BakedBezierInterpolator;

/**
 * An animating ImageView that is drawn on top of the progress bar. This will animate over the
 * current length of the progress bar only if the progress bar is static for some amount of time.
 */
public class ToolbarProgressBarAnimatingView extends ImageView {

    /** The drawable inside this ImageView. */
    private final ColorDrawable mAnimationDrawable;

    /** The amount of time the fast part of the animation should take in ms. */
    private static final int FAST_ANIMATION_DURATION_MS = 900;

    /** The amount of time the slow part of the animation should take in ms. */
    private static final int SLOW_ANIMATION_DURATION_MS = 1600;

    /** The time between animation sequences. */
    private static final int ANIMATION_DELAY_MS = 1000;

    /** The width of the animating bar relative to the current width of the progress bar. */
    private static final float ANIMATING_BAR_SCALE = 0.5f;

    /** Interpolator for enter and exit animation. */
    private final BakedBezierInterpolator mBezier = BakedBezierInterpolator.TRANSFORM_CURVE;

    /** The current width of the progress bar. */
    private float mProgressWidth = 0;

    /** The set of individual animators that constitute the whole animation sequence. */
    private final AnimatorSet mAnimatorSet;

    /** Track if the animation has been canceled. */
    private boolean mIsCanceled;

    /** If the layout is RTL. */
    private boolean mIsRtl;

    /** The update listener for the animation. */
    private ProgressBarUpdateListener mListener;

    /** The last fraction of the animation that was drawn. */
    private float mLastAnimatedFraction;

    /**
     * An animation update listener that moves an ImageView across the progress bar.
     */
    private class ProgressBarUpdateListener implements AnimatorUpdateListener {
        @Override
        public void onAnimationUpdate(ValueAnimator animation) {
            mLastAnimatedFraction = animation.getAnimatedFraction();
            updateAnimation(mLastAnimatedFraction);
        }
    }

    /**
     * @param context The Context for this view.
     * @param height The LayoutParams for this view.
     */
    public ToolbarProgressBarAnimatingView(Context context, LayoutParams layoutParams) {
        super(context);

        setLayoutParams(layoutParams);
        mIsRtl = LocalizationUtils.isLayoutRtl();

        mAnimationDrawable = new ColorDrawable(Color.WHITE);

        setImageDrawable(mAnimationDrawable);

        mListener = new ProgressBarUpdateListener();
        mAnimatorSet = new AnimatorSet();

        ValueAnimator fastAnimation = new ValueAnimator();
        fastAnimation.setFloatValues(0.0f, 1.0f);
        fastAnimation.setDuration(FAST_ANIMATION_DURATION_MS);
        fastAnimation.addUpdateListener(mListener);

        ValueAnimator slowAnimation = new ValueAnimator();
        slowAnimation.setFloatValues(0.0f, 1.0f);
        slowAnimation.setDuration(SLOW_ANIMATION_DURATION_MS);
        slowAnimation.addUpdateListener(mListener);

        mAnimatorSet.playSequentially(fastAnimation, slowAnimation);
        mAnimatorSet.setStartDelay(ANIMATION_DELAY_MS);

        slowAnimation.addListener(new AnimatorListenerAdapter() {
            @Override
            public void onAnimationEnd(Animator a) {
                // Replay the animation if it has not been canceled.
                if (mIsCanceled) return;
                mAnimatorSet.start();
            }
        });
    }

    /**
     * Start the animation if it hasn't been already.
     */
    public void startAnimation() {
        mIsCanceled = false;
        if (!mAnimatorSet.isStarted()) {
            // Reset position.
            setScaleX(0.0f);
            setTranslationX(0.0f);
            mAnimatorSet.start();
            // Fade in to look nice on sites that trigger many loads that end quickly.
            animate().alpha(1.0f)
                    .setDuration(500)
                    .setInterpolator(BakedBezierInterpolator.FADE_IN_CURVE);
        }
    }

    /**
     * Update the animating view.
     * @param animatedFraction The current fraction of completion for the animation.
     */
    private void updateAnimation(float animatedFraction) {
        float bezierProgress = mBezier.getInterpolation(animatedFraction);

        // Left and right bound change based on if the layout is RTL.
        float leftBound = mIsRtl ? -mProgressWidth : 0.0f;
        float rightBound = mIsRtl ? 0.0f : mProgressWidth;

        // Include the width of the animating bar in this computation so it comes from
        // off-screen.
        float animatingWidth = mProgressWidth * ANIMATING_BAR_SCALE;
        float animatorCenter =
                ((mProgressWidth + animatingWidth) * bezierProgress) - animatingWidth / 2.0f;
        if (mIsRtl) animatorCenter *= -1.0f;

        // The left and right x-coordinate of the animating view.
        float animatorRight = animatorCenter + (animatingWidth / 2.0f);
        float animatorLeft = animatorCenter - (animatingWidth / 2.0f);

        // "Clip" the view so it doesn't go past where the progress bar starts or ends.
        if (animatorRight > rightBound) {
            animatingWidth -= Math.abs(animatorRight - rightBound);
            animatorCenter -= Math.abs(animatorRight - rightBound) / 2.0f;
        } else if (animatorLeft < leftBound) {
            animatingWidth -= Math.abs(animatorLeft - leftBound);
            animatorCenter += Math.abs(animatorLeft - leftBound) / 2.0f;
        }

        setScaleX(animatingWidth);
        setTranslationX(animatorCenter);
    }

    /**
     * @return True if the animation is running.
     */
    public boolean isRunning() {
        return mAnimatorSet.isStarted();
    }

    /**
     * Cancel the animation.
     */
    public void cancelAnimation() {
        mIsCanceled = true;
        mAnimatorSet.cancel();
        // Reset position and alpha.
        setScaleX(0.0f);
        setTranslationX(0.0f);
        animate().cancel();
        setAlpha(0.0f);
        mLastAnimatedFraction = 0.0f;
    }

    /**
     * Update info about the progress bar holding this animating block.
     * @param progressWidth The width of the contaiing progress bar.
     */
    public void update(float progressWidth) {
        mProgressWidth = progressWidth;
        updateAnimation(mLastAnimatedFraction);
    }

    /**
     * @param color The Android color that the animating bar should be.
     */
    public void setColor(int color) {
        mAnimationDrawable.setColor(color);
    }
}
