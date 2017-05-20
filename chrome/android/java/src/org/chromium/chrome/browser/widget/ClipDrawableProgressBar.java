// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.widget;

import android.content.Context;
import android.graphics.Color;
import android.graphics.Rect;
import android.graphics.drawable.ClipDrawable;
import android.graphics.drawable.ColorDrawable;
import android.support.v4.view.ViewCompat;
import android.view.Gravity;
import android.view.ViewGroup.LayoutParams;
import android.widget.ImageView;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;

/**
 * An alternative progress bar implemented using ClipDrawable for simplicity and performance.
 */
public class ClipDrawableProgressBar extends ImageView {
    /**
     * Structure that has complete {@link ClipDrawableProgressBar} drawing information.
     */
    public static class DrawingInfo {
        public final Rect progressBarRect = new Rect();
        public final Rect progressBarBackgroundRect = new Rect();

        public int progressBarColor;
        public int progressBarBackgroundColor;
    }

    // ClipDrawable's max is a fixed constant 10000.
    // http://developer.android.com/reference/android/graphics/drawable/ClipDrawable.html
    private static final int CLIP_DRAWABLE_MAX = 10000;

    private final ColorDrawable mForegroundDrawable;
    private int mBackgroundColor = Color.TRANSPARENT;
    private float mProgress;
    private int mDesiredVisibility;

    /**
     * Create the progress bar with a custom height.
     * @param context An Android context.
     * @param height The height in px of the progress bar.
     */
    public ClipDrawableProgressBar(Context context, int height) {
        super(context);

        mDesiredVisibility = getVisibility();

        int foregroundColor =
                ApiCompatibilityUtils.getColor(getResources(), R.color.progress_bar_foreground);
        mBackgroundColor =
                ApiCompatibilityUtils.getColor(getResources(), R.color.progress_bar_background);

        mForegroundDrawable = new ColorDrawable(foregroundColor);
        setImageDrawable(
                new ClipDrawable(mForegroundDrawable, Gravity.START, ClipDrawable.HORIZONTAL));
        setBackgroundColor(mBackgroundColor);

        setLayoutParams(new LayoutParams(LayoutParams.MATCH_PARENT, height));
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

        mProgress = progress;
        getDrawable().setLevel(Math.round(progress * CLIP_DRAWABLE_MAX));
    }

    /**
     * @return Foreground color of the progress bar.
     */
    public int getForegroundColor() {
        return mForegroundDrawable.getColor();
    }

    /**
     * Get progress bar drawing information.
     * @param drawingInfoOut An instance that the result will be written.
     */
    public void getDrawingInfo(DrawingInfo drawingInfoOut) {
        int foregroundColor = mForegroundDrawable.getColor();
        float effectiveAlpha = getVisibility() == VISIBLE ? getAlpha() : 0.0f;
        drawingInfoOut.progressBarColor = applyAlpha(foregroundColor, effectiveAlpha);
        drawingInfoOut.progressBarBackgroundColor = applyAlpha(mBackgroundColor, effectiveAlpha);

        if (ViewCompat.getLayoutDirection(this) == LAYOUT_DIRECTION_LTR) {
            drawingInfoOut.progressBarRect.set(
                    getLeft(),
                    getTop(),
                    getLeft() + Math.round(mProgress * getWidth()),
                    getBottom());
            drawingInfoOut.progressBarBackgroundRect.set(
                    drawingInfoOut.progressBarRect.right,
                    getTop(),
                    getRight(),
                    getBottom());
        } else {
            drawingInfoOut.progressBarRect.set(
                    getRight() - Math.round(mProgress * getWidth()),
                    getTop(),
                    getRight(),
                    getBottom());
            drawingInfoOut.progressBarBackgroundRect.set(
                    getLeft(),
                    getTop(),
                    drawingInfoOut.progressBarRect.left,
                    getBottom());
        }
    }

    private void updateInternalVisibility() {
        int oldVisibility = getVisibility();
        int newVisibility = mDesiredVisibility;
        if (getAlpha() == 0 && mDesiredVisibility == VISIBLE) newVisibility = INVISIBLE;
        if (oldVisibility != newVisibility) super.setVisibility(newVisibility);
    }

    private int applyAlpha(int color, float alpha) {
        return (Math.round(alpha * (color >>> 24)) << 24) | (0x00ffffff & color);
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
    }

    /**
     * Sets the color for the foreground (i.e. the moving part) of the progress bar.
     * @param color The new color of the progress bar foreground.
     */
    public void setForegroundColor(int color) {
        mForegroundDrawable.setColor(color);
    }

    @Override
    protected boolean onSetAlpha(int alpha) {
        updateInternalVisibility();
        return super.onSetAlpha(alpha);
    }
}
