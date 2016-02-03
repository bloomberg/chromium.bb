// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import android.animation.Animator;
import android.animation.ObjectAnimator;
import android.content.res.ColorStateList;
import android.content.res.Resources;
import android.graphics.BitmapFactory;
import android.graphics.Canvas;
import android.graphics.Paint.Align;
import android.graphics.Rect;
import android.graphics.Typeface;
import android.text.TextPaint;
import android.util.Property;

import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.widget.TintedDrawable;
import org.chromium.ui.interpolators.BakedBezierInterpolator;

import java.text.NumberFormat;

/**
 * A drawable for the tab switcher icon.
 */
public class TabSwitcherDrawable extends TintedDrawable {

    private static final int MAX_COUNT_TO_DISPLAY = 99;

    private final float mSingleDigitTextSize;
    private final float mDoubleDigitTextSize;

    private Animator mLastRollAnimator;
    private int mActualCount;
    private float mDisplayedCount;
    private TextDrawingCache mUpText;
    private TextDrawingCache mDownText;

    private static final Property<TabSwitcherDrawable, Float> COUNT_PROPERTY =
            new Property<TabSwitcherDrawable, Float>(Float.class, "") {
        @Override
        public void set(TabSwitcherDrawable view, Float value) {
            view.updateDisplayedCount(value);
        }

        @Override
        public Float get(TabSwitcherDrawable view) {
            return view.mDisplayedCount;
        }
    };

    private class TextDrawingCache {
        private int mNumber;
        private String mString;
        private final TextPaint mTextPaint = new TextPaint();
        private final Rect mTextBounds = new Rect();

        private TextDrawingCache() {
            mTextPaint.setAntiAlias(true);
            mTextPaint.setTextAlign(Align.LEFT);
            mTextPaint.setTypeface(Typeface.create("sans-serif-condensed", Typeface.BOLD));
        }

        private void draw(Canvas canvas, float alpha, float centerX, float centerY) {
            int saveCount = canvas.save();
            canvas.translate(centerX, centerY);
            canvas.saveLayerAlpha(mTextBounds.left - mTextBounds.centerX(),
                    mTextBounds.top - mTextBounds.centerY(),
                    mTextBounds.right - mTextBounds.centerX(),
                    mTextBounds.bottom - mTextBounds.centerY(),
                    (int) (256 * alpha), Canvas.ALL_SAVE_FLAG);
            canvas.drawText(mString, -mTextBounds.centerX(), -mTextBounds.centerY(), mTextPaint);
            canvas.restoreToCount(saveCount);
        }

        private void updateTextColor() {
            mTextPaint.setColor(mTint.getColorForState(getState(), 0));
        }

        private void updateNumber(int number) {
            if (mNumber == number && mString != null) return;

            mNumber = number;
            mString = getDisplayString(mNumber);

            mTextPaint.setTextSize(mString.length() <= 1
                    ? mSingleDigitTextSize : mDoubleDigitTextSize);
            mTextPaint.getTextBounds(mString, 0, mString.length(), mTextBounds);
        }
    }

    /**
     * Creates a {@link TabSwitcherDrawable}.
     * @param resources A {@link Resources} instance.
     * @param tint      Tinting {@link ColorStateList} used for the icon and the count texts.
     */
    TabSwitcherDrawable(Resources resources, ColorStateList tint) {
        super(resources, BitmapFactory.decodeResource(resources, R.drawable.btn_tabswitcher));
        mSingleDigitTextSize =
                resources.getDimension(R.dimen.toolbar_tab_count_text_size_1_digit);
        mDoubleDigitTextSize =
                resources.getDimension(R.dimen.toolbar_tab_count_text_size_2_digit);

        mUpText = new TextDrawingCache();
        mDownText = new TextDrawingCache();

        setTint(tint);
        updateDisplayedCount(0);
    }

    /**
     * Update the visual state based on the number of tabs present.
     * @param count The number of tabs.
     */
    void setCount(int count) {
        mActualCount = count;

        if (mLastRollAnimator != null) mLastRollAnimator.cancel();

        mLastRollAnimator = ObjectAnimator.ofFloat(this, COUNT_PROPERTY,
                Math.min(count, MAX_COUNT_TO_DISPLAY + 1));
        mLastRollAnimator.setInterpolator(BakedBezierInterpolator.TRANSFORM_CURVE);
        mLastRollAnimator.setDuration((long) (mLastRollAnimator.getDuration()
                * Math.sqrt(Math.abs(count - mDisplayedCount))));
        mLastRollAnimator.start();
    }

    /**
     * @return The current tab count this drawable is displaying.
     */
    @VisibleForTesting
    public int getTabCount() {
        return mActualCount;
    }

    private void updateDisplayedCount(float count) {
        mDisplayedCount = count;
        mUpText.updateNumber((int) count);
        mDownText.updateNumber((int) count + 1);
        invalidateSelf();
    }

    private String getDisplayString(int number) {
        if (number <= 0) {
            return "";
        } else if (number > MAX_COUNT_TO_DISPLAY) {
            return ":D";
        } else {
            return NumberFormat.getIntegerInstance().format(number);
        }
    }

    @Override
    public void draw(Canvas canvas) {
        final float fraction = mDisplayedCount % 1.0f;
        final Rect drawableBounds = getBounds();

        mUpText.draw(canvas, 1.0f - fraction,
                drawableBounds.centerX(),
                drawableBounds.centerY() - (drawableBounds.height() / 3.0f * fraction));
        mDownText.draw(canvas, fraction,
                drawableBounds.centerX(),
                drawableBounds.centerY() + (drawableBounds.height() / 3.0f * (1.0f - fraction)));

        super.draw(canvas);
    }

    @Override
    protected boolean onStateChange(int[] state) {
        boolean retVal = super.onStateChange(state);
        if (retVal) {
            mUpText.updateTextColor();
            mDownText.updateTextColor();
            invalidateSelf();
        }
        return retVal;
    }

    @Override
    public void setTint(ColorStateList tint) {
        super.setTint(tint);
        mUpText.updateTextColor();
        mDownText.updateTextColor();
        invalidateSelf();
    }
}
