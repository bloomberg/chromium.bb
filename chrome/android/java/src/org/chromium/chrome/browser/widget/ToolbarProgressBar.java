// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.widget;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.ObjectAnimator;
import android.content.Context;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.LayerDrawable;
import android.util.AttributeSet;
import android.view.View;

import org.chromium.base.VisibleForTesting;
import org.chromium.ui.interpolators.BakedBezierInterpolator;

/**
 * Progress bar for use in the Toolbar view.
 */
public class ToolbarProgressBar extends SmoothProgressBar {
    private static final long PROGRESS_CLEARING_DELAY_MS = 200;
    private static final int SHOW_HIDE_DURATION_MS = 100;

    private final Runnable mClearLoadProgressRunnable;
    private int mDesiredVisibility;
    private Animator mShowAnimator;
    private Animator mHideAnimator;

    /**
     * Creates a toolbar progress bar.
     * @param context the application environment.
     * @param attrs the xml attributes that should be used to initialize this view.
     */
    public ToolbarProgressBar(Context context, AttributeSet attrs) {
        super(context, attrs);
        // The base constructor will trigger a progress change and alter the expected
        // visibility, so force a visibility change to reset the state.
        setVisibility(VISIBLE);

        mClearLoadProgressRunnable = new Runnable() {
            @Override
            public void run() {
                setProgress(0);
            }
        };

        // Hide the background portion of the system progress bar.
        Drawable progressDrawable = getProgressDrawable();
        if (progressDrawable instanceof LayerDrawable) {
            Drawable progressBackgroundDrawable =
                    ((LayerDrawable) progressDrawable)
                            .findDrawableByLayerId(android.R.id.background);
            if (progressBackgroundDrawable != null) {
                progressBackgroundDrawable.setVisible(false, false);
                progressBackgroundDrawable.setAlpha(0);
            }
        }
    }

    @Override
    public void setSecondaryProgress(int secondaryProgress) {
        super.setSecondaryProgress(secondaryProgress);
        setVisibilityForProgress();
    }

    @Override
    protected void setProgressInternal(int progress) {
        super.setProgressInternal(progress);

        if (progress == getMax()) {
            postDelayed(mClearLoadProgressRunnable, PROGRESS_CLEARING_DELAY_MS);
        }

        setVisibilityForProgress();
    }

    @Override
    public void setVisibility(int v) {
        mDesiredVisibility = v;
        setVisibilityForProgress();
    }

    private void setVisibilityForProgress() {
        if (mDesiredVisibility != VISIBLE) {
            super.setVisibility(mDesiredVisibility);
            return;
        }

        int progress = Math.max(getProgress(), getSecondaryProgress());
        super.setVisibility(progress == 0 ? INVISIBLE : VISIBLE);
    }

    @Override
    protected void onSizeChanged(int w, int h, int oldw, int oldh) {
        super.onSizeChanged(w, h, oldw, oldh);

        // Some versions of Android have a bug where they don't properly update the drawables with
        // the correct bounds.  setProgressDrawable has been overridden to properly push the bounds
        // but on rotation they weren't always being set.  Forcing a bounds update on size changes
        // fixes the problem.
        setProgressDrawable(getProgressDrawable());
    }

    @Override
    protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
        super.onLayout(changed, left, top, right, bottom);
        buildAnimators();
        setPivotY(getHeight());
    }

    @Override
    public void setProgressDrawable(Drawable d) {
        Drawable currentDrawable = getProgressDrawable();

        super.setProgressDrawable(d);

        if (currentDrawable != null && d instanceof LayerDrawable) {
            LayerDrawable ld = (LayerDrawable) d;
            for (int i = 0; i < ld.getNumberOfLayers(); i++) {
                ld.getDrawable(i).setBounds(currentDrawable.getBounds());
            }
        }
    }

    /**
     * @return Whether or not this progress bar has animations running for showing/hiding itself.
     */
    @VisibleForTesting
    boolean isAnimatingForShowOrHide() {
        return (mShowAnimator != null && mShowAnimator.isStarted())
                || (mHideAnimator != null && mHideAnimator.isStarted());
    }

    private void buildAnimators() {
        if (mShowAnimator != null && mShowAnimator.isRunning()) mShowAnimator.end();
        if (mHideAnimator != null && mHideAnimator.isRunning()) mHideAnimator.end();

        mShowAnimator = ObjectAnimator.ofFloat(this, View.SCALE_Y, 0.f, 1.f);
        mShowAnimator.setDuration(SHOW_HIDE_DURATION_MS);
        mShowAnimator.setInterpolator(BakedBezierInterpolator.FADE_IN_CURVE);
        mShowAnimator.addListener(new AnimatorListenerAdapter() {
            @Override
            public void onAnimationStart(Animator animation) {
                setSecondaryProgress(getMax());
            }
        });

        mHideAnimator = ObjectAnimator.ofFloat(this, View.SCALE_Y, 1.f, 0.f);
        mHideAnimator.setDuration(SHOW_HIDE_DURATION_MS);
        mHideAnimator.setInterpolator(BakedBezierInterpolator.FADE_OUT_CURVE);
        mHideAnimator.addListener(new AnimatorListenerAdapter() {
            @Override
            public void onAnimationEnd(Animator animation) {
                setSecondaryProgress(0);
            }
        });
    }

    @Override
    public void setProgress(int progress) {
        // If the show animator has started, the progress bar needs to be tracked as if it is
        // currently showing.  This makes sure we trigger the proper hide animation and cancel the
        // show animation if we show/hide the bar very fast.  See crbug.com/453360.
        boolean isShowing =
                getProgress() > 0 || (mShowAnimator != null && mShowAnimator.isStarted());
        boolean willShow = progress > 0;

        removeCallbacks(mClearLoadProgressRunnable);
        super.setProgress(progress);

        if (isShowing != willShow) {
            if (mShowAnimator == null || mHideAnimator == null) buildAnimators();

            if (mShowAnimator.isRunning()) mShowAnimator.end();
            if (mHideAnimator.isRunning()) mHideAnimator.end();

            if (willShow) {
                mShowAnimator.start();
            } else {
                mHideAnimator.start();
            }
        }
    }
}
