// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.widget;

import android.content.Context;
import android.content.res.TypedArray;
import android.graphics.Color;
import android.graphics.Rect;
import android.graphics.drawable.ClipDrawable;
import android.graphics.drawable.ColorDrawable;
import android.support.v4.view.ViewCompat;
import android.util.AttributeSet;
import android.view.Gravity;
import android.widget.ImageView;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;

/**
 * An alternative progress bar implemented using ClipDrawable for simplicity and performance.
 */
public class ClipDrawableProgressBar extends ImageView {

    // ClipDrawable's max is a fixed constant 10000.
    // http://developer.android.com/reference/android/graphics/drawable/ClipDrawable.html
    private static final int CLIP_DRAWABLE_MAX = 10000;

    private final Rect mDirtyBound = new Rect();
    private InvalidationListener mListener;
    private int mProgressBarColor = Color.TRANSPARENT;
    private int mBackgroundColor = Color.TRANSPARENT;
    private boolean mIsLastInvalidationProgressChange;
    private float mProgress;
    private int mDesiredVisibility;

    /**
     * Interface for listening to drawing invalidation.
     */
    public interface InvalidationListener {
        /**
         * Called on drawing invalidation.
         * @param dirtyRect Invalidated area.
         */
        void onInvalidation(Rect dirtyRect);
    }

    /**
     * Constructor for inflating from XML.
     */
    public ClipDrawableProgressBar(Context context, AttributeSet attrs) {
        super(context, attrs);
        init(context, attrs, 0);
    }

    private void init(Context context, AttributeSet attrs, int defStyle) {
        mDesiredVisibility = getVisibility();

        assert attrs != null;
        TypedArray a = context.obtainStyledAttributes(
                attrs, R.styleable.ClipDrawableProgressBar, defStyle, 0);
        mProgressBarColor = a.getColor(
                R.styleable.ClipDrawableProgressBar_progressBarColor, Color.TRANSPARENT);
        assert mProgressBarColor != Color.TRANSPARENT;
        mBackgroundColor = a.getColor(
                R.styleable.ClipDrawableProgressBar_backgroundColor, Color.TRANSPARENT);
        a.recycle();

        setImageDrawable(new ClipDrawable(new ColorDrawable(mProgressBarColor), Gravity.START,
                ClipDrawable.HORIZONTAL));
        setBackgroundColor(mBackgroundColor);
    }

    /**
     * Get the progress bar's current level of progress.
     *
     * @return The current progress, between 0.0 and 1.0.
     */
    public float getProgress() {
        return mProgress;
    }

    /**
     * Set the current progress to the specified value.
     *
     * @param progress The new progress, between 0.0 and 1.0.
     */
    public void setProgress(float progress) {
        assert 0.0f <= progress && progress <= 1.0f;
        if (mProgress == progress) return;

        float currentProgress = mProgress;
        mProgress = progress;
        getDrawable().setLevel(Math.round(progress * CLIP_DRAWABLE_MAX));

        if (getVisibility() == VISIBLE && mListener != null) {
            if (mIsLastInvalidationProgressChange) {
                float left = Math.min(currentProgress, progress);
                float right = Math.max(currentProgress, progress);

                if (ApiCompatibilityUtils.isLayoutRtl(this)) {
                    float leftTemp = left;
                    left = 1.0f - right;
                    right = 1.0f - leftTemp;
                }

                mDirtyBound.set((int) (left * getWidth()), 0,
                        (int) Math.ceil(right * getWidth()), getHeight());
                mListener.onInvalidation(mDirtyBound);
            } else {
                mListener.onInvalidation(null);
            }

            mIsLastInvalidationProgressChange = true;
        }
    }

    /**
     * @return Background color of this progress bar.
     */
    public int getProgressBarBackgroundColor() {
        return mBackgroundColor;
    }

    /**
     * Adds an observer to be notified of drawing invalidation.
     *
     * @param listener The observer to be added.
     */
    public void setInvalidationListener(InvalidationListener listener) {
        mListener = listener;
        mIsLastInvalidationProgressChange = false;
    }

    /**
     * Get a rect that contains all the non-opaque pixels.
     * @param alphaBound Instance used to return the result.
     */
    public void getAlphaDrawRegion(Rect alphaBound) {
        if (getAlpha() < 1.0f) {
            alphaBound.set(0, 0, getWidth(), getHeight());
            return;
        }

        alphaBound.setEmpty();
        if (Color.alpha(mBackgroundColor) < 255) {
            if (ApiCompatibilityUtils.isLayoutRtl(this)) {
                alphaBound.set(0, 0, (int) Math.ceil(mProgress * getWidth()), getHeight());
            } else {
                alphaBound.set((int) (mProgress * getWidth()), 0, getWidth(), getHeight());
            }
        }
        if (Color.alpha(mProgressBarColor) < 255) {
            if (ApiCompatibilityUtils.isLayoutRtl(this)) {
                alphaBound.union((int) (mProgress * getWidth()), 0, getWidth(), getHeight());
            } else {
                alphaBound.union(0, 0, (int) Math.ceil(mProgress * getWidth()), getHeight());
            }
        }

        return;
    }

    /**
     * @return Whether the actual internal visibility is changed.
     */
    private boolean updateInternalVisibility() {
        int oldVisibility = getVisibility();
        int newVisibility = mDesiredVisibility;
        if (getAlpha() == 0 && mDesiredVisibility == VISIBLE) {
            newVisibility = INVISIBLE;
        }

        if (oldVisibility != newVisibility) {
            super.setVisibility(newVisibility);
            if (mListener != null) {
                mListener.onInvalidation(null);
                mIsLastInvalidationProgressChange = false;
            }
            return true;
        }

        return false;
    }

    // View implementations.

    /**
     * Note that this visibility might not be respected for optimization. For example, if alpha
     * is 0, it will remain View#INVISIBLE even if this is called with View#VISIBLE.
     */
    @Override
    public void setVisibility(int visibility) {
        mDesiredVisibility = visibility;
        updateInternalVisibility();
    }

    @Override
    public void setBackgroundColor(int color) {
        if (color == Color.TRANSPARENT) {
            setBackground(null);
        } else {
            super.setBackgroundColor(color);
        }

        mBackgroundColor = color;

        if (mListener != null && getVisibility() == VISIBLE) {
            mListener.onInvalidation(null);
            mIsLastInvalidationProgressChange = false;
        }
    }

    @Override
    protected boolean onSetAlpha(int alpha) {
        boolean result = super.onSetAlpha(alpha);

        if (mListener != null && !updateInternalVisibility()) {
            mListener.onInvalidation(null);
            mIsLastInvalidationProgressChange = false;
        }

        return result;
    }

    @Override
    protected void onSizeChanged(int w, int h, int oldw, int oldh) {
        super.onSizeChanged(w, h, oldw, oldh);

        if (mListener != null && getVisibility() == VISIBLE && (w != oldw || h != oldh)) {
            mListener.onInvalidation(null);
            mIsLastInvalidationProgressChange = false;
        }
    }

    @Override
    public void onRtlPropertiesChanged(int layoutDirection) {
        int currentLayoutDirection = ViewCompat.getLayoutDirection(this);
        super.onRtlPropertiesChanged(layoutDirection);

        if (mListener != null && getVisibility() == VISIBLE
                && currentLayoutDirection != layoutDirection) {
            mListener.onInvalidation(null);
            mIsLastInvalidationProgressChange = false;
        }
    }
}
