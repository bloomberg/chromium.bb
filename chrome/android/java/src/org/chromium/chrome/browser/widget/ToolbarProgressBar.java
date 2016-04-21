// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.widget;

import android.animation.TimeAnimator;
import android.animation.TimeAnimator.TimeListener;
import android.content.Context;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.view.ViewGroup;
import android.widget.FrameLayout.LayoutParams;

import org.chromium.base.CommandLine;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.util.ColorUtils;
import org.chromium.components.variations.VariationsAssociatedData;
import org.chromium.ui.UiUtils;
import org.chromium.ui.interpolators.BakedBezierInterpolator;

/**
 * Progress bar for use in the Toolbar view.
 */
public class ToolbarProgressBar extends ClipDrawableProgressBar {

    private static final String ANIMATION_FIELD_TRIAL_NAME = "ProgressBarAnimationAndroid";
    private static final String PROGRESS_BAR_UPDATE_COUNT_HISTOGRAM =
            "Omnibox.ProgressBarUpdateCount";
    private static final String PROGRESS_BAR_BREAK_POINT_UPDATE_COUNT_HISTOGRAM =
            "Omnibox.ProgressBarBreakPointUpdateCount";

    /**
     * Interface for progress bar animation interpolation logics.
     */
    interface AnimationLogic {
        /**
         * Resets internal data. It must be called on every loading start.
         */
        void reset();

        /**
         * Returns interpolated progress for animation.
         *
         * @param targetProgress Actual page loading progress.
         * @param frameTimeSec   Duration since the last call.
         * @param resolution     Resolution of the displayed progress bar. Mainly for rounding.
         */
        float updateProgress(float targetProgress, float frameTimeSec, int resolution);
    }

    // The amount of time in ms that the progress bar has to be stopped before the indeterminate
    // animation starts.
    private static final long ANIMATION_START_THRESHOLD = 1000;

    private static final long PROGRESS_FRAME_TIME_CAP_MS = 50;
    private long mAlphaAnimationDurationMs = 140;
    private long mHidingDelayMs = 100;

    private boolean mIsStarted;
    private float mTargetProgress;
    private int mTargetProgressUpdateCount;
    private AnimationLogic mAnimationLogic;
    private boolean mAnimationInitialized;
    private int mMarginTop;
    private ViewGroup mControlContainer;

    private ToolbarProgressBarAnimatingView mAnimatingView;

    private final Runnable mHideRunnable = new Runnable() {
        @Override
        public void run() {
            animateAlphaTo(0.0f);
        }
    };

    private final Runnable mStartIndeterminate = new Runnable() {
        @Override
        public void run() {
            if (mIsStarted) mAnimatingView.startAnimation();
        }
    };

    private final TimeAnimator mProgressAnimator = new TimeAnimator();
    {
        mProgressAnimator.setTimeListener(new TimeListener() {
            @Override
            public void onTimeUpdate(TimeAnimator animation, long totalTimeMs, long deltaTimeMs) {
                // Cap progress bar animation frame time so that it doesn't jump too much even when
                // the animation is janky.
                float progress = mAnimationLogic.updateProgress(mTargetProgress,
                        Math.max(deltaTimeMs, PROGRESS_FRAME_TIME_CAP_MS) * 0.001f, getWidth());
                ToolbarProgressBar.super.setProgress(progress);

                if (mAnimatingView != null) {
                    int width = getDrawable().getBounds().right - getDrawable().getBounds().left;
                    mAnimatingView.update(progress * width);

                    // If the progress bar was updated, reset the callback that triggers the
                    // indeterminate animation.
                    removeCallbacks(mStartIndeterminate);
                    if (progress == 1.0) {
                        mAnimatingView.cancelAnimation();
                    } else if (!mAnimatingView.isRunning()) {
                        postDelayed(mStartIndeterminate, ANIMATION_START_THRESHOLD);
                    }
                }

                if (getProgress() == mTargetProgress) {
                    if (!mIsStarted) postOnAnimationDelayed(mHideRunnable, mHidingDelayMs);
                    mProgressAnimator.end();
                    return;
                }
            }
        });
    }

    /**
     * Creates a toolbar progress bar.
     *
     * @param context the application environment.
     * @param attrs the xml attributes that should be used to initialize this view.
     */
    public ToolbarProgressBar(Context context, AttributeSet attrs) {
        super(context, attrs);
        setAlpha(0.0f);
    }

    /**
     * Prepare the progress bar for being attached to the window.
     * @param toolbarHeight The height of the toolbar.
     */
    public void prepareForAttach(int toolbarHeight) {
        LayoutParams curParams = new LayoutParams(getLayoutParams());
        mMarginTop = toolbarHeight - curParams.height;
        curParams.topMargin = mMarginTop;
        setLayoutParams(curParams);
    }

    /**
     * @param container This View's container.
     */
    public void setControlContainer(ViewGroup container) {
        mControlContainer = container;
    }

