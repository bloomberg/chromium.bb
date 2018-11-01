// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.ArgbEvaluator;
import android.animation.ObjectAnimator;
import android.animation.ValueAnimator;
import android.util.Property;
import android.view.View;

import org.chromium.chrome.browser.compositor.layouts.ChromeAnimation;
import org.chromium.chrome.browser.widget.MaterialProgressBar;

import java.util.ArrayDeque;
import java.util.Queue;

/**
 * Wrapper around {@link MaterialProgressBar} to animate progress changes and enable/disable
 * pulsing.
 */
public class AnimatedProgressBar {
    // TODO(806868): Move these properties into MaterialProgressBar.java.
    private static final Property<MaterialProgressBar, Integer> PROGRESS_PROPERTY =
            new Property<MaterialProgressBar, Integer>(Integer.class, "progress") {
                @Override
                public Integer get(MaterialProgressBar progressBar) {
                    // TODO(806868): Implement get once this property is moved into
                    // MaterialProgressBar.java.
                    throw new UnsupportedOperationException();
                }

                @Override
                public void set(MaterialProgressBar progressBar, Integer progress) {
                    progressBar.setProgress(progress);
                }
            };

    private static final Property<MaterialProgressBar, Integer> COLOR_PROPERTY =
            new Property<MaterialProgressBar, Integer>(Integer.class, "progressColor") {
                @Override
                public Integer get(MaterialProgressBar progressBar) {
                    // TODO(806868): Implement get once this property is moved into
                    // MaterialProgressBar.java.
                    throw new UnsupportedOperationException();
                }

                @Override
                public void set(MaterialProgressBar progressBar, Integer progressColor) {
                    progressBar.setProgressColor(progressColor);
                }
            };

    // The number of ms the progress bar would take to go from 0 to 100%.
    private static final int PROGRESS_BAR_SPEED_MS = 3_000;
    private static final int PROGRESS_BAR_PULSING_DURATION_MS = 1_000;

    private final MaterialProgressBar mProgressBar;
    private final int mNormalColor;
    private final int mPulsedColor;

    private boolean mIsRunningProgressAnimation = false;
    private int mLastProgress = 0;
    private Queue<ObjectAnimator> mPendingIncreaseAnimations = new ArrayDeque<>();
    private ObjectAnimator mPulseAnimation = null;

    public AnimatedProgressBar(MaterialProgressBar progressBar, int normalColor, int pulsedColor) {
        mProgressBar = progressBar;
        mNormalColor = normalColor;
        mPulsedColor = pulsedColor;
    }

    public void show() {
        mProgressBar.setVisibility(View.VISIBLE);
    }

    public void hide() {
        mProgressBar.setVisibility(View.INVISIBLE);
    }

    /**
     * Set the progress to {@code progress} if it is higher than the current progress, or do nothing
     * if it is not (hence it is OK to call this method with the same value multiple times).
     */
    public void maybeIncreaseProgress(int progress) {
        if (progress > mLastProgress) {
            ObjectAnimator progressAnimation =
                    ObjectAnimator.ofInt(mProgressBar, PROGRESS_PROPERTY, mLastProgress, progress);
            progressAnimation.setDuration(PROGRESS_BAR_SPEED_MS * (progress - mLastProgress) / 100);
            progressAnimation.setInterpolator(ChromeAnimation.getAccelerateInterpolator());
            progressAnimation.addListener(new AnimatorListenerAdapter() {
                @Override
                public void onAnimationEnd(Animator animation) {
                    if (mPendingIncreaseAnimations.isEmpty()) {
                        mIsRunningProgressAnimation = false;
                    } else {
                        mIsRunningProgressAnimation = true;
                        mPendingIncreaseAnimations.poll().start();
                    }
                }
            });
            mLastProgress = progress;

            if (mIsRunningProgressAnimation) {
                mPendingIncreaseAnimations.offer(progressAnimation);
            } else {
                mIsRunningProgressAnimation = true;
                progressAnimation.start();
            }
        }
    }

    public void enablePulsing() {
        if (mPulseAnimation == null) {
            mPulseAnimation =
                    ObjectAnimator.ofInt(mProgressBar, COLOR_PROPERTY, mNormalColor, mPulsedColor);
            mPulseAnimation.setDuration(PROGRESS_BAR_PULSING_DURATION_MS);
            mPulseAnimation.setEvaluator(new ArgbEvaluator());
            mPulseAnimation.setRepeatCount(ValueAnimator.INFINITE);
            mPulseAnimation.setRepeatMode(ValueAnimator.REVERSE);
            mPulseAnimation.setInterpolator(ChromeAnimation.getAccelerateInterpolator());
            mPulseAnimation.addListener(new AnimatorListenerAdapter() {
                @Override
                public void onAnimationCancel(Animator animation) {
                    mProgressBar.setProgressColor(mNormalColor);
                }
            });
            mPulseAnimation.start();
        }
    }

    public void disablePulsing() {
        if (mPulseAnimation != null) {
            mPulseAnimation.cancel();
            mPulseAnimation = null;
        }
    }
}
