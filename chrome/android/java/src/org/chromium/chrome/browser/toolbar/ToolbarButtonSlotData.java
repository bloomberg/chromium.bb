// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.AnimatorSet;
import android.animation.ObjectAnimator;
import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.drawable.Drawable;
import android.support.v4.graphics.drawable.DrawableCompat;
import android.support.v7.content.res.AppCompatResources;
import android.view.View;
import android.view.View.OnClickListener;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.widget.TintedImageButton;
import org.chromium.ui.interpolators.BakedBezierInterpolator;

/**
 * This class can hold two buttons, one to be shown in browsing mode and one for tab switching mode.
 */
class ToolbarButtonSlotData {
    /** The button to be shown when in browsing mode. */
    public final ToolbarButtonData browsingModeButtonData;

    /** The button to be shown when in tab switcher mode. */
    public ToolbarButtonData tabSwitcherModeButtonData;

    /** Fade in/out time in milliseconds. */
    private static final int FADE_DURATION = 300;

    /**
     * @param browsingModeButton The button to be shown when in browsing mode.
     * @param tabSwitcherModeButton The button to be shown when in tab switcher mode.
     */
    ToolbarButtonSlotData(ToolbarButtonData browsingModeButton) {
        browsingModeButtonData = browsingModeButton;
    }

    /**
     * A class that holds all the state for a bottom toolbar button. It is used to swap between
     * buttons when entering or leaving tab switching mode.
     */
    static class ToolbarButtonData {
        private final Drawable mDrawable;
        private final CharSequence mLightAccessibilityString;
        private final CharSequence mDarkAccessibilityString;
        private final OnClickListener mOnClickListener;

        private final ColorStateList mLightTint;
        private final ColorStateList mDarkTint;

        /**
         * @param drawable The {@link Drawable} that will be shown in the button slot.
         * @param lightAccessibilityString The accessibility string to be used in light mode.
         * @param darkAccessibilityString The accessibility string to be used in dark mode.
         * @param onClickListener The listener that will be fired when this button is clicked.
         * @param context The {@link Context} that is used to obtain tinting information.
         */
        ToolbarButtonData(Drawable drawable, CharSequence lightAccessibilityString,
                CharSequence darkAccessibilityString, OnClickListener onClickListener,
                Context context) {
            mLightTint = AppCompatResources.getColorStateList(context, R.color.light_mode_tint);
            mDarkTint = AppCompatResources.getColorStateList(context, R.color.dark_mode_tint);

            mDrawable = drawable;
            mLightAccessibilityString = lightAccessibilityString;
            mDarkAccessibilityString = darkAccessibilityString;
            mOnClickListener = onClickListener;
        }

        /**
         * @param imageButton The {@link TintedImageButton} this button data will fill.
         * @param isLight Whether or not to use light mode.
         */
        void updateButton(TintedImageButton imageButton, boolean isLight) {
            imageButton.setOnClickListener(mOnClickListener);
            updateButtonDrawable(imageButton, isLight);
        }

        /**
         * @param imageButton The {@link TintedImageButton} this button data will fill.
         * @param isLight Whether or not to use light mode.
         */
        void updateButtonDrawable(TintedImageButton imageButton, boolean isLight) {
            ObjectAnimator fadeOutAnim =
                    ObjectAnimator.ofFloat(imageButton, View.ALPHA, 1.0f, 0.0f);
            fadeOutAnim.setDuration(FADE_DURATION / 2);
            fadeOutAnim.setInterpolator(BakedBezierInterpolator.FADE_OUT_CURVE);

            ObjectAnimator fadeInAnim = ObjectAnimator.ofFloat(imageButton, View.ALPHA, 0.0f, 1.0f);
            fadeInAnim.setDuration(FADE_DURATION / 2);
            fadeInAnim.setInterpolator(BakedBezierInterpolator.FADE_IN_CURVE);
            fadeInAnim.addListener(new AnimatorListenerAdapter() {
                @Override
                public void onAnimationStart(Animator animator) {
                    if (mDrawable != null) {
                        DrawableCompat.setTintList(mDrawable, isLight ? mLightTint : mDarkTint);
                    }
                    imageButton.setImageDrawable(mDrawable);
                    imageButton.setContentDescription(
                            isLight ? mLightAccessibilityString : mDarkAccessibilityString);
                    imageButton.invalidate();
                }

                @Override
                public void onAnimationEnd(Animator animator) {
                    if (mDrawable != null) imageButton.setEnabled(true);
                }
            });

            imageButton.setEnabled(false);
            AnimatorSet animatorSet = new AnimatorSet();
            animatorSet.playSequentially(fadeOutAnim, fadeInAnim);
            animatorSet.start();
        }

        static void clearButton(TintedImageButton button) {
            ToolbarButtonData emptyButtonData =
                    new ToolbarButtonData(null, "", "", null, button.getContext());
            emptyButtonData.updateButton(button, false);
        }
    }
}