    @Override
    public void setAlpha(float alpha) {
        super.setAlpha(alpha);
        if (mAnimatingView != null) mAnimatingView.setAlpha(alpha);
    }

    /**
     * Initializes animation based on command line configuration. This must be called when native
     * library is ready.
     */
    public void initializeAnimation() {
        if (mAnimationInitialized) return;
        mAnimationInitialized = true;

        assert mAnimationLogic == null;

        String animation = CommandLine.getInstance().getSwitchValue(
                ChromeSwitches.PROGRESS_BAR_ANIMATION);
        if (TextUtils.isEmpty(animation)) {
            animation = VariationsAssociatedData.getVariationParamValue(
                    ANIMATION_FIELD_TRIAL_NAME, ChromeSwitches.PROGRESS_BAR_ANIMATION);
        }

        if (TextUtils.equals(animation, "smooth")) {
            mAnimationLogic = new ProgressAnimationSmooth();
        } else if (TextUtils.equals(animation, "smooth-indeterminate")) {
            mAnimationLogic = new ProgressAnimationSmooth();

            LayoutParams animationParams = new LayoutParams(getLayoutParams());
            animationParams.width = 1;
            animationParams.topMargin = mMarginTop;

            mAnimatingView = new ToolbarProgressBarAnimatingView(getContext(), animationParams);
            mAnimatingView.setColor(ColorUtils.getProgressBarAnimationColor(getForegroundColor()));
            UiUtils.insertAfter(mControlContainer, mAnimatingView, this);
        } else if (TextUtils.equals(animation, "fast-start")) {
            mAnimationLogic = new ProgressAnimationFastStart();
        } else if (TextUtils.equals(animation, "linear")) {
            mAnimationLogic = new ProgressAnimationLinear();
        } else {
            assert TextUtils.isEmpty(animation) || TextUtils.equals(animation, "disabled");
        }
    }

    /**
     * Start showing progress bar animation.
     */
    public void start() {
        mIsStarted = true;

        if (mAnimatingView != null) {
            removeCallbacks(mStartIndeterminate);
            postDelayed(mStartIndeterminate, ANIMATION_START_THRESHOLD);
        }

        mTargetProgressUpdateCount = 0;
        resetProgressUpdateCount();
        super.setProgress(0.0f);
        if (mAnimationLogic != null) mAnimationLogic.reset();
        removeCallbacks(mHideRunnable);
        animateAlphaTo(1.0f);
    }

    /**
     * Start hiding progress bar animation.
     * @param delayed Whether a delayed fading out animation should be posted.
     */
    public void finish(boolean delayed) {
        mIsStarted = false;

        if (delayed) {
            updateVisibleProgress();
            RecordHistogram.recordCount1000Histogram(PROGRESS_BAR_UPDATE_COUNT_HISTOGRAM,
                    getProgressUpdateCount());
            RecordHistogram.recordCount100Histogram(
                    PROGRESS_BAR_BREAK_POINT_UPDATE_COUNT_HISTOGRAM,
                    mTargetProgressUpdateCount);
        } else {
            removeCallbacks(mHideRunnable);
            animate().cancel();
            if (mAnimatingView != null) {
                removeCallbacks(mStartIndeterminate);
                mAnimatingView.cancelAnimation();
            }
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
        float alphaDiff = targetAlpha - getAlpha();
        if (alphaDiff == 0.0f) return;

        long duration = (long) Math.abs(alphaDiff * mAlphaAnimationDurationMs);

        BakedBezierInterpolator interpolator = BakedBezierInterpolator.FADE_IN_CURVE;
        if (alphaDiff < 0) interpolator = BakedBezierInterpolator.FADE_OUT_CURVE;

        animate().alpha(targetAlpha)
                .setDuration(duration)
                .setInterpolator(interpolator);

        if (mAnimatingView != null) {
            mAnimatingView.animate().alpha(targetAlpha)
                    .setDuration(duration)
                    .setInterpolator(interpolator);
        }
    }

    private void updateVisibleProgress() {
        if (mAnimationLogic == null) {
            super.setProgress(mTargetProgress);
            if (!mIsStarted) postOnAnimationDelayed(mHideRunnable, mHidingDelayMs);
        } else {
            if (!mProgressAnimator.isStarted()) mProgressAnimator.start();
        }
    }

    // ClipDrawableProgressBar implementation.

    @Override
    public void setProgress(float progress) {
        assert mIsStarted;
        if (mTargetProgress == progress) return;

        mTargetProgressUpdateCount += 1;
        mTargetProgress = progress;
        updateVisibleProgress();
    }

    @Override
    public void setVisibility(int visibility) {
        super.setVisibility(visibility);
        if (mAnimatingView != null) mAnimatingView.setVisibility(visibility);
    }

    @Override
    public void setForegroundColor(int color) {
        super.setForegroundColor(color);
        // Set the animation color to a faded version of the input color.
        if (mAnimatingView != null) {
            mAnimatingView.setColor(ColorUtils.getProgressBarAnimationColor(color));
        }
    }
}
