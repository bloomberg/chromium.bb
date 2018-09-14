// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.list.view;

import android.content.Context;
import android.content.res.TypedArray;
import android.graphics.drawable.Animatable;
import android.graphics.drawable.Drawable;
import android.support.annotation.DrawableRes;
import android.support.annotation.IntDef;
import android.support.annotation.StringRes;
import android.support.annotation.StyleableRes;
import android.support.v7.content.res.AppCompatResources;
import android.support.v7.widget.AppCompatImageButton;
import android.util.AttributeSet;

import org.chromium.chrome.browser.util.MathUtils;
import org.chromium.chrome.download.R;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * A representation of a progress bar that supports (1) an indeterminate state, (2) a determinate
 * state, and (3) running, paused, and retry states.
 *
 * The determinate {@link Drawable} will have it's level set via {@link Drawable#setLevel(int)}
 * based on the progress (0 - 10,000).
 *
 * The indeterminate {@link Drawable} supports {@link Animatable} drawables and the animation will
 * be started/stopped when shown/hidden respectively.
 */
public class CircularProgressView extends AppCompatImageButton {
    /**
     * The value to use with {@link #setProgress(int)} to specify that the indeterminate
     * {@link Drawable} should be used.
     */
    public static final int INDETERMINATE = -1;

    /** The various states this {@link CircularProgressView} can be in. */
    @IntDef({UiState.RUNNING, UiState.PAUSED, UiState.RETRY})
    @Retention(RetentionPolicy.SOURCE)
    public @interface UiState {
        /** This progress bar will look like it is actively running based on the XML drawable. */
        int RUNNING = 0;

        /** This progress bar will look like it is paused based on the XML drawable. */
        int PAUSED = 1;

        /** This progress bar will look like it is able to be retried based on the XML drawable. */
        int RETRY = 2;
    }

    private static final int MAX_LEVEL = 10000;

    private final Drawable mIndeterminateProgress;
    private final Drawable mDeterminateProgress;
    private final Drawable mResumeButtonSrc;
    private final Drawable mPauseButtonSrc;
    private final Drawable mRetryButtonSrc;

    // Tracking this here as the API {@link View#getForeground()} is not available in all supported
    // Android versions.
    private Drawable mForegroundDrawable;

    /**
     * Creates an instance of a {@link CircularProgressView}.
     * @param context  The {@link Context} to use.
     * @param attrs    An {@link AttributeSet} instance.
     */
    public CircularProgressView(Context context, AttributeSet attrs) {
        super(context, attrs);

        TypedArray types =
                context.obtainStyledAttributes(attrs, R.styleable.CircularProgressView, 0, 0);
        try {
            mIndeterminateProgress = getDrawable(
                    context, types, R.styleable.CircularProgressView_indeterminateProgress);
            mDeterminateProgress = getDrawable(
                    context, types, R.styleable.CircularProgressView_determinateProgress);
            mResumeButtonSrc =
                    getDrawable(context, types, R.styleable.CircularProgressView_resumeSrc);
            mPauseButtonSrc =
                    getDrawable(context, types, R.styleable.CircularProgressView_pauseSrc);
            mRetryButtonSrc =
                    getDrawable(context, types, R.styleable.CircularProgressView_retrySrc);
        } finally {
            types.recycle();
        }
    }

    /**
     * Sets the progress of this {@link CircularProgressView} to {@code progress}.  If {@code
     * progress} is {@link #INDETERMINATE} an indeterminate {@link Drawable} will be used.
     * Otherwise the value will be clamped between 0 and 100 and a determinate {@link Drawable} will
     * be used and have it's level set via {@link Drawable#setLevel(int)}.
     *
     * @param progress The progress value (0 to 100 or {@link #INDETERMINATE}) to show.
     */
    public void setProgress(int progress) {
        if (progress == INDETERMINATE) {
            if (mForegroundDrawable != mIndeterminateProgress) {
                setForeground(mIndeterminateProgress);
                if (mIndeterminateProgress instanceof Animatable) {
                    ((Animatable) mIndeterminateProgress).start();
                }
            }
        } else {
            progress = MathUtils.clamp(progress, 0, 100);
            mDeterminateProgress.setLevel(progress * MAX_LEVEL / 100);
            setForeground(mDeterminateProgress);
        }

        // Stop any animation that might have previously been running.
        if (mForegroundDrawable != mIndeterminateProgress) {
            ((Animatable) mIndeterminateProgress).stop();
        }
    }

    /**
     * The state this {@link CircularProgressView} should show.  This can be one of the three
     * UiStates defined above.  This will determine what the action drawable is in the view.
     * @param state The UiState to use.
     */
    public void setState(@UiState int state) {
        Drawable imageDrawable;
        @StringRes
        int contentDescription;
        switch (state) {
            case UiState.RUNNING:
                imageDrawable = mPauseButtonSrc;
                contentDescription = R.string.download_notification_pause_button;
                break;
            case UiState.PAUSED:
                imageDrawable = mResumeButtonSrc;
                contentDescription = R.string.download_notification_resume_button;
                break;
            case UiState.RETRY:
            default:
                imageDrawable = mRetryButtonSrc;
                contentDescription = R.string.download_notification_resume_button;
                break;
        }

        setImageDrawable(imageDrawable);
        setContentDescription(getContext().getText(contentDescription));
    }

    // View implementation.
    @Override
    public void setForeground(Drawable foreground) {
        mForegroundDrawable = foreground;
        super.setForeground(foreground);
    }

    private static final Drawable getDrawable(
            Context context, TypedArray attrs, @StyleableRes int attrId) {
        @DrawableRes
        int resId = attrs.getResourceId(attrId, -1);
        if (resId == -1) return null;
        return AppCompatResources.getDrawable(context, resId);
    }
}