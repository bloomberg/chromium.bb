// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.widget;

import android.content.Context;
import android.util.AttributeSet;
import android.widget.ProgressBar;

import org.chromium.base.ApiCompatibilityUtils;

/**
 * A progress bar that smoothly animates incremental updates.
 * <p>
 * Consumers of this class need to be aware that calls to {@link #getProgress()} will return
 * the currently visible progress value and not the one set in the last call to
 * {@link #setProgress(int)}.
 */
public class SmoothProgressBar extends ProgressBar {
    private static final int MAX = 100;

    // The amount of time between subsequent progress updates. 16ms is chosen to make 60fps.
    private static final long PROGRESS_UPDATE_DELAY_MS = 16;

    private boolean mIsAnimated = false;
    private int mTargetProgress;

    // Since the progress bar is being animated, the internal progress bar resolution should be
    // at least fine as the width, not MAX. This multiplier will be applied to input progress
    // to convert to a finer scale.
    private int mResolutionMutiplier = 1;

    private Runnable mUpdateProgressRunnable = new Runnable() {
        @Override
        public void run() {
            if (getProgress() == mTargetProgress) return;
            if (!mIsAnimated) {
                setProgressInternal(mTargetProgress);
                return;
            }
            // Every time, the progress bar get's at least 20% closer to mTargetProcess.
            // Add 3 to guarantee progressing even if they only differ by 1.
            setProgressInternal(getProgress() + (mTargetProgress - getProgress() + 3) / 4);
            ApiCompatibilityUtils.postOnAnimationDelayed(
                    SmoothProgressBar.this, this, PROGRESS_UPDATE_DELAY_MS);
        }
    };

    /**
     * Create a new progress bar with range 0...100 and initial progress of 0.
     * @param context the application environment.
     * @param attrs the xml attributes that should be used to initialize this view.
     */
    public SmoothProgressBar(Context context, AttributeSet attrs) {
        super(context, attrs);
        setMax(MAX * mResolutionMutiplier);
    }

    @Override
    protected void onSizeChanged(int w, int h, int oldw, int oldh) {
        super.onSizeChanged(w, h, oldw, oldh);

        int normalizedProgress = getProgress() / mResolutionMutiplier;

        // Choose an integer resolution multiplier that makes the scale at least fine as the width.
        mResolutionMutiplier = Math.max(1, (w + MAX - 1) / MAX);
        setMax(mResolutionMutiplier * MAX);
        setProgressInternal(normalizedProgress * mResolutionMutiplier);
    }

    @Override
    public void setProgress(int progress) {
        final int targetProgress = progress * mResolutionMutiplier;
        if (mTargetProgress == targetProgress) return;
        mTargetProgress = targetProgress;
        removeCallbacks(mUpdateProgressRunnable);
        ApiCompatibilityUtils.postOnAnimation(this, mUpdateProgressRunnable);
    }

    /**
     * Sets whether to animate incremental updates or not.
     * @param isAnimated True if it is needed to animate incremental updates.
     */
    public void setAnimated(boolean isAnimated) {
        mIsAnimated = isAnimated;
    }

    /**
     * Called to update the progress visuals.
     * @param progress The progress value to set the visuals to.
     */
    protected void setProgressInternal(int progress) {
        super.setProgress(progress);
    }
}
